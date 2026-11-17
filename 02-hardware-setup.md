# ส่วนที่ 2 — การต่อบอร์ดและการตั้งค่า

> Board Game Turn Timer · [1. ภาพรวม](01-overview.md) · [2. การต่อบอร์ดและการตั้งค่า](02-hardware-setup.md) · [3. การแบ่งงานและแผน](03-team-plan.md)


## 2.1 รายการอุปกรณ์ (BOM)

- [ ] NUCLEO-F411RE ×1 + สาย USB
- [ ] ESP32-S3 NodeMCU ×1 + สาย USB
- [ ] KY-040 ×1, KY-004 ×1, KY-016 ×1 จากชีตของวิชา
- [ ] ตัวต้านทาน 220Ω ×3, breadboard, สายจัมเปอร์
- [ ] กล่องสำหรับใส่เครื่อง (ตัดจากกล่องกระดาษแข็ง/อะคริลิก หรือพิมพ์ 3D)
- [ ] USB-to-Serial (CP2102/CH340, 3.3V) ×1 ให้ EMB กับ NET ทดสอบ UART แยกกันได้
- [ ] Powerbank 2 ช่อง (≥ 2A รวม) สำหรับวันเดโม

## 2.2 การต่อขา (NUCLEO-F411RE)

ทุกโมดูลใช้ไฟ 3.3V จากขา 3V3 ของ Nucleo และต้องต่อ GND ร่วมกันทั้งหมด รวมถึง GND ของ ESP32-S3

| อุปกรณ์ | ขาโมดูล | ขา STM32 | ขาบน Nucleo (Arduino header) | ฟังก์ชัน |
| --- | --- | --- | --- | --- |
| KY-040 Rotary encoder | CLK | PA0 | A0 | TIM2\_CH1 (Encoder mode) |
| KY-040 Rotary encoder | DT | PA1 | A1 | TIM2\_CH2 (Encoder mode) |
| KY-040 Rotary encoder | SW | PB4 | D5 | GPIO EXTI4, pull-up, falling edge |
| KY-040 Rotary encoder | + / GND | — | 3V3 / GND | ไฟเลี้ยง |
| KY-004 Key switch | S | PB5 | D4 | GPIO EXTI9\_5, pull-up, both edges (จับกดค้าง) |
| KY-004 Key switch | ขากลาง (+) / − | — | 3V3 / GND | ไฟเลี้ยง |
| KY-016 RGB LED | R | PA6 | D12 | TIM3\_CH1 PWM (ผ่าน 220Ω) |
| KY-016 RGB LED | G | PA7 | D11 | TIM3\_CH2 PWM (ผ่าน 220Ω) |
| KY-016 RGB LED | B | PB0 | A3 | TIM3\_CH3 PWM (ผ่าน 220Ω) |
| KY-016 RGB LED | − | — | GND | ขารวม (common cathode) |
| ESP32-S3 | RX (GPIO18) | PA9 | D8 | USART1\_TX, 115200 8N1 |
| ESP32-S3 | TX (GPIO17) | PA10 | D2 | USART1\_RX |
| ESP32-S3 | GND | — | GND | ต้องต่อ ไม่งั้น UART อ่านเป็นขยะ |

แถวขาไฟใส่ตามลายขาทั่วไปของโมดูลตระกูล KY แต่ละล็อตอาจต่างกัน ให้ดูสกรีนบนโมดูลจริงก่อนต่อ

**ขาที่เว้นว่างไว้:** PB6 (D10, มี TIM4\_CH1 PWM) และ PA4 (A2, ADC) สำหรับโมดูลที่สี่ ถ้า TA ไม่นับ encoder + ปุ่มเป็น 2 ประเภท

**ขั้นตอนต่อ**

