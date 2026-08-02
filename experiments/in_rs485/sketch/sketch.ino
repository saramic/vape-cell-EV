#include "Arduino_RouterBridge.h"
#include <zephyr/drivers/uart.h>
#include <string.h>

// ── RS-485 speed test — EVAL-ADM3068EEBZ, half-duplex, 2 UNO Q nodes ──────────
//
// One board is PRIMARY (drives the sweep), the other is SECONDARY (echoes
// and follows along). The value here is just the local default —
// `mise run upload:athena` / `upload:briana` each force the correct role on
// the remote copy after rsyncing (via sed), so this doesn't need to be
// hand-edited before uploading to either node.
#define IS_PRIMARY 1

// UART: physical pins are D1 (TX, PB6) / D0 (RX, PB7) = usart1, confirmed
// against Arduino's official UNO Q Full Pinout doc
// (reference/UNO_Q_full_pinout.pdf). Wire D1->DI, D0<-RO on the
// EVAL-ADM3068EEBZ.
//
// IMPORTANT: this does NOT go through the Arduino `Serial` object. Including
// Arduino_RouterBridge.h aliases `Serial` to the same internal MPU<->MCU
// Bridge/Monitor RPC channel as `Monitor` (see
// Arduino_RouterBridge/src/monitor.h: `extern BridgeMonitor<> Serial;`) — it
// is NOT the physical UART, and has no `.end()`/real baud control (confirmed
// by an on-device build error: "'class BridgeMonitor<>' has no member named
// 'end'"). `Serial1` (lpuart1) is real hardware but its pins (PG7/PG8) aren't
// exposed on any connector on this board at all.
//
// So: talk to usart1 directly via the Zephyr UART driver API instead of the
// Arduino Stream wrapper — same idea as the CAN experiment using raw Zephyr
// CAN driver calls instead of an Arduino CAN class.
static const struct device* rs485_dev = DEVICE_DT_GET(DT_NODELABEL(usart1));
static bool rs485Ready = false;

// Single-wire direction control, tied to the ADM3068E's combined RE/DE node
// (see Table 1 wiring below). D2 = PB3 — a plain free GPIO, not claimed by
// usart1 or any default peripheral.
#define DERE_PIN 2
#define DERE_TRANSMIT HIGH  // DE=1 (drive enabled) & RE=1 (receiver disabled)
#define DERE_RECEIVE  LOW   // DE=0 (drive disabled) & RE=0 (receiver enabled)

// EVAL-ADM3068EEBZ jumper configuration for this test (per UG-1540 Table 1):
//   LK2 = C   DE driven from the J3 terminal wired to DERE_PIN
//   LK1 = D   RE tied to the same node as DE — one GPIO drives both
//   LK4, LK6  inserted — folds the full-duplex A/B (in) and Y/Z (out) pairs
//             into a single half-duplex pair per node: A+Y and B+Z
//   LK3       inserted (120R across A/B) at BOTH nodes — the two physical
//             bus ends
//   LK5       left open (LK3+LK5 together would halve the termination to 60R)
//   LK7       inserted — ties VIO to VCC so logic levels match the UNO Q's
//             3.3V header (run VCC at 3.3V from the board's 3V3 pin)
// Bus wiring: node A's (A+Y) <-> node B's (A+Y) and node A's (B+Z) <-> node
// B's (B+Z), over ONE twisted pair + a shared ground/shield wire.

// ── Auto baud sweep, handshake-driven ─────────────────────────────────────────
// 9600 is the permanent "home" baud — proven reliable (see WORK_LOG.md), and
// solid enough to carry a handshake. An earlier version swept bauds on a
// free-running timer (`millis()/STEP_MS`), but each board's `millis()` runs
// from its OWN power-on with no clock sync between them, so two
// independently-started boards drift out of phase over any long unattended
// run. This version fixes that differently: PRIMARY tells SECONDARY which
// baud to try next *over the reliable home link*, both switch, both run a
// short local timer (independent, but only needs to stay aligned for a few
// seconds, not the whole run), then both revert to home on their own and
// PRIMARY re-synchronises before requesting the next baud. Drift can never
// accumulate past one cycle.
#define HOME_BAUD 9600
static const uint32_t BAUD_TABLE[] = {
    19200, 38400, 57600, 115200, 230400, 460800, 921600, 1000000, 2000000, 3000000
};
static const int      NUM_BAUDS        = sizeof(BAUD_TABLE) / sizeof(BAUD_TABLE[0]);
static const uint32_t TEST_DURATION_MS = 3000; // how long each baud (incl. home) is tested
static const uint32_t CTRL_MAX_RETRIES = 5;    // give up and skip a baud after this many failed handshakes

