---
layout: post
title: "Vape Cell EV - part II - RS485 comms"
date: 2026-08-03 01:00:00 +1000
---

I have been out on a few unplanned mid-winter trips (Southern Hemisphere here
🦘) away from home which has put a huge deficit in my ability to deliver.
Secondly I have been scratching my head watching a bunch of online videos
mentioning how people have burnt their house down charging lithium batteries,
let alone unlabelled ones from vapes. Also a bunch of contradictory advice on
when to use a BMS (Battery Management System) and how to build one. For today I
want to focus on the Design Challenge kit I was lucky to receive, and getting
RS-485 up and running between 2 Arduino UNO Q's using the EVAL-ADM3068EEBZ
Evaluation Board.

# Recap

Vape Cell EV is the idea of a smart cell charging system that can handle cells
with unknown histories, charging, power distribution, any safety issues and
ultimately if a cell should be removed from a battery of many cells.

- [Vape Cell EV - part I - What's in a Vape][vape-cell-ev-part-I-whats-in-a-vape]

## The Design Challenge Kit

The design challenge kit is made up of 2 UNO Q's, Arduino module with a docker
capable MPU attached, an evaluation board for the ADM3068E RS-485 Transceiver
capable of 50 Mbps, some Molex connectors, access to LabView, some cool stickers
and a metal Element 14 pin.

![](/vape-cell-EV/assets/2026-08-03_developer_kit.jpg)

I must say I am not much of a fan of the stickers but a pin is exactly the kind
of identification you need in a crowd.

![](/vape-cell-EV/assets/2026-08-03_E14_pin.jpg)

## Analog Devices **EVAL-ADM3068EEBZ** RS485 transceiver

The Analog Devices **EVAL-ADM3068EEBZ** is a small breakout board for the
ADM3068E `RS485` transceiver. It has screw terminals for power, data and bus
signals and is rated **50 Mbps**. Worth mentioning that this figure seems to be
based on a measurement at the transceiver's own pins, essentially zero cable
— a bench loopback number, not a claim about what survives a real cable run.

`RS485` is differential — data is the voltage _difference_ between two
wires, not one wire against ground — and that's why the cable has to be a
twisted pair specifically. The twist is what gives common-mode noise
rejection and keeps the pair's impedance consistent enough to match the
120 Ω termination resistors on the board. The classic rate-vs-distance rule
of thumb (10 Mbps at ~12 m, 1 Mbps at ~120 m, 100 kbps at up to ~1200 m) is
worth keeping in mind, even though I'm not yet sure exactly where RS485 will
end up being used on my cell monitor — I'm fairly confident none of these
distances or speeds will actually be needed.

## Which jumpers, though

The eval board has seven jumpers (`LK1`–`LK7`) controlling driver/receiver
enable, half- vs full-duplex wiring, and termination. Table 1 in the
datasheet lays it all out, but in fairly technical terms — it took me a while
to fully understand what each jumper actually did, and a few of them got
plugged in by gut feel.

The board ships configured for **its own bench self-test**: `LK1=B` (receiver
always on), `LK2=A` (driver always on). That turned out to be genuinely useful:
with `LK4`/`LK6` inserted (folding the chip's full-duplex A/B/Y/Z pins into one
half-duplex pair), the factory jumpers already give you a single-board loopback
test for free — the same idea as a CAN controller's internal loopback mode
which I explored in my previous project [Green Brain - Part I - CAN bus
introduction][green-brain-part-I-can-bus-introduction]. Whatever goes out `DI`
comes straight back on `RO`, no second board or cable required. Good for smoke
testing the sketch's framing and checksum logic before anything else is wired
up.

I attempted to get half-duplex control working, which needs `LK1=D`, `LK2=C`
— tying the driver enable (`DE`) and receiver enable (`RE̅`, active-low)
together onto one control line, so a single GPIO can flip the whole board
between transmit and receive. I got led astray for a while by some MCU (Micro
Controller Unit) / MPU (Micro Processor Unit) interop quirks in Zephyr, which
had me convinced this wasn't working as expected.

## An echo test that barely touched the board

The single-board loopback test worked nicely once the sketch's UART code was
right (more on that below) — but in hindsight it barely exercised anything
`RS485`-specific. Driver and receiver were both hard-wired permanently on;
there was no direction switching, no real bus, no second board's independent
clock. Moving to the actual two-node config (`LK1=D`, `LK2=C`, a twisted
pair between two separate boards) immediately surfaced problems the echo
test had no way of catching — which led me down the goose chase of the
direction-control wire mentioned above, and a forgotten ground wire between the
two boards' `RS485` sides. Neither would have mattered for a board looping back
to itself.

## When two boards refuse to talk

First symptom: total silence, on both boards, at every baud. Turned out my
naive idea of sweeping baud rates on a timer was the culprit:

```cpp
int idx = (int)((millis() / STEP_MS) % NUM_BAUDS);
```

`millis()` counts from each board's _own_ power-on, so two independently
started boards drift out of phase almost immediately — over a 28-second full
sweep they can end up spending the whole test on different baud rates from
each other. Fix: pin both boards to a single fixed baud rate before
attempting to sweep across multiple speeds.

Still silence after that — after a bunch of wire changes, including the
direction-control wire, the symptom finally changed from silence to
garbage: real activity on the bus, but corrupted frames and a fast-climbing
error count. To rule the transceiver out entirely, the next step was a direct
crossover — UNO Q `D1` straight to the other UNO Q's `D0` and back, no eval
board in the loop at all:

- **Node A D1 (TX) → Node B D0 (RX)**
- **Node A D0 (RX) ← Node B D1 (TX)**
- shared GND between the two boards

This should have been plain old UART between two boards — I'd even written up
a simple example in a comment on my recent [Smart Security and Surveillance
Sentinel Box forum post][sentinel-box-part-II-uart-comment]. Instead, I got
the same corruption. That single test ruled out the transceiver, the
jumpers, and the bus wiring all at once — whatever was wrong was in the
MCU/software layer, not the `RS485` hardware.

## Back to the UART — the `Serial` trap

The sketch had been writing to `Serial`, same as any Arduino sketch would.
On real hardware, that hit a build error deep into an unrelated change:

```
error: 'class BridgeMonitor<>' has no member named 'end'
```

`Serial` on the **UNO Q**, it turns out, isn't the physical UART at all once
`Arduino_RouterBridge.h` is included. This is one of the recurring confusions
of working across an MPU and an MCU — which one actually owns the pins, and
which "serial" you're really talking to. In this case it turned out to be
the internal Bridge/Monitor channel used for debug printing:

```cpp
// Arduino_RouterBridge/src/monitor.h
extern BridgeMonitor<> Serial;   // aliased to the same object as Monitor
```

Zephyr bit me again here — I'd run into similar MCU/MPU quirks before in
[Green Brain - Part II - Dev setup][green-brain-part-II-dev-setup] and had
mostly learned to live with them. The fix was to skip the Arduino `Serial`/`Stream`
wrapper and talk to the STM32's `usart1` peripheral directly through Zephyr's
own UART driver API:

```cpp
#include <zephyr/drivers/uart.h>
static const struct device* rs485_dev = DEVICE_DT_GET(DT_NODELABEL(usart1));
// uart_configure(rs485_dev, &cfg)   — set baud at runtime
// uart_poll_out(rs485_dev, byte)    — blocking single-byte write
// uart_poll_in(rs485_dev, &byte)    — non-blocking single-byte read
```

## The buffer that wasn't there

With real hardware access sorted, a byte counter told the next story: the
same-board loopback test was receiving **exactly 2 bytes back for every
23-byte frame sent**, every single time, regardless of baud. That precision
ruled out noise and pointed at something structural — this UART has almost
no receive buffering in polling mode, just a ~2-byte pipeline (shift
register plus holding register). The original code wrote a whole frame in
one blocking loop before ever checking for incoming bytes; on a looped-back
wire the echo starts arriving _while_ still transmitting, and without
draining it concurrently, everything past the first couple of bytes was
lost to overrun. Interleaving the write with polling reads fixed the
same-board test cleanly.

Cross-board traffic, though, stayed badly corrupted even at a generous
9600 baud — a rate that should have made timing-related errors rare. The
clue came from an old, unrelated UART proof-of-concept in [Smart Security and
Surveillance Sentinel Box forum post][sentinel-box-part-II-uart-comment], on
completely different hardware, whose own comment explained why it just worked:

```c
// TMR_Delay is blocking for TICK_MS but the hardware RX FIFO
// (32 bytes) holds incoming bytes while we wait.
```

That MCU has a real 32-byte hardware FIFO. Ours has essentially none, _and_
Zephyr — unlike that PoC's bare-metal loop — is a genuine preemptive RTOS.
Our `loop()` can be paused by other threads (the Bridge/RPC channel, USB,
kernel housekeeping) for however long the scheduler decides, and with only a
2-byte cushion behind it, any such pause silently drops data no matter how
carefully the polling loop is written. That's a fundamentally different
problem from "the baud rate is too high."

The real fix was getting reception off the polling loop entirely — a proper
interrupt handler draining the peripheral into a ring buffer the instant a
byte arrives, independent of whatever the main loop happens to be doing:

```cpp
static void rs485Isr(const struct device* dev, void* user_data) {
    while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
        uint8_t b;
        while (uart_fifo_read(dev, &b, 1) == 1) {
            // push b into a ring buffer
        }
    }
}
// in setup():
uart_irq_callback_user_data_set(rs485_dev, rs485Isr, NULL);
uart_irq_rx_enable(rs485_dev);
```

The difference was immediate: the receiving board's error count went from
climbing continuously to 8 total (a brief startup transient) then flat zero
across the next ~490 clean frames.

## Timing budgets that don't scale with baud

A smaller, satisfying bug fell out right behind the buffer fix: the fixed
50 ms reply timeout didn't scale with baud. At 9600 baud a bare 23-byte round
trip already takes ~48 ms on its own, leaving almost no slack for the other
board's own turnaround — late replies were routinely missing the deadline
and landing during the _next_ round instead, silently miscounted rather than
just timed out. Scaled the timeout to 5× the theoretical round trip at the
current baud, and it cleared up at 9600.

It came back once bauds got extreme, just from the opposite direction. At
3,000,000 baud, a chunk of replies kept landing as mismatched rather than
timed out — same symptom, different cause this time. The 5×-round-trip
formula assumes overhead shrinks along with wire time, but the _other_
board's own turnaround (sync detection, checksum, flipping the transceiver
direction, ordinary scheduler jitter) costs roughly the same wall-clock time
no matter the baud — trivial next to 48 ms at 9600 baud, the dominant cost
once wire time itself drops to ~150 µs at 3 Mbaud. Added a flat 5 ms floor on
top of the scaled portion and that gap closed too. Two different bugs,
same underlying lesson: a timing budget derived purely from bit-time doesn't
account for the parts of the round trip that don't shrink with it.

