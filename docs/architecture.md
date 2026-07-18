# Architecture

SlideLink separates command input, presentation intent and USB transport so
future web or CYD inputs cannot bypass validation or send raw HID reports.

```text
UART parser ─┐
             ├─> command_router (ID + queue[8] + statistics) ─> presenter ─> usb_hid
BOOT button ─┘                                                     │
                                                                  └─> TinyUSB
```

## Components

### `usb_hid`

Owns the keyboard report/configuration descriptors, TinyUSB lifecycle, partial
chip-derived serial number, mount/suspend state and USB session counter. A key
tap has a bounded sequence:

1. Reject if USB is not mounted.
2. Wait at most 250 ms for the HID endpoint.
3. Send the pressed report and wait for transfer completion.
4. Hold for 20 ms.
5. Send the all-keys-released report and wait for completion.
6. Attempt another release report on any transfer error.

### `presenter`

Maps the fixed presentation command enum to HID usages. It has no knowledge of
UART syntax or the queue. `goto` emits each decimal digit and then Enter.

### `command_router`

Allocates monotonically increasing non-zero IDs, accepts at most eight pending
items, runs one worker and collects statistics. Each queue item records the
current USB session. A detach changes the session; stale items are rejected and
the rest of the queue is cleared, even if the device reconnects quickly.

### `presenter_console`

Reads UART0 through the ESP-IDF UART VFS. The parser is case-insensitive for
command names, enforces the 64-character limit and never exposes raw HID input.
`status` and `help` do not enter the execution queue.

### `presenter_button`

Polls GPIO0 every 10 ms, applies 40 ms debounce and produces one event on
release. It remains disarmed until GPIO0 has first been released, preventing a
download-mode/boot hold from becoming a presentation command.

## Failure behavior

- Not mounted: reject before enqueue.
- Endpoint busy: bounded 250 ms wait, then fail and release all keys.
- Disconnect during execution: abort, attempt release, invalidate the session,
  clear pending items.
- Full queue: reject the new item; never overwrite an older command.
- Reset: queue and statistics are RAM-only, and no key reports are sent at boot.