1. **รางไฟ:** ขา 3V3 ของ Nucleo → ราง + ของ breadboard, ขา GND → ราง −
2. **KY-040:** CLK → A0, DT → A1, SW → D5, + → ราง 3V3, GND → ราง GND
3. **KY-004:** S → D4, ขากลาง → ราง 3V3, − → ราง GND (วัดก่อนว่ากดแล้ว S ต่อกับ − หรือไม่ = active-low)
4. **KY-016:** R/G/B ผ่านตัวต้านทาน 220Ω ไปที่ D12 / D11 / A3, ขา − → ราง GND (ถ้าเป็นรุ่น common anode ให้ต่อขารวมเข้า 3V3 และกลับค่า duty ในโค้ดเป็น 999 − ค่าเดิม)
5. **ESP32-S3:** GPIO18 → D8, GPIO17 → D2 (TX↔RX ไขว้), GND → GND ของ Nucleo

**ข้อควรระวัง**

- **ห้ามใช้ PA2/PA3 (D1/D0)** เพราะเป็น USART2 ที่ต่อกับ ST-Link (เก็บไว้ใช้ `printf` debug ผ่าน USB)
- **PA5 (D13) คือ LED บนบอร์ด (LD2)** ใช้เป็นไฟบอกสถานะ debug ได้ ไม่ต้องต่ออะไรเข้าขานี้
- **KY-016** ส่วนใหญ่เป็น common cathode และบางรุ่นไม่มีตัวต้านทานบนบอร์ด ให้ใส่ 220Ω ทุกขาสี
- **KY-040** มี pull-up 10kΩ บนโมดูลที่ CLK/DT แล้ว ป้อนไฟ 3.3V (ไม่ใช่ 5V) เพื่อไม่ให้สัญญาณเกินแรงดันขา
- **KY-004** ตรวจขาด้วยมัลติมิเตอร์ก่อน แต่ละล็อตต่อตัวต้านทานต่างกัน แล้วตั้งให้เป็นแบบ active-low พร้อม pull-up ภายใน
- **TX ต่อ RX ไขว้กัน** ระหว่าง STM32 กับ ESP32 ทั้งคู่เป็น logic 3.3V ต่อตรงได้ไม่ต้องแปลงระดับ
- **ไฟเลี้ยง:** ช่วงพัฒนาใช้ USB แยกสองเส้น ตอนเดโมใช้ powerbank 2 ช่อง จ่าย Nucleo และ ESP32-S3 คนละช่อง (ไม่จ่าย ESP32 จากขา 5V ของ Nucleo เพราะ ESP32-S3 กินไฟพีค 350–500 mA ตอนส่ง WiFi อาจรีเซ็ตเอง ดู [architecture D8](docs/architecture.md#7-key-decisions-adr-lite)) แต่ GND ยังต้องต่อร่วมกัน

**เช็กก่อนเสียบ USB**

- [ ] วัดความต้านทานระหว่างราง 3V3 กับ GND ต้องไม่เป็น 0Ω (ไม่ช็อต)
- [ ] ไม่มีสายไหนต่อเข้าขา 5V ของ Nucleo
- [ ] ทดสอบทีละโมดูล: encoder (ดูค่าผ่าน `printf`) → ปุ่ม → LED

## 2.3 Firmware บน STM32

เขียนด้วย STM32CubeIDE + HAL โครงหลักคือ state machine ใน `main` loop ที่ไม่บล็อก ส่วน interrupt ทำแค่ตั้ง flag

**การตั้งค่าใน CubeMX**

| Peripheral | ตั้งค่า | ใช้ทำอะไร |
| --- | --- | --- |
| RCC / Clock | HCLK 100 MHz (APB1 50 MHz, timer clock 100 MHz) | ความเร็วสูงสุดของ F411 |
| SYS | Debug: Serial Wire | อัปโหลด/ดีบักผ่าน ST-Link |
| TIM2 | Combined Channels: Encoder Mode TI1 and TI2, ARR = 0xFFFFFFFF, Input filter = 10 | อ่าน KY-040 ด้วยฮาร์ดแวร์ ไม่พลาดสเต็ป |
| TIM3 | CH1–CH3 PWM Generation, PSC = 99, ARR = 999 (1 kHz, duty 0–999) | ผสมสี RGB |
| USART1 | Asynchronous 115200 8N1, เปิด global interrupt (+ DMA RX ถ้าทำ) | คุยกับ ESP32 |
| USART2 | Asynchronous 115200 (ค่าเดิมของ Nucleo) | `printf` debug ผ่าน USB |
| GPIO PB4, PB5 | GPIO\_EXTI, Pull-up, PB4 falling / PB5 rising+falling, เปิด NVIC EXTI4, EXTI9\_5 | ปุ่ม encoder และ KY-004 |

**โมดูลโค้ด (แยกไฟล์ให้แบ่งงานง่าย)**

| ไฟล์ | หน้าที่ |
| --- | --- |
| `game.c/.h` | state machine, ข้อมูลผู้เล่น, คำนวณเวลาที่เหลือ/overtime |
| `input.c/.h` | อ่าน encoder (`__HAL_TIM_GET_COUNTER(&htim2)` หาร 4 ต่อ 1 คลิก ตรวจกับโมดูลจริง), debounce 30 ms, แยกกดสั้น/กดค้าง |
| `led.c/.h` | `led_set_rgb(r,g,b)`, กะพริบ, fade โดยคำนวณจาก `HAL_GetTick()` |
| `link.c/.h` | ส่ง/รับข้อความ UART กับ ESP32 โดยใช้ parser กลาง [`common/proto.c`](common/proto.c) (ใช้ร่วมกับ ESP32) |

**โครง main loop**

```c
while (1) {
    uint32_t now = HAL_GetTick();
    input_poll(now);            // แปลง flag จาก EXTI เป็น event: BTN_SHORT, BTN_LONG, ENC_PRESS, ENC_DELTA
    link_poll();                // อ่านบรรทัดคำสั่งจาก ESP32 (CFG, CMD)
    game_update(now);           // state machine ตัดสินใจจาก event
    led_update(now, game_view());
    if (now - last_state_tx >= 1000) { link_send_state(); last_state_tx = now; }
}
```

**จุดที่ต้องระวัง**

- ใน callback `HAL_GPIO_EXTI_Callback` ห้ามเรียก `HAL_Delay` หรือ `printf` ให้บันทึกเวลาแล้วตั้ง flag อย่างเดียว
- นับเวลาด้วย `HAL_GetTick()` แบบลบกัน (`now - start`) จะไม่มีปัญหาตอนตัวนับวนรอบ
- เวลาที่หยุดชั่วคราวต้องไม่ถูกนับรวม: เก็บ `paused_total` แล้วหักออก
- ส่ง UART แบบ `HAL_UART_Transmit_IT` หรือ DMA เพื่อไม่ให้ไฟกะพริบสะดุด

## 2.4 โปรโตคอล STM32 ↔ ESP32

ใช้ข้อความ ASCII ทีละบรรทัด คั่นด้วย `,` จบด้วย `\n` ที่ 115200 baud อ่านด้วยตาได้ใน Serial Monitor จึง debug ง่าย และไม่ต้องใช้ไลบรารี JSON บน STM32

**สเปกฉบับล็อก: [`docs/protocol.md`](docs/protocol.md) (v1.0)** ข้อความทั้งหมด ช่วงค่า และกติกาความทนทานอยู่ที่นั่นที่เดียว โค้ด parse/format อยู่ที่ [`common/proto.c`](common/proto.c) ใช้ร่วมกันทั้ง STM32 และ ESP32 พร้อมเทสต์ (`make -C common test`)

ตัวอย่างหน้าตา:

```
STM> START,9f03c2aa,4,20
STM> STATE,9f03c2aa,RUN,4,20,1,1,13500      (game_id, state, players, limit_s, turn_no, player, remaining_ms)
STM> TURN,9f03c2aa,1,1,17250,0              (game_id, turn_no, player, duration_ms, overtime_ms)
ESP> ACK,TURN,1
ESP> CMD,PAUSE
```

ระหว่างพัฒนา ใช้ [`tools/fake_device.py`](tools/fake_device.py) จำลองเครื่องแทน STM32 ได้ ทำให้ทีม ESP32 และเว็บเริ่มงานได้ก่อนฮาร์ดแวร์เสร็จ

## 2.5 ESP32-S3 และคลาวด์

ESP32-S3 เป็นสะพานอย่างเดียว: อ่านบรรทัดจาก STM32 → แปลงเป็น JSON → เขียนขึ้นคลาวด์ และฟังคำสั่งจากคลาวด์ส่งกลับไป STM32 ส่วนตรรกะของเกมอยู่ที่ STM32 ทั้งหมด

**เลือกคลาวด์:** แนะนำ **Firebase Realtime Database** เพราะทำเว็บเองได้อิสระ ฟังข้อมูลแบบ real-time ได้ทั้งจาก ESP32 และเว็บ มี Hosting ฟรีสำหรับขึ้นเว็บ ถ้าทีมอยากใช้ NETPIE ตามที่วิชาแนะนำ ใช้ MQTT (PubSubClient) กับ Shadow สำหรับสถานะ + Feed สำหรับประวัติแทนได้ โครงข้อมูลเหมือนกัน

**การตั้งค่า ESP32-S3 (Arduino IDE หรือ PlatformIO)**

- `Serial1.begin(115200, SERIAL_8N1, 18, 17);` (RX = GPIO18, TX = GPIO17) สำหรับคุยกับ STM32
- `Serial` (USB) สำหรับ debug
- เชื่อม WiFi แล้วซิงก์เวลาด้วย NTP (`configTzTime("ICT-7", "pool.ntp.org")`) เพื่อใส่ timestamp ให้ทุกเทิร์น เพราะ STM32 ไม่รู้เวลาจริง
- ไลบรารี Firebase สำหรับ Arduino-ESP32 (เช่น FirebaseClient ของ Mobizt) ใช้ stream ฟัง `/device/config` และ `/device/command`
- เก็บ WiFi SSID/password และ Firebase key ในไฟล์ `secrets.h` ที่อยู่ใน `.gitignore` (repo ต้องเป็น public ห้าม commit กุญแจ)

**โครงข้อมูล การแปลงข้อความ UART ↔ DB และ security rules:** ดู [`docs/cloud.md`](docs/cloud.md) (v1.0) และ [`firebase/database.rules.json`](firebase/database.rules.json) ใช้ Anonymous Auth ตั้งแต่วันแรก

## 2.6 เว็บแดชบอร์ด

**Stack ที่แนะนำ:** Vite + React (หรือ HTML/JS ล้วนถ้าทีมไม่ถนัด React) + Firebase Web SDK + Chart.js สำหรับกราฟ ขึ้นเว็บด้วย Firebase Hosting

**สถิติที่คำนวณบนเว็บ** (จาก `/games/{gameId}/turns`)

- เวลาเฉลี่ย, มัธยฐาน และเทิร์นที่นานที่สุดของแต่ละคน
- อัตราการเกินเวลา = จำนวนเทิร์นที่ `overtime_ms > 0` ÷ จำนวนเทิร์นทั้งหมดของคนนั้น
- ฉายาท้ายเกมเพื่อความสนุกตอนเดโม เช่น "นักคิดแห่งโต๊ะ" (เฉลี่ยนานสุด), "สายฟ้าแลบ" (เร็วสุด)

**สถานะออนไลน์:** ถ้า `/device/status/updated_at` เก่ากว่า 5 วินาที แสดงว่าเครื่องออฟไลน์ (รายละเอียดการแสดงผล เช่น นับถอยหลังให้ลื่น ดู [`docs/cloud.md`](docs/cloud.md) §4)