// ── Frame format ──────────────────────────────────────────────────────────────
// Two frame *kinds*, same 23-byte layout, told apart by the leading sync
// byte: SYNC_BYTE for normal ping/echo data frames, CTRL_SYNC for the "switch
// baud now" handshake — reuses the exact same build/validate/echo machinery,
// just with a different marker. The control frame's payload carries the
// actual target baud (first 4 bytes) — it used to be a bare synchronisation
// pulse ("advance to your next BAUD_TABLE entry"), relying on both sides'
// independently-incremented sweep index staying in lockstep. That broke the
// first time only one board rebooted (e.g. via arduino-app-cli's default-app
// auto-start): its index reset to 0 while the other board's didn't, so the
// handshake itself still succeeded (it's just a pulse at HOME_BAUD) but each
// side then advanced to a *different* baud, silently — 9600 kept working,
// the sweep didn't, with no obvious reason why. Sending the explicit baud
// makes PRIMARY the sole authority: SECONDARY just does what it's told
// instead of maintaining a counter that can drift. See WORK_LOG.md.
#define SYNC_BYTE   0xA5
#define CTRL_SYNC   0x5A
#define PAYLOAD_LEN 16

struct __attribute__((packed)) Frame {
    uint8_t  sync;
    uint32_t seq;
    uint8_t  len;
    uint8_t  payload[PAYLOAD_LEN];
    uint8_t  checksum;
};
#define FRAME_SIZE (int)sizeof(Frame)

static uint8_t frameChecksum(const uint8_t* bytes, int n) {
    uint8_t c = 0;
    for (int i = 0; i < n; i++) c ^= bytes[i];
    return c;
}

static void buildFrame(Frame& f, uint8_t sync, uint32_t seq) {
    f.sync = sync;
    f.seq  = seq;
    f.len  = PAYLOAD_LEN;
    for (int i = 0; i < PAYLOAD_LEN; i++) f.payload[i] = (uint8_t)(seq + i);
    f.checksum = frameChecksum((uint8_t*)&f, FRAME_SIZE - 1);
}

// Control frame: same layout, but payload[0..3] carries the actual target
// baud (little-endian, native STM32 byte order) instead of the seq-derived
// filler pattern — see comment above.
static void buildCtrlFrame(Frame& f, uint32_t ctrlSeq, uint32_t targetBaud) {
    f.sync = CTRL_SYNC;
    f.seq  = ctrlSeq;
    f.len  = PAYLOAD_LEN;
    memset(f.payload, 0, PAYLOAD_LEN);
    memcpy(f.payload, &targetBaud, sizeof(targetBaud));
    f.checksum = frameChecksum((uint8_t*)&f, FRAME_SIZE - 1);
}

static uint32_t ctrlFrameTargetBaud(const Frame& f) {
    uint32_t baud;
    memcpy(&baud, f.payload, sizeof(baud));
    return baud;
}

static bool frameValid(const Frame& f) {
    return (f.sync == SYNC_BYTE || f.sync == CTRL_SYNC)
        && f.len == PAYLOAD_LEN
        && f.checksum == frameChecksum((uint8_t*)&f, FRAME_SIZE - 1);
}

// Result codes for sendAndWaitEcho() (defined further down). Plain #defines,
// not an enum: the Arduino build tool auto-generates forward declarations
// for every function and inserts them very early in the file — before even
// this point, it turns out (moving a custom enum here still wasn't early
// enough and the auto-generated prototype failed the same way). A built-in
// return type (int) sidesteps the problem entirely since there's no custom
// type that could be out of order.
#define ECHO_OK      0
#define ECHO_TIMEOUT 1
#define ECHO_BAD     2

// ── Raw UART helpers (bypassing Arduino Serial — see note above) ────────────
static uint32_t txDrainUs    = 100;   // recomputed per-baud in rs485SetBaud()
static uint32_t pollTimeoutUs = 50000UL; // recomputed per-baud in rs485SetBaud()

