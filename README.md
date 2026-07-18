# SlideLink — ESP32-S3 Remote Presenter

SlideLink is a driverless USB HID presentation controller built with ESP32-S3
and ESP-IDF. It appears to a computer as a standard USB keyboard and accepts
only a strict allowlist of presentation commands.

Current milestone: **v0.1.0 — USB HID Presenter Core**.

## Features

- Native ESP32-S3 USB HID keyboard; no Windows driver is required
- PowerPoint controls over a strict UART command interface
- FreeRTOS queue with eight pending-command slots
- Guaranteed key press and release reports with bounded endpoint waits
- Protection against stale commands after USB disconnect/reconnect
- Short BOOT-button press for `next`; press for at least one second for `previous`
- Runtime USB state, queue and command statistics
- Unique `SL-XXXXXXXX` serial derived from part of the chip identifier
- No Wi-Fi, web server, OTA or arbitrary keyboard input in v0.1.0

## Hardware connection

The two USB roles are deliberately separate:

```text
ESP32-S3 USB-UART connector (CH343/CP210x) ──> flashing + 115200-baud console
ESP32-S3 native USB/OTG connector            ──> Windows HID keyboard
                                                 GPIO19 = D-
                                                 GPIO20 = D+
```

Use the connector labelled `USB`, `OTG`, or `Native USB` for HID. A connector
that appears as CH343/CP210x is only the UART bridge and cannot carry the HID
interface. Both cables may be connected at the same time.

## Build and flash

Prerequisites: ESP-IDF 6.0.2 and an ESP32-S3 board with native USB exposed.

```powershell
idf.py --version
idf.py set-target esp32s3
idf.py build
idf.py -p COM10 flash monitor
```

The component manager resolves the pinned dependency graph in
`dependencies.lock`. The current production image occupies about 213 KiB.

## Console

Open the USB-UART port at 115200 baud. Commands are case-insensitive, empty
lines are ignored, and input is limited to 64 characters.

| Command | PowerPoint action | HID input |
|---|---|---|
| `next` | Next slide | Right Arrow |
| `previous` | Previous slide | Left Arrow |
| `start` | Start from first slide | F5 |
| `start-current` | Start from current slide | Shift+F5 |
| `stop` | End slideshow | Escape |
| `black` | Toggle black screen | B |
| `white` | Toggle white screen | W |
| `first` | First slide | Home |
| `last` | Last slide | End |
| `goto 12` | Go to slide 12 | `1`, `2`, Enter |
| `status` | Print device state | none |
| `help` | Print command list | none |

`goto` accepts only decimal slide numbers from 1 through 9999. Unsupported
input is never interpreted as raw text or HID keycodes.

Example:

```text
> next
OK id=1 queued

> goto 0
ERR invalid slide number; expected 1..9999

> status
firmware: 0.1.0
usb: mounted
hid: ready
queue_depth: 0
```

Commands submitted while native USB is disconnected are rejected immediately
with `ERR usb not mounted`; they are not replayed after reconnection.

## Tests

The dedicated test application replaces USB with a safe test double, so parser,
mapping and queue tests cannot type into the host:

```powershell
idf.py -C tests -B build-tests set-target esp32s3
idf.py -C tests -B build-tests build
idf.py -C tests -B build-tests -p COM10 flash monitor
```

After testing, restore production firmware with `idf.py -p COM10 flash` from
the repository root. See [docs/usb-testing.md](docs/usb-testing.md) for the
full Windows and PowerPoint checklist.

## USB identity and VID/PID notice

- Manufacturer: `Maggogerka`
- Product: `SlideLink USB Presenter`
- Device class: HID keyboard
- Development VID:PID: Espressif default `303A:4004`

The Espressif development VID/PID is used for this non-commercial prototype.
A product distributed commercially needs an appropriately assigned USB VID/PID;
do not invent or copy another vendor's identifier.

## Safety

SlideLink is intended only to control presentations on its owner's computer.
It accepts no arbitrary text, scripts, operating-system commands, or raw HID
reports. It sends no HID input at boot and persists no command queue.

## Documentation

- [Architecture](docs/architecture.md)
- [USB and PowerPoint test plan](docs/usb-testing.md)
- [Compatibility matrix](docs/compatibility.md)

## Roadmap

- v0.1.0: USB HID core
- v0.2.0: Local Wi-Fi web remote
- v0.3.0: Pairing, profiles and configuration
- v0.4.0: CYD hardware remote
- v0.5.0: Optional Windows companion application

Licensed under the [MIT License](LICENSE).
