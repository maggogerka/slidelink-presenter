# Security model

SlideLink assumes its WPA2 SoftAP is the local trust boundary. It provides HTTP,
not TLS, because the autonomous microcontroller has no public hostname or
certificate authority. Do not reuse the SlideLink Wi-Fi password or PIN for
other services.

## Controls

- First boot exposes only `SlideLink-XXXX-Setup` with the documented temporary
  WPA2 password; setup can run only while the device is unconfigured.
- The chosen Wi-Fi password must contain 8–63 characters. The control PIN is
  4–8 decimal digits.
- NVS stores a random 16-byte salt and SHA-256 PIN digest, never the PIN.
- Login compares digests without early exit. Five failed attempts impose a
  30-second delay.
- Successful login returns a random 256-bit token. At most four tokens exist,
  only in RAM, with a 30-minute sliding expiry. A reset invalidates all tokens.
- Profile keys and modifiers are parsed against compile-time allowlists. GUI
  modifiers, arbitrary HID usages, arbitrary text and raw reports are rejected.
- Resetting a profile requires a fresh PIN check. Holding BOOT for eight seconds
  is the physical recovery path and erases security plus profile configuration.

The current prototype does not implement HTTPS, Internet routing, cloud access,
OTA updates or remote wakeup.
