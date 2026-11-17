# Board Game Turn Timer — Embedded System Lab Final Project (2026)

A box in the middle of the table: turn the encoder to set the time per turn, press the button to end
your turn, the RGB LED shows how much time is left, and every turn goes to the cloud for a live web
dashboard with per-player stats.

**Hardware:** NUCLEO-F411RE (game logic) + ESP32-S3 (WiFi bridge) · KY-040 encoder · KY-004 button ·
KY-016 RGB LED · Firebase Realtime DB + Hosting

## Docs

| Doc | What's in it |
| --- | --- |
| [1. ภาพรวม](01-overview.md) | idea, behaviour, LED colours, system diagram, web pages |
| [2. การต่อบอร์ดและการตั้งค่า](02-hardware-setup.md) | BOM, pin map, wiring steps, CubeMX, ESP32 setup |
| [3. การแบ่งงานและแผน](03-team-plan.md) | roles, 4-week plan with gates, deliverables, demo script, risks |
| [Architecture](docs/architecture.md) | responsibilities, data flow, state machine, failure handling, design decisions |
| [Protocol v1.0](docs/protocol.md) | STM32 ↔ ESP32 UART messages (contract EMB ↔ NET) |
| [Cloud v1.0](docs/cloud.md) | Firebase data tree and rules (contract NET ↔ WEB) |
| [Test checklist](docs/test-checklist.md) | bench, box-only, end-to-end and demo tests |

## Repository layout

```
01–03-*.md  project docs (Thai)
docs/       architecture + interface contracts, images, report
common/     proto.c/.h shared by both firmwares + PC unit tests
tools/      fake_device.py — simulated box for testing ESP32/web without hardware
firebase/   database.rules.json
stm32/      STM32CubeIDE project (EMB)
esp32/      ESP32-S3 bridge (NET) — secrets.h is git-ignored, see secrets.example.h
web/        Vite + React dashboard (WEB) — .env.local is git-ignored, see .env.example
```

## Quick start

```bash
make -C common test                                       # protocol unit tests
python3 tools/fake_device.py --stdout --auto --speed 5    # watch a simulated game in the terminal
python3 tools/fake_device.py --firebase <DB_URL> --api-key <WEB_API_KEY> --auto   # feed the web dashboard
```

## Team

| Role | Member | Owns |
| --- | --- | --- |
| System Architect & Team Lead (LEAD) | Chikawa (Boeing) | `docs/`, `common/`, `tools/`, wiring, enclosure, integration, report |
| Embedded Developer (EMB) | Usagi (Ice) | `stm32/` |
| Connectivity & Cloud (NET) | Furuhonya (Pinn) | `esp32/`, `firebase/` |
| UI/UX & Web (WEB) | Rakku (Ou) | `web/` |

Workflow: branch per task → Pull Request → review → merge to `main`. Interface changes go through `docs/` first.
