# Compatibility

Only combinations actually exercised are marked verified.

| Host | Application | Connection | Status | Notes |
|---|---|---|---|---|
| Windows 10 Pro 22H2, build 19045 | PowerPoint 16.0, build 14332 | Native USB HID + SoftAP web API | Pass | Full command and 200-switch run on 2026-07-18 |
| Windows 11 | Microsoft PowerPoint | Native USB HID | Not tested | No claim |
| Any | Google Slides | Browser | Not tested | Default profile supplied, application validation pending |
| Any | LibreOffice Impress | Native USB HID | Not tested | LibreOffice was not installed on the Windows 10 validation host; default profile supplied |
| Any | PDF viewer | Native USB HID | Not tested | Viewer shortcuts vary |

The phone session shown in the README GIF confirms the responsive web UI in a
real mobile browser. Its exact phone OS and browser builds were not recorded,
so it is deliberately not listed as a verified environment above.

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
| Physical native-USB cable removal with a full queue | Pass: depth 8, detach session 1→2, remount session 3, queue remained empty |
| Windows sleep/resume | Pass: Windows event IDs 42/1 and post-resume HID advanced PowerPoint |

The Unity suite was executed manually on the ESP32-S3 over COM10. GitHub
Actions only compiles test firmware because the runner has no attached board.

The PowerPoint automation created a 12-slide presentation, observed each slide
position/state through the PowerPoint object model, and sent every command
through the ESP32 HTTP API and real native USB HID path. The physical cable
test filled all eight queue slots, observed TinyUSB detach/remount and verified
an empty queue five seconds after reconnection. Windows recorded sleep at
06:42:14 and wake at 06:42:32; the first post-resume HID command advanced
PowerPoint from slide 1 to slide 2.
