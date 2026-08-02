import json
import os
import queue
import threading
import time

from flask import Flask, Response, jsonify, send_from_directory, stream_with_context
from arduino.app_utils import App, Bridge

WEB_PORT   = 8080
STATIC_DIR = os.path.join(os.path.dirname(__file__), 'static')
LOG_MAX    = 100   # rolling per-baud-phase results kept for the dashboard

stats        = {}
log          = []   # newest first — one entry per completed baud phase
last_seq_num = None # lastPhaseSeqNum last seen, to detect a newly-completed phase
sse_clients  = set()
sse_lock     = threading.Lock()

# ── SSE broadcast ─────────────────────────────────────────────────────────────
def sse_push(event_data):
    payload = f"data: {json.dumps(event_data)}\n\n"
    with sse_lock:
        dead = set()
        for q in sse_clients:
            try:
                q.put_nowait(payload)
            except queue.Full:
                dead.add(q)
        sse_clients.difference_update(dead)

def payload():
    return {'stats': stats, 'log': log}

# ── Flask web server ──────────────────────────────────────────────────────────
web = Flask(__name__)

# Permissive CORS so one node's dashboard (e.g. athena:8080) can pull the
# other node's /events and /api/stats directly from the browser (e.g. when
# the user points it at briana:8080 via the "add remote node" box). This is
# a LAN-only diagnostic tool with no auth and no sensitive data, so a
# wide-open Access-Control-Allow-Origin is a reasonable trade — would not be
# an appropriate default for anything internet-facing.
@web.after_request
def add_cors(resp):
    resp.headers['Access-Control-Allow-Origin'] = '*'
    return resp

@web.route('/')
def index():
    return send_from_directory(STATIC_DIR, 'index.html')

@web.route('/events')
def events():
    q = queue.Queue(maxsize=10)
    with sse_lock:
        sse_clients.add(q)
    initial = json.dumps(payload())

    def generate():
        try:
            yield f"data: {initial}\n\n"
            while True:
                try:
                    yield q.get(timeout=30)
                except queue.Empty:
                    yield ": heartbeat\n\n"
        finally:
            with sse_lock:
                sse_clients.discard(q)

    return Response(
        stream_with_context(generate()),
        content_type='text/event-stream',
        headers={'Cache-Control': 'no-cache', 'X-Accel-Buffering': 'no'},
    )

@web.route('/api/stats')
def api_stats():
    return jsonify(payload())

@web.route('/health')
def health():
    return jsonify({'status': 'ok'})

threading.Thread(
    target=lambda: web.run(host='0.0.0.0', port=WEB_PORT, debug=False,
                           use_reloader=False, threaded=True),
    daemon=True,
).start()

# ── Bridge poll loop ──────────────────────────────────────────────────────────
# Polls faster than the old 1s cadence so the live radar/pulse animation on
# the dashboard feels responsive. lastPhaseSeqNum (see sketch.ino) increments
# once per completed baud phase — the moment it changes from what we last
# saw, the sketch's own lastPhase* fields hold a frozen, race-free snapshot
# of that just-finished phase, which is what turns into one log row here.
# The live stat* counters in `stats` reset to 0 the instant a phase ends, so
# they're fine for a real-time gauge but not for a result history.
def loop():
    global stats, last_seq_num
    try:
        stats = json.loads(Bridge.call("get_stats"))
    except Exception as e:
        print(f"[bridge] {e}")
        time.sleep(0.25)
        return

    seq_num = stats.get('lastPhaseSeqNum')
    if seq_num is not None and seq_num != last_seq_num:
        if last_seq_num is not None:  # skip the very first poll — no phase has "just completed" yet
            entry = {
                'seqNum': seq_num,
                'baud':   stats.get('lastPhaseBaud'),
                'role':   stats.get('role'),
            }
            if stats.get('role') == 'primary':
                entry['sent']     = stats.get('lastPhaseSent')
                entry['acked']    = stats.get('lastPhaseAcked')
                entry['timeouts'] = stats.get('lastPhaseTimeouts')
                entry['badEcho']  = stats.get('lastPhaseBadEcho')
                entry['rttUs']    = stats.get('lastPhaseRtt')
                total = entry['sent'] or 0
                entry['successPct'] = round(100 * (entry['acked'] or 0) / total, 1) if total else None
            else:
                entry['rxOk']   = stats.get('lastPhaseRxOk')
                entry['rxBad']  = stats.get('lastPhaseRxBad')
                entry['echoed'] = stats.get('lastPhaseEchoed')
                total = (entry['rxOk'] or 0) + (entry['rxBad'] or 0)
                entry['successPct'] = round(100 * (entry['rxOk'] or 0) / total, 1) if total else None
            log.insert(0, entry)
            del log[LOG_MAX:]
        last_seq_num = seq_num

    sse_push(payload())
    time.sleep(0.25)

print(f"Web UI → http://<this-node>.local:{WEB_PORT}/")
App.run(user_loop=loop)