static void rs485SetBaud(uint32_t baud) {
    struct uart_config cfg = {
        .baudrate  = baud,
        .parity    = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };
    uart_configure(rs485_dev, &cfg);
    // 20 bit-times (two full byte periods) of margin, so we don't drop DE
    // before the last byte has actually finished shifting out on the wire.
    // uart_poll_out() only guarantees the byte was handed to the transmit
    // register once the PREVIOUS byte has moved into the shift register —
    // it does not wait for that previous byte to finish shifting out. So
    // when the last uart_poll_out() (the checksum byte) returns, the byte
    // before it can still have up to a full bit-time left to transmit, and
    // the checksum byte itself hasn't started yet: worst case is two byte
    // periods, not one. A 10-bit-time margin was consistently too short —
    // every single frame lost its last byte, 100% of the time, not just
    // occasionally — see WORK_LOG.md.
    txDrainUs = (uint32_t)((20000000ULL + baud - 1) / baud);

    // Poll timeout: 5x the bare-minimum wire round-trip time (FRAME_SIZE
    // bytes out, FRAME_SIZE bytes back, 10 bit-times each), PLUS a fixed
    // floor. The wire-time term alone shrinks toward zero as baud climbs,
    // but the other side's own turnaround (sync detection, checksum, DE/RE
    // GPIO switching, scheduler jitter) costs roughly the same wall-clock
    // time regardless of baud — trivial next to a 48 ms wire time at
    // 9600 baud, but the dominant cost once wire time drops to ~150 us at
    // 3 Mbaud. See WORK_LOG.md.
    uint64_t minRoundTripUs = (uint64_t)FRAME_SIZE * 2 * 10 * 1000000ULL / baud;
    static const uint32_t FIXED_OVERHEAD_US = 5000; // covers turnaround + jitter, any baud
    pollTimeoutUs = (uint32_t)(minRoundTripUs * 5) + FIXED_OVERHEAD_US;
}

static void rs485Write(const uint8_t* buf, int n) {
    for (int i = 0; i < n; i++) uart_poll_out(rs485_dev, buf[i]);
}

// ── Interrupt-driven RX ───────────────────────────────────────────────────────
// This UART has ~2 bytes of hardware buffering (shift register + holding
// register), no real FIFO. Draining it by polling from loop() isn't
// reliable under Zephyr's preemptive scheduler — loop() can be preempted by
// other threads (Bridge/RPC, USB, kernel housekeeping) for however long the
// scheduler decides, and any gap longer than a byte or two silently overruns
// no matter how tightly the polling loop is written. Fix: let the hardware
// ISR drain the peripheral into a ring buffer the moment a byte arrives,
// independent of whatever loop() happens to be doing. Whether this actually
// works depends on CONFIG_UART_INTERRUPT_DRIVEN being enabled in the
// on-device firmware — a runtime Kconfig setting with no way to check from a
// dev machine; if uart_irq_rx_enable() is a no-op or returns -ENOTSUP here,
// this won't behave any differently from polling, which is itself a useful
// (if disappointing) result. See WORK_LOG.md.
#define RX_RING_SIZE 256
static volatile uint8_t  rxRing[RX_RING_SIZE];
static volatile uint16_t rxHead = 0; // written only by the ISR
static volatile uint16_t rxTail = 0; // written only by loop()
static uint32_t statRingOverflow = 0;

static void rs485Isr(const struct device* dev, void* user_data) {
    while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
        uint8_t b;
        while (uart_fifo_read(dev, &b, 1) == 1) {
            uint16_t next = (uint16_t)((rxHead + 1) % RX_RING_SIZE);
            if (next == rxTail) { statRingOverflow++; continue; } // ring full, drop
            rxRing[rxHead] = b;
            rxHead = next;
        }
    }
}

static bool rs485ReadByte(uint8_t& out) {
    if (rxTail == rxHead) return false;
    out = rxRing[rxTail];
    rxTail = (uint16_t)((rxTail + 1) % RX_RING_SIZE);
    return true;
}

// Overrun/parity/framing errors latch on this UART until explicitly checked
// (checking the status register is what clears them at the hardware level).
static uint32_t statUartErrors = 0;
static void rs485ClearErrors() {
    int err = uart_err_check(rs485_dev);
    if (err != 0) statUartErrors++;
}

