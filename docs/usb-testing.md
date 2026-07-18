# USB and PowerPoint test plan

Use two data-capable cables: USB-UART for logs and native USB/OTG for HID.
Keep a plain text editor closed while testing presentation commands so focus
cannot accidentally make an expected key visible elsewhere.

## Enumeration

1. Flash production firmware over USB-UART.
2. Keep `idf.py -p COM10 monitor` open.
3. Connect native USB to Windows 10.
4. Confirm `USB_HID: mounted session=...` in the UART log.
5. In Device Manager, confirm a HID Keyboard Device whose properties show
   `VID_303A&PID_4004` and product `SlideLink USB Presenter`.
6. Run `status`; expect `usb: mounted` and normally `hid: ready`.
7. Reset the board while PowerPoint is focused; verify no slide changes.

## PowerPoint commands

Open a presentation containing at least 12 slides.

| # | Input | Expected result |
|---:|---|---|
| 1 | `next` | Next slide |
| 2 | `previous` | Previous slide |
| 3 | `start` | Slideshow starts at slide 1 |
| 4 | `start-current` | Slideshow starts at selected slide |
| 5 | `stop` | Slideshow closes |
| 6 | `black` twice | Black screen toggles on and off |
| 7 | `white` twice | White screen toggles on and off |
| 8 | `first` | First slide |
| 9 | `last` | Last slide |
| 10 | `goto 12` | Slide 12 opens |
| 11 | Short BOOT press | Next slide, once |
| 12 | BOOT press >= 1 s | Previous slide, once |

## Robustness

1. Send 200 alternating `next`/`previous` commands. Some may be explicitly
   rejected when the queue is full; the firmware must not hang or leave a key
   held.
2. Fill all eight pending-command slots, then disconnect native USB while the
   queue is non-empty. Reconnect it and verify no old command is replayed.
3. Suspend and resume Windows; verify new commands work after resume.
4. Try `goto 0`, `goto -5`, `goto 10000`, `goto abc`, an unknown command and a
   65-character line. Verify each is rejected and no slide changes.
5. Reset the board at least five times with PowerPoint focused. Verify no
   spontaneous slide movement.

Record the Windows version, PowerPoint version, board model and cable/port used
in `docs/compatibility.md` when a full pass is completed.

## Recorded run: 2026-07-18

Windows 10 Pro build 19045 and PowerPoint 16.0 build 14332 passed native USB
enumeration, every command in the table, 200 alternating Next/Previous
commands, five board resets and a reset while commands were queued. No stale
command replay or spontaneous slide movement was observed.

Physical cable removal and Windows sleep/resume still require an operator and
remain explicitly pending in the compatibility matrix. A board reset exercises
USB detach/re-enumeration but is not recorded as a cable-removal substitute.
