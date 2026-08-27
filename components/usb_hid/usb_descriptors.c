/*
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>
#include "class/hid/hid_device.h"
#include "class/net/net_device.h"
#include "tinyusb.h"
#include "tusb.h"

enum {
    USB_INTERFACE_HID = 0,
    USB_INTERFACE_NCM_CONTROL,
    USB_INTERFACE_NCM_DATA,
    USB_INTERFACE_COUNT,
};

enum {
    USB_STRING_NONE = 0,
    USB_STRING_MANUFACTURER,
    USB_STRING_PRODUCT,
    USB_STRING_SERIAL,
    USB_STRING_HID,
    /* esp_tinyusb's NCM driver resolves the MAC string at index 5 when NCM is
     * the only managed class. Keep this index in sync with its descriptor
     * controller; the NCM interface itself intentionally has no string. */
    USB_STRING_NCM_MAC,
};

#define USB_HID_ENDPOINT_IN       0x81
#define USB_NCM_NOTIFICATION_IN   0x82
#define USB_NCM_DATA_OUT          0x03
#define USB_NCM_DATA_IN           0x83
#define USB_HID_ENDPOINT_SIZE     8
#define USB_HID_POLL_INTERVAL_MS  10
#define USB_NCM_ENDPOINT_SIZE     64
#define USB_CONFIG_TOTAL_LEN      (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + \
                                   TUD_CDC_NCM_DESC_LEN)

const tusb_desc_device_t slidelink_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    /* Development identity only. A commercial run must use an assigned VID
     * and PID; this PID differs from the v0.2 HID-only identity so Windows
     * does not reuse the old descriptor cache. */
    .idVendor = CONFIG_SLIDELINK_USB_VID,
    .idProduct = CONFIG_SLIDELINK_USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = USB_STRING_MANUFACTURER,
    .iProduct = USB_STRING_PRODUCT,
    .iSerialNumber = USB_STRING_SERIAL,
    .bNumConfigurations = 1,
};

const uint8_t slidelink_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

const uint8_t slidelink_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, USB_INTERFACE_COUNT, 0, USB_CONFIG_TOTAL_LEN,
                          0, 100),
    TUD_HID_DESCRIPTOR(USB_INTERFACE_HID, USB_STRING_HID, true,
                       sizeof(slidelink_hid_report_descriptor),
                       USB_HID_ENDPOINT_IN, USB_HID_ENDPOINT_SIZE,
                       USB_HID_POLL_INTERVAL_MS),
    TUD_CDC_NCM_DESCRIPTOR(USB_INTERFACE_NCM_CONTROL, USB_STRING_NONE,
                           USB_STRING_NCM_MAC, USB_NCM_NOTIFICATION_IN,
                           USB_NCM_ENDPOINT_SIZE, USB_NCM_DATA_OUT,
                           USB_NCM_DATA_IN, USB_NCM_ENDPOINT_SIZE,
                           CFG_TUD_NET_MTU),
};

const size_t slidelink_configuration_descriptor_size =
    sizeof(slidelink_configuration_descriptor);
