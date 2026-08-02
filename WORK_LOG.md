# Work Log

## Sun 2 Aug 2026

### `experiments/in_rs485` — survive-reboot autostart, and a sweep-desync bug it exposed

#### Autostart on boot: `arduino-app-cli properties set default`, not a Docker restart policy

Wanted `mise run start:athena` to survive a reboot, not just a manual stop.
First guess was wrong — checked `arduino-app-cli app`/`app start`/`system`/
`config` `--help` on the real hardware (SSH to `athena`/`briana`), found no
autostart flag there, and wrote a Docker `restart: unless-stopped` policy
workaround instead. That was the wrong layer: the CLI has its own native
mechanism under a subcommand I hadn't checked, `properties`:

```sh
ssh briana 'arduino-app-cli app list'
# -> user:vape-cell-EV-in_rs485   (the "user:" prefix marks it apart from
#    bundled "examples:..." apps)
ssh briana 'arduino-app-cli properties get default'
ssh briana 'arduino-app-cli properties set default user:vape-cell-EV-in_rs485'
# `properties set --help` confirms: "Use 'none' to unset a property"
```

Replaced the Docker-based tasks with `set-start-on-reboot:<board>` /
`unset-start-on-reboot:<board>` in `mise.toml`, wrapping
`properties set default user:vape-cell-EV-in_rs485` / `properties set default
none`.

#### Bring-up bug #8: an independent reboot desynced the baud sweep, silently

With autostart wired up, a reboot of just one board (exactly the scenario
autostart is for) surfaced a real bug: 9600 (home baud) kept working fine
after the reboot, but the sweep itself stalled — with no obvious reason why,
since the home link that carries the handshake was demonstrably fine.

Root cause: the control frame that tells SECONDARY "advance to the next
baud" (see the auto-sweep feature above) was a bare synchronisation pulse —
it didn't carry the target baud itself, just a "go" signal, and each side
independently tracked its own position in `BAUD_TABLE[]` via its own
`sweepIndex`. That's fine as long as both counters started from the same
place and never diverged — but a reboot resets one side's counter to 0
while the other side's (never rebooted) is however far into the sweep it
had already gotten. The handshake still succeeds every time (it's just a
pulse at the reliable home baud), so nothing *looks* broken — both sides
just quietly switch to two different bauds, and neither side can tell.

**Fix:** stopped treating the control frame as a bare pulse and put the
actual target baud in its payload instead (`buildCtrlFrame`/
`ctrlFrameTargetBaud`, reusing the same 23-byte layout/checksum/echo
machinery as data frames). PRIMARY is now the sole authority on the sweep
position; SECONDARY no longer keeps its own `sweepIndex` at all, it just
does whatever baud it's told. Removes the entire class of bug rather than
patching the specific desync.

Also added visibility into "what's coming up next" at 9600 (home baud),
since that's exactly the state where the old bug was invisible:
`get_stats()` now exposes `nextBaud` (PRIMARY only), PRIMARY's periodic
status line prints `next=...`, and the dashboard shows a NEXT stat next to
BAUD in the header (hidden on SECONDARY, which only learns the target baud
the moment a control frame actually arrives, not ahead of time).

**Not yet done:** re-flash both boards and confirm this actually survives a
real independent reboot of just one side — the fix is code-complete but
unverified on hardware as of this entry.

## Fri 31 Jul 2026

### RS-485 speed test — `experiments/in_rs485`, 2× UNO Q + 2× EVAL-ADM3068EEBZ

New experiment, structured the same way as `green-brain`'s
`in_can_garden_hub` (app.yaml + sketch/ + python/), with matching
`upload:` / `start:` / `stop:` mise tasks. Not part of the vape/train build —
this is a standalone comms bench-test to see what RS-485 throughput is
actually achievable, before deciding whether it's worth using for
cell-monitor node telemetry.

**Files added:**

- [experiments/in_rs485/app.yaml](experiments/in_rs485/app.yaml)
- [experiments/in_rs485/sketch/sketch.ino](experiments/in_rs485/sketch/sketch.ino)
  — the STM32/Zephyr firmware, same for both nodes except one `#define`
- [experiments/in_rs485/sketch/sketch.yaml](experiments/in_rs485/sketch/sketch.yaml)
- [experiments/in_rs485/python/main.py](experiments/in_rs485/python/main.py)
  — polls stats off the Bridge, serves a live SSE dashboard
- [mise.toml](mise.toml) — `upload:in_rs485-a` / `-b`, `start:`/`stop:` pairs

#### The "5 Mbps" claim — actually 50 Mbps, and it doesn't mean what it sounds like

Checked the reference doc
([EVAL_ADM3068E_RS-485Tx.pdf](reference/EVAL_ADM3068E_RS-485Tx.pdf), UG-1540):
the ADM3068E is rated **50 Mbps**, not 5 — easy number to misremember. But
that figure is measured at the transceiver's own pins, effectively zero cable
(bench loopback / test points on the eval board itself). It is not a claim
about what survives a real cable run.

**Why twisted pair specifically, and why it matters at speed:**

- RS-485 is a *differential* signalling standard — data is the voltage
  difference between two wires (A/B), not one wire referenced to ground.
  Twisting the pair matters because any noise picked up couples onto both
  wires roughly equally (common-mode) and cancels out at the receiver, which
  only looks at the difference. A flat, untwisted 2-wire cable doesn't reject
  common-mode noise anywhere near as well, and — just as importantly — its
  characteristic impedance is inconsistent, so it won't match the 120 Ω
  termination resistors on the board (RT1/RT3, jumpers LK3/LK5). Mismatched
  impedance causes reflections, which get worse as edge rates get faster
  (i.e. exactly when you push the baud rate up).
- The classic RS-485 rate-vs-distance rule of thumb (24 AWG twisted pair,
  properly terminated): roughly 10 Mbps at ~12 m, 1 Mbps at ~120 m, 100 kbps
  at up to ~1200 m. It's a rough trade-off curve, not a hard spec — but the
  shape is the point: **speed and reach trade off directly**. Getting
  anywhere near 50 Mbps means a cable a few tens of centimetres long at
  most, basically board-to-board on a bench, not a run across a room.
- So: yes, twisted pair, terminated at both physical ends (not every node —
  see wiring below), and the real ceiling for *this* experiment is likely to
  be set by the cable length we actually use, well before the transceiver's
  50 Mbps figure is relevant.

**Another likely bottleneck: the UNO Q's own UART.** Rather than guess a
ceiling, the sketch sweeps baud rates upward (115200 → 3 Mbps) and reports
where frames actually stop decoding cleanly — that might turn out to be the
MCU's UART, not the cable, not the transceiver. Worth checking with a scope
on the twisted pair if the numbers look surprising.

#### Board wiring (EVAL-ADM3068EEBZ jumpers, per UG-1540 Table 1)

Each node uses one eval board, wired half-duplex (single twisted pair, not
the full 4-wire full-duplex mode). **Shipped/factory default is LK1=B,
LK2=B, LK4/LK6 open** — a deliberate safe-bench state (receiver always on,
driver always off, full-duplex pairs not bonded together), not our config.
Changes needed from default:

- **LK2: B → C (required).** Factory ties DE→GND, driver permanently
  disabled — no transmission is possible at all until this moves, since DE
  needs to come from our external control line instead.
- **LK1: B → D (simplification, not strictly required).** Ties RE to the
  same node as DE, so *one* GPIO drives both: driver enabled ⇒ receiver
  disabled. Avoids self-echo — with LK1 left at the factory B (receiver
  always on), the node also hears its own transmitted bytes loop back into
  RO while driving, which works but means the firmware has to recognize and
  discard its own echo rather than mistaking it for the reply.
- **LK4 + LK6: open → inserted (this is the half-duplex/full-duplex
  choice).** Folds the chip's full-duplex pairs (A/B in, Y/Z out) into one
  half-duplex pair per node: A+Y become one bus wire, B+Z the other. This is
  the same jumper pair the datasheet uses for its own loopback self-test
  (Figure 3) — applied per-node here instead, so each node's driver and
  receiver share one physical pair. **Leaving LK4/LK6 open instead (the
  factory state) gives true 4-wire full-duplex**: A/B stays a dedicated
  receive pair, Y/Z a dedicated transmit pair, permanently — DE tied high
  (LK2=A) and RE tied low (LK1=B) forever, both sides transmit and receive
  simultaneously with no turnaround latency at all, at the cost of needing
  *two* twisted pairs, crossed (node A's Y/Z → node B's A/B, and vice versa)
  instead of one. Since this is point-to-point (2 nodes, not a real
  multidrop bus), full-duplex is a legitimate variant to try later — it
  isolates raw UART+cable bandwidth from protocol-turnaround overhead. Half
  duplex first because it's the more realistic case for an eventual
  multi-node cell-monitor bus.
- **LK3 inserted, LK5 left open** — 120 Ω termination at each node. Since
  LK4/LK6 bond RT1 (across A/B) and RT3 (across Y/Z) onto the same two
  wires, inserting *both* would put 60 Ω at each end instead of the standard
  120 Ω — so only one of the pair should be in.
- **LK7 inserted** — ties VIO to VCC, so run VCC at 3.3 V from the UNO Q's
  header to keep RS-485 logic levels matched to the board.
- **Bus wiring: A/B only.** With LK4/LK6 bonding Y/Z onto A/B at each node,
  the Y/Z terminals carry the same signal — no need to also wire them
  externally. Just node A's **A ↔ B** node B's A, node A's **B ↔** node B's
  B (one twisted pair), plus a shared ground/shield wire.
