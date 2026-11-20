# DJI Mavic Mini 1 — Battery Service & Programmer Tool

Samodzielna aplikacja dla Windows (`.exe`) oraz firmware dla **Arduino Nano (ATmega328P)** do pełnej diagnostyki, naprawy, odblokowywania i edycji pamięci EEPROM (Data Flash) akumulatorów **DJI Mavic Mini 1 (`MB2-2400mAh-7.2V`, kontroler TI `bq9003` / `BQ40z307`)** po magistrali SMBus (`0x0B`).

---

## Najważniejsze funkcje

* **Wszystko w jednym pliku [DJI_MavicMini_Battery_Tool.exe](DJI_MavicMini_Battery_Tool.exe):**
  * **Automatyczne sprawdzanie i instalacja sterowników CH340/CH341:** wykrywa brak sterownika w systemie Windows i pobiera oraz instaluje go automatycznie.
  * **Wbudowany programator Arduino Nano:** zawiera wkompilowany `avrdude` oraz gotowy wsad `.hex` — wgrywa firmware na czyste Arduino jednym kliknięciem (obsługuje zarówno nowy bootloader `115200`, jak i stary `57600`).
  * **Wbudowany schemat podłączenia:** graficzny diagram w zakładce *Wgrywanie kodu & Schematy*.
* **Diagnostyka na żywo i Kondycja baterii (SOH %):**
  * Czytelny wskaźnik kondycji (`FCC / DesignCapacity`) z oceną:
    * `>= 90%` — **Bardzo dobra kondycja**
    * `80% – 89%` — **Dobra kondycja**
    * `70% – 79%` — **Zła kondycja (< 80%)**
    * `< 70%` — **Bardzo zła kondycja (< 70%)**
  * Paski napięć obu ogniw (`Cell 1` / `Cell 2`), różnica **Delta (`mV`)**, temperatura, prąd, `RSOC / ASOC` oraz statusy `DSG_FET`, `CHG_FET`, `PF`, `XDSG`, `XCHG`, `FUSE`, `SEC`.
* **Pełne odblokowanie i modyfikacja Data Flash (`0x4000–0x5FFF`):**
  * Obsługa kluczy DJI: **Unseal (`0xCCDF7EE0`)** i **Full Access (`0xE0BCBF17`)**.
  * Kasowanie wszystkich 3 warstw blokad: standardowego `PF (0x0029)`, własnościowego `DJI PF2 (0x4062)` oraz zatrzasku błędu `30074 (0x4A60)`.
  * Edycja parametrów: **Liczba cykli (`0x4340`)**, **DJI S/N (`0x494B` + `KLV0`)**, **Design Capacity (`0x4726`)**, **Design Energy (`0x4728`)**, **Design Voltage (`0x472A`)**, **Producent (`0x4070`)**, **Nazwa (`0x4085`)**, **Chemia (`0x409A`)**.
  * Pełny zrzut kopii zapasowej EEPROM (`0x4000–0x5FFF`) do `.bin` i `.txt` oraz edytor HEX/ASCII.

---

## Schemat podłączenia (Arduino Nano ↔ Bateria 6-pin 2,0 mm)

| Pin baterii Mavic Mini 1 | Funkcja | Połączenie z Arduino Nano | Uwagi |
| :--- | :--- | :--- | :--- |
| **Pin 1** | `SCL` (SMBC) | **`A5`** | Opcjonalnie pull-up `4,7 kΩ` do `3,3 V` |
| **Pin 2** | `GND` (`PACK-`) | **`GND`** | Wspólna masa z zasilaczem wybudzającym |
| **Pin 3** | `B+` (`PACK+`) | **NIE PODŁĄCZAĆ!** | Tylko do zasilacza `8,0–8,4 V` ($\le 0,5\text{ A}$) przy wybudzaniu |
| **Pin 4** | `B+` (`PACK+`) | **NIE PODŁĄCZAĆ!** | Zdublowany z Pinem 3 |
| **Pin 5** | `GND` (`PACK-`) | **`GND`** | Zdublowany z Pinem 2 |
| **Pin 6** | `SDA` (SMBD) | **`A4`** | Opcjonalnie pull-up `4,7 kΩ` do `3,3 V` |

> [!CAUTION]
> **Nigdy nie podłączaj pinów `B+` (Pin 3 / Pin 4) do Arduino!** Do wybudzania uśpionego BMS po wymianie ogniw użyj osobnego zasilacza laboratoryjnego ustawionego na **8,0–8,4 V** z ograniczeniem prądu **$\le 0,5\text{ A}$** podłączonego równolegle do `B+` i `GND` ze wspólną masą z Arduino.

---

## Szybki start

1. Pobierz plik **`DJI_MavicMini_Battery_Tool.exe`** z repozytorium lub zakładki **Releases**.
2. Podłącz **Arduino Nano** przez USB. Jeśli brakuje sterowników CH340, program sam zaproponuje ich pobranie i instalację.
3. W zakładce **Wgrywanie kodu & Schematy** wybierz port `COM` i kliknij **Wgraj wbudowany firmware na Arduino Nano** (jeśli Arduino nie ma jeszcze wgranego programu).
4. Podłącz baterię zgodnie ze schematem, kliknij **Połącz** i odczytaj stan baterii.

---

## Struktura repozytorium

* `DJI_MavicMini_Battery_Tool.exe` — gotowa aplikacja portable dla Windows (x64)
* `src/app/dji_battery_tool.py` — kod źródłowy aplikacji GUI (Python + Tkinter + PySerial)
* `src/firmware/dji_battery_inspector/dji_battery_inspector.ino` — kod źródłowy firmware Arduino Nano (`Wire.h` SMBus `0x0B`)
* `src/firmware/dji_battery_inspector.ino.hex` — skompilowany wsad dla ATmega328P

### Budowanie `.exe` ze źródeł

```bash
pip install -r requirements.txt
pyinstaller --noconfirm --onefile --windowed --name "DJI_MavicMini_Battery_Tool" ^
  --add-data "path/to/avrdude.exe;fw" ^
  --add-data "path/to/avrdude.conf;fw" ^
  --add-data "src/firmware/dji_battery_inspector.ino.hex;fw" ^
  src/app/dji_battery_tool.py
```
