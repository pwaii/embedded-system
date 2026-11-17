# Cloud Contract — Firebase Realtime Database v1.0

Contract between the ESP32 bridge (NET) and the web dashboard (WEB). Rules file:
[`firebase/database.rules.json`](../firebase/database.rules.json).

## 1. Tree

```
/device
  /status                     ← ESP32 writes (on every STATE line)
      game_id      "9f03c2aa" | null
      state        "SETUP" | "READY" | "RUN" | "PAUSE" | "OVER"
      players      4
      limit_s      20
      turn_no      7
      player       3
      remaining_ms -2100            (negative = overtime)
      wifi         true             (always true when written; staleness = offline)
      fw           { stm: "1.0", esp: "1.0" }
      updated_at   {".sv":"timestamp"}   (server time, ms)
  /config                     ← WEB writes, ESP32 listens → CFG
      players      4
      limit_s      20
      names        { "1": "Usagi", "2": "Furuhonya", "3": "Rakku", "4": "Chikawa" }
      updated_at   {".sv":"timestamp"}
  /command                    ← WEB writes, ESP32 forwards as CMD then DELETES the node
      cmd          "START" | "PAUSE" | "RESUME" | "END" | "RESET"
      ts           {".sv":"timestamp"}

/games/{game_id}              ← key = game_id from STM32 (8 hex chars)
  /meta
      players      4
      limit_s      20
      names        { "1": "Usagi", ... }     (ESP32 copies /device/config/names at START)
      started_at   1763359200000           (ESP32 NTP epoch ms, or null if never synced)
      ended_at     1763360400000 | absent  (absent = in progress or unfinished)
      end_reason   "USER" | "RESET" | absent
      total_turns  24 | absent
  /turns/{tNNN}               ← key "t001", "t002", ... (zero-padded turn_no)
      turn_no      7
      player       3
      duration_ms  62100
      overtime_ms  2100
      ended_at     1763359650000           (ESP32 time when the line arrived; null if no NTP)
```

## 2. Mapping UART → DB (ESP32)

| Line from STM32 | DB operation |
| --- | --- |
| `STATE,...` | `set /device/status` (whole object, `updated_at` = server timestamp) |
| `START,id,p,l` | `update /games/id/meta {players, limit_s, names, started_at}` |
| `TURN,id,n,p,d,o` | `set /games/id/turns/t%03d {…}` then reply `ACK,TURN,n` |
| `END,id,n,r` | `update /games/id/meta {ended_at, end_reason, total_turns}` |
| `HELLO,STM,v` | `update /device/status/fw {stm: v}` |

All writes except `STATE` go through the **outbox** (queued when offline, flushed in order when back).
`STATE` is not queued — only the latest matters. ACK the `TURN` once it's in the outbox, not after upload.

| DB event (ESP32 stream) | UART line |
| --- | --- |
| `/device/config` changed (incl. initial snapshot) | `CFG,<players>,<limit_s>` (STM32 rejects with BUSY mid-game — that's fine) |
| `/device/command` appears | `CMD,<cmd>`, then `remove /device/command` |

## 3. Why the keys look like this (gotchas)

- **`t001` not `1`:** RTDB converts objects with mostly-integer keys into JS arrays (with holes as
  `undefined`). Prefixing avoids it and keeps lexical order = numeric order.
- **`names` keyed `"1"`…`"6"` as strings** in the rules; the web should handle both object and array
  shapes defensively anyway.
- **Turn key = `turn_no`, not `push()`:** retransmitted turns overwrite themselves instead of duplicating.
- **Commands are deleted after forwarding:** otherwise a rebooting ESP32 replays the last command.

## 4. Web rules of thumb

- **Online indicator:** device is offline if `Date.now() - status.updated_at > 5000`. Use
  `.info/serverTimeOffset` to correct the phone's clock: `serverNow = Date.now() + offset`.
- **Smooth countdown:** on each status update store `(remaining_ms, receivedAt = performance.now())`
  and render `remaining_ms - (performance.now() - receivedAt)` while `state == "RUN"`.
- **LED ring colour** must use the same thresholds as the box: > 30 % green, 10–30 % yellow,
  < 10 % red, < 0 red blinking, PAUSE blue.
- **Stats** (from `/games/{id}/turns`): per player mean / median / max `duration_ms`, overtime rate
  = turns with `overtime_ms > 0` ÷ turns, total overtime, slowest single turn. Titles:
  "นักคิดแห่งโต๊ะ" (highest mean), "สายฟ้าแลบ" (lowest mean).
- Show games without `ended_at` and older than 1 h as "unfinished".

## 5. Auth

Dev phase: both ESP32 and web use **Anonymous Auth** from day one (so we never have to "add security later").
The rules require `auth != null` and validate shapes. Before the demo, optionally restrict writes to
`/games` to the ESP32's UID.

Secrets (`API_KEY`, `DATABASE_URL`, WiFi passwords) live in `esp32/secrets.h` and `web/.env.local`,
both git-ignored. Templates: `esp32/secrets.example.h`, `web/.env.example`.