- **The gold SMA jack (`DI_`) is a second path into DI, not needed here.**
  Per the UG, it exists so a lab signal generator can feed DI through 50 Ω
  coax for a clean edge at the chip's rated 50 Mbps bench-loopback tests
  (Figure 3) — a different kind of "impedance matters at speed" concern than
  the bus twisted pair, this time on the single-ended input side. The UNO
  Q's UART is driving DI at a few Mbps at most over a discrete wire, nowhere
  near where that connector matters — wire DI to the plain **J3 screw
  terminal** instead, leave the SMA jack alone.

#### UNO Q pin notes — corrected against the official pinout, not just the devicetree

First pass at this went off the compiled Zephyr devicetree alone
(`zephyr-arduino_uno_q_stm32u585xx.dts`) and got the header mapping wrong —
its `arduino_header` `gpio-map` array turned out to list A0–A5 *before*
D0–D13, not after, which flipped which physical pin was which. Re-checked
against Arduino's own
[UNO_Q_full_pinout.pdf](reference/UNO_Q_full_pinout.pdf) (all 4 pages) and
corrected below — that doc is the source of truth for anything wiring-related
on this board, the devicetree is only good for confirming a peripheral is
enabled at the OS level, not where it physically lands.

- **The physical UART is `usart1` = D1 (TX, PB6) / D0 (RX, PB7).** This is
  the *only* hardware UART exposed on any connector on this board — checked
  the main D0–D21/A0–A5 header, QWIIC, SPI2, JCTL, LED matrix, and the
  JMISC/JMEDIA high-density connectors, nothing else is labelled UART/USART.
- **`Serial1` (`lpuart1`, PG7 TX / PG8 RX) doesn't appear anywhere in the
  official pinout at all** — not on the main header, not on any of the
  advanced-section connectors. It's enabled in the devicetree
  (`status = "okay"`) but there's nowhere to physically wire it on this board
  revision. **Dropped from the design.**
- **Direction-control GPIO = D2 (PB3).** Free — not a default I2C/SPI/UART
  pin (its only alternate function, TIM2_CH2, isn't engaged by plain
  `pinMode`/`digitalWrite`).

#### `Serial` doesn't mean what it looks like it means on this board

First on-device build attempt failed here:

```
sketch.ino:108:22: error: 'class BridgeMonitor<>' has no member named 'end'
```

Turned out `Serial.end()` (called from what was originally `RS485_SERIAL`,
`#define`d to `Serial`) doesn't exist because **`Serial` isn't the hardware
UART at all once `Arduino_RouterBridge.h` is included.** The library ships
`Arduino_RouterBridge/src/monitor.h` with:

```cpp
#ifdef ARDUINO_ROUTERBRIDGE_PROVIDES_SERIAL
extern BridgeMonitor<> Serial;   // aliased to the same object as Monitor
#endif
```

— so `Serial` is silently redefined to the same internal MPU↔MCU RPC channel
as `Monitor`, not the `usart1` peripheral. This only shows up once the actual
on-device toolchain compiles the sketch (this can't be caught by a local
`arduino-cli compile`, which fails earlier and differently — see below); the
devicetree and the generic core headers on a dev machine give no hint of it,
since the override only exists inside the RouterBridge library itself.

**Fix: bypass the Arduino `Serial`/`Stream` abstraction and talk to `usart1`
directly via the Zephyr UART driver API** — the same approach the CAN
experiment already uses for FDCAN instead of an Arduino CAN wrapper class:

```cpp
#include <zephyr/drivers/uart.h>
static const struct device* rs485_dev = DEVICE_DT_GET(DT_NODELABEL(usart1));
// uart_configure(rs485_dev, &cfg)   — set baud at runtime
// uart_poll_out(rs485_dev, byte)    — blocking single-byte write
// uart_poll_in(rs485_dev, &byte)    — non-blocking single-byte read, 0=got one
```

`uart_poll_out()` only guarantees the byte was handed to the peripheral, not
that the stop bit has actually left the wire — there's no simple polling
"flush" in this API — so the sketch waits ~10 bit-times (recomputed whenever
the baud changes) before dropping DE, instead of relying on `Serial.flush()`.

Wire: EVAL board **DI ← UNO Q D1**, **RO → UNO Q D0**, direction control ←
UNO Q D2.

#### Bring-up bug #2: blocking writes without polling RX in between silently overrun

With the raw-UART rewrite building and running, self-test (LK1=B/LK2=A
factory jumpers + LK4/LK6 for the loopback, per the earlier bring-up plan)
showed 100% timeouts at every baud. Bypassing the transceiver entirely — a
bare wire from D1 straight to D0 on the UNO Q itself — still showed 0 acked,
which ruled out the transceiver/jumpers/bus wiring completely and pointed
at the STM32 UART code. Added a `rawBytes` counter (any byte seen via
`rs485ReadByte()`, matched or not) to tell "nothing arrives" apart from
"garbage arrives," and it came back **exactly 2 bytes per 23-byte frame
sent, every time, regardless of baud** (62/31, 138/69, 214/107 — always a
clean 2.0 ratio).

That precision — constant regardless of baud — ruled out signal integrity
and pointed at something structural: this UART has essentially no RX
buffering in polling mode, roughly a 2-byte pipeline (one byte in the shift
register, one in the holding register) before an unread byte causes an
overrun and drops everything after it. The original code sent the whole
23-byte frame in one blocking loop (`rs485Write`) *before* ever calling
`uart_poll_in()` — so on a looped-back wire, the echo starts arriving on RX
immediately as we transmit, but nothing drains it until the write finishes.
Only the ~2 bytes that fit in that tiny hardware pipeline survive; the rest
vanish to overrun.

**Fix:** interleave send and receive instead of treating them as two
sequential phases — call `uart_poll_in()` after every single
`uart_poll_out()` during the frame write, not just after. This mainly
matters for the self-test/loopback configs where TX and RX are the same
wire; in real half-duplex operation (LK1=D, receiver disabled while
transmitting) the interleaved read attempt just finds nothing to read
during TX, so it's a no-op there — cheap to leave in place either way.
Only applied to PRIMARY's send path so far (SECONDARY's echo-write happens
after it's already fully assembled and validated an inbound frame, with
nothing else arriving concurrently in the real 2-node design, so it doesn't
need the same treatment).

**Open TODOs (revised):**
- [x] Re-ran the self-test with this fix: `acked` climbs — the interleave fix
      works. Confirmed a clean RTT-vs-baud curve tracking bit-time up to
      1 Mbaud, then flattening at ~131 µs for both 2 Mbaud and 3 Mbaud — a
      real software-polling-loop ceiling, reached before the transceiver or
      any cable ever became the bottleneck. Also found a reproducible,
      non-monotonic partial-loss pattern across baud (460800 ≈49%, 921600/
      1M ≈67%, 2M ≈85%, 3M bursts then flatlines to exactly 0% mid-step)
      that persisted identically even after adding `uart_err_check()`
      (see next entry) — still unexplained, and since it only showed up in
      the self-loop bench config (never in the real 2-node case, see below),
      not chased further for now.

#### Bring-up bug #3: two independent boards can't sync a baud sweep off `millis()`

Moved to the real 2-node test (LK1=D, LK2=C, second board wired via twisted
pair, A↔A/B↔B) and got **zero received bytes on both boards, at every baud,
for the entire run** — not the partial/degrading pattern from the self-test,
total silence. `uart_err_check()` (added to rule out a latched overrun/
framing/parity fault) reported 0 the whole time on the self-test too, so
that theory was cleanly ruled out before this even came up.

Root cause, once asked directly "how do these two boards know to be on the
same baud at the same time?": **they don't.** The original design stepped
through `BAUD_TABLE` on a timer — `millis() / STEP_MS % NUM_BAUDS` — but
`millis()` counts from each board's own power-on with no clock sync between
them at all. Flash and start the two boards with separate `mise run`
commands a few seconds apart (completely normal) and their step timers are
permanently out of phase — over a 7-step, 28-second full sweep, being off
by even a couple of seconds means the two boards spend nearly the entire
test on *different* baud rates from each other. The self-loop bench test
never surfaced this because it was one board talking to itself — no second
clock to be out of phase with. The original design's own comment ("expect a
handful of dropped frames right at each step boundary") badly underestimated
this — it's not a boundary effect, it can be the dominant failure mode.

**Fix:** dropped the automatic timer-driven sweep entirely. `sketch.ino` now
sets one fixed `#define TEST_BAUD` in `setup()` and stays there — test a
different rate by changing it, re-uploading to **both** boards, and
restarting them **together**. Loses the unattended multi-baud sweep
convenience; a real fix (e.g. in-band baud-change announcements sent over
the link itself before both sides switch) is possible but is meaningfully
more machinery than this experiment needs — removing the sync problem
outright was the simpler correct move.

**Open TODOs (revised again):**
- [ ] Re-run the real 2-node test with fixed-baud + synchronized restart and
      confirm `acked`/`echoed` actually climb on both boards
- [ ] If that works, step `TEST_BAUD` up manually run-by-run (both boards,
      each time) to find the real 2-node ceiling with actual cable in the
      loop — this is the number that actually answers the original "will it
      work over wire" question, not the self-loop numbers above
- [ ] Revisit the self-test's non-monotonic partial-loss pattern only if the
      same shape reappears on the real bus — if it doesn't, it was a
      self-loop test artifact, not worth more time

#### Bring-up bug #4 (real hardware): missing GND wasn't it, direction-control wiring wasn't it either — it was the polling architecture itself

