# Web API v1

The same server listens on USB NCM and Wi-Fi. JSON endpoints reject unknown
fields, invalid types, and bounded-size violations. Protected requests use
`Authorization: Bearer <64 hex characters>`; tokens live only in RAM and expire
after 30 minutes of inactivity.

| Method | Path | Authentication | Purpose |
|---|---|---|---|
| GET | `/api/v1/system` | no | firmware, HID/NCM/Wi-Fi, heap, security, profile |
| GET | `/api/v1/setup-credential` | first-run only | manufacturing label data |
| POST | `/api/v1/setup` | first-run only | set Wi-Fi password and PIN |
| POST | `/api/v1/session` | PIN | create session token |
| PUT | `/api/v1/settings` | token + current PIN | change Wi-Fi password and/or PIN |
| POST | `/api/v1/factory-reset` | token + current PIN | erase settings and profiles |
| POST | `/api/v1/firmware` | token | stream `application/octet-stream` OTA image |
| POST | `/api/v1/commands` | token | submit allowlisted action |
| GET | `/api/v1/profiles` | token | list profiles |
| PUT | `/api/v1/profiles/{id}` | token + PIN header | replace validated profile |
| POST | `/api/v1/profiles/{id}/activate` | token | activate profile |
| POST | `/api/v1/profiles/{id}/reset` | token + PIN header | restore factory profile |
| POST | `/api/v1/profiles/{id}/test` | token + PIN header | execute temporary binding |
| GET | `/ws` | token in first message | commands and execution results |

Actions are `next`, `previous`, `start`, `start-current`, `stop`, `black`,
`white`, `first`, `last`, and `goto`. `goto` requires `slide` from 1 to 9999.
HTTP/WebSocket acceptance only means the bounded router accepted the item; the
final `executed` or `failed` event carries the same request ID.

## Firmware update

Upload the raw application `slidelink.bin`, not a merged flash image. The server
rejects an empty/oversized body, write error, incomplete transfer, invalid ESP
image, or an image whose project name is not `slidelink`. Secure production
builds also enforce the ESP-IDF image signature. A successful update schedules
a restart; boot rollback remains active until the full application startup
sequence succeeds.

## First-run credential exposure

`GET /api/v1/setup-credential` exists only while the device is unconfigured.
It enables a manufacturing station connected over direct USB to print the
unique SSID/password label. After setup, the endpoint returns 404. A client that
reaches it over Wi-Fi already possesses that same Wi-Fi credential.
