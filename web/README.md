# web/ — dashboard (WEB)

Vite + React + Firebase Web SDK + Chart.js, hosted on Firebase Hosting. Contract: [`docs/cloud.md`](../docs/cloud.md).

- `cp .env.example .env.local` and fill in
- Pages: Live (`/device/status`, writes `/device/command`), Setup (writes `/device/config`), Stats (`/games`)
- Fake data while hardware isn't ready: `python3 tools/fake_device.py --firebase <DB_URL> --api-key <KEY> --auto`