## A wiring mix-up, and a way to test a driver with just a multimeter

Wiring the real two-board bus (as opposed to the single-board loopback
above) turned up one more of the "forgotten/misremembered wire" family: the
UNO Q's D0 and D2 pins got swapped on one board — `RO` (the transceiver's
data output) ended up wired to `D2`, the direction-control pin, instead of
`D0`, the real hardware UART's receive pin. Software has no way to notice
this: `D2` is just a GPIO the sketch actively drives, so it was fighting the
transceiver's own output on that pin the whole time, while the STM32's real
UART receiver sat on a disconnected, floating `D0`.

With no oscilloscope on hand, the multimeter still settled a real question:
was a driver's signal _weak_ or was it _not there at all_? A rapidly-toggling
half-duplex signal only pulses "on" for a fraction of each round, so a plain
DC meter, which effectively averages what it sees, reads a small fraction of
the true swing even on a perfectly healthy line — 0.07–0.1 V measured this
way is not the same claim as "the driver only makes 0.1 V." Trick: force the
driver into a _steady_ state instead of a fast pulse, so the meter has
something constant to read. Jumper the direction-control pin straight to
3V3 (driver permanently enabled, bypassing the MCU), disconnect the data
input so nothing's toggling it, then read across A/B. A healthy driver holds
a steady 2–5 V that way; this one read 2.2–2.4 V, confirming the transceiver
and its enable wiring were both fine — the fault really was the D0/D2 swap,
not the chip.

