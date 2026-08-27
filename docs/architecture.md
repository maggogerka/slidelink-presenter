# Architecture

SlideLink has one presentation-intent path and transport-independent HTTP
backend. Web, UART, and physical buttons can request only typed commands; none
can submit arbitrary HID reports.

```text
                         +--> USB HID keyboard --> presentation software
buttons --\              |
UART ------> command_router --> presenter/profile allowlist
HTTP/WS --/       |      |
                  |      +--> bounded key taps + explicit release
                  +--> result callback --> WebSocket

browser --> HTTP server --> session/settings/profiles/OTA
                ^    ^
                |    +-- Wi-Fi SoftAP 192.168.4.1 (fallback)
                +------- USB NCM 192.168.55.1 (primary, no gateway/DNS)
```

## Composite USB

The TinyUSB device uses the Miscellaneous/Common/IAD device class required for
a composite NCM device on Windows. Full-speed endpoints are:

| Function | Endpoint |
|---|---|
| HID keyboard | interrupt IN 1 |
| NCM notifications | interrupt IN 2 |
| NCM data | bulk OUT 3 / IN 3 |

This fits the ESP32-S3 endpoint allocation while keeping HID independent of the
network stack. The device descriptor is owned by `usb_hid`; `usb_network`
attaches an Ethernet-style `esp_netif`, DHCP server, and TinyUSB NCM callbacks.
TinyUSB RX memory is copied before passing it to the asynchronous network stack.

The USB DHCP server provides a host address and `/24` subnet only. Router and
DNS DHCP options are explicitly disabled, and the ESP route priority is below
normal interfaces. SlideLink therefore does not become the Windows default
route or DNS server.

## Command safety

`command_router` serializes work through an eight-item queue. Every item
captures its USB session plus profile ID/revision. Disconnect, reset, or profile
change invalidates stale work. HID waits are bounded, every successful press is
followed by a release, and error paths make a best-effort all-keys release.

`presenter` accepts only the compiled key/modifier allowlist. A profile binding
has at most four steps and bounded delays. Profile updates stop intake, empty
the queue, wait for active execution, release keys, and atomically publish the
new revision.

## Persistence and updates

- `profile_store`: six fixed profiles in a CRC-protected, schema-versioned NVS
  blob; corrupt data falls back to factory profiles.
- `session_manager`: schema migration, unique setup credential, Wi-Fi password,
  PIN salt/KDF result, and no plaintext PIN.
- `firmware_update`: streams directly to the inactive OTA partition, validates
  the ESP image and project name, changes the boot partition only after success,
  and confirms the new image only after all application subsystems initialize.
- `partitions.csv`: encrypted-capable NVS, OTA metadata, NVS key partition,
  coredump, and two 3 MiB OTA application slots on 16 MiB flash.

## Inputs

Input policy is configured, not embedded in presentation logic:

- development default GPIO0: short Next, >=1 s Previous, >=8 s reset;
- production PCB: configurable Next GPIO and optional independent Previous GPIO;
- RST/EN remains hardware reset.

Sampling is 10 ms with 40 ms debounce. A button held during boot is disarmed
until released, preventing an accidental command or factory reset.
