# Compatibility

Only combinations actually exercised should be marked verified.

| Host | Application | Connection | Status | Notes |
|---|---|---|---|---|
| Windows 10 Pro 22H2, build 19045 | PowerPoint 16.0, build 14332 | Native USB HID | Pass | Full command and 200-switch run on 2026-07-18 |
| Windows 11 | Microsoft PowerPoint | Native USB HID | Not tested | Expected, not claimed |
| Any | Google Slides | Browser | Not tested | Outside v0.1.0 guarantee |
| Any | LibreOffice Impress | Native USB HID | Not tested | Outside v0.1.0 guarantee |
| Any | PDF viewer | Native USB HID | Not tested | Viewer shortcuts vary |

## Development-board validation

| Item | Result |
|---|---|
| Board MCU | ESP32-S3 QFN56 revision 0.2 |
| USB-UART | CH343 on COM10 |
| ESP-IDF | 6.0.2 |
| Production build and verified flash | Pass (2026-07-18) |
| UART boot, status, validation and help | Pass |
| On-device Unity suite | Pass: 8 tests, 0 failures, 0 ignored (2026-07-18) |
| Native USB mount | Pass: `HID\\VID_303A&PID_4004`, HID keyboard started |
| PowerPoint end-to-end | Pass: all commands and 200 alternating switches |
| Five resets with PowerPoint open | Pass: no spontaneous movement |
| Reset with queued commands | Pass: no stale replay after reconnect |
| Physical native-USB cable removal with a full queue | Pending physical operator step |
| Windows sleep/resume | Pending physical operator step |

The Unity result above is from a manual run on the ESP32-S3 over COM10. CI only
compiles the test firmware; it cannot execute these hardware tests.

The PowerPoint run created a 12-slide presentation and observed every command
through the PowerPoint object model while input travelled through the real
native USB HID device. Reset testing confirms detach/re-enumeration and stale
queue handling, but is not reported as a physical cable-removal test.