## The vanishing frame

With the wiring fixed, most frames arrived clean — but a fraction still
failed, and only ever the _same_ way: a complete, checksum-_invalid_ frame,
every field correct except the very last byte. Random noise doesn't behave
like that; noise-corrupted bytes should be unpredictable, not the same wrong
value every time. A raw hex dump (every byte of the frame, not just the
derived pass/fail) confirmed it precisely:

```
raw: A5 00 00 00 00 10 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F A5
```

`sync`, the 4-byte sequence number, `len`, all 16 payload bytes — every one
matched `(seq+i) mod 256` exactly. Only the final byte was wrong, and it was
wrong in a very specific way: it always read back as `0xA5` — the sync byte,
not a checksum. Dumping the _transmit_ side too (bytes about to go out, not
what came back) showed why: the sender was transmitting every sequence
number in order, but the receiver only ever logged the _even_ ones. Odd
frames vanished completely — not corrupted, just gone. The receiver's
checksum slot for frame N was silently being filled by frame N+1's real sync
byte, and the rest of frame N+1 got scanned through as noise looking for the
next sync match.

Root cause: the delay before dropping the transceiver's drive-enable line
assumed 10 bit-times of margin was enough — one byte period — for the last
byte to finish shifting out. It isn't. The UART call used to write that byte
only guarantees it moved into the transmit register once the _previous_
byte vacated it — not that the previous byte finished transmitting. Worst
case, the byte before the checksum can still have a full bit-time left to
go, and the checksum byte itself hasn't started: two byte periods needed,
not one. That mismatch was exactly big enough to reliably clip the last
byte's transmission every single time — not occasionally, which is what
made the "always exactly `0xA5`" signature so recognisable once dumped raw.
Doubling the margin fixed it outright: **100% clean, zero errors, hundreds
of frames straight**.