After the sync fix, the real 2-node bus still showed total silence, then (once
a missing DE/RE wire and a missing inter-board GND were both found and fixed)
heavy corruption rather than silence — `rxBad`/`uartErrors` climbing fast on
SECONDARY, `rawBytes=0` forever on PRIMARY. Isolated it further with a direct
D1↔D0 crossover between the two boards' MCUs, bypassing the ADM3068E/jumpers
entirely — same corruption, which ruled out the transceiver and jumper wiring
completely and pointed at the software/UART layer instead.

The actual cause: found by comparing against a known-working UART PoC in
`../sentinel-box/experiments/in_uart_comms` (different MCU, MAX32630, not
directly portable) — its own comment credits a **32-byte hardware RX FIFO**
for tolerating a 10 ms blocking gap in its main loop. Our STM32U585 `usart1`
has essentially none of that in polling mode (the ~2-byte pipeline from
bring-up bug #2). Worse: Zephyr is a real preemptive RTOS, unlike that PoC's
bare-metal loop — our `loop()` can be preempted by other threads (Bridge/RPC,
USB, kernel housekeeping) for however long the scheduler decides, and with
only a 2-byte hardware cushion behind it, any such gap silently drops data.
That explains why corruption stayed heavy even at 9600 baud, where nominal
timing margin should have made errors rare — the limiting factor was never
baud, it was scheduler jitter racing a nearly-nonexistent hardware buffer.

**Fix:** moved RX off polling entirely. `sketch.ino` now registers a real
UART ISR (`uart_irq_callback_user_data_set` + `uart_irq_rx_enable`) that
drains the peripheral into a 256-byte ring buffer the instant a byte arrives,
independent of what `loop()` is doing. `rs485ReadByte()` just drains that
ring. Whether this actually helps depends on `CONFIG_UART_INTERRUPT_DRIVEN`
being enabled in the on-device firmware — no way to check that from a dev
machine, only from the result. The result: SECONDARY went from continuously
climbing errors to **8 total errors (startup transient) then zero across the
next ~490 frames, rxBad staying at 0** — the interrupt-driven rewrite worked.

**Bring-up bug #5, found immediately after: fixed timeout doesn't scale with
baud.** With SECONDARY now clean, PRIMARY still showed `acked` freezing while
`sent` kept climbing, and `rtt_us` stuck at exactly 48197 µs — suspiciously
close to the 50000 µs `POLL_TIMEOUT_US` constant. At 9600 baud a bare 23-byte
round trip (there and back, 10 bit-times/byte) alone takes ~48 ms, leaving
almost no slack in a fixed 50 ms budget for SECONDARY's own turnaround. Late
echoes were routinely missing the deadline, then landing during the *next*
round's window instead — a real, checksum-valid frame with the wrong `seq`,
counted as `badEcho` (which wasn't even in the Monitor print line, only in
`get_stats` — invisible in the logs we were looking at). Fixed by computing
the timeout per-baud instead of hardcoding it: 5× the bare-minimum round-trip
time, recalculated in `rs485SetBaud()` alongside `txDrainUs`. Also added
`badEcho` to the visible Monitor line so this class of bug shows up directly
next time instead of needing to be inferred from what doesn't add up.

**Re-ran at 9600 with the scaled timeout: clean.** `sent == acked` on every
single line (0 timeouts, 0 badEcho), SECONDARY's `uartErrors` settled at 10
(startup transient, same shape as before) then flat zero across hundreds of
frames, `ringOverflow` stayed 0 throughout. `rtt_us` sat steady around
~48.2 ms — matches the theoretical minimum round trip at 9600 baud almost
exactly, barely any overhead beyond the wire time itself. This is the first
fully clean, verified two-node `RS485` link over real wire, transceivers,
and jumpers — every prior failure really was one of: sync, wiring, the
`Serial` trap, the missing RX buffer, or the fixed timeout, and none of them
were the bus itself.

**Jumped straight to 115200 (skipping the incremental steps): clean, after a
startup-race red herring.** First ~1319 attempts all timed out — PRIMARY
polling before SECONDARY had finished booting, a one-time race, not an
ongoing problem. Once both sides were actually up, `Δsent == Δacked` on
every subsequent window (100%), `rtt_us` settled at ~4.05–4.1 ms (theoretical
minimum at 115200 is ~4.0 ms). Lesson for next time: start/reset SECONDARY
first, give it a few seconds, then start PRIMARY, to avoid the confusing
wall of timeouts at the front of every log.

**Jumped to 1,000,000 baud: same startup race, then a new, real bug.** After
the boot race settled, `acked` climbed for a while (up to 667) then **froze
completely** while `badEcho` climbed rapidly (14 → 814 → 2188 → 3572+) and
`timeouts` barely moved — every subsequent round landing as a
checksum-valid frame with the *wrong sequence number*, persistently, with no
self-correction. Different shape from the earlier timeout bug (which
resolved itself once the deadline was widened) — this one never recovers on
its own.

Root cause: PRIMARY's receive loop never resyncs on the sync byte the way
SECONDARY's already does —

```cpp
// SECONDARY — resyncs before accepting anything
while (rs485ReadByte(b)) {
    if (filled == 0 && b != SYNC_BYTE) continue;
    ...

// PRIMARY (before fix) — fills positionally, no resync at all
while (...) { if (rs485ReadByte(b)) { rx[got++] = b; ... } }
```

One stray/late byte left in the ring buffer at the start of any round (a
late echo tail arriving just after a timeout's drain loop finished, say)
permanently shifts PRIMARY's read alignment by that many bytes — and with no
resync mechanism, *every* subsequent round reads at the wrong offset
forever. More likely to get triggered at higher baud (tighter timing), but
the bug itself has nothing to do with baud — it's a missing framing
safeguard that SECONDARY happened to already have and PRIMARY didn't.

**Fix:** added the identical sync-byte resync to PRIMARY's read loop — skip
bytes until one matches `SYNC_BYTE` before starting to accumulate a frame,
same as SECONDARY. Not yet re-tested on hardware as of this entry.

**Open TODOs:**
- [ ] Re-run at 1,000,000 baud with the resync fix and confirm `acked`
      keeps climbing indefinitely instead of freezing
- [ ] Continue upward from there (e.g. straight to 2,000,000 / 3,000,000) to
      find where the *real* two-node ceiling with actual twisted-pair cable
      in the loop actually sits — that number is the honest answer to "will
      it work over wire," not the self-loop numbers or the 50 Mbps datasheet
      figure
- [ ] Once a ceiling is found, put a scope on the bus at that rate to see
      whether it's timing/signal-integrity limited (matches the self-test's
      earlier non-monotonic pattern) or something else new
- [ ] Fold real throughput numbers into the blog post once known

**Jumped to 1,000,000, then straight to 3,000,000 baud: resync fix worked
(no more permanent freeze), but a new timeout-formula gap showed up at the
extreme end.** At 3,000,000 baud, `badEcho` settled into a high but
*non-frozen* rate (climbing proportionally with `sent`, ~80%+ of attempts)
instead of the earlier dead stop — the resync fix is doing its job, nothing
gets permanently stuck anymore. But SECONDARY's own numbers stayed solid
(~95%+ clean `rxOk` vs `rxBad`), so the remaining loss was back on PRIMARY's
side of the timing, not framing.

The tell: successful `rtt_us` values were landing at 529–742 µs, right up
against the computed timeout ceiling (5× the bare wire round trip ≈ 765 µs
at this baud). The 5× wire-time formula has a blind spot: it assumes
overhead scales down with baud alongside wire time, but SECONDARY's real
turnaround (sync detection, checksum, DE/RE GPIO switching, scheduler
jitter) costs roughly the same wall-clock time *regardless* of baud — it's
CPU cycles, not bit-times. Trivial next to a 48 ms wire time at 9600 baud;
the dominant cost once wire time drops to ~150 µs at 3 Mbaud. Same root
category of bug as bring-up bug #5, just showing up again at the opposite
end of the baud range the first fix didn't cover.

**Fix:** added a fixed 5 ms floor on top of the wire-time-scaled portion of
`pollTimeoutUs`, so the budget stops collapsing toward zero as baud climbs.
Not yet re-tested on hardware as of this entry.

**Open TODOs:**
- [ ] Re-run at 3,000,000 baud with the timeout floor and confirm `badEcho`
      drops back down and `acked` tracks `sent` closely
- [ ] Once a real ceiling is found (bandwidth- or signal-integrity-limited,
      not a software timing artifact), that's the number worth writing up
- [ ] Put a scope on the bus at whatever that ceiling turns out to be
- [ ] Fold real throughput numbers into the blog post once known

#### Bring-up bug #7: after a wiring mix-up on SECONDARY got fixed (D0↔D2
had been swapped), a genuinely new, 100%-reproducible corruption pattern
showed up — solved with a raw byte dump, not more counter-log guessing.

Added a per-frame raw hex dump (first 8 frames, capped) on SECONDARY to see
actual bytes instead of just derived pass/fail counts. Result: `sync`,
`seq`, `len`, and every payload byte were **completely correct** on every
logged frame — matched `(seq+i) mod 256` exactly, no exceptions. The only
wrong byte was the last one: it should have been the real per-frame
checksum but always read back as exactly `0xA5` — not random corruption
(which would vary), the *same specific valid byte* every time.

