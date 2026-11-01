# Flashing Firmware to Ai-Thinker BW16

This guide covers wiring, bootloader activation, and flashing the BW16 Wi-Fi Repeater firmware to an Ai-Thinker BW16 module or development board.

## Hardware requirements

- Ai-Thinker BW16 module or BW16-TypeC development board (Realtek RTL8720DN).
- USB-to-UART adapter (3.3V logic levels).
- 3.3V DC power source capable of supplying at least 500 mA peak current.

## Pinout and wiring

When connecting a standalone BW16 module to a USB-to-UART adapter:

| BW16 Pin | Function | USB-to-UART Pin | Note |
| :--- | :--- | :--- | :--- |
| 3V3 | Power Input | 3.3V | Must provide at least 500 mA |
| GND | Ground | GND | Common ground |
| PA7 | UART2 TX (Log) | RXD | Default log and bootloader TX |
| PA8 | UART2 RX (Log) | TXD | Default log and bootloader RX |
| PB1 | GPIO / BURN | GND (via button) | Pull to GND to enter bootloader mode |
| CHIP_EN | RST | GND (via button) | Pull to GND momentarily to reset |

If using a BW16-TypeC development board, connect the USB-C cable directly. The onboard buttons are labelled `BURN` (or `BOOT`) and `RST`.

## Entering bootloader download mode

To allow the flashing tool to write to flash memory, the RTL8720DN must boot into download mode:

1. Press and hold the **BURN** (BOOT / PB1) button down.
2. Press and release the **RST** (CHIP_EN) button.
3. Release the **BURN** button.

The chip is now waiting for a UART synchronization handshake at 115200 baud, which will then negotiate up to 1,500,000 baud for image transfer.

## Flashing with do_flash.py (Recommended)

The repository provides a cross-platform flashing script that automatically detects serial ports and writes the firmware image:

- **Windows**:
  ```cmd
  python do_flash.py
  # Or with explicit COM port:
  python do_flash.py --port COM3
  ```
- **macOS**:
  ```bash
  python3 do_flash.py
  # Or with explicit port:
  python3 do_flash.py --port /dev/cu.usbserial-10
  ```
- **Linux**:
  ```bash
  python3 do_flash.py
  # Or with explicit port:
  python3 do_flash.py --port /dev/ttyUSB0
  ```

The script performs the following steps:
1. Connects to the serial port.
2. Sends the bootloader handshake sequence.
3. Transmits the KM0 and KM4 firmware partitions.
4. Resets the module into operational firmware.

## Flashing with Arduino CLI

Alternatively, you can flash using the AmebaD upload tool via the shell script:

```bash
PORT=/dev/cu.usbserial-10 FQBN=realtek:AmebaD:Ai-Thinker_BW16 bash scripts/flash.sh
```

## Post-flash verification

Open the serial monitor at 115200 baud to view system initialization logs:

```bash
python3 monitor.py
```

Expected startup output:
```text
[DRD] Normal boot (clean state)
[LED] Initialized RGB pins
[Config] Loaded config from flash (or default)
[WiFi] Starting AP: BW16-Repeater on channel 1
[WebUI] Server started on port 80
[DNS] Captive portal running on port 53
```

## Flashing troubleshooting

### No response / Handshake timeout
- Ensure the board is in bootloader mode (hold BURN, click RST, release BURN).
- Verify TX and RX pins are crossed: BW16 PA7 (TX) goes to adapter RX, PA8 (RX) goes to adapter TX.
- Ensure the USB-to-UART adapter is operating at 3.3V logic levels. 5V signals can damage the RTL8720DN.

### Board resets during flash
- Insufficient power: RTL8720DN draws high peak currents during flash write cycles. Use a dedicated 3.3V regulator or an active USB hub if powering from a laptop port.