// ── Direction control ─────────────────────────────────────────────────────────
static void goTransmit() {
    digitalWrite(DERE_PIN, DERE_TRANSMIT);
    delayMicroseconds(10);  // transceiver enable settle — tune against a scope
}
static void goReceive() {
    delayMicroseconds(txDrainUs);
    digitalWrite(DERE_PIN, DERE_RECEIVE);
}

// Send `tx` and wait up to pollTimeoutUs for a valid echo with matching
// sync+seq. Used for both normal data pings and the control handshake —
// resyncs on whatever sync byte WE sent (SYNC_BYTE or CTRL_SYNC), since
// that's what we expect echoed back. Distinguishes timeout (nothing usable
// came back in time) from a bad echo (a complete, checksum-valid frame that
// still doesn't match — e.g. a stale reply from a previous round) — that
// distinction was the key diagnostic signal behind bring-up bugs #6 and #7,
// so it's worth keeping visible rather than collapsing into one "failed"
// (return value is one of ECHO_OK/ECHO_TIMEOUT/ECHO_BAD, declared near Frame).
static int sendAndWaitEcho(const Frame& tx, uint32_t& rttUsOut) {
    uint8_t rx[FRAME_SIZE];
    int got = 0;
    uint32_t t0 = micros();

    goTransmit();
    rs485Write((const uint8_t*)&tx, FRAME_SIZE);
    goReceive();

    while ((uint32_t)(micros() - t0) < pollTimeoutUs && got < FRAME_SIZE) {
        uint8_t b;
        if (!rs485ReadByte(b)) continue;
        if (got == 0 && b != tx.sync) continue; // resync on our own sync byte
        rx[got++] = b;
    }

    if (got == FRAME_SIZE) {
        Frame* echo = (Frame*)rx;
        if (frameValid(*echo) && echo->sync == tx.sync && echo->seq == tx.seq) {
            rttUsOut = micros() - t0;
            return ECHO_OK;
        }
        return ECHO_BAD;
    }
    // drain any late/partial bytes before the next round
    uint8_t b;
    while (rs485ReadByte(b)) {}
    return ECHO_TIMEOUT;
}

static uint32_t currentBaud = HOME_BAUD;

// File-scope (not a loop()-local static) so get_stats() can expose "what's
// coming up next" — PRIMARY is the only one that ever advances this; on
// SECONDARY it's unused, since it no longer needs to guess ahead (see the
// control-frame comment above).
static int sweepIndex = 0;

// ── Stats — reset at the start of each baud phase, so each report reflects
// just that phase, not the whole run ─────────────────────────────────────────
static uint32_t statSent = 0, statAcked = 0, statTimeouts = 0, statBadEcho = 0;
static uint32_t statRxOk = 0, statRxBad = 0, statEchoed = 0;
static uint32_t lastRttUs = 0;

// Snapshot of the most recently *completed* phase, frozen (not reset) until
// the next phase finishes. The live stat* counters above reset to 0 the
// instant a phase ends, so a dashboard polling get_stats() on its own
// schedule can easily poll right after that reset and see a zeroed-out
// result instead of the real final tally — a race, not a bug in the poll
// itself. lastPhaseSeqNum increments once per completed phase so a client
// can tell "a new result arrived" apart from "still the same one" without
// depending on polling faster than the phase length.
static uint32_t lastPhaseSeqNum = 0;
static uint32_t lastPhaseBaud = 0;
static uint32_t lastPhaseSent = 0, lastPhaseAcked = 0, lastPhaseTimeouts = 0, lastPhaseBadEcho = 0, lastPhaseRtt = 0;
static uint32_t lastPhaseRxOk = 0, lastPhaseRxBad = 0, lastPhaseEchoed = 0;

