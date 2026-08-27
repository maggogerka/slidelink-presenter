# Build and flash on Windows 11

## Prerequisites

- ESP-IDF 6.0.2 installed through Espressif tools
- Python from that installation
- data-capable USB-UART cable for COM flashing
- a second data-capable native USB/OTG cable if the board has separate ports
- 16 MiB ESP32-S3 flash

The helper scripts default to the installed profile
`C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1`. Override
`-IdfProfile` if ESP-IDF is elsewhere.

## Development

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1 -Configuration dev
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\flash-dev.ps1 -Port COM3
```

Manual equivalent:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { . 'C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1'; idf.py set-target esp32s3; idf.py build; idf.py -p COM3 flash monitor }"
```

Exit the IDF monitor with `Ctrl+]`. The DEV UART log prints the device-specific
setup credential. Normal DEV flash never enables Secure Boot or Flash
Encryption eFuses. `flash-dev.ps1` always regenerates the DEV configuration
before flashing, so a preceding production build cannot leak security settings
into an ordinary development flash.

## Tests

```powershell
python .\tools\validate_frontend.py
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { . 'C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1'; idf.py -C tests -B build-tests set-target esp32s3; idf.py -C tests -B build-tests build; idf.py -C tests -B build-tests -p COM3 flash monitor }"
```

The Unity application runs all tests at boot. After recording its final
summary, exit the monitor and restore the DEV application with `flash-dev.ps1`.

## Production build

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1 -Configuration production
```

This writes `build-product` but does not flash it. Do not use the DEV flash
script with that directory. Follow `docs/production-provisioning.md`.

## OTA file

Distribute `build/slidelink.bin` for DEV or the signed
`build-product/slidelink.bin` for the matching production trust domain. Do not
upload a merged image, bootloader, or partition table through the UI.
