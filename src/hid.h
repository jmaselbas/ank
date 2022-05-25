#pragma once
#include "hid_key.h"

#define USB_HID_REQ_GET_REPORT		0x01
#define USB_HID_REQ_GET_IDLE		0x02
#define USB_HID_REQ_GET_PROTOCOL	0x03
#define USB_HID_REQ_SET_REPORT		0x09
#define USB_HID_REQ_SET_IDLE		0x0a
#define USB_HID_REQ_SET_PROTOCOL	0x0b

/* HID REPORT */

/* Usage Page */
#define HID_GENERIC_DESKTOP	0x01
#define HID_KEY_CODES		0x07
#define HID_LEDS		0x08

/* Usage */
#define HID_KEYBOARD		0x06

/* Collection */
#define HID_APPLICATION		0x01
/* Input / Output / Feature */
#define HID_CONSTANT		BIT(0) /* DATA / CONSTANT */
#define HID_VARIABLE		BIT(1) /* ARRAY / VARIABLE */
#define HID_ARRAY		0
#define HID_RELATIVE		BIT(2) /* ABSOLUTE / RELATIVE */

#define HID_USAGE_PAGE(x)	0x05, (x)
#define HID_USAGE(x)		0x09, (x)
#define HID_COLLECTION(x)	0xa1, (x)
#define HID_END_COLLECTION	0xc0
#define HID_USAGE_MINIMUM(x)	0x19, (x)
#define HID_USAGE_MAXIMUM(x)	0x29, (x)
#define HID_LOGICAL_MINIMUM(x)	0x15, (x)
#define HID_LOGICAL_MAXIMUM(x)	0x25, (x)
#define HID_REPORT_SIZE(x)	0x75, (x)
#define HID_REPORT_COUNT(x)	0x95, (x)
#define HID_INPUT(x)		0x81, (x)
#define HID_OUTPUT(x)		0x91, (x)

struct keyboard_boot_report {
	u8 modifier;
	u8 reserved;
	u8 keycodes[6];
};
