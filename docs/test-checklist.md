# Integration Test Checklist

Run top to bottom before every gate and the demo. Record date + result; anything ❌ becomes a GitHub issue.
`R1–R3` = course requirements (STM32↔NodeMCU processing, 3 sensor/actuator types, Internet + cloud).

| ID | Req | Setup | Steps | Expected | Result |
| --- | --- | --- | --- | --- | --- |
| **Unit / bench** | | | | | |
| T01 | — | PC | `make -C common test` | `0 failed` | |
| T02 | R2 | Nucleo + USB | Rotate encoder 10 detents CW, 10 CCW (printf) | count +10 then back to 0, no skipped/extra steps | |
| T03 | R2 | Nucleo | Tap KY-004 20× quickly | exactly 20 short presses, 0 long | |
| T04 | R2 | Nucleo | Hold KY-004 2.5 s | one long press at 2.0 s ± 50 ms, no short press | |
| T05 | R2 | Nucleo | LED test pattern | green, yellow, red, blue, white distinct; blink 2 Hz; breathing smooth | |
| T06 | R2 | Nucleo | 4th module test (only if added, see architecture D7) | behaves as specified in `architecture.md` | |
| T07 | R1 | ESP32 + USB-TTL | `fake_device.py --serial` | every line from sim appears correctly in Firebase | |
| **Box only (no WiFi)** | | | | | |
| T10 | R1 | Box, ESP32 unplugged | Setup 4 players / 20 s, start, play 8 turns, long-press | all colours seen; ends in OVER; magenta flash every 5 s | |
| T11 | — | Box | During a turn pause 10 s, resume, end turn | `TURN` duration excludes the 10 s (check printf/serial) | |
| T12 | — | Box | Let a turn run to 25 s with 20 s limit | red blinking after 20 s; `TURN … ,25xxx,5xxx` | |
| T13 | — | Box | Press KY-004 twice within 300 ms | only one turn ends | |
| T14 | — | Box | Run 30 min with turns every ~10 s | no hang, timing still accurate vs phone stopwatch (± 1 s) | |
| **End-to-end** | | | | | |
| T20 | R1 R3 | Full system | Change config on web Setup page | box goes READY with new values within 2 s; READY chirp | |
| T21 | R3 | Full system | Start from web; end 3 turns on box | Live page follows each turn within 1–2 s; 3 turns in `/games/<id>/turns` | |
| T22 | R3 | Full system | Pause / Resume / Reset from web | box obeys within 2 s; RESET writes `end_reason: RESET` | |
| T23 | R3 | Full system | Change config on web mid-game | box ignores it (`ERR,CFG,BUSY`), game unaffected | |
| T24 | R3 | Full system | Turn hotspot off for 3 turns, then on | box keeps playing; all turns appear after reconnect, none duplicated | |
| T25 | — | Full system | Reset ESP32 (EN button) mid-game | web resumes live updates within ~5 s; game continues | |
| T26 | — | Full system | Unplug box power | web shows "offline" within 5 s | |
| T27 | R3 | Phone on 4G | Open hosted URL on a phone not on the same WiFi | Live/Stats load; sign-in works | |
| T28 | — | Full system | Stats page after a 12-turn game | per-player average, overtime rate and titles match a hand calculation | |
| **Demo readiness** | | | | | |
| T30 | — | Demo room | Connect to room WiFi or hotspot | ESP32 online < 15 s from power-on | |
| T31 | — | Power bank only | Full demo script | runs within 4:00 | |