Adding the identical raw dump to PRIMARY's transmit side (showing bytes
right before `rs485Write()`) made the mechanism obvious: PRIMARY sent every
`seq` in order (0,1,2,3,4,5...), but SECONDARY only ever logged the *even*
ones (0,2,4,6...) — every odd frame vanished entirely, not as bad, just
gone. Since every frame starts with `sync=0xA5`, and frame N's real
checksum byte was never being captured, SECONDARY's parser was reading
frame N+1's sync byte into frame N's checksum slot instead, then scanning
through the rest of frame N+1 as noise (none of it coincidentally matching
0xA5) until it locked onto frame N+2's real sync — silently eating one
whole frame between each one that surfaced, every single round, no
exceptions.

That 100%-reproducible, zero-exceptions consistency was the tell that this
wasn't marginal timing noise — it was systematic. Root cause: `goReceive()`'s
`txDrainUs` margin (10 bit-times) assumed `uart_poll_out()` returning meant
the *previous* byte had finished transmitting. It doesn't — it only means
the previous byte moved from the holding register into the shift register.
So when the last `uart_poll_out()` call (the checksum byte) returns, the
byte before it can still have up to a full bit-time left to shift out, and
the checksum byte itself hasn't started yet — the real worst case is two
byte periods, not one. A margin that's short by roughly half explains a
failure that happens *every time*, not occasionally.

**Fix:** doubled `txDrainUs` to 20 bit-times.

**Confirmed on hardware:** re-ran at 9600 — completely clean. `sent == acked`
on every single line, `rxOk == echoed` matching exactly, `rxBad = 0`
throughout. This was the last bug in the chain — the 2-node link over the
real transceivers and cable genuinely works.

#### Feature: handshake-driven auto baud sweep, replacing the old fixed-baud-per-run model

