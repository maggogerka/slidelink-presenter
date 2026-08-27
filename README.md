# SlideLink Product v1

SlideLink turns an ESP32-S3 into a driverless presentation controller. One
native USB cable exposes a composite device to Windows 11:

- USB HID keyboard for presentation commands;
- USB NCM network adapter for the embedded web interface;
- isolated address `http://192.168.55.1` with no advertised gateway or DNS;
- `http://slidelink.local` through mDNS when the host resolves it.

The same HTTP/API backend remains available through the WPA2 SoftAP fallback at
`192.168.4.1`. No desktop companion, cloud service, CDN, or Internet connection
is required.

## Product v1 features

- Next, Previous, start from first/current, end, black/white screen, first/last,
  and direct slide number
- responsive mobile-first web remote and elapsed timer
- one RU/EN HTML application; automatic language choice plus persistent
  `RU | EN` switch
- six persistent, editable, strictly allowlisted presentation profiles
- bounded command queue, USB-session invalidation, explicit key release, and
  WebSocket execution results
- BOOT on the development board: short = Next, hold 1 s = Previous, hold 8 s =
  factory reset
- configurable separate Next/Previous GPIO inputs for a production PCB
- unique per-device first-run Wi-Fi credential; no shared setup password
- PBKDF2-HMAC-SHA-256 PIN storage, rate-limited login, random RAM-only sessions
- authenticated local firmware update, two 3 MiB OTA slots, image/product
  validation, and boot rollback
- separate DEV and PRODUCTION security configurations

## Hardware

The firmware requires an ESP32-S3 with the native USB D-/D+ signals exposed
(GPIO19/GPIO20) and 16 MiB flash for the supplied partition table. The board
currently connected during v1 development reports ESP32-S3 QFN56 rev 0.2,
16 MiB flash, and 8 MiB PSRAM.

Many development boards have two connectors:

```text
USB-UART bridge connector -> flashing, COM port, 115200-baud DEV logs
Native USB / OTG connector -> HID keyboard + NCM web interface
```

RST/EN is a hardware reset input and is never treated as a GPIO button. GPIO0
BOOT is used only by the development input configuration.

## Quick start on Windows 11

Prerequisite: ESP-IDF 6.0.2. In PowerShell from this repository:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\flash-dev.ps1 -Port COM3
```

Connect the native USB port, wait for Windows to enumerate the composite
device, then open `http://192.168.55.1`. On first run, choose a private Wi-Fi
password and 4-8 digit PIN. The USB path does not require joining the SoftAP.

For Wi-Fi-only first setup, use the unique credential printed in the DEV UART
log or placed on the product label. The SSID is `SlideLink-XXXX-Setup`. To
capture a printable production label over USB:

```powershell
python -m pip install "qrcode[pil]"
python .\tools\make_setup_label.py --require-qr
```

See [User quick start](docs/user-quick-start.md) for the full user flow.

## Build, test, and release

```powershell
# Development firmware
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1 -Configuration dev

# Compile the device-side Unity suite
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { . 'C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1'; idf.py -C tests -B build-tests set-target esp32s3; idf.py -C tests -B build-tests build }"

# Dependency-free UI/i18n release checks
python .\tools\validate_frontend.py

# Production build only (requires an offline-managed signing key)
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1 -Configuration production
```

Never flash `build-product` with the ordinary DEV command. A production
bootloader can permanently enable Secure Boot, Flash Encryption, secure ROM
download mode, and JTAG lockdown on first boot. Follow
[Production provisioning](docs/production-provisioning.md); its script defaults
to audit/build only and requires two exact manual confirmations before reset.

## USB identity

DEV defaults use Espressif VID `303A` and SlideLink PID `4005`, with a
chip-derived serial number. This identity is for development only. A shipping
product must configure a manufacturer-assigned or sublicensed VID/PID through
`CONFIG_SLIDELINK_USB_VID` and `CONFIG_SLIDELINK_USB_PID`.

## Documentation

- [Architecture](docs/architecture.md)
- [Build and flash](docs/build-flash.md)
- [User quick start](docs/user-quick-start.md)
- [Web API](docs/web-api.md)
- [Security model](docs/security.md)
- [Production provisioning](docs/production-provisioning.md)
- [Windows 11 validation plan](docs/usb-testing.md)
- [Compatibility matrix](docs/compatibility.md)

Licensed under the [MIT License](LICENSE).
