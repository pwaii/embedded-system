# stm32/ — game firmware (EMB)

STM32CubeIDE project for NUCLEO-F411RE. Pins + CubeMX settings: [`02-hardware-setup.md`](../02-hardware-setup.md) §2.2–2.3.
State machine + timing: [`docs/architecture.md`](../docs/architecture.md) §4–5. UART: [`docs/protocol.md`](../docs/protocol.md).

- Add `../common` to *Project → Properties → C/C++ General → Paths and Symbols* (include path + source location) so `proto.c` is built.
- Modules: `game.c` (no HAL — keep it PC-testable), `input.c`, `led.c`, `link.c`.
- Debug output: `printf` over USART2 → ST-Link virtual COM port, 115200.
