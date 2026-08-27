# Compatibility matrix

Only an end-to-end run on the exact v1 composite firmware is marked Pass.
Compiled profiles are not treated as application validation.

| Host / client | Scenario | Product v1 status | Notes |
|---|---|---|---|
| Windows 11 25H2, build 26200.9168 | Firmware boot, SoftAP and HTTP/NCM backend initialization | Pass, 2026-08-27 | Clean boot to `ready` on the connected ESP32-S3; NCM DHCP starts at `192.168.55.1` with no gateway/DNS offers |
| Windows 11 25H2, build 26200.9168 | HID + USB NCM host enumeration | Blocked by physical connection | Only the CH343 USB-UART connector was attached; connect the separate native USB/OTG data port |
| Windows 11 Chrome | Web UI over `192.168.55.1` | Pending hardware/browser run | RU/EN static validation passes |
| Windows 11 Edge 151.0.4129.107 | Static UI boot and RU rendering | Pass, 2026-08-27 | Headless browser loaded HTML, CSS, JS and manifest; device API over NCM remains pending |
| Microsoft 365 PowerPoint | Full command set | Pending manual run | PowerPoint profile included |
| Google Slides / Chrome | Full command set | Pending manual run | Google Slides profile included |
| Google Slides / Edge | Full command set | Pending manual run | Google Slides profile included |
| LibreOffice Impress | Full command set | Not run; application absent | LibreOffice profile included |
| Chrome/Edge PDF viewer | Presentation/navigation | Pending manual run | Generic PDF profile included; viewer shortcuts vary |
| Android Chrome | Responsive web remote | Pending real-device run | Mobile-first layout implemented |
| iPhone Safari | Responsive web remote | Pending real-device run | Safe-area layout implemented |

## Development hardware observed for v1

| Item | Observed |
|---|---|
| MCU | ESP32-S3 QFN56 rev 0.2 |
| Flash | 16 MiB |
| Embedded PSRAM | 8 MiB |
| Flash/UART port | COM3 |
| ESP-IDF | 6.0.2 |
| Development USB identity | `303A:4005`, composite HID + NCM |
| UART bridge | CH343, `USB VID_1A86:PID_55D3`, COM3 |

The on-device Unity suite passed 11/11 tests on this board, including a bounded
queue-overflow case followed by 500 alternating Next/Previous executions with
zero command failures. This proves the command path and HID report generation;
it does not replace host-side enumeration and presentation-application tests.

The board model/schematic was not present in the repository and cannot be
derived from the chip ID. GPIO0 is therefore retained as the documented BOOT
default; RST/EN is treated only as hardware reset. Record the exact board and
native-USB connector before turning this into a production PCB definition.

Historical v0.2 HID-only tests on Windows 10 do not prove the v1 NCM composite
path and are intentionally not reported as v1 passes.

Update this file with OS/browser/application build numbers, date, cables, and
objective results after every release qualification run.
