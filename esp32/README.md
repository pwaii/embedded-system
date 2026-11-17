# esp32/ — WiFi bridge (NET)

ESP32-S3 firmware: UART ↔ Firebase bridge. Contract: [`docs/protocol.md`](../docs/protocol.md) and [`docs/cloud.md`](../docs/cloud.md).

- `Serial1.begin(115200, SERIAL_8N1, 18, 17);` (RX=GPIO18, TX=GPIO17)
- Parse lines with `common/proto.c` (copy or symlink `proto.h`/`proto.c` into the sketch or `src/` folder)
- `cp secrets.example.h secrets.h` and fill in
- Test without the STM32: `python3 tools/fake_device.py --serial /dev/ttyUSB0` through a 3.3 V USB-TTL adapter