static String handle_get_stats() {
    String r = "{";
    r += "\"role\":\""     + String(IS_PRIMARY ? "primary" : "secondary") + "\"";
    r += ",\"baud\":"      + String(currentBaud);
#if IS_PRIMARY
    r += ",\"sent\":"      + String(statSent);
    r += ",\"acked\":"     + String(statAcked);
    r += ",\"timeouts\":"  + String(statTimeouts);
    r += ",\"badEcho\":"   + String(statBadEcho);
    r += ",\"lastRttUs\":" + String(lastRttUs);
    r += ",\"nextBaud\":"  + String(BAUD_TABLE[sweepIndex]);
#else
    r += ",\"rxOk\":"      + String(statRxOk);
    r += ",\"rxBad\":"     + String(statRxBad);
    r += ",\"echoed\":"    + String(statEchoed);
#endif
    r += ",\"uartErrors\":"   + String(statUartErrors);
    r += ",\"ringOverflow\":" + String(statRingOverflow);
    r += ",\"uptime\":"       + String(millis() / 1000);
    // Frozen snapshot of the last completed phase — see comment above.
    r += ",\"lastPhaseSeqNum\":" + String(lastPhaseSeqNum);
    r += ",\"lastPhaseBaud\":"   + String(lastPhaseBaud);
#if IS_PRIMARY
    r += ",\"lastPhaseSent\":"     + String(lastPhaseSent);
    r += ",\"lastPhaseAcked\":"    + String(lastPhaseAcked);
    r += ",\"lastPhaseTimeouts\":" + String(lastPhaseTimeouts);
    r += ",\"lastPhaseBadEcho\":"  + String(lastPhaseBadEcho);
    r += ",\"lastPhaseRtt\":"      + String(lastPhaseRtt);
#else
    r += ",\"lastPhaseRxOk\":"   + String(lastPhaseRxOk);
    r += ",\"lastPhaseRxBad\":"  + String(lastPhaseRxBad);
    r += ",\"lastPhaseEchoed\":" + String(lastPhaseEchoed);
#endif
    r += "}";
    return r;
}

static void reportAndResetPhaseStats() {
    Monitor.print("=== baud="); Monitor.print(currentBaud);
    lastPhaseBaud = currentBaud;
#if IS_PRIMARY
    Monitor.print(" sent=");     Monitor.print(statSent);
    Monitor.print(" acked=");    Monitor.print(statAcked);
    Monitor.print(" timeouts="); Monitor.print(statTimeouts);
    Monitor.print(" badEcho=");  Monitor.print(statBadEcho);
    Monitor.print(" lastRttUs="); Monitor.print(lastRttUs);
    lastPhaseSent = statSent; lastPhaseAcked = statAcked;
    lastPhaseTimeouts = statTimeouts; lastPhaseBadEcho = statBadEcho;
    lastPhaseRtt = lastRttUs;
    statSent = statAcked = statTimeouts = statBadEcho = 0;
    lastRttUs = 0;
#else
    Monitor.print(" rxOk=");   Monitor.print(statRxOk);
    Monitor.print(" rxBad=");  Monitor.print(statRxBad);
    Monitor.print(" echoed="); Monitor.print(statEchoed);
    lastPhaseRxOk = statRxOk; lastPhaseRxBad = statRxBad; lastPhaseEchoed = statEchoed;
    statRxOk = statRxBad = statEchoed = 0;
#endif
    Monitor.println(" ===");
    lastPhaseSeqNum++;
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(DERE_PIN, OUTPUT);
    digitalWrite(DERE_PIN, DERE_RECEIVE);

    Bridge.begin();
    Monitor.begin();
    Bridge.provide_safe("get_stats", handle_get_stats);

    rs485Ready = device_is_ready(rs485_dev);
    if (!rs485Ready) {
        Monitor.println("usart1 device not ready — check devicetree");
    } else {
        uart_irq_callback_user_data_set(rs485_dev, rs485Isr, NULL);
        uart_irq_rx_enable(rs485_dev);
    }

    currentBaud = HOME_BAUD;
    rs485SetBaud(currentBaud);
    Monitor.print(IS_PRIMARY ? "RS-485 speed test — PRIMARY @ " : "RS-485 speed test — SECONDARY @ ");
    Monitor.println(currentBaud);
}

