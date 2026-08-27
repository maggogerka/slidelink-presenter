# SlideLink user quick start

## USB (recommended)

1. Connect the port labelled Native USB, USB, or OTG to the Windows 11 PC.
2. Wait for Windows to install its built-in keyboard and USB NCM drivers.
3. Open `http://192.168.55.1` in Chrome or Edge. Try
   `http://slidelink.local` when the name is available.
4. On first use, choose a private Wi-Fi fallback password and a 6-8 digit PIN.
5. After the restart, open the page again and enter the PIN.
6. Select the application profile, focus the presentation, and use the large
   Previous/Next controls.

SlideLink's USB adapter deliberately has no default gateway or DNS. Your normal
Ethernet/Wi-Fi Internet connection should remain unchanged.

## Wi-Fi fallback

Join `SlideLink-XXXX` with the password chosen during setup, then open
`http://192.168.4.1` or `http://slidelink.local`. Before first setup the SSID is
`SlideLink-XXXX-Setup` and its unique password is printed on the product label.
The network intentionally reports no Internet.

## Physical button

On the development board, a short BOOT press sends Next, a hold of at least one
second sends Previous, and a hold of at least eight seconds performs factory
reset. RST/EN only restarts the device.

## Settings and update

The gear opens device/connection/security status, credential changes, firmware
update, and factory reset. Credential changes and reset require the current PIN
and restart the device. Upload only an official SlideLink application `.bin`.

If USB commands stop, reconnect native USB and wait for `USB ready`; queued
commands from the old connection are discarded. If the page is unavailable,
use the numeric IP rather than the `.local` name.
