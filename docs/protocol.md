# STM32 ↔ ESP32 UART Protocol — v1.0

Status: **draft → LOCKED once EMB, NET and LEAD sign off below.** After locking, any change needs a
PR that bumps the version and updates `common/proto.c` + its tests in the same PR.

Reference implementation: [`common/proto.h`](../common/proto.h) / [`common/proto.c`](../common/proto.c)
(compiled into both firmwares). Test on a PC with `make -C common test`.

## 1. Physical layer

| Item | Value |
| --- | --- |
| STM32 side | USART1 — PA9 (TX, Nucleo D8) / PA10 (RX, Nucleo D2) |
| ESP32-S3 side | `Serial1` — GPIO17 (TX) / GPIO18 (RX) |
| Wiring | STM32 TX → ESP32 RX, STM32 RX ← ESP32 TX, **common GND** (3.3 V logic both sides) |
| Format | 115200 baud, 8N1, no flow control |

## 2. Framing

- One message per line: ASCII, fields separated by `,`, terminated by `\n`. A `\r` before `\n` is ignored.
- **Max 64 bytes including `\n`.** Receivers discard longer lines (resync at the next `\n`).
- Field 0 is the message type (upper case). Fields never contain commas or spaces.
- Unknown types → reply `ERR,<type>,UNKNOWN` (or ignore if it's a reply type). Malformed fields → `ERR,<type>,PARSE` or `ERR,<type>,RANGE`.
- Numbers are base-10 integers. `game_id` is exactly 8 lower-case hex chars, or `-` when no game exists.

## 3. Common field ranges

| Field | Range | Notes |
| --- | --- | --- |
| `players` | 2–6 | |
| `limit_s` | 10–300, multiple of 5 | seconds |
| `player` | 1–`players`, or 0 in SETUP/READY | 1-based |
| `turn_no` | 0–999 | 1-based; 0 = no turn yet |
| `remaining_ms` | −9 999 999 … 9 999 999 | **signed**, negative = overtime |
| `duration_ms`, `overtime_ms` | 0 … 9 999 999 | |
| `state` | `SETUP` `READY` `RUN` `PAUSE` `OVER` | |

## 4. Messages — STM32 → ESP32

| Message | When | Example |
| --- | --- | --- |
| `HELLO,STM,<fw>` | boot, and in reply to `HELLO,ESP,...` | `HELLO,STM,1.0` |
| `STATE,<game_id>,<state>,<players>,<limit_s>,<turn_no>,<player>,<remaining_ms>` | every state change **and** every 1000 ms | `STATE,1a2b3c4d,RUN,4,60,7,3,-2100` |
| `START,<game_id>,<players>,<limit_s>` | entering RUN from SETUP/READY | `START,1a2b3c4d,4,60` |
| `TURN,<game_id>,<turn_no>,<player>,<duration_ms>,<overtime_ms>` | each finished turn | `TURN,1a2b3c4d,7,3,62100,2100` |
| `END,<game_id>,<total_turns>,<reason>` | game ends; reason `USER` or `RESET` | `END,1a2b3c4d,24,USER` |
| `ACK,<type>` | after accepting `CFG` / `CMD` | `ACK,CFG` |
| `ERR,<type>,<reason>` | rejecting a message; reason `PARSE` `RANGE` `BUSY` `UNKNOWN` | `ERR,CFG,BUSY` |

In SETUP, `STATE` shows the values currently being edited: `STATE,-,SETUP,4,60,0,0,60000`.

## 5. Messages — ESP32 → STM32

| Message | When | STM32 reaction | Example |
| --- | --- | --- | --- |
| `HELLO,ESP,<fw>` | ESP32 boot | reply `HELLO,STM,...` then immediately send `STATE` | `HELLO,ESP,1.0` |
| `CFG,<players>,<limit_s>` | `/device/config` changed | accept in SETUP/READY/OVER → READY, `ACK,CFG`; else `ERR,CFG,BUSY` | `CFG,3,90` |
| `CMD,<cmd>` | web button; cmd = `START` `PAUSE` `RESUME` `END` `RESET` | apply if valid for state → `ACK,CMD`, else `ERR,CMD,BUSY` | `CMD,PAUSE` |
| `WIFI,<0\|1>` | on change **and every 5 s** (heartbeat) | 1 = cloud reachable. No line from ESP32 for 12 s ⇒ treat as bridge down | `WIFI,1` |
| `ACK,TURN,<turn_no>` | after `TURN` is queued/stored | stop retransmitting that turn | `ACK,TURN,7` |

## 6. Reliability rules

1. **`STATE` is a full snapshot.** Anyone who missed messages (e.g. a rebooted ESP32) is fully in
   sync after the next `STATE` (≤ 1 s).
2. **`TURN` retransmit (SHOULD):** STM32 keeps the last unacknowledged `TURN`; resend after 500 ms
   without `ACK,TURN,<n>`, max 3 tries, then drop it. Keep a small queue (≥ 4) if turns can end faster
   than acks arrive.
3. **Idempotency (MUST):** the ESP32 writes `TURN` to `/games/<game_id>/turns/t<NNN>`, so a duplicate
   is a harmless overwrite. It still ACKs duplicates.
4. **No blocking:** neither side ever waits for a reply before continuing its loop.
5. Neither side relies on `ACK,CFG` / `ACK,CMD` for correctness — the next `STATE` is the truth.

## 7. Example session

```
STM> HELLO,STM,1.0
STM> STATE,-,SETUP,4,60,0,0,60000
ESP> HELLO,ESP,1.0
STM> HELLO,STM,1.0
STM> STATE,-,SETUP,4,60,0,0,60000
ESP> WIFI,1
ESP> CFG,4,20
STM> ACK,CFG
STM> STATE,-,READY,4,20,0,0,20000
ESP> CMD,START
STM> ACK,CMD
STM> START,9f03c2aa,4,20
STM> STATE,9f03c2aa,RUN,4,20,1,1,20000
STM> STATE,9f03c2aa,RUN,4,20,1,1,19000
...
STM> TURN,9f03c2aa,1,1,17250,0
ESP> ACK,TURN,1
STM> STATE,9f03c2aa,RUN,4,20,2,2,20000
...
STM> TURN,9f03c2aa,2,2,23400,3400
ESP> ACK,TURN,2
...                                      (long press on KY-004)
STM> TURN,9f03c2aa,9,1,8100,0
STM> END,9f03c2aa,9,USER
STM> STATE,9f03c2aa,OVER,4,20,9,1,11900
```

## 8. Developing without the other side

- **NET without STM32:** `python3 tools/fake_device.py --serial /dev/ttyUSB0` with a USB-TTL adapter
  on GPIO17/18 (or read the generated lines with `--stdout` and paste them into the ESP32 code as test input).
- **EMB without ESP32:** type `CFG,3,20` / `CMD,PAUSE` into a serial terminal on the USB-TTL adapter connected to PA9/PA10.
- **WEB without either:** `python3 tools/fake_device.py --firebase <db-url>` writes the same data the ESP32 would.

## Sign-off

| Role | Name | Date |
| --- | --- | --- |
| LEAD | Chikawa (Boeing) | |
| EMB | Usagi (Ice) | |
| NET | Furuhonya (Pinn) | |
