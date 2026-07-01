# RTOS-Based Environmental Monitor

An embedded firmware project for the **STM32F4-Discovery** board that reads temperature, humidity, and pressure from a BME280 sensor over I2C and streams the data over UART. Built on **FreeRTOS** (via CMSIS-RTOS2) with a live UART command interface.

---

## Hardware

| Component | Part |
|---|---|
| Microcontroller | STM32F407VGT6 (STM32F4-Discovery) |
| Sensor | BME280 (temperature, humidity, pressure) |
| Communication | USART2 via external USB-to-UART adapter |

---

## Wiring

Connect the BME280 breakout to the Discovery board using the silk screen labels:

| BME280 Breakout Pin | Discovery Board Pin | Purpose |
|---|---|---|
| VIN | 3V | Power (3.3V) |
| GND | GND | Ground |
| SCK | PB6 | I2C1 SCL |
| SDI | PB9 | I2C1 SDA |
| CS | 3V | Pull high — selects I2C mode |
| SDO | GND | Pull low — sets I2C address to 0x76 |
| 3Vo | — | Leave unconnected (it is a regulator output, not an input) |

> PB6 and PB9 are on the bottom expansion connector (P2), labeled directly by port name on the silk screen.

---

## UART Command Interface

The STM32F4-Discovery's ST-Link/V2 does not expose a virtual COM port, so USART2 is wired to an external USB-to-UART adapter (CP2102, CH340, FTDI, etc.):

| Discovery Silk Screen | USB-to-UART Adapter Pin |
|---|---|
| PA2 | RX |
| PA3 | TX |
| GND | GND |

> Connect PA2 (board TX) to the adapter's RX, and PA3 (board RX) to the adapter's TX.

Open the adapter's COM port at **115200 8N1** using PuTTY or Tera Term on Windows, or `screen`/`minicom` on Linux/macOS.

On power-up you will see:

```
=== Environmental Monitor ===
Type START to begin. Type HELP for commands.
```

Commands are **case-insensitive**:

| Command | Description |
|---|---|
| `START` | Begin streaming sensor readings once per second |
| `STOP` | Pause sensor output |
| `STATUS` | Print free heap, active task count, and uptime |
| `CLEAR` | Clear the terminal screen |
| `HELP` | List available commands |

Example output after typing `START`:

```
Starting Sensor Read:
T: 74.2F | H: 60.9% | P: 1007.6hPa
T: 74.2F | H: 60.9% | P: 1007.7hPa
```

---

## BME280 Error Handling

The firmware handles two sensor failure scenarios at runtime:

**Init failure (on boot)**
If the BME280 does not respond during startup — wrong wiring, missing pull-ups, or wrong address — `bme280_init` returns `false`. The firmware prints an error over UART and blinks the red LED (LD5, PD14) at 5 Hz indefinitely. A reset is required after fixing the wiring.

```
ERROR: BME280 not found. Check wiring (PB6=SCL, PB9=SDA) and SDO/CS strapping.
```

**Disconnection during operation**
If the sensor stops responding after a successful init — loose wire, power loss to the breakout — `bme280_read_sensors` returns `false`. The firmware prints an error once and stops output automatically.

```
ERROR: BME280 disconnected. Reconnect and type START.
```

To recover without resetting the board:
1. Reconnect the BME280 wiring
2. Type `START`

The firmware will re-run `bme280_init` before resuming reads, which re-applies the operating mode and re-reads calibration. If the sensor still does not respond it will print an additional error and wait for another `START`.

---

## FreeRTOS Task Overview

| Task | Priority | Stack | Role |
|---|---|---|---|
| `Sensor_Task` | High | 1 KB | Reads BME280 every 1 s, pushes data to queue |
| `UART_Task` | Normal | 1 KB | Dequeues sensor data and transmits over UART |
| `LED_Task` | Low | 512 B | Heartbeat blink on PD12 (green LED) at 1 Hz |
| `defaultTask` | Normal | 512 B | CubeMX default, initialises USB host |

Sensor data flows through an `osMessageQueue` from `Sensor_Task` to `UART_Task`. `UART_Task` blocks on `osMessageQueueGet` with `osWaitForever` and only wakes when data is available. `Sensor_Task` uses `osDelayUntil` for drift-free 1-second periods.

---

## Project Structure

```
rtos-based-enviormental-monitor/
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   └── bme280.h          # BME280 driver header
│   └── Src/
│       ├── main.c            # Tasks, UART parser, application logic
│       └── bme280.c          # BME280 I2C driver
├── Middlewares/
│   └── Third_Party/FreeRTOS/ # FreeRTOS kernel source
├── cmake/
│   ├── gcc-arm-none-eabi.cmake
│   └── stm32cubemx/
├── CMakeLists.txt
└── rtos-based-enviormental-monitor.ioc  # CubeMX project file
```

---

## Building with VS Code + STM32 Extension

This is the recommended setup — it handles the toolchain, CMake, and flashing through a single extension.

### Prerequisites

1. [VS Code](https://code.visualstudio.com/)
2. **STM32 VS Code Extension** — search `stm32-vscode-extension` in the Extensions panel and install it. It will prompt you to install its dependencies automatically:
   - STM32CubeCLT (bundles arm-none-eabi-gcc, CMake, Ninja, and the programmer CLI)
3. Connect the board via the mini-USB ST-Link port

### 1. Clone and open

```bash
git clone https://github.com/your-username/rtos-based-enviormental-monitor.git
```

Open the cloned folder in VS Code (`File → Open Folder`).

### 2. Import the project

- Open the STM32 extension panel from the Activity Bar (the ST logo)
- Click **Import CMake Project**
- Select the repo root — it will detect `CMakeLists.txt` and configure everything automatically

### 3. Build

Click the **Build** button in the STM32 extension panel, or use the CMake extension's build button in the status bar. The output ELF lands at `build/Debug/rtos-based-enviormental-monitor.elf`.

### 4. Flash and debug

Click **Flash** in the STM32 extension panel to program the board over SWD. To start a debug session, press `F5` — breakpoints, watch variables, and the RTOS task viewer all work out of the box.

---

> **Regenerating HAL code:** The `.ioc` file is included so you can open it in STM32CubeMX and re-generate HAL/middleware at any time without losing application code. All user code lives inside `/* USER CODE BEGIN */ / /* USER CODE END */` guards and is preserved on re-generation.

---

## Troubleshooting

**Red LED (LD5) blinking rapidly on startup**
The BME280 did not respond on I2C init. The UART terminal will also print an error message. Check:
- SCK → PB6, SDI → PB9
- CS pulled to 3V, SDO pulled to GND
- VIN connected to 3V (not 5V)

**Terminal prints `ERROR: BME280 disconnected` and stops updating**
The sensor lost contact during operation. Sensor output is automatically paused. Reconnect the wiring and reset the board.