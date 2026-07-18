# Compatibility

Only combinations actually exercised are marked verified.

| Host | Application | Connection | Status | Notes |
|---|---|---|---|---|
| Windows 10 Pro 22H2, build 19045 | PowerPoint 16.0, build 14332 | Native USB HID + SoftAP web API | Pass | Full command and 200-switch run on 2026-07-18 |
| Windows 11 | Microsoft PowerPoint | Native USB HID | Not tested | No claim |
| Any | Google Slides | Browser | Not tested | Default profile supplied, application validation pending |
| Any | LibreOffice Impress | Native USB HID | Not tested | Default profile supplied, application validation pending |
| Any | PDF viewer | Native USB HID | Not tested | Viewer shortcuts vary |

## Development-board validation

| Item | Result |
|---|---|
| Board MCU | ESP32-S3 QFN56 revision 0.2 |
| USB-UART | CH343 on COM10 |
| ESP-IDF | 6.0.2 |
| Production v0.2.0 build and verified flash | Pass (2026-07-18) |
| UART boot and version metadata | Pass |
| On-device Unity suite | Pass: 11 tests, 0 failures, 0 ignored |
| Native USB enumeration | Pass: `HID\\VID_303A&PID_4004`, HID keyboard started |
| Web assets and authenticated API | Pass: setup, login, six profiles, edit/test/reset/activate |
| NVS profile persistence across reset | Pass |
| PowerPoint end-to-end command set | Pass |
| 200 alternating slide commands | Pass: final position matched start |
| Five resets while PowerPoint was open | Pass: no spontaneous movement |
| Reset with queued commands | Pass: no stale replay after reconnect |
| Physical native-USB cable removal with a full queue | Pending physical operator step |
| Windows sleep/resume | Pending physical operator step |

The Unity suite was executed manually on the ESP32-S3 over COM10. GitHub
Actions only compiles test firmware because the runner has no attached board.

The PowerPoint automation created a 12-slide presentation, observed each slide
position/state through the PowerPoint object model, and sent every command
through the ESP32 HTTP API and real native USB HID path. Reset testing used the
board reset line; it confirms re-enumeration and stale-queue handling but is not
reported as a physical cable-removal test.
