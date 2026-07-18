/*
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>
#include "class/hid/hid_device.h"
#include "tusb.h"

#define USB_HID_INTERFACE_NUMBER 0
#define USB_HID_INTERFACE_COUNT  1
#define USB_HID_ENDPOINT_IN      0x81
#define USB_HID_ENDPOINT_SIZE    8
#define USB_HID_POLL_INTERVAL_MS 10
#define USB_CONFIG_TOTAL_LEN     (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

const uint8_t slidelink_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

const uint8_t slidelink_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, USB_HID_INTERFACE_COUNT, 0, USB_CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(USB_HID_INTERFACE_NUMBER, 4, true,
                       sizeof(slidelink_hid_report_descriptor),
                       USB_HID_ENDPOINT_IN, USB_HID_ENDPOINT_SIZE,
                       USB_HID_POLL_INTERVAL_MS),
};

const size_t slidelink_configuration_descriptor_size =
    sizeof(slidelink_configuration_descriptor);
