# Web API

The API is versioned under `/api/v1`. JSON request bodies reject malformed
types, oversized payloads and unknown fields. Except for system status,
first-run setup and login, requests require
`Authorization: Bearer <64-hex-character-token>`.

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/api/v1/system` | Firmware, uptime, heap, USB, Wi-Fi, queue and active profile |
| `POST` | `/api/v1/setup` | One-time Wi-Fi password and PIN provisioning |
| `POST` | `/api/v1/session` | Exchange PIN for a RAM-only token |
| `POST` | `/api/v1/commands` | Submit an allowlisted presentation action |
| `GET` | `/api/v1/profiles` | Read all profiles and active ID |
| `PUT` | `/api/v1/profiles/{id}` | Validate and replace a profile |
| `POST` | `/api/v1/profiles/{id}/activate` | Persist and activate a profile |
| `POST` | `/api/v1/profiles/{id}/reset` | Restore one factory profile; PIN required |
| `POST` | `/api/v1/profiles/{id}/test` | Execute a temporary validated binding |
| `GET` | `/ws` | WebSocket status and command-result stream |

Command actions are `next`, `previous`, `start`, `start-current`, `stop`,
`black`, `white`, `first`, `last` and `goto`. `goto` additionally requires a
`slide` value from 1 through 9999. HTTP acceptance means the item entered the
router; final `executed`/`failed` state arrives over the WebSocket with the same
request ID.

The browser application uses these endpoints directly. It is embedded with
`EMBED_TXTFILES` and has no external runtime dependencies.
