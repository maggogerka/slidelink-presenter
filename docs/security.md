# Security model

SlideLink has no cloud trust boundary. USB is a direct physical link; Wi-Fi is
a local WPA2/PMF fallback. The embedded site uses HTTP, so users must not reuse
its Wi-Fi password or PIN for any other service.

## Application controls

- Every blank/corrupt device configuration receives a random 12-character
  setup credential from the hardware RNG. There is no shared factory password.
- The setup credential can be read for label printing only before first-run
  provisioning; a factory reset generates a new one.
- PINs are 4-8 digits and are stored as PBKDF2-HMAC-SHA-256 with a random
  128-bit salt and 50,000 iterations, never plaintext. A legacy v0.2 salted
  SHA-256 value is upgraded after the first successful login.
- PIN comparisons do not exit early. Five failures cause a 30-second block.
- Login creates a random 256-bit token. At most four tokens exist in RAM and
  use a sliding 30-minute expiry. Reset and credential change invalidate them.
- Credential/profile/reset/update mutations require a valid bearer session;
  sensitive mutations additionally require the current PIN.
- HID input is a compile-time allowlist. Raw reports, arbitrary text, OS
  commands, scripts, GUI keys, and unbounded delays are not API features.
- USB command state is never persisted or replayed after reconnect/reboot.

PIN throttling limits online guessing; the PRODUCTION configuration's NVS and
flash encryption protect the persisted verifier against straightforward flash
readout. A 4-digit PIN is still weaker than a longer PIN, so 6-8 digits are
recommended.

## DEV versus PRODUCTION

DEV (`sdkconfig.defaults`) keeps UART/JTAG and normal flashing. It logs the
unique setup credential to UART for a development board. It does not enable
eFuse-backed security.

PRODUCTION (`sdkconfig.production.defaults`) prepares:

- Secure Boot v2 with RSA-3072 signed bootloader/application;
- Flash Encryption in Release mode (AES-256);
- NVS encryption with keys protected by Flash Encryption;
- WARN production log policy and no application console;
- secure ROM download mode and the default Secure Boot JTAG lockdown;
- signed OTA verification and two-slot boot rollback.

Anti-rollback is prepared as an opt-in lifecycle step, not enabled by default.
It consumes monotonic eFuse secure-version bits and must follow a tested release
number, key-rotation, and recovery policy.

Building production artifacts does not burn eFuses. Booting their secure
bootloader on an unprovisioned device can. See
[production provisioning](production-provisioning.md).

## Key policy

`keys/` and `provisioning-output/` are ignored by Git. A shipping signing key
must be generated with a high-quality entropy source, held offline or in an
HSM/signing service, backed up under dual control, and never copied into CI.
The CI workflow uses a disposable key only to prove that the security profile
compiles.

The firmware's default Espressif USB VID is not a commercial identity. Assign a
licensed VID/PID before distribution.

Espressif references:

- [Secure Boot v2](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/secure-boot-v2.html)
- [Flash Encryption](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/flash-encryption.html)
- [Security enablement workflows](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/security-features-enablement-workflows.html)
