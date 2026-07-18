# Compatibility

Only combinations actually exercised should be marked verified.

| Host | Application | Connection | Status | Notes |
|---|---|---|---|---|
| Windows 10 | Microsoft PowerPoint | Native USB HID | Pending manual pass | Primary v0.1.0 target |
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
| Native USB mount | Pending second/native USB cable |
| PowerPoint end-to-end | Pending native USB mount |

The Unity result above is from a manual run on the ESP32-S3 over COM10. CI only
compiles the test firmware; it cannot execute these hardware tests.