## Teaching the boards to run the sweep themselves

With a fully clean 9600-baud link finally confirmed, it was worth rebuilding
the automatic baud sweep — properly this time. The first attempt (mentioned
above) swept bauds on a free-running timer and fell apart because two
independently-booted boards' clocks drift apart with nothing to re-anchor
them. The fix: make 9600 a permanent "home" baud that's proven reliable
enough to carry its own handshake.

- One side sends a control frame — the _same_ 23-byte frame format, told
  apart only by a different leading byte (`0x5A` instead of `0xA5`), reusing
  every bit of the already-debugged build/checksum/echo code.
- The other side echoes it back as an acknowledgement — again, the exact
  same mechanism already used for ordinary data frames — and only then
  switches its own baud rate.

First attempt at the control frame was just a bare pulse meaning "advance to
your next rate," relying on both boards independently counting their own
position through the same compiled-in list. That worked fine until a board
got restarted on its own (once autostart-on-boot was wired up) — its counter
reset to zero while the other board's didn't, so the pulse still succeeded
every time, each side just quietly advanced to a _different_ rate. Fix: send
the actual target baud — the real number, e.g. `115200`, packed as a 4-byte
value straight into the control frame's payload — instead of an
index/offset into the shared list. One side is now the sole authority and
the other just applies whatever number it's handed, with no shared table
position to keep in sync at all.

