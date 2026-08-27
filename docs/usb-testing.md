# Windows 11 release validation

Use a sacrificial DEV-secured board first. Keep USB-UART monitoring on COM3 and
connect the native USB/OTG port separately when the board exposes two ports.
Do not mark a row in `compatibility.md` Pass without performing it.

## 1. Enumeration and route isolation

```powershell
Get-PnpDevice -PresentOnly | Where-Object InstanceId -Match 'VID_303A&PID_4005'
Get-NetAdapter | Sort-Object InterfaceDescription | Format-Table Name,InterfaceDescription,Status
Get-NetIPConfiguration | Format-List InterfaceAlias,IPv4Address,IPv4DefaultGateway,DNSServer
route print -4
curl.exe --noproxy "*" http://192.168.55.1/api/v1/system
```

Pass criteria:

- one composite parent yields a working HID keyboard and NCM network adapter;
- the NCM host receives `192.168.55.x/24`;
- NCM has no default gateway and no DNS server;
- the existing default route and Internet remain on the user's normal adapter;
- `192.168.55.1` opens repeatedly in Chrome and Edge;
- native USB disconnect/reconnect restores both functions without reboot.

Also try `slidelink.local`; failure of `.local` is acceptable only if the
documented numeric fallback remains reliable.

## 2. Presentation applications

Use a 12+ slide deck and verify each action by observing the application, not
only an API acceptance response:

1. Next and Previous.
2. Start from first and start from current.
3. End presentation.
4. Black screen twice and white screen twice.
5. First, Last, and Go to slide 12.
6. Short and >=1 s physical button actions.

Repeat for Microsoft 365 PowerPoint, Google Slides in Chrome and Edge,
LibreOffice Impress when installed, and Chrome/Edge PDF presentation modes.

## 3. Stability

- 500+ alternating Next/Previous executions with final slide position checked
- rapid physical/web commands together; bounded rejections are acceptable,
  hangs and stuck keys are not
- fill the eight-item queue and remove native USB; no old command may replay
- ten native USB unplug/replug cycles
- five board resets with presentation focus; no spontaneous key
- Windows sleep/resume followed by immediate HID and web commands
- WebSocket/network disconnect/reconnect during commands
- Wi-Fi SoftAP disconnect/reconnect and two-client limit
- boot with no network client for 30 minutes
- corrupt test NVS, verify safe defaults; then factory reset and re-provision
- credential/profile persistence across at least five resets

Collect UART reset reasons, minimum heap, NCM dropped-packet counter, and any
watchdog output. A reset, watchdog, permanent wait, or stuck key is a failure.

## 4. OTA and rollback

1. Upload the current valid app and verify the alternate slot boots.
2. Verify settings and profiles persist.
3. Reject a truncated image, random file, wrong project image, and (production)
   unsigned image.
4. On a sacrificial board, force a new image to fail before app confirmation
   and verify bootloader rollback.
5. Interrupt upload at multiple offsets; the previous slot must still boot.

## 5. Browser/mobile UI

For Chrome, Edge, Android Chrome, and iPhone Safari:

- first-run setup, login/throttle, and session expiry
- manual RU/EN switching and persistence after reload
- every presenter control, timer, slide number constraints
- profile edit/test/save/reset/activate
- credential change, device details, update error reporting, factory reset
- portrait/landscape and narrow viewport without clipped critical controls