// ── PRIMARY loop: drives the sweep, pings at whatever baud is current ───────
#if IS_PRIMARY
void loop() {
    if (!rs485Ready) { delay(1000); rs485Ready = device_is_ready(rs485_dev); return; }
    rs485ClearErrors();

    static uint32_t seq = 0;
    static uint32_t ctrlSeq = 0;
    static uint32_t phaseStartMs = 0;
    static uint32_t ctrlRetries = 0;

    if (millis() - phaseStartMs >= TEST_DURATION_MS) {
        if (currentBaud == HOME_BAUD) {
            // Advance the sweep: handshake at the reliable home baud first,
            // sending the actual target baud explicitly (see the control-
            // frame comment near Frame) rather than trusting SECONDARY's own
            // counter to still be in step.
            uint32_t targetBaud = BAUD_TABLE[sweepIndex];
            Frame ctrl;
            buildCtrlFrame(ctrl, ctrlSeq, targetBaud);
            uint32_t rtt;
            if (sendAndWaitEcho(ctrl, rtt) == ECHO_OK) {
                reportAndResetPhaseStats();
                ctrlSeq++;
                ctrlRetries = 0;
                currentBaud = targetBaud;
                sweepIndex  = (sweepIndex + 1) % NUM_BAUDS;
                rs485SetBaud(currentBaud);
                Monitor.print("-> switching to "); Monitor.println(currentBaud);
                phaseStartMs = millis();
            } else if (++ctrlRetries >= CTRL_MAX_RETRIES) {
                // Couldn't reach the other side even at home baud — skip
                // this handshake attempt and just try again next phase
                // rather than getting stuck forever.
                Monitor.println("ctrl handshake failed repeatedly, skipping");
                ctrlRetries = 0;
                phaseStartMs = millis();
            }
            // else: leave phaseStartMs alone so this retries immediately
            // on the next loop() call.
        } else {
            reportAndResetPhaseStats();
            currentBaud = HOME_BAUD;
            rs485SetBaud(currentBaud);
            Monitor.println("-> reverting to home 9600");
            phaseStartMs = millis();
        }
        return;
    }

    Frame tx;
    buildFrame(tx, SYNC_BYTE, seq);
    statSent++;
    uint32_t rtt;
    switch (sendAndWaitEcho(tx, rtt)) {
        case ECHO_OK:      statAcked++; lastRttUs = rtt; break;
        case ECHO_BAD:     statBadEcho++; break;
        case ECHO_TIMEOUT: statTimeouts++; break;
    }
    seq++;

    static uint32_t lastReport = 0;
    if (millis() - lastReport >= 1000) {
        lastReport = millis();
        Monitor.print("baud="); Monitor.print(currentBaud);
        Monitor.print(" sent="); Monitor.print(statSent);
        Monitor.print(" acked="); Monitor.print(statAcked);
        Monitor.print(" rtt_us="); Monitor.print(lastRttUs);
        Monitor.print(" next="); Monitor.println(BAUD_TABLE[sweepIndex]);
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
}

// ── SECONDARY loop: listen, validate, echo — follows PRIMARY's baud calls ──
#else
void loop() {
    if (!rs485Ready) { delay(1000); rs485Ready = device_is_ready(rs485_dev); return; }
    rs485ClearErrors();

    static uint32_t testEndMs  = 0;

    if (currentBaud != HOME_BAUD && millis() >= testEndMs) {
        reportAndResetPhaseStats();
        currentBaud = HOME_BAUD;
        rs485SetBaud(currentBaud);
        Monitor.println("-> reverting to home 9600");
    }

    static uint8_t buf[FRAME_SIZE];
    static int filled = 0;

    uint8_t b;
    while (rs485ReadByte(b)) {
        if (filled == 0 && b != SYNC_BYTE && b != CTRL_SYNC) continue; // resync
        buf[filled++] = b;

        if (filled == FRAME_SIZE) {
            Frame* f = (Frame*)buf;
            bool valid = frameValid(*f);

            if (valid && f->sync == CTRL_SYNC) {
                uint32_t targetBaud = ctrlFrameTargetBaud(*f);
                goTransmit();
                rs485Write(buf, FRAME_SIZE);
                goReceive();
                reportAndResetPhaseStats();
                currentBaud = targetBaud;
                rs485SetBaud(currentBaud);
                testEndMs = millis() + TEST_DURATION_MS;
                Monitor.print("-> switching to "); Monitor.println(currentBaud);
            } else if (valid) {
                statRxOk++;
                goTransmit();
                rs485Write(buf, FRAME_SIZE);
                goReceive();
                statEchoed++;
            } else {
                statRxBad++;
            }
            filled = 0;
        }
    }

    static uint32_t lastReport = 0;
    if (millis() - lastReport >= 1000) {
        lastReport = millis();
        Monitor.print("baud="); Monitor.print(currentBaud);
        Monitor.print(" rxOk="); Monitor.print(statRxOk);
        Monitor.print(" rxBad="); Monitor.print(statRxBad);
        Monitor.print(" echoed="); Monitor.println(statEchoed);
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
}
#endif