- Reverting back to 9600 needs no handshake at all — each side just counts
  down a short local timer (a few seconds) from the moment it entered the
  test baud, then reverts on its own. That only requires the two sides to
  _enter_ the test baud within about one handshake round trip of each
  other, not to stay in sync for an entire run — so drift can never
  accumulate past a single short cycle, which is what actually broke the
  original design.

## Where things stand

`RS485` itself never turned out to be the problem — every wrong turn along
the way was a jumper misunderstanding, a forgotten wire, or a software
assumption two or three layers removed from the bus itself. The pattern that
got through each one was the same: isolate ruthlessly (a bare wire loop
instead of the transceiver, a direct crossover instead of the bus, a forced
steady-state instead of a fast pulse for the multimeter), don't trust an API
name at face value (`Serial` wasn't `Serial`), and let a raw byte dump settle
an argument that theorising alone kept getting wrong.

With the auto-sweep running end to end, here's the actual reliability
curve across a real two-node bus — twisted pair, real transceivers, real
half-duplex turnaround — not a self-loop bench test and not a datasheet
figure:

| Baud           | Success rate | Round trip           |
| -------------- | ------------ | -------------------- |
| 9600           | ~96–100%     | ~48 ms               |
| 19200 – 460800 | ~98–100%     | scales with bit-time |
| 921600         | ~91%         | ~1.3 ms              |
| 1,000,000      | ~95%         | ~1.4 ms              |
| 2,000,000      | ~83.5%       | ~1.2 ms              |
| 3,000,000      | ~60%         | ~1.1 ms              |

Clean and reliable from 9600 all the way through 460800 baud. Real,
gradual degradation starts around 921,600–1,000,000, and gets substantial by
2–3 Mbaud. Round trip time tracks bit-time almost exactly at every rate,
with no artificial timeout ceiling distorting it anymore — this is the
genuine electrical/timing ceiling, not a software one, and it's a believable
curve: nowhere near the 50 Mbps bench figure, but comfortably enough for a
cell-monitor poll bus running well under 1 Mbaud.

![](/vape-cell-EV/assets/2026-08-03_web-display-RS-485-node-network.png)

The video below shows 2 UNO Q's connected to the ADM3068E eval board. The
RS-485 boards are connected with a twisted pair. The screen displays an
overkill display hosted on one of the UNO Q's. At the top is the NODE, and
anothr node connected. In the middle is the radar like view of the topology of
the network, in this case a primary and a secondary. At the bottom are the logs
of the various baud speeds being tested and a graph on the bottom right shows
the link quality.

<video width="740" controls>
  <source src="/vape-cell-EV/assets/2026-08-03_2_node_RS-485_UNO_Q.mp4" type="video/mp4">
  Your browser does not support the video tag.
</video>

## Next

- onto the batteries and charging them, _I think I have the necessary knowledge
  and courage to proceed safely_.

## Source

[https://github.com/saramic/vape-cell-EV][github-vape-cell-EV]

[element-14-EZ-EV]: https://community.element14.com/challenges-projects/design-challenges/ez-ev-challenge
[vape-cell-ev-part-I-whats-in-a-vape]: https://community.element14.com/challenges-projects/design-challenges/ez-ev-challenge/f/forum/57077/vape-cell-ev---part-i---what-s-in-a-vape
[green-brain-part-I-can-bus-introduction]: https://community.element14.com/challenges-projects/design-challenges/on-the-line/f/forum/56921/green-brain---part-i---can-bus-introduction
[green-brain-part-II-dev-setup]: https://community.element14.com/challenges-projects/design-challenges/on-the-line/f/forum/57001/green-brain---part-ii---dev-setup
[sentinel-box-part-II-uart-comment]: https://community.element14.com/challenges-projects/design-challenges/smart-security-and-surveillance/f/forum/56894/sentinel-box---part-ii---back-to-c/235251
[github-vape-cell-EV]: https://github.com/saramic/vape-cell-EV
