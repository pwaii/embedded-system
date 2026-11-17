#!/usr/bin/env python3
"""
fake_device.py — software stand-in for the timer box, so NET and WEB can work before the hardware is ready.

It runs the same state machine as the STM32 (docs/architecture.md §4) and speaks protocol v1.0
(docs/protocol.md). Pick where the output goes:

  --stdout              print protocol lines; type CFG/CMD lines or keys on stdin   (no dependencies)
  --serial PORT         act as the STM32 on a USB-TTL adapter wired to the ESP32    (pip install pyserial)
  --firebase DB_URL     act as STM32 + ESP32 and write straight to Firebase via REST (no dependencies)
                        add --api-key KEY to sign in anonymously (needed once the rules require auth)

Keys (type + Enter):  n = end turn   p = pause/resume (encoder SW)   e = end game (long press)
                      s = start      r = reset        q = quit
                      or any raw protocol line, e.g.  CFG,4,20   CMD,PAUSE
--auto plays by itself: random turn lengths around the limit, some overtime, new game after each end.

Examples:
  python3 tools/fake_device.py --stdout --auto --speed 5
  python3 tools/fake_device.py --firebase https://<project>-default-rtdb.asia-southeast1.firebasedatabase.app \
          --api-key AIza... --auto
"""
import argparse
import json
import queue
import random
import sys
import threading
import time
import urllib.request

FW = "sim-1.0"
STATES = ("SETUP", "READY", "RUN", "PAUSE", "OVER")


def now_ms():
    return int(time.monotonic() * 1000)


# --------------------------------------------------------------------------- game (mirror of game.c)
class Game:
    def __init__(self, emit, speed=1.0):
        self.emit = emit            # emit(line_without_newline)
        self.speed = speed          # >1 makes game time run faster than wall time
        self.state = "SETUP"
        self.players, self.limit_s = 4, 60
        self.game_id = None
        self.turn_no = self.player = 0
        self.turn_start = self.pause_start = 0
        self.paused_total = 0
        self.last_state_tx = 0

    # time in "game ms" (scaled by --speed)
    def t(self):
        return int(now_ms() * self.speed)

    def elapsed(self):
        end = self.pause_start if self.state == "PAUSE" else self.t()
        return end - self.turn_start - self.paused_total

    def remaining(self):
        if self.state in ("SETUP", "READY") or self.game_id is None:
            return self.limit_s * 1000
        return self.limit_s * 1000 - self.elapsed()

    def send_state(self):
        gid = self.game_id or "-"
        self.emit(f"STATE,{gid},{self.state},{self.players},{self.limit_s},"
                  f"{self.turn_no},{self.player},{max(-9999999, self.remaining())}")
        self.last_state_tx = now_ms()

    def set_state(self, s):
        self.state = s
        self.send_state()

    def _begin_turn(self):
        self.turn_no += 1
        self.turn_start = self.t()
        self.paused_total = 0

    def _emit_turn(self):
        d = max(0, self.elapsed())
        o = max(0, d - self.limit_s * 1000)
        self.emit(f"TURN,{self.game_id},{self.turn_no},{self.player},{d},{o}")

    # ---- events (same names as input.c / link.c events)
    def start(self):
        if self.state not in ("SETUP", "READY"):
            return False
        self.game_id = f"{random.getrandbits(32):08x}"
        self.turn_no, self.player = 0, 1
        self._begin_turn()
        self.emit(f"START,{self.game_id},{self.players},{self.limit_s}")
        self.set_state("RUN")
        return True

    def end_turn(self):
        if self.state != "RUN":
            return False
        self._emit_turn()
        self.player = self.player % self.players + 1
        self._begin_turn()
        self.send_state()
        return True

    def toggle_pause(self):
        if self.state == "RUN":
            return self.pause()
        if self.state == "PAUSE":
            return self.resume()
        return False

    def pause(self):
        if self.state != "RUN":
            return False
        self.pause_start = self.t()
        self.set_state("PAUSE")
        return True

    def resume(self):
        if self.state != "PAUSE":
            return False
        self.paused_total += self.t() - self.pause_start
        self.set_state("RUN")
        return True

    def end_game(self):
        if self.state not in ("RUN", "PAUSE"):
            return False
        if self.state == "PAUSE":
            self.resume_silently()
        self._emit_turn()
        self.emit(f"END,{self.game_id},{self.turn_no},USER")
        self.set_state("OVER")
        return True

    def resume_silently(self):
        self.paused_total += self.t() - self.pause_start
        self.state = "RUN"

    def reset(self):
        if self.state in ("RUN", "PAUSE"):
            self.emit(f"END,{self.game_id},{max(0, self.turn_no - 1)},RESET")
        self.game_id = None
        self.turn_no = self.player = 0
        self.set_state("SETUP")
        return True

    def cfg(self, players, limit_s):
        if self.state not in ("SETUP", "READY", "OVER"):
            return False
        self.players, self.limit_s = players, limit_s
        self.game_id = None
        self.turn_no = self.player = 0
        self.set_state("READY")
        return True

    def tick(self):
        if now_ms() - self.last_state_tx >= 1000:
            self.send_state()

    # ---- incoming protocol line from the ESP32 side
    def handle_line(self, line):
        f = line.strip().split(",")
        if not f or not f[0]:
            return
        kind = f[0]
        if kind == "CFG":
            try:
                p, l = int(f[1]), int(f[2])
                assert len(f) == 3
            except (ValueError, IndexError, AssertionError):
                return self.emit("ERR,CFG,PARSE")
            if not (2 <= p <= 6 and 10 <= l <= 300 and l % 5 == 0):
                return self.emit("ERR,CFG,RANGE")
            if self.state not in ("SETUP", "READY", "OVER"):
                return self.emit("ERR,CFG,BUSY")
            self.emit("ACK,CFG")
            self.cfg(p, l)
        elif kind == "CMD":
            fn = {"START": self.start, "PAUSE": self.pause, "RESUME": self.resume,
                  "END": self.end_game, "RESET": self.reset}.get(f[1] if len(f) > 1 else "")
            if fn is None:
                return self.emit("ERR,CMD,RANGE")
            # decide validity first so ACK precedes the state change, like the firmware
            valid = {"START": self.state in ("SETUP", "READY"), "PAUSE": self.state == "RUN",
                     "RESUME": self.state == "PAUSE", "END": self.state in ("RUN", "PAUSE"),
                     "RESET": True}[f[1]]
            if not valid:
                return self.emit("ERR,CMD,BUSY")
            self.emit("ACK,CMD")
            fn()
        elif kind == "HELLO" and len(f) > 1 and f[1] == "ESP":
            self.emit(f"HELLO,STM,{FW}")
            self.send_state()
        elif kind in ("WIFI", "ACK"):
            pass
        else:
            self.emit(f"ERR,{kind},UNKNOWN")


