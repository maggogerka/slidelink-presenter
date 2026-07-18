# Architecture

SlideLink separates inputs, presentation intent, persistent configuration and
USB transport. Neither the web API nor UART can submit arbitrary HID reports.

```text
UART console ----\
BOOT button ------> command_router --> presenter/profile snapshot --> usb_hid --> TinyUSB
HTTP API --------/        |                         ^
                          +--> WebSocket results    |
web UI --> session auth --> web_server --> profile_store (NVS + CRC32)
                 |              |
                 +--> Wi-Fi AP --+--> device_state
```

## Components

### `usb_hid`

Owns descriptors, TinyUSB lifecycle, chip-derived serial, coherent USB-state
snapshots and the session counter. A key tap has bounded endpoint waits, a
20 ms hold and an explicit all-keys-released report. Any transport failure
attempts another release. The descriptor does not advertise remote wakeup;
commands are rejected while the host is suspended.

### `presenter` and `profile_store`

`presenter` validates and executes presentation bindings. Profiles contain no
more than four steps per action and use only the key/modifier allowlist. Per-step
and total delays are bounded. `profile_store` keeps six fixed-ID profiles, the
active ID, a schema version and CRC32 in one NVS blob. Invalid or incompatible
storage is replaced with factory defaults.

### `command_router`

Allocates non-zero request IDs, accepts at most eight pending items and runs one
worker. Each item snapshots USB session, profile ID and profile revision.
Disconnects, resets and profile changes invalidate queued work. Result callbacks
publish `executed` or `failed` with duration and an error enum; formatting is
outside critical sections.

### `session_manager`, `wifi_manager` and `web_server`

`session_manager` owns provisioning, PIN verification, login throttling and up
to four RAM-only session tokens. `wifi_manager` creates a WPA2/PMF SoftAP with
at most two clients and advertises `slidelink.local` by mDNS. `web_server`
serves compiled-in assets plus the authenticated JSON API and WebSocket.

### `presenter_console` and `presenter_button`

The console retains the bounded v0.1 UART parser and reports the ESP-IDF project
version. GPIO0 is sampled every 10 ms with 40 ms debounce. It stays disarmed
until released after boot. Short and one-second holds submit Next/Previous; an
eight-second hold erases security/profile configuration and restarts.

## Concurrency and failure behavior

- USB lifecycle state is copied under one `portMUX` as a coherent snapshot.
- Profile data is protected by mutexes and published to `presenter` atomically.
- Profile changes pause command intake, clear the queue and release all keys.
- Endpoint busy waits are bounded; errors cannot leave a key intentionally held.
- Queue full rejects the new item and never overwrites an old command.
- Reset loses all RAM queue/session state; no key report is emitted at boot.
- Static assets require no CDN, DNS server or Internet connection.
