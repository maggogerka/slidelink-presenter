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
| Production build and verified flash | Pass |
| UART boot, status, validation and help | Pass |
| On-device Unity suite | 8 passed, 0 failed |
| Native USB mount | Pending second/native USB cable |
| PowerPoint end-to-end | Pending native USB mount |