# --------------------------------------------------------------------------- Firebase sink (plays the ESP32)
class Firebase:
    """Minimal REST client that performs the UART -> DB mapping from docs/cloud.md §2."""

    def __init__(self, url, api_key=None):
        self.url = url.rstrip("/")
        self.api_key = api_key
        self.token, self.token_t = None, 0
        self.names = {}
        self.last_cfg = None

    def _auth(self):
        if not self.api_key:
            return ""
        if not self.token or time.time() - self.token_t > 50 * 60:
            req = urllib.request.Request(
                f"https://identitytoolkit.googleapis.com/v1/accounts:signUp?key={self.api_key}",
                data=json.dumps({"returnSecureToken": True}).encode(),
                headers={"Content-Type": "application/json"}, method="POST")
            with urllib.request.urlopen(req, timeout=10) as r:
                self.token = json.load(r)["idToken"]
            self.token_t = time.time()
        return f"?auth={self.token}"

    def req(self, method, path, body=None):
        data = None if body is None else json.dumps(body).encode()
        r = urllib.request.Request(f"{self.url}/{path}.json{self._auth()}", data=data, method=method,
                                   headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(r, timeout=10) as resp:
            return json.load(resp)

    def on_line(self, line):
        f = line.split(",")
        ts = int(time.time() * 1000)
        if f[0] == "STATE":
            self.req("PUT", "device/status", {
                "game_id": None if f[1] == "-" else f[1], "state": f[2], "players": int(f[3]),
                "limit_s": int(f[4]), "turn_no": int(f[5]), "player": int(f[6]),
                "remaining_ms": int(f[7]), "wifi": True, "fw": {"stm": FW, "esp": FW},
                "updated_at": {".sv": "timestamp"}})
        elif f[0] == "START":
            self.req("PATCH", f"games/{f[1]}/meta", {
                "players": int(f[2]), "limit_s": int(f[3]), "names": self.names, "started_at": ts})
        elif f[0] == "TURN":
            n = int(f[2])
            self.req("PUT", f"games/{f[1]}/turns/t{n:03d}", {
                "turn_no": n, "player": int(f[3]), "duration_ms": int(f[4]),
                "overtime_ms": int(f[5]), "ended_at": ts})
        elif f[0] == "END":
            self.req("PATCH", f"games/{f[1]}/meta", {
                "ended_at": ts, "end_reason": f[3], "total_turns": int(f[2])})

    def poll(self):
        """Returns protocol lines the ESP32 would forward to the STM32."""
        out = []
        cfg = self.req("GET", "device/config")
        if cfg and cfg != self.last_cfg:
            self.last_cfg = cfg
            names = cfg.get("names") or {}
            if isinstance(names, list):  # tolerate array shape
                names = {str(i): n for i, n in enumerate(names) if n}
            self.names = names
            out.append(f"CFG,{cfg.get('players')},{cfg.get('limit_s')}")
        cmd = self.req("GET", "device/command")
        if cmd and cmd.get("cmd"):
            self.req("DELETE", "device/command")
            out.append(f"CMD,{cmd['cmd']}")
        return out


# --------------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sink = ap.add_mutually_exclusive_group(required=True)
    sink.add_argument("--stdout", action="store_true")
    sink.add_argument("--serial", metavar="PORT")
    sink.add_argument("--firebase", metavar="DB_URL")
    ap.add_argument("--api-key", help="Firebase Web API key (anonymous sign-in)")
    ap.add_argument("--auto", action="store_true", help="play automatically")
    ap.add_argument("--speed", type=float, default=1.0, help="game-time speed-up (auto mode)")
    ap.add_argument("--players", type=int, default=4)
    ap.add_argument("--limit", type=int, default=20, help="seconds per turn")
    ap.add_argument("--turns", type=int, default=12, help="turns per auto game")
    ap.add_argument("--quiet", action="store_true", help="don't echo protocol lines")
    a = ap.parse_args()

    inbox = queue.Queue()          # lines/keys coming *into* the device
    ser = fb = None

    def log(prefix, line):
        if not a.quiet:
            print(f"{prefix} {line}", flush=True)

    def emit(line):
        log("STM>", line)
        if ser:
            ser.write((line + "\n").encode())
        if fb:
            try:
                fb.on_line(line)
            except Exception as e:  # keep playing offline, like the real box
                log("!!", f"firebase write failed: {e}")

    if a.serial:
        import serial  # pip install pyserial
        ser = serial.Serial(a.serial, 115200, timeout=0.1)

        def serial_reader():
            buf = b""
            while True:
                buf += ser.read(64)
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    inbox.put(raw.decode(errors="replace").strip())
        threading.Thread(target=serial_reader, daemon=True).start()

    if a.firebase:
        fb = Firebase(a.firebase, a.api_key)

        def fb_poller():
            while True:
                try:
                    for line in fb.poll():
                        inbox.put(line)
                except Exception as e:
                    log("!!", f"firebase poll failed: {e}")
                time.sleep(1.0)
        threading.Thread(target=fb_poller, daemon=True).start()

    def stdin_reader():
        for raw in sys.stdin:
            inbox.put(raw.strip())
        inbox.put("q")
    if not a.auto or a.stdout:
        threading.Thread(target=stdin_reader, daemon=True).start()

    g = Game(emit, speed=a.speed if a.auto else 1.0)
    g.players, g.limit_s = a.players, a.limit
    emit(f"HELLO,STM,{FW}")
    g.send_state()

    keys = {"n": g.end_turn, "p": g.toggle_pause, "e": g.end_game, "s": g.start, "r": g.reset}
    next_auto = now_ms() + 2000
    turn_target = 0

    while True:
        try:
            item = inbox.get(timeout=0.05)
            if item == "q":
                break
            if item in keys:
                keys[item]()
            elif item:
                log("ESP>", item)
                g.handle_line(item)
        except queue.Empty:
            pass

        if a.auto:
            if g.state in ("SETUP", "READY", "OVER") and now_ms() >= next_auto:
                if g.state == "OVER":
                    g.reset()
                g.start()
                turn_target = int(g.limit_s * 1000 * random.uniform(0.3, 1.4))
            elif g.state == "RUN" and g.elapsed() >= turn_target:
                if g.turn_no >= a.turns:
                    g.end_game()
                    next_auto = now_ms() + 5000
                else:
                    g.end_turn()
                    turn_target = int(g.limit_s * 1000 * random.uniform(0.3, 1.4))
        g.tick()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
