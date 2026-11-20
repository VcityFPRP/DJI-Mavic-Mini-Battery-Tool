[Polski](README.md) | [English](README_EN.md)

# DJI Mavic Mini 1 — Battery Service & Programmer Tool

Standalone Windows GUI application (`.exe`) and **Arduino Nano (ATmega328P)** firmware for full diagnostics, repair, unlocking, cycle reset, and EEPROM (Data Flash) editing of **DJI Mavic Mini 1 (`MB2-2400mAh-7.2V`, TI `bq9003` / `BQ40z307`)** smart batteries over SMBus (`0x0B`).

---

## Key Features

* **All-in-One Portable [DJI_MavicMini_Battery_Tool.exe](DJI_MavicMini_Battery_Tool.exe):**
  * **Automatic CH340/CH341 Driver Check & Installer:** detects missing USB-UART drivers on Windows, downloads the official driver package, and installs it automatically.
  * **Built-in Arduino Nano Firmware Flasher:** bundles `avrdude` and the compiled `.hex` inside the `.exe` — flash a blank Arduino Nano in one click (supports `Auto 115200 -> 57600` for both new and old bootloaders).
  * **Built-in Wiring Diagram:** visual pinout schematic in the *Flashing & Schematics* tab.
* **Live Diagnostics & Battery Health (SOH %):**
  * Clear State of Health (`FCC / DesignCapacity`) indicator with thresholds:
    * `>= 90%` — **Excellent**
    * `80% – 89%` — **Good**
    * `70% – 79%` — **Poor (< 80%)**
    * `< 70%` — **Very Poor / Critical (< 70%)**
  * Individual cell voltage bars (`Cell 1` / `Cell 2`), **Delta (`mV`)**, temperature, current, `RSOC / ASOC`, and decoded `DSG_FET`, `CHG_FET`, `PF`, `XDSG`, `XCHG`, `FUSE`, `SEC` flags.
* **Full Unlock & Data Flash Editor (`0x4000–0x5FFF`):**
  * Uses verified DJI keys: **Unseal (`0xCCDF7EE0`)** and **Full Access (`0xE0BCBF17`)**.
  * Clears all 3 lock layers: standard `PF (0x0029)`, proprietary `DJI PF2 (0x4062)`, and `Error 30074 (0x4A60)` latch.
  * Edit parameters directly: **Cycle Count (`0x4340`)**, **DJI S/N (`0x494B` + `KLV0`)**, **Design Capacity (`0x4726`)**, **Design Energy (`0x4728`)**, **Design Voltage (`0x472A`)**, **Manufacturer (`0x4070`)**, **Device Name (`0x4085`)**, **Chemistry (`0x409A`)**.
  * Full EEPROM backup (`0x4000–0x5FFF`) to `.bin` & `.txt`, plus a raw HEX/ASCII editor.

---

## Wiring Diagram (Arduino Nano ↔ 6-Pin 2.0 mm Battery Connector)

| Mavic Mini 1 Pin | Signal | Arduino Nano Pin | Notes |
| :--- | :--- | :--- | :--- |
| **Pin 1** | `SCL` (SMBC) | **`A5`** | Works directly on Arduino internal pull-ups (optional `4.7 kΩ` to `3.3 V`) |
| **Pin 2** | `GND` (`PACK-`) | **`GND`** | Common ground with wake-up PSU |
| **Pin 3** | `B+` (`PACK+`) | **DO NOT CONNECT!** | Only for `8.0–8.4 V` ($\le 0.5\text{ A}$) wake-up PSU |
| **Pin 4** | `B+` (`PACK+`) | **DO NOT CONNECT!** | Bridged internally with Pin 3 |
| **Pin 5** | `GND` (`PACK-`) | **`GND`** | Bridged internally with Pin 2 |
| **Pin 6** | `SDA` (SMBD) | **`A4`** | Works directly on Arduino internal pull-ups (optional `4.7 kΩ` to `3.3 V`) |

> [!NOTE]
> **No external pull-up resistors needed:** With the I²C clock set to `50 kHz` and `INPUT_PULLUP` enabled on `A4`/`A5`, communication works reliably using **Arduino Nano's internal pull-ups alone** (just 3 wires: `A5`, `A4`, `GND`). External `4.7 kΩ` pull-ups to `3.3 V` are only needed as a fallback for long wires or noisy setups.

> [!CAUTION]
> **Never connect `B+` (Pin 3 / Pin 4) to the Arduino!** To wake up a sleeping BMS after cell replacement, connect a current-limited lab PSU set to **8.0–8.4 V ($\le 0.5\text{ A}$)** across `B+` and `GND` sharing a common ground with the Arduino.

---

## Quick Start

1. Download **`DJI_MavicMini_Battery_Tool.exe`** from the repository or the **Releases** page.
2. Plug in an **Arduino Nano** via USB. If CH340 drivers are missing, the app will prompt to download and install them automatically.
3. Go to the **Wgrywanie kodu & Schematy** (*Flashing & Schematics*) tab, select the `COM` port, and click **Wgraj wbudowany firmware na Arduino Nano** to flash the board.
4. Connect the battery as shown in the diagram, click **Połącz** (*Connect*), and read/edit your battery.

---

## Repository Structure

* `DJI_MavicMini_Battery_Tool.exe` — portable Windows x64 executable
* `src/app/dji_battery_tool.py` — GUI application source (Python + Tkinter + PySerial)
* `src/firmware/dji_battery_inspector/dji_battery_inspector.ino` — Arduino Nano SMBus (`0x0B`) firmware source
* `src/firmware/dji_battery_inspector.ino.hex` — pre-compiled ATmega328P hex image

### Building `.exe` from Source

```bash
pip install -r requirements.txt
pyinstaller --noconfirm --onefile --windowed --name "DJI_MavicMini_Battery_Tool" ^
  --add-data "path/to/avrdude.exe;fw" ^
  --add-data "path/to/avrdude.conf;fw" ^
  --add-data "src/firmware/dji_battery_inspector.ino.hex;fw" ^
  src/app/dji_battery_tool.py
```

---

## Tags / Keywords

`dji` `mavic-mini` `mavic-mini-1` `wm160` `mb2-2400mah` `ba01wm160` `bq9003` `bq40z307` `bq40z50` `arduino-nano` `smbus` `i2c` `bms-reset` `dji-battery-unlock` `clear-pf` `clear-pf2` `error-30074` `battery-cell-error` `cycle-count-reset` `eeprom-dump` `data-flash` `ch340` `recelling`
