# Production provisioning

This procedure is intentionally separate from normal development flashing.
Practice the entire flow on sacrificial development hardware, including OTA,
rollback, recovery limits, label capture, and rejection of unsigned images,
before approving a manufacturing lot.

## 1. Release prerequisites

1. Assign the commercial USB VID/PID and manufacturer string in the production
   defaults.
2. Generate an RSA-3072 Secure Boot v2 signing key in the approved offline/HSM
   environment. Store it as `keys/production_signing_key.pem` only on the
   controlled signing station; never commit it.
3. Archive the release source, dependency lock, signed binaries, hashes,
   partition table, toolchain version, key ID, and validation report.
4. Keep anti-rollback disabled until secure-version and recovery procedures are
   separately approved.
5. Verify the target has 16 MiB flash and default/unlocked security eFuses.

The production partition table is intentionally located at `0x10000`, leaving
room for the Secure Boot v2 signature sector. Its data partitions differ from
DEV, while both OTA application slots remain at `0x30000` and `0x330000`.

Key-generation example for a pilot key (not a substitute for the key ceremony):

```powershell
New-Item -ItemType Directory -Force keys
openssl genrsa -out keys\production_signing_key.pem 3072
```

## 2. Safe audit/build

The default invocation reads the chip, writes an eFuse audit file, and builds
signed production artifacts. It does not flash or reset the board:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\provision-production.ps1 -Port COM3
```

Review `provisioning-output/efuse-before-*.txt`. Stop if the chip is already
secured under an unknown key, flash size/revision differs, or any security bit
is unexpected.

## 3. Explicit staging and irreversible enablement

Only after review, run with `-Execute`. The script requires two case-sensitive
phrases containing the exact device MAC:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\provision-production.ps1 -Port COM3 -Execute
```

The first confirmation stages the signed bootloader, partition table, OTA data,
and app using `--no-stub --after no-reset`. No deliberate reset occurs. The second
confirmation resets the chip; first boot then enables Flash Encryption Release,
Secure Boot v2, secure ROM download mode, and JTAG lockdown. This eFuse state is
not reversible. Do not remove power for at least 60 seconds.

If the second phrase is not entered, the script deliberately leaves the board
without a reset. Keep it powered and in download mode while an engineer decides
whether to restage or proceed.

## 4. Per-device completion

After the secure first boot:

1. Connect native USB and verify Windows shows both HID keyboard and NCM.
2. Verify `192.168.55.1`, no USB default gateway/DNS, and ordinary Internet.
3. Capture the unique setup credential and label before completing setup:
   `python tools/make_setup_label.py --require-qr`.
4. Complete setup, reboot, and verify settings/profile persistence.
5. Install a signed pilot update through the UI, verify the new slot boots, and
   verify an unsigned/wrong-product image is rejected.
6. Read and archive the final eFuse summary and device identity.
7. Run the Windows/product compatibility checklist and record the result.

For a high-volume line, prefer Espressif's external security enablement workflow
with a unique host-generated Flash Encryption key per device and auditable HSM
signing. The included script is a guarded pilot/small-run flow using ESP-IDF's
secure first boot; it is not a substitute for a factory key-management system.