With a fully clean 9600-baud baseline confirmed, rebuilt the sweep — but
properly this time, not the free-running-timer version from earlier in this
log (bring-up bug #4) that drifted two independently-booted boards out of
phase. New design:

- **9600 is now a permanent "home" baud**, not just a starting point. Both
  boards always return to it between tests.
- **PRIMARY commands the next baud over the reliable home link** — a control
  frame (same 23-byte layout as data frames, told apart by a different sync
  byte, `0x5A` instead of `0xA5`, reusing the exact same
  build/validate/echo/checksum code). SECONDARY echoes it back as an ack
  (identical mechanism to echoing data frames) and only then switches its own
  UART — the ack over a proven-reliable link is what actually coordinates
  the switch, not a shared clock.
- **Neither the target baud nor a duration needs to travel over the wire** —
  both boards share the same compiled-in `BAUD_TABLE[]`/`TEST_DURATION_MS`,
  so the control frame is really just a synchronisation pulse meaning
  "advance to your next table entry." Simpler and less to get wrong than
  packing/unpacking values into the payload.
- **Reverting to home needs no handshake at all** — each side independently
  counts down `TEST_DURATION_MS` (3 s) from the moment it entered the test
  baud, then reverts on its own. This only requires the two sides to enter
  the test baud within about one handshake round-trip of each other (easily
  true), not to stay in sync for an entire unattended run — so drift can
  never accumulate past a single ~3 s cycle, unlike the original design.
- Stats (`sent`/`acked`/`rxOk`/etc.) reset at the start of each phase and get
  printed as a `=== baud=... ... ===` summary line right before the reset,
  so each baud's result is self-contained instead of cumulative across the
  whole run.
- Removed the temporary raw-hex diagnostic dumps from bring-up bug #7 now
  that the fix is confirmed — not needed going forward.
- `sendAndWaitEcho()` factors out the send-and-wait-for-matching-echo logic
  shared by both normal pings and the control handshake, and now returns a
  three-way `EchoResult` (`ECHO_OK` / `ECHO_TIMEOUT` / `ECHO_BAD`) rather
  than a bool — that OK/timeout/bad-echo distinction was the actual
  diagnostic signal behind bring-up bugs #6 and #7, worth keeping visible
  rather than collapsing back into a single pass/fail.

Known minor edge case, not yet worth engineering around: if SECONDARY's ack
is lost after it's already switched baud, PRIMARY retries at home baud
(where SECONDARY no longer is) until either it gives up after
`CTRL_MAX_RETRIES` or SECONDARY's own local timer independently brings it
back home — self-healing within about one cycle, not a permanent stall.

**Open TODOs:**
- [ ] Confirm the auto-sweep actually runs cleanly end-to-end on hardware —
      designed and reviewed, not yet run
- [ ] Watch where in `BAUD_TABLE` (19200 → 3,000,000) results start
      degrading — that's the real answer to "how fast will this actually go
      over wire," now obtainable in one continuous unattended run instead of
      a manual restart per rate
- [ ] Once a ceiling is found, put a scope on the bus at that rate
- [ ] Fold real throughput numbers into the blog post once known

#### Protocol (sketch.ino) — superseded by the auto-sweep feature above; kept for history

Simple poll/response over the shared pair, not a continuous stream — this is
also how half-duplex RS-485 gets used in practice (Modbus RTU-style turnaround),
so it exercises real DE/RE switching at each baud rate rather than an
idealised one-directional blast:

1. **Primary** builds a 23-byte frame (sync + seq + 16-byte payload +
   XOR checksum), asserts DE/RE, writes it, flips back to receive, and
   waits up to 50 ms for the echo.
2. **Secondary** listens continuously, validates the checksum, and echoes
   the frame straight back.
3. Both sides re-derive "which baud rate are we on" from `millis() / 4000`
   independently — no handshake between them. They're not perfectly
   synchronised (whichever board powers on later drifts a few seconds
   out of phase), so expect a handful of dropped/garbled frames right at
   each 4-second step boundary — that's expected, not a bug. Steady-state
   numbers mid-step are what matters.
4. `get_stats` is exposed over the Bridge so the Linux-side Python app can
   show live sent/acked/timeout counts and last round-trip time per baud
   step on the small web dashboard (`python/static/index.html`).

**To build node B:** flip `#define IS_PRIMARY 1` to `0` at the top of
`sketch.ino` before uploading — the rest of the file is shared.

**Open TODOs:**

- [ ] Re-upload and confirm the raw Zephyr `uart_poll_in`/`uart_poll_out`
      rewrite actually builds and runs on athena — the fix above is inferred
      from reading the cached Zephyr headers locally, not yet confirmed by a
      real on-device build (which is the only build that's caught anything
      wrong so far)
- [ ] Get real mDNS hostnames for the two boards, replace the
      `rs485-a.local` / `rs485-b.local` placeholders in `mise.toml`
- [ ] Scope the DE/RE transition and the bus eye pattern at the top working
      baud rate — the 10 µs turnaround delay in the sketch is a guess, not
      from a timing spec (this eval-board UG doesn't include tPHZ/tPZH; that's
      in the full ADM3068E datasheet)
- [ ] Try a couple of different cable lengths/types (twisted pair vs flat
      ribbon) once the baud ceiling is known, to see how much distance the
      rate-vs-distance trade-off actually costs here

## Tue 28 Jul 2026

### Setup some new UNO Q

**NOTE:** _also noticed that the new Arduino App Lab mentions a new board
coming soon VENTUNO Q_
- Dragonwing IQ8 with NPU IQ-8275
- STM32H5F5 microcontroller
- RAM 16GB
- eMMC 64GB
- ready to run bricks:
  - ROS 2 (Robot Operating System 2) compatible
  - local LLMs like Qwen, VLM (Visual Language Model)
  - Melo TTS and Whisper
  - MediaPipe gesture recognition
  - YOLO-X object tracking
  - PoseNet for pose tracking
- check out Github for things still being developed

Add short cut ssh config to new board

```sh
# generate a key or use existing
ssh-keygen -o -a 100 -t ed25519

# with a specific name
find ~/.ssh/id_ed25519_UNO_Q*
~/.ssh/id_ed25519_UNO_Q
~/.ssh/id_ed25519_UNO_Q.pub

# copy to paste buffer
cat ~/.ssh/id_ed25519_UNO_Q.pub | pbcopy

# upload it to the UNO Q
ssh pollyanna.local
mkdir .ssh
chmod 700 .ssh
vi .ssh/authorized_keys
# paste it here
chmod 600 .ssh/authorized_keys

# helper to connect
cat << EOF >> ~/.ssh/config
Host athena
    # HostName athena.local
    HostName 192.168.68.132
    User arduino
    IdentityFile ~/.ssh/id_ed25519_UNO_Q
    IdentitiesOnly yes
EOF

# put it in shell mode
cat << EOF >> ~/.bashrc

  # VI everywhere
  set -o vi
EOF

# turn off graphical mode
sudo systemctl get-default
> graphical.target

sudo systemctl set-default multi-user.target

sudo systemctl get-default
multi-user.target

# restart
sudo shutdown 0
```

## TODO

NEXT:

- UNO Q dev env setup and update
- literature on re-using vape cells

**More on battery charging**

- [x] My Power Bank Rivals Commercial Ones?! Super Fast! (DIY or Buy) -
      GreatScott!

  [![
  My Power Bank Rivals Commercial Ones?! Super Fast! (DIY or Buy) -
  GreatScott!
](http://i.ytimg.com/vi/_WI9Nwqvplo/hqdefault.jpg)](https://youtu.be/_WI9Nwqvplo)
  - good watch
  - not really much about the power bank - recommends other videos
  - key is a USB C power bank board that he can power via his own power bank
  - he still uses a BMS to charge his powerbank, separate from the board above

- [x] The Surprising Flaws in 18650 Lithium-Ion Batteries - Adam Savage’s
      Tested

  [![
  The Surprising Flaws in 18650 Lithium-Ion Batteries - Adam Savage’s Tested
](http://i.ytimg.com/vi/-Y23nfAOiXQ/hqdefault.jpg)](https://youtu.be/-Y23nfAOiXQ)
  - Lumafield's Battery quality report:
    https://www.lumafield.com/battery-report
  - using Lumafield's CT scanner, previewing cheap batteries 18650's with badly
    aligned anodes.
  - felt more like an add for buying brand name cells
  - mention of a garage fire

- [x] Don't Fast Charge your Phone before Watching this Video! - GreatScott!

  [![
  Don't Fast Charge your Phone before Watching this Video! - GreatScott!
](http://i.ytimg.com/vi/iMn2yVoEqPs/hqdefault.jpg)](https://youtu.be/iMn2yVoEqPs)
  - have watched this before
  - main idea is the circuit to discharge a batter with a known current, so
    based on time, can calculate it's capacity
  - only discharge to 3V
  - rig to repeat to test impact on charge/discharge cycle

- [x] simple homemade BMS
  - **Homemade BMS - Balanced LiPo Charger Multiple Cells and Current Limit -
    Electronoobs**

  [![
  Homemade BMS - Balanced LiPo Charger Multiple Cells and Current Limit -
  Electronoobs
](http://i.ytimg.com/vi/qRVEJjk5B_g/hqdefault.jpg)](https://youtu.be/qRVEJjk5B_g)
  - shows need for separate charging of batteries
    - need balanced charging - get to 4.2v safely
    - using zener TL431 reference voltage using trim pot and BD140
    - fails over to using diodes as a load
    - LM317 to limit charging current 600mA
    - another LM317 to adjust voltage at 4.2V
  - https://electronoobs.io/shop/

- **BMS (Battery Management System) || DIY or Buy || Properly protecting
  Li-Ion/Li-Po Battery Packs - GreatScott!**

  [![
  BMS (Battery Management System) || DIY or Buy || Properly protecting
  Li-Ion/Li-Po Battery Packs - GreatScott!
](http://i.ytimg.com/vi/rT-1gvkFj60/hqdefault.jpg)](https://youtu.be/rT-1gvkFj60)
  - DIY or buy
    - [AliExpress: ~$4 3S 4S 40A 60A Li-ion Lithium Battery Protection Board
      BMS 12V 16.8V Overcharge Protection with Balance Enhanced for Drill
      DIY](https://www.aliexpress.com/item/1005012031202191.html)
  - https://github.com/stuartpittaway/diyBMS
  - https://github.com/chickey/diyBMS
  - pretty cool but a pretty difficult build
    - has a Web view via ESP32
    - can set voltage calibration
    - can discharge cells
    - have thermistor

- Most BMSs (Battery Management Systems) don't cut it... I Built a BETTER
  One. - Haase Industries

  [![
  Most BMSs (Battery Management Systems) don't cut it... I Built a BETTER One. - Haase Industries
](http://i.ytimg.com/vi/UUr-CJudg38/hqdefault.jpg)](https://youtu.be/UUr-CJudg38)
  - has a go at the Great Scott DIY BMS as well as simple homemade BMS by
    Electronoobs
  - although a bit hard to know what he is exactly on about appart from the
    waste of current in the alternative shunting and using some maybe
    slightly smarter components:
    - [Digikey: Infineon Technologies IQDH35N03LM5ATMA1 N-ch 30v 66A
      MOSFET](https://www.digikey.com.au/en/products/detail/infineon-technologies/IQDH35N03LM5ATMA1/21675799)
    - [Digikey: Texas Instruments BQ7791508PWR IC Batt Li-Ion
      3-5C](https://www.digikey.com/en/products/detail/texas-instruments/BQ7791508PWR/15856804)
    - [TI: bq77915 3-5S Low Power Protector Evaluation
      Module](https://www.ti.com/lit/ug/sluubu2b/sluubu2b.pdf)
    - [TI: BQ77915 ACTIVE 3-series to 5-series stackable ultra-low-power
      primary protector with autonomous cell
      balancing](https://www.ti.com/product/BQ77915)

### BMS landscape review — where this project sits

Three approaches reviewed:

|                       | Electronoobs   | GreatScott diyBMS      | Haase Industries     | This project                |
| --------------------- | -------------- | ---------------------- | -------------------- | --------------------------- |
| Per-cell voltage      | threshold only | yes (RS485)            | yes (BQ77915)        | yes (INA219)                |
| Balancing             | diode dump     | passive bleed resistor | autonomous (BQ77915) | TBD                         |
| Temperature           | no             | yes                    | yes                  | yes (NTC)                   |
| Data logging          | no             | web UI only            | no                   | CSV on eMMC                 |
| Discharge load        | diodes (waste) | passive bleed          | n/a                  | N-scale train (useful work) |
| Intelligence          | none           | web dashboard          | hardware IC          | ML layer on Linux           |
| Cell characterisation | **no**         | **no**                 | **no**               | **yes — the whole point**   |

**The gap none of them fill: cell characterisation for salvaged cells.**

Electronoobs just protects. GreatScott monitors a pack you already trust — the
passive bleed resistors make it only practical for stationary power walls.
Haase uses better ICs (BQ77915 does autonomous hardware balancing, genuinely
good) but doesn't log anything or characterise cells.

**The "extra" this project adds — a cell passport workflow:**

1. Charge CC/CV, log full CC→CV taper via INA219 → reveals actual charge
   acceptance
2. Discharge via constant-current load, log voltage curve → real capacity (mAh)
   and voltage sag under load (proxy for internal resistance)
3. Temperature profile from NTC during both → flags cells running hot
4. Repeat 3–5 cycles → capacity fade between cycles flags dying cells
5. Score each cell: capacity vs rated, thermal rise, fade rate → keep / caution
   / discard

**The UNO Q angle:** once a corpus of good and bad discharge curves exists,
train a small anomaly model on the Linux side to classify new cells
automatically. No hobbyist BMS does this.

**The train-as-discharge-load:** instead of burning energy in resistors, the
discharge IS useful work. Energy-per-lap becomes a real metric — battery dies,
train stops, you know how much was stored.

**Honest note on Haase's BQ77915:** hardware-level protection and balancing at
silicon speed is genuinely better for a final pack than software cutoffs. Not in
competition — BQ77915 handles the protection layer, UNO Q handles the
characterisation and intelligence layer above it.

- **How to keep LiPos from burning down your house (safe lipo charging) -
  Joshua Bardwell**

  [![
    How to keep LiPos from burning down your house (safe lipo charging) - Joshua Bardwell
  ](http://i.ytimg.com/vi/n3urBpFIBgY/hqdefault.jpg)](https://youtu.be/n3urBpFIBgY)
  - good overview of battery pack sizing
  - the idea of charging outside and keep it attended

- [ ] reasonable build online with lots of build tricks of a power bank
  - How to make Super 20,000 mAh Power Bank (120W) - DIY fast charge Power
    Bank - Penguin DIY

  [![
   How to make Super 20,000 mAh Power Bank (120W) - DIY fast charge Power
   Bank - Penguin DIY
  ](http://i.ytimg.com/vi/xAyOeGTdyX4/hqdefault.jpg)](https://youtu.be/xAyOeGTdyX4)

- [ ] might have some ideas
  - I built an ADVANCED Battery Bank (Open Source) - Ben Makes Everything

  [![
  I built an ADVANCED Battery Bank (Open Source) - Ben Makes Everything
](http://i.ytimg.com/vi/i2HRpcJS6Vk/hqdefault.jpg)](https://youtu.be/i2HRpcJS6Vk)

- [ ] nice build and a bunch of extra boards
  - I Built My Dream Power Bank | CNC Aluminum - Penguin DIY

  [![
  I Built My Dream Power Bank | CNC Aluminum - Penguin DIY
](http://i.ytimg.com/vi/r30Q7xbooYs/hqdefault.jpg)](https://youtu.be/r30Q7xbooYs)

**More on vape cell reuse**

- [ ] Can you reuse these batteries? - Becky Stern

  [![
  Can you reuse these batteries? - Becky Stern
](http://i.ytimg.com/vi/qiUyMLdVyfI/hqdefault.jpg)](https://youtu.be/qiUyMLdVyfI)

- [ ] inside a disposable with charging port - bigclivedotcom

  [![
  inside a disposable with charging port - bigclivedotcom
](http://i.ytimg.com/vi/hBgaqY9CG3g/hqdefault.jpg)](https://youtu.be/hBgaqY9CG3g)

- [ ] What _Really_ happens to used Electric Car Batteries? - (you might be surprised) - JerryRigEverything

  [![
  What *Really* happens to used Electric Car Batteries? - (you might be
  surprised) - JerryRigEverything
](http://i.ytimg.com/vi/s2xrarUWVRQ/hqdefault.jpg)](https://youtu.be/s2xrarUWVRQ)

- [x] I Powered My House Using 500 Disposable vapes - Chris Doel

  [![
  I Powered My House Using 500 Disposable vapes - Chris Doel
](http://i.ytimg.com/vi/dy-wFixuRVU/hqdefault.jpg)](https://youtu.be/dy-wFixuRVU)
  - massive build
  - segragate into working and not working
  - supply from vape stores that take old vapes
  - use a battery tester to get similar size batteries
  - build out parallel and series battery to 50V
  - power house via inverter
  - all batteries are fuse connected to +ve power rail
  - THERE IS NO WAY to charge this? this was a one off charge and build
  - certainly no safe way to charge, no BMS, no cutoff when/if batteries charge
    at different rates

- [ ] I turned a VAPE into a Li-Ion BATTERY CHARGER for some reason - StezStix
      Fix?

  [![
  I turned a VAPE into a Li-Ion BATTERY CHARGER for some reason - StezStix
  Fix?
](http://i.ytimg.com/vi/gSzApAJgZA8/hqdefault.jpg)](https://youtu.be/gSzApAJgZA8)

- [ ] More free street-lithium reclamation - bigclivedotcom

  [![
  More free street-lithium reclamation - bigclivedotcom
](http://i.ytimg.com/vi/PsJMj7FtroY/hqdefault.jpg)](https://youtu.be/PsJMj7FtroY)

- [ ] Extracting Free Lithium-ion Batteries From Used Vapes - LeftyMaker

  [![
  Extracting Free Lithium-ion Batteries From Used Vapes - LeftyMaker
](http://i.ytimg.com/vi/TBy1W2_3aOg/hqdefault.jpg)](https://youtu.be/TBy1W2_3aOg)

- [ ] How I recycle vape batteries - @CidDwyer
  - https://www.youtube.com/shorts/EW8fcs8YHsE

- [ ] How to reuse VAPE batteries - FixitEasy

  [![
  How to reuse VAPE batteries - FixitEasy
](http://i.ytimg.com/vi/VIqjmY_UMhk/hqdefault.jpg)](https://youtu.be/VIqjmY_UMhk)

- [ ] Don't Toss it! 3 Fun Ways to Repurpose Disposable Vapes! - The Doubtful
      Technician

  [![
  Don't Toss it! 3 Fun Ways to Repurpose Disposable Vapes! - The Doubtful Technician
](http://i.ytimg.com/vi/kKobDxM6Thc/hqdefault.jpg)](https://youtu.be/kKobDxM6Thc)

- [ ] Reuse lipo cells - RC MULTIROTOR & ELECTRONIC

  [![
  Reuse lipo cells - RC MULTIROTOR & ELECTRONIC
](http://i.ytimg.com/vi/lWCb9tKKBQw/hqdefault.jpg)](https://youtu.be/lWCb9tKKBQw)

- [https://interestingengineering.com/innovation/youtuber-builds-power-system-using-vape-cells](https://interestingengineering.com/innovation/youtuber-builds-power-system-using-vape-cells)

  Video: YouTuber turns disposable vapes into battery wall that runs his whole
  workshop Chris Doel turned vape waste into a functioning 2.52 kWh power wall
  that runs his kettle, microwave, and computer.
  - see https://www.youtube.com/watch?v=dy-wFixuRVU above ^^

- [https://www.instructables.com/How-to-Reuse-Disposable-Vape-Lithium-Batteries/](https://www.instructables.com/How-to-Reuse-Disposable-Vape-Lithium-Batteries/)

  How to Reuse Disposable Vape Lithium Batteries By bekathwia

- https://www.instagram.com/reels/DXcNKdHhr7r/

  > What sounds like a “dying robot kazoo” and keeps the lithium batteries from
  > used vapes out of the landfill? Vape Synth! The tiny novelty synthesizer
  > created by a group of @itp_nyu makers playfully calls attention to the
  > serious problem of e-waste generated by the millions of disposable vapes
  > are sold in the United States every month.<br><br>To create them, the team
  > breaks apart spent Elf Bar nicotine vaporizers and hacks them into digital
  > musical instruments. The resulting device still looks like a vape
  > cartridge, but with a small speaker nestled amid an array of lights and
  > buttons. To play it, you just have to suck in the way you would on a vape.
  >
  > Just before Earth Day, we chatted with @NYUTisch faculty David Rios, Kari
  > Love, and Shuang Cai about the open-source project, which was recently
  > featured in WIRED, among other outlets. Read the article at the link in our
  > bio.
  >
  > 📹 Video by David

- [ ] NYU professors and DIY-ers turned disposable vapes into a silly sounding
      playable synth - New York University

  [![
  NYU professors and DIY-ers turned disposable vapes into a silly sounding
  playable synth - New York University
](http://i.ytimg.com/vi/W3Gt10VuNGM/hqdefault.jpg)](https://youtu.be/W3Gt10VuNGM)

- [ ] Repurposing Disposable Vape Batteries: The Why, The How, and the Vape
      Synth - The Open Source Hardware Association

  [![
  Repurposing Disposable Vape Batteries: The Why, The How, and the Vape Synth
  - The Open Source Hardware Association
    ](
    http://i.ytimg.com/vi/QmgaqjXy8qE/hqdefault.jpg
    )](https://youtu.be/QmgaqjXy8qE)

- [https://www.instagram.com/p/Csyl_5ArD-G/?hl=en](https://www.instagram.com/p/Csyl_5ArD-G/?hl=en)
- [https://www.reddit.com/r/diyelectronics/comments/1jann89/been_repurposing_rechargable_vape_batteries_any/](https://www.reddit.com/r/diyelectronics/comments/1jann89/been_repurposing_rechargable_vape_batteries_any/)
- [https://www.facebook.com/groups/DIYBATTERY/posts/3237110399917434/](https://www.facebook.com/groups/DIYBATTERY/posts/3237110399917434/)

- [x] I Turned Disposable Vapes Into Elegant Power Banks - Inventors Den

  [![
  I Turned Disposable Vapes Into Elegant Power Banks - Inventors Den
](http://i.ytimg.com/vi/sVzkVDMlBvY/hqdefault.jpg)](https://youtu.be/sVzkVDMlBvY)
  - more about elegant and wrapping them in timber, making the timber rounded,
    filling gaps with glue and tiber dust
  - fun idea of epoxy coating the timber and in particular to make see through panel
  - polish the epoxy "windows" with car headlight buffing paste and buffer
  - NO BMS - just hacked together in parallel and hope for the best

- [ ] [https://www.instagram.com/reels/DHEJ5HXIbEV/](https://www.instagram.com/reels/DHEJ5HXIbEV/)

- [ ] [https://www.tiktok.com/@whynotbuildit/video/7547364785168436493](https://www.tiktok.com/@whynotbuildit/video/7547364785168436493)

- [ ] [https://www.rs-online.com/designspark/activist-engineering-disposable-vapes-take-to-the-skies](https://www.rs-online.com/designspark/activist-engineering-disposable-vapes-take-to-the-skies)

- How to Train YOLOX on a Custom Dataset - Roboflow
  [![
  How to Train YOLOX on a Custom Dataset - Roboflow
](http://i.ytimg.com/vi/q3RbFbaQQGw/hqdefault.jpg)](https://youtu.be/q3RbFbaQQGw)
  - data set [https://public.roboflow.com/object-detection/bccd/3/download/voc](https://public.roboflow.com/object-detection/bccd/3/download/voc)
  - notebook [Colab: Train YOLOX on a Custom Dataset - YouTube.ipynb](https://colab.research.google.com/drive/1_xkARB35307P0-BTnqMy0flmYrfoYi5T#scrollTo=igwruhYxE_a7)
  - YOLO X [https://github.com/Megvii-BaseDetection/YOLOX](https://github.com/Megvii-BaseDetection/YOLOX)
  - Whitepaper [YOLOX: Exceeding YOLO Series in 2021](https://arxiv.org/pdf/2107.08430)
  - [Blog: What is Mean Average Precision (mAP) in Object Detection?](https://blog.roboflow.com/mean-average-precision/)

  > Training a YOLOX model for train detection involves gathering a diverse
  > dataset of trains, annotating bounding boxes using tools like Roboflow
  > Universe or CVAT, and fine-tuning the model starting from pre-trained COCO
  > weights.
  >
  > 1. Dataset Preparation
  >    To achieve high-accuracy detection:
  >    - Collect Data: Gather hundreds of images of trains from different angles,
  >      distances, and lighting conditions.
  >    - Labeling: Annotate the trains and separate the data into train, val, and
  >      test directories.
  >    - Format: Convert your annotations to the standard YOLO text format or
  >      Pascal VOC format, depending on your YOLOX training script.
  > 2. Setting Up YOLOX
  >
  >    You can train and test using the official YOLOX GitHub repository or MATLAB's built-in computer vision tools.
  >    Clone Repository:bashgit clone https://github.com
  >
  >    ```bash
  >    cd YOLOX
  >    pip3 install -v -e .
  >    ```
  >
  >    Configuration: Edit an experiment config file (e.g., in
  >    exps/default/yolox_s.py) to specify your train parameters, number of
  >    classes (just 1 if only detecting trains), and image size.
  >
  > 3. Training the ModelIt is highly recommended to use the COCO-pretrained
  >    weights (such as yolox_s.pth for the smallest/fastest model, or yolox_x.pth
  >    for highest accuracy).Start the training process using the command-line
  >    interface:
  >    ```bash
  >    python -m yolox.tools.train \
  >      -n yolox-s \
  >      -d 1 \
  >      -b 64 \
  >      --fp16 \
  >      -o
  >    ```
  >    Parameter breakdown: -n specifies the model size, -d is the number of GPUs,
  >    -b is your batch size, and --fp16 enables mixed-precision training to speed
  >    things up.
  > 4. Running InferenceOnce trained, select the best model checkpoint and test it
  >    on new images or videos:
  >    ```bashp
  >    python tools/demo.py video \
  >      -n yolox-s \
  >      -c /path/to/your/best_ckpt.pth \
  >      --path /path/to/train_video.mp4 \
  >      --conf 0.25 \
  >      --nms 0.45 \
  >      --save_result
  >    ```

## Sun 5 Jul 2026

### Infinity Train — detect laps with vision model on UNO Q

The idea: use the N-scale model railway as the EV discharge load, running it
around a loop until the vape cell is flat. Count laps automatically using a
camera + object detection model running on the UNO Q. "Infinity train" —
battery dies, train stops, you know how much energy was discharged.

**The pipeline at a glance:**

```
Camera → captured frame → YOLO inference → train detected? →
  crossing virtual lap line? → increment counter → log energy/lap
```

---

#### Step 1 — Camera placement

- Mount a small USB or ribbon camera so the **same section of track** (ideally
  a straight) is always in frame — this becomes your virtual "lap line"
- Overhead works well for N-scale; avoids perspective distortion on the tiny
  locomotive
- Fixed mount matters: if the camera moves, your lap-line logic breaks

---

#### Step 2 — Decide: existing dataset vs roll-your-own

**Option A — Use a public dataset from Roboflow Universe**

Search [universe.roboflow.com](https://universe.roboflow.com) for "train",
"locomotive", "model train". Unlikely to find N-scale specifically but a
general train detector might work as a starting point for transfer learning.

**Option B — Capture your own (recommended for N-scale)**

N-scale locos are tiny and look nothing like full-size trains in training data.

1. Record 5–10 min of video of the train going around — vary lighting,
   speed, maybe add/remove wagons
2. Extract frames at ~1 fps: `ffmpeg -i train.mp4 -vf fps=1 frames/frame_%04d.jpg`
3. Upload frames to [app.roboflow.com](https://app.roboflow.com) → Annotate →
   draw bounding boxes → label `train`
4. Export in **YOLO v8 format** (works for both YOLOX and Ultralytics)
5. Aim for ~200–400 annotated frames; augmentation in Roboflow (flip, blur,
   brightness) multiplies it for free

---

#### Step 3 — Model choice: YOLOX or Ultralytics YOLOv8/v11?

|                      | YOLOX (Megvii)              | Ultralytics YOLOv8/v11           |
| -------------------- | --------------------------- | -------------------------------- |
| Tutorial quality     | Good (Roboflow video below) | Excellent, huge community        |
| CLI ease             | Moderate                    | Very easy (`yolo train ...`)     |
| Edge export          | ONNX, TensorRT              | ONNX, TFLite, CoreML, Hailo, etc |
| Nano model available | yolox-nano                  | yolov8n / yolo11n                |
| Active development   | Slowing                     | Very active                      |

**Recommendation: start with Ultralytics YOLOv11-nano** — simpler CLI,
better export pipeline for edge hardware, and the Roboflow YOLOX tutorial
workflow maps 1:1 to it. If you hit a wall, YOLOX is well-documented too.

- Ultralytics docs: [https://docs.ultralytics.com](https://docs.ultralytics.com)
- YOLOX GitHub: [https://github.com/Megvii-BaseDetection/YOLOX](https://github.com/Megvii-BaseDetection/YOLOX)

---

#### Step 4 — Train the model (Google Colab or local)

**Ultralytics path (recommended):**

```bash
pip install ultralytics
yolo train model=yolo11n.pt data=train_dataset.yaml epochs=50 imgsz=640
```

The Roboflow export gives you a `data.yaml` directly — point `data=` at it.

**YOLOX path (if you prefer):**

Follow the Roboflow Colab notebook already linked in this log (see entry above
↑). Uses the BCCD dataset as a template — swap in your train dataset.

- Colab: [Train YOLOX on Custom Dataset](https://colab.research.google.com/drive/1_xkARB35307P0-BTnqMy0flmYrfoYi5T#scrollTo=igwruhYxE_a7)

Both are free on Colab T4. ~15–30 min for a nano model on a small dataset.

---

#### Step 5 — Lap counting logic

Detection alone isn't laps. The simplest approach that works:

1. Define a **virtual line** as a horizontal pixel band in the frame (e.g.
   y = 200–220px) over the straight section of track
2. Each time the detected bounding box centre crosses from above → below (or
   left → right) that band, increment `lap_count`
3. Add a **cooldown** (e.g. 3 seconds) so a slow/stopped train doesn't
   double-count

```python
# pseudocode
if bbox_cy in lap_zone and not in_cooldown:
    lap_count += 1
    last_lap_time = now
```

More robust: use Ultralytics built-in tracker (`yolo track`) — it assigns a
persistent ID so you track the same object across frames without re-triggering.

---

#### Step 6 — Deploy to UNO Q

- Export model to ONNX: `yolo export model=best.pt format=onnx`
- Run inference with `onnxruntime` or the UNO Q's native SDK
- UNO Q dev env setup is still on the TODO list (see top of this log)
- If the UNO Q is too slow for real-time, drop resolution (320×320) or use
  a lower frame rate — laps take seconds, not milliseconds

---

#### Tutorials / references

- How to Train YOLOX on a Custom Dataset — Roboflow (already ↑ in this log)

  [![How to Train YOLOX on a Custom Dataset — Roboflow](http://i.ytimg.com/vi/q3RbFbaQQGw/hqdefault.jpg)](https://youtu.be/q3RbFbaQQGw)

- [ ] Ultralytics Quickstart: [https://docs.ultralytics.com/quickstart/](https://docs.ultralytics.com/quickstart/)
- [ ] Roboflow Annotate walkthrough: [https://docs.roboflow.com/annotate](https://docs.roboflow.com/annotate)
- [ ] Ultralytics export guide (ONNX / edge targets): [https://docs.ultralytics.com/modes/export/](https://docs.ultralytics.com/modes/export/)
- [ ] Ultralytics tracking (for lap counter): [https://docs.ultralytics.com/modes/track/](https://docs.ultralytics.com/modes/track/)

---

#### Immediate next actions

- [ ] Record 5–10 min of train video on the loop
- [ ] Extract frames with ffmpeg, upload to Roboflow, annotate ~300 frames
- [ ] Train yolo11n on Colab, get a working checkpoint
- [ ] Write lap-counter script with virtual line logic
- [ ] Sort out UNO Q dev env so inference can run on-device

## Tue 30 Jun 2026

Watched somde videos. Seems a bunch of people do not use a proper BMS with per
cell charging. Probably this one from Great Scott is the best

- BMS (Battery Management System) || DIY or Buy || Properly protecting
  Li-Ion/Li-Po Battery Packs - GreatScott!

  [![
  BMS (Battery Management System) || DIY or Buy || Properly protecting
  Li-Ion/Li-Po Battery Packs - GreatScott!
](http://i.ytimg.com/vi/rT-1gvkFj60/hqdefault.jpg)](https://youtu.be/rT-1gvkFj60)
  - [https://github.com/stuartpittaway/diyBMS](https://github.com/stuartpittaway/diyBMS)
  - [https://github.com/chickey/diyBMS](https://github.com/chickey/diyBMS)

## Sat 27 Jun 2026

- got N-scale minature Railroad and track,
  - will use this as the EV - electric vehichle
  - used the MOSFET and simple arduino sketch to power it on a track
  - as no P-ch cannot easily make an H-bridge
  - could probably do it with some BJT transistors - but for now can only go in 1 direction
  - the 430Hz hum from the PWM is noticeable - might try the timer switch to move it to ~32kHz and see if it works
- need to look at a vape battery for blog post 1
- also thinking of adding my upcycling background
- local telco make a music synth from recycled e-waste
  - Telstra Partners with +61 and Bear Meets Eagle On Fire to Build a
    Synthesizer from Reclaimed E-Waste - Branding in Asia

  [![
  Telstra Partners with +61 and Bear Meets Eagle On Fire to Build a Synthesizer
  from Reclaimed E-Waste - Branding in Asia
](http://i.ytimg.com/vi/mX5pt4ZuCaM/hqdefault.jpg)](https://youtu.be/mX5pt4ZuCaM)
  - https://www.telstra.com.au/exchange/why-we-built-a-synthesiser-from-reclaimed-e-waste-with-the-avala
  - https://www.tiktok.com/@telstra/video/7654095635737595154
  - https://www.instagram.com/reels/DZ4ISifB984/

- vape power wall
- vape okarina
- real world electric train
  - https://www.reddit.com/r/EngineeringPorn/comments/1ptqcy1/worlds_largest_landmobile_batteries_equipped/

- This Train Runs on Gravity (And Never Needs Refueling) - German Science Guy

  [![
  This Train Runs on Gravity (And Never Needs Refueling) - German Science Guy
](http://i.ytimg.com/vi/b_38zdEcd70/hqdefault.jpg)](https://youtu.be/b_38zdEcd70)
  - infinity train
  - recuperation uisng induction from spinning wheels
  - the route is downhill to recover energy and then travel back uptill with
    empty wagons as ore was dumped at sea
  - similar concept in recuperation
    - https://www.topgear.com/car-news/electric/all-hail-edumper-largest-ev-world
  - not much invformation from the companny on the site

  - The physics problem that killed Fortescue’s Infinity Train - The Driven

    [![
  The physics problem that killed Fortescue’s Infinity Train - The Driven
](http://i.ytimg.com/vi/hqdefault.jpg)](https://youtu.be/2mBY8oB5ri4)

- Mining giant unveils electric train in quest for zero emissions | ABC NEWS -
  ABC News (Australia)

  [![
  Mining giant unveils electric train in quest for zero emissions | ABC NEWS -
  ABC News (Australia)
](http://i.ytimg.com/vi/iEZCcgFq3lE/hqdefault.jpg)](https://youtu.be/iEZCcgFq3lE)

- https://www.facebook.com/fortescuemetalsgroupltd/videos/we-now-have-not-one-but-two-of-the-worlds-largest-land-mobile-batteries-powering/1955758248317251/

### My Upcycling philosophy

Core framing for posts:

- Main concept: upcycling (not just recycling). Take high-value lithium cells
  from disposable products and give them a second life in an EV project.
- Big-picture frame: circular economy + right to repair + pushback against
  planned obsolescence.

Core historical examples (planned obsolescence arc):

1. 1925 Phoebus cartel: lightbulb life intentionally reduced.
2. 1930s nylon stockings: durability reduced to increase repeat sales.
3. Razor/blade model: keep the user buying consumables forever.
4. Smartphone era: sealed batteries and software-driven replacement cycles.
5. Disposable vapes: the endpoint, no reusable "handle," entire product is
   waste.

Core vape call-outs:

- Disposable vapes combine addiction economics, cheap mini lithium cells, and
  stylish single-use design.
- Central irony: critical battery materials for clean transport are being burned
  through in throwaway nicotine devices.
- Project thesis line: "VapeCell EV takes that lithium back."

Personal/philosophical call-outs to keep:

- Scarcity mindset explains why "save it, fix it, it might be useful" becomes a
  lifelong pattern.
- Eastern Bloc repair culture (kombinowac) is a strength: practical ingenuity
  under constraint.
- Tension to acknowledge: resourcefulness vs accumulation; keep items with a
  realistic path to reuse.
- The challenge structure helps: deadlines and public outputs turn hoarding into
  making.

Strong opener candidate:

"The vape industry took battery technology that could help decarbonise
transport, sealed it inside addictive disposable products, and normalized
throwing it away. This project takes that material back and proves it still has
value."

Reading/watch list (short):

- Giles Slade, Made to Break (2006)
- The Light Bulb Conspiracy (2010)
  - The Light Bulb Conspiracy (2010) with hard coded English subtitles. - Carl Wong

    [![
  The Light Bulb Conspiracy (2010) with hard coded English subtitles. - Carl Wong
](http://i.ytimg.com/vi/7ZX5uGSo-tk/hqdefault.jpg)](https://youtu.be/7ZX5uGSo-tk)

  - Planned Obsolescence documentary - The Light Bulb Conspiracy (2010) RENT / BUY
    TO MORE GREAT WORK - Documentary For Better World

    [![
  Planned Obsolescence documentary - The Light Bulb Conspiracy (2010) RENT / BUY
  TO MORE GREAT WORK - Documentary For Better World
](http://i.ytimg.com/vi/wzJI8gfpu5Y/hqdefault.jpg)](https://youtu.be/wzJI8gfpu5Y)

- Mullainathan and Shafir, Scarcity (2013)

### Forum Post 1 plan (EZ EV competition)

1. Opening hook: disposable vapes are tiny batteries wrapped in a throwaway habit.
2. State the e-waste view clearly: this is not just litter, it is stranded
   lithium and missed energy value.
3. Frame your personal stance: a waste-not mindset shaped by fixing, reusing,
   and refusing to bin useful hardware.
4. Be honest about scale at home: you have collected a pile of discarded vapes
   because you see recoverable value in them.
5. Connect to wider maker culture: people are already turning vape waste into
   useful and expressive builds.
6. Example call-out 1: vape synth projects show that "trash" devices can become
   creative instruments.
7. Example call-out 2: vape power-wall builds prove these cells can aggregate
   into meaningful stored energy.
8. Example call-out 3: vape ocarina/sound projects show playful reuse can still
   drive serious e-waste awareness.
9. Pivot to your project problem: reuse is only credible if the cells are
   monitored properly, not guessed.
10. Introduce the smart BMS angle (from proposal): per-cell visibility,
    voltage/temperature tracking, and health-aware decisions instead of a simple
    cutoff board.
11. Explain why intelligence matters for salvaged cells: mixed history and
    uneven quality demand observability and safety logic.
12. Reveal the EV direction: the final platform is an N-scale train EV inspired
    by Fortescue's Australian electric trains.
13. Explain why train model format works: compact, testable, visual, and perfect
    for demonstrating cell behavior under real load.
14. Close with the project thesis: "VapeCell EV takes discarded lithium,
    instruments it with a smart monitoring stack, and turns e-waste into
    motion."

## Mon 22 Jun 2026

### Prepare for 2 weeks away

The plan is to take a limited kit to do some real testing and live the dream of
"electronics on the road", multimeter, breadboard, soldering iron and a handful
of components. Seems the most logical idea is to charge and discharte a known
battery like 18650 (preferably from a reputable source - uh oh). The idea will
be to charge the battery directly using the FINIRSI DPS-150 and it's native
CC/CV (Continuous Current and Continuous Voltage).

- I have chosen to get some FQP30N06L mosfets to do 3.3v use in future
- might use the UNO Q for data logging on it's 16GB eMMC built in storage
- I don't think I have any power resistors so I will parallel some ¼W resistors
  instead:
  - For a constant current sink you want a sense resistor of around 1 Ω
    carrying your target discharge current. In parallel, resistors divide: two
    2.2 Ω ¼W resistors in parallel give you 1.1 Ω at ½W. Four 3.9 Ω ¼W in
    parallel give 0.975 Ω at 1W. Just make sure the total wattage rating
    exceeds your expected dissipation with margin. At 500 mA discharge current
    through 1 Ω that's 0.25 W
    — two 2.2 Ω ¼W resistors in parallel handles it comfortably.
  - For the main discharge load resistor (not the sense resistor), same
    principle. At 4.2 V discharging at ~500 mA you need roughly 8 Ω carrying
    ~2W total. Eight 68 Ω ¼W resistors in parallel gives 8.5 Ω at 2W. Ugly but
    it works — just lay them flat on the breadboard.

#### Full travel parts list

**Power & measurement**

- FNIRSI DPS-150
- 65W+ USB-C GaN charger with 20V PD to get full range of DPS-150
- Multimeter
- USB-C cable for UNO Q

**Cell & safety**

- 2–3× known 18650 cells (Samsung 30Q, Molicel P26A, or similar)
  - Buy fresh from a reputable source, not eBay
- TP4056 module × 2Backup / safety reference charger, ~$1 each
- Cell holder (single 18650, with leads)

**Constant current discharge circuit**

- FQP30N06L MOSFET
- LM358 op-amp DIP-8
- 2× 2.2 Ω ¼W resistors in parallel
- Sense resistor ~1.1 Ω — 8× 68 Ω ¼W resistors in parallel
- Discharge load ~8.5 Ω
- 10 kΩ resistors × a few - Reference divider for op-amp, ADC pullup
- 100 kΩ resistor × 1 Voltage divider for cell voltage ADC reading

**Temperature sensing**

- NTC 10 kΩ thermistors × 3–4
- hook up wire
- Small piece of kapton tape or thermal pad to hold thermistor against cell
  body

**Logging / control**

- Arduino UNO Q — Linux side logs to eMMC as CSV
- Breadboard (half-size is fine for travel)
- Jumper wires

#### Plan of attack

**Days 1–2 — bench setup and cell baseline**

Get the DPS-150 running. Set 4.2 V / 500 mA CC/CV and charge one known cell
from whatever state it arrives in to full. Watch the current taper to near zero
— that transition from CC to CV is the first useful thing to observe. No code
yet, just understand what you're looking at on the display.

**Days 3–4 — build the discharge circuit**

Wire the op-amp constant current sink on the breadboard. The classic circuit:
LM358 non-inverting input gets a reference voltage (a divider from 5V sets your
target current via V_ref = I_target × R_sense), inverting input reads across
the sense resistor, output drives the MOSFET gate. Set R_sense to ~1.1 Ω and
your reference to set ~500 mA discharge current. Test it with a bench voltage
first — set DPS-150 to 3.7 V and confirm the circuit draws a steady current.
Adjust until it's stable.

**Days 5–6 — first full charge/discharge cycle with manual logging**

Charge cell to 4.2 V via DPS-150. Connect discharge circuit. Every 5 minutes
read voltage off DPS-150 display and write it down (or just watch the screen).
Run until cell hits 3.0 V cutoff — add a simple voltage comparator cutoff if
you want to be tidy, or just watch it and disconnect manually. You now have
your first real capacity data point: mAh = current × time.

**Days 7–9 — wire up UNO Q for automated logging**

Connect cell voltage (via resistor divider to keep it in 0–3.3 V range for the
STM32 ADC), thermistor (voltage divider to ADC), and optionally the sense
resistor voltage to read actual current. Write a simple Arduino sketch on the
STM32 side that samples every 30 seconds and sends CSV over serial. On the
Linux side, a 10-line Python script reads serial and appends to a CSV file on
the eMMC. You now have automated logging with temperature.

**Days 10–12 — run repeated cycles**

Let the system charge and discharge 3–4 full cycles unattended overnight. The
UNO Q logs everything. Each morning plot the discharge curves — you'll start to
see the cell's capacity and any temperature behaviour during discharge. This is
real data for your blog post.

**Days 13–14 — reflect and document**

Write up your findings. What was the actual capacity vs rated? How much did
temperature rise during discharge? Did capacity change between cycles? This
becomes Forum Post 1 of your submission: "What's actually inside a vape battery
— and what did our first experiments reveal?"

You come home with a working logging rig, real data, and the first blog post
essentially written.

### Blog setup

Blog setup, based on prior art

~~## Thu 22 Apr 2026 - from Green Brain~~

added a Jekyll Github pages blog using the commands

```sh
mise use ruby@3.2.2
gem install jekyll bundler
jekyll new docs

cd docs

# downgrade jekyll to 3.9.6
# gem "jekyll", "~> 3.9.5" # to work with github-pages
bundle add github-pages webrick

# configure the _config.yml

# run
bundle exec jekyll serve --port 8888

# open
http://127.0.0.1:8888/vape-cell-EV/
```
