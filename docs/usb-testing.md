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

1. Send 100 alternating `next`/`previous` commands. Some may be explicitly
   rejected when the queue is full; the firmware must not hang or leave a key
   held.
2. Disconnect native USB while commands are queued. Reconnect it and verify no
   old command is replayed.
3. Suspend and resume Windows; verify new commands work after resume.
4. Try `goto 0`, `goto -5`, `goto 10000`, `goto abc`, an unknown command and a
   65-character line. Verify each is rejected and no slide changes.
5. Reset the board repeatedly with PowerPoint focused. Verify no spontaneous
   slide movement.

Record the Windows version, PowerPoint version, board model and cable/port used
in `docs/compatibility.md` when a full pass is completed.
