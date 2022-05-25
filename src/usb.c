#include "common.h"
#include "string.h"
#include "usb.h"
#include "hid.h"

#ifdef CONFIG_USB_LOWSPEED
#define USB_MAXPACKET_SIZE 8
#else
#define USB_MAXPACKET_SIZE 64
#endif

void usb_init(void);
void usb_poll(void);
void usb_ep_xfer(int ep, void *buf, size_t len);

struct ep_state {
	struct usb_ctrlrequest req;
	u8 *const dma;
	u8 *buf;
	u16 rem;
};

__attribute__ ((aligned(4))) static u8 ep0_buf[64];
__attribute__ ((aligned(4))) static u8 ep1_buf[64];
__attribute__ ((aligned(4))) static u8 ep2_buf[64];

struct ep_state ep_state[8] = {
	[0] = { .dma = ep0_buf },
	[1] = { .dma = ep1_buf },
	[2] = { .dma = ep2_buf },
};

static u8 hid_report_dt[] = {
	HID_USAGE_PAGE(HID_GENERIC_DESKTOP),
	HID_USAGE(HID_KEYBOARD),
	HID_COLLECTION(HID_APPLICATION),
		HID_REPORT_SIZE(1),
		HID_REPORT_COUNT(8),
		HID_USAGE_PAGE(HID_KEY_CODES),
		HID_USAGE_MINIMUM(224), /* left control */
		HID_USAGE_MAXIMUM(231), /* right meta */
		HID_LOGICAL_MINIMUM(0),
		HID_LOGICAL_MAXIMUM(1),
		HID_INPUT(HID_VARIABLE), /* modifiers */

		HID_REPORT_SIZE(1),
		HID_REPORT_COUNT(8),
		HID_INPUT(HID_CONSTANT), /* reserved */

		HID_REPORT_SIZE(1),
		HID_REPORT_COUNT(5),
		HID_USAGE_PAGE(HID_LEDS),
		HID_USAGE_MINIMUM(1),
		HID_USAGE_MAXIMUM(5),
		HID_OUTPUT(HID_VARIABLE), /* led report */
		HID_REPORT_SIZE(1),
		HID_REPORT_COUNT(3),
		HID_OUTPUT(HID_CONSTANT),

		HID_REPORT_SIZE(8),
		HID_REPORT_COUNT(6),
		HID_USAGE_PAGE(HID_KEY_CODES),
		HID_USAGE_MINIMUM(0),
		HID_USAGE_MAXIMUM(255),
		HID_LOGICAL_MINIMUM(0),
		HID_LOGICAL_MAXIMUM(255),
		HID_INPUT(HID_ARRAY),

	HID_END_COLLECTION,
};

#define USB_IPRODUCT 1
static const char *usb_string_desc[] = {
	[0] = "",
	[USB_IPRODUCT] = "ank keyboard r0.0",
};

static struct descriptor {
	struct usb_device_descriptor device;
	struct usb_config_descriptor config;
	struct usb_interface_descriptor interface;
	struct hid_descriptor hid;
	struct usb_endpoint_descriptor endpoint[2];
} __attribute__ ((packed)) descriptor = {
	.device = {
		.bLength = USB_DT_DEVICE_SIZE,
		.bDescriptorType = USB_DT_DEVICE,
		.bcdUSB = 0x0200,
		.bDeviceClass = 0,
		.bDeviceSubClass = 0,
		.bDeviceProtocol = 0,
		.bMaxPacketSize0 = USB_MAXPACKET_SIZE,
		.idVendor = 0x1209, /* pid.codes */
		.idProduct = 0x6e6b,
		.bcdDevice = 0x0000,
		.iManufacturer = 0,
		.iProduct = USB_IPRODUCT,
		.iSerialNumber = 0,
		.bNumConfigurations = 1,
	},
	.config = {
		.bLength                = USB_DT_CONFIG_SIZE,
		.bDescriptorType        = USB_DT_CONFIG,
		.wTotalLength           = (USB_DT_CONFIG_SIZE +
					   USB_DT_INTERFACE_SIZE +
					   1 * USB_DT_ENDPOINT_SIZE +
					   USB_DT_HID_SIZE),
		.bNumInterfaces         = 1,
		.bConfigurationValue    = 1,
		.iConfiguration         = 0,
		.bmAttributes           = USB_CONFIG_ATT_ONE,
		.bMaxPower              = 50, /* 100mA */
	},
	.interface = {
		.bLength		= USB_DT_INTERFACE_SIZE,
		.bDescriptorType	= USB_DT_INTERFACE,
		.bInterfaceNumber	= 0,
		.bAlternateSetting	= 0,
		.bNumEndpoints		= 1,
		.bInterfaceClass	= USB_CLASS_HID,
		.bInterfaceSubClass	= USB_INTERFACE_SUBCLASS_BOOT,
		.bInterfaceProtocol	= USB_INTERFACE_PROTOCOL_KEYBOARD,
		.iInterface		= 0
	},
	.endpoint[0] = {
		.bLength		= USB_DT_ENDPOINT_SIZE,
		.bDescriptorType	= USB_DT_ENDPOINT,
		.bEndpointAddress	= 1 | USB_DIR_IN, /* 0x81 */
		.bmAttributes		= USB_ENDPOINT_XFER_INT,
		.wMaxPacketSize		= 8,
		.bInterval		= 8, /* ms */
	},
	.endpoint[1] = {
		.bLength		= USB_DT_ENDPOINT_SIZE,
		.bDescriptorType	= USB_DT_ENDPOINT,
		.bEndpointAddress	= 2 | USB_DIR_OUT, /* 0x02 */
		.bmAttributes		= USB_ENDPOINT_XFER_INT,
		.wMaxPacketSize		= 8,
		.bInterval		= 8, /* ms */
	},
	.hid = {
		.bLength		= USB_DT_HID_SIZE,
		.bDescriptorType	= HID_DT_HID,
		.bcdHID			= 0x0101,
		.bCountryCode		= 0x00,
		.bNumDescriptors	= 1,
		.desc[0] = {
			.bDescriptorType   = HID_DT_REPORT,
			.wDescriptorLength = sizeof(hid_report_dt),
		},
	},
};

static u16 usb_fill_strle16(u16 *d, char *s)
{
	u16 size;
	u16 i;

	for (i = 0; s[i]; i++) {
		d[i + 1] = s[i];
	}
	size = (i + 1) * sizeof(u16);
	d[0] = (0x3 << 8) | (size & 0xff);
	return size;
}

static int usb_get_descriptor(struct ep_state *ep)
{
	static u16 str16_buf[64];
	struct usb_ctrlrequest *req = &ep->req;
	u16 index = req->wValue >> 8;
	u16 len;
	u8 *buf;

	switch (index) {
	case USB_DT_DEVICE:
		printf("dt device\r\n");
		len = descriptor.device.bLength;
		buf = (u8 *)&descriptor.device;
		break;
	case USB_DT_CONFIG:
		printf("dt config\r\n");
		len = descriptor.config.wTotalLength;
		buf = (u8 *)&descriptor.config;
		break;
	case USB_DT_STRING:
		index = req->wValue & 0xff;
		printf("dt string %d\r\n", index);
		if (index == 0) {
			buf = "\x04\x03\x09\x04";
			len = 4;
		} else if (index < LEN(usb_string_desc)) {
			buf = (u8 *)str16_buf;
			len = usb_fill_strle16(str16_buf, "ank keyboard r0.0");
			printf("len %d\r\n", len);
		} else {
			printf("unknown string index: %d\r\n", index);
			return -1;
		}
		break;
	case HID_DT_REPORT:
		len = sizeof(hid_report_dt);
		buf = hid_report_dt;
		break;
	default:
		printf("unknown descriptor: %x\r\n", index);
		return -1;
	}

	ep->rem = MIN(len, req->wLength);
	ep->buf = buf;

	return 0;
}

#define USBHD_BASE IOMEM(0x40023400)
#define USBHD_CTRL 0x00
#define USBHD_UDEV 0x01
#define USBHD_ITEN 0x02
#define USBHD_ADDR 0x03
#define USBHD_STAT 0x04
#define USBHD_ITFG 0x06
#define USBHD_ITST 0x07
#define USBHD_RXLEN 0x08

#define USBHD_EP1_MOD 0x0c
#define USBHD_EP4_MOD 0x0c
#define USBHD_EP1_RX_EN BIT(7)
#define USBHD_EP1_TX_EN BIT(6)

#define USBHD_EP_DMA(n)   (0x10 + 0x4 * (n))
#define USBHD_EP_TXLEN(n) (0x30 + 0x4 * (n))
#define USBHD_EP_CTRL(n)  (0x32 + 0x4 * (n))
#define USBHD_EP_TX_ACK   (0b00 << 0)
#define USBHD_EP_TX_NUL   (0b01 << 0) /* no response */
#define USBHD_EP_TX_NAK   (0b10 << 0) /* nak or busy */
#define USBHD_EP_TX_ERR   (0b11 << 0) /* stall or error */
#define USBHD_EP_RX_ACK   (0b00 << 2)
#define USBHD_EP_RX_NUL   (0b01 << 2) /* no response */
#define USBHD_EP_RX_NAK   (0b10 << 2) /* nak or busy */
#define USBHD_EP_RX_ERR   (0b11 << 2) /* stall or error */
#define USBHD_EP_TX_TOG   BIT(6)
#define USBHD_EP_RX_TOG   BIT(7)
#define USBHD_EP_TX_MASK USBHD_EP_TX_ERR
#define USBHD_EP_RX_MASK USBHD_EP_RX_ERR

#define USBHD_ITST_ENDP(x)  (((x) & 0b001111) >> 0)
#define USBHD_ITST_TOKEN(x) (((x) & 0b110000) >> 4)
#define USBHD_ITST_NAK      BIT(7)

#define USBHD_PID_OUT   0
#define USBHD_PID_SOF   1
#define USBHD_PID_IN    2
#define USBHD_PID_SETUP 3

#define USBHD_ITFG_BUSRST BIT(0)
#define USBHD_ITFG_DETECT BIT(0)
#define USBHD_ITFG_XFER   BIT(1)
#define USBHD_ITFG_WAKEUP BIT(2)
#define USBHD_ITFG_DEVNAK BIT(6)
#define USBHD_ITFG_DEVSOF BIT(7)

#define USBHD_CTR_DMA_EN    BIT(0)
#define USBHD_CTR_CLEAR_ALL BIT(1)
#define USBHD_CTR_RESET_SIE BIT(2)
#define USBHD_CTR_AUTOBUSY BIT(3)
#define USBHD_CTR_PU_EN BIT(5) /* bit 4 and 5 */
#define USBHD_CTR_SYS_DEV_DISABLE 0
#define USBHD_CTR_SYS_DEV_ENABLE  BIT(4)
#define USBHD_CTR_SYS_DEV_PULLUP_ENABLE  BIT(5) /* 0b1x */
#define USBHD_CTR_LOW_SPEED BIT(6)
#define USBHD_CTR_HOST_MODE BIT(7)

#ifdef CONFIG_USB_LOWSPEED
#define USBHD_CTR_SPEED USBHD_CTR_LOW_SPEED
#else
#define USBHD_CTR_SPEED 0
#endif

#define USBHD_CTR_DEFCONFIG (USBHD_CTR_PU_EN | USBHD_CTR_AUTOBUSY | USBHD_CTR_SPEED)

#define USBHD_DEV_PD_DIS         BIT(7)                    // disable USB UDP/UDM pulldown resistance: 0=enable pulldown, 1=disable
#define USBHD_DEV_DP_PIN         BIT(5)                      // ReadOnly: indicate current UDP pin level
#define USBHD_DEV_DM_PIN         BIT(4)                 // ReadOnly: indicate current UDM pin level
#define USBHD_DEV_LOW_SPEED      BIT(2)                      // enable USB physical port low speed: 0=full speed, 1=low speed
#define USBHD_DEV_GP_BIT         BIT(1)                       // general purpose bit
#define USBHD_DEV_PORT_EN        BIT(0)                       // enable USB physical port I/O: 0=disable, 1=enable

static void
usb_set_addr(u8 addr)
{
	write8(USBHD_BASE + USBHD_ADDR, addr);
}

static void
usb_ep_ctrl(u8 epn, u8 ctrl)
{
	write8(USBHD_BASE + USBHD_EP_CTRL(epn), ctrl);
}

static void
usb_ep_toggle(u8 epn, u8 tog)
{
	u8 reg = read8(USBHD_BASE + USBHD_EP_CTRL(epn));
	reg ^= tog;
	write8(USBHD_BASE + USBHD_EP_CTRL(epn), reg);
}

static void
usb_ep_dma(u8 epn, u8 *buf)
{
	u16 off = (u16) (u32) buf;
	write16(USBHD_BASE + USBHD_EP_DMA(epn), off);
}

static void
usb_xfer(u8 pid, u8 ep)
{
	struct ep_state *eps;
	struct usb_ctrlrequest *req;
	u8 rxlen = read8(USBHD_BASE + USBHD_RXLEN);
	u8 txlen;

	if (ep >= LEN(ep_state))
		return;
	eps = &ep_state[ep];
	req = &eps->req;

	switch (pid) {
	case USBHD_PID_SETUP:
		usb_ep_ctrl(ep, USBHD_EP_RX_ACK | USBHD_EP_TX_NAK);
		if (ep != 0 || rxlen != sizeof(struct usb_ctrlrequest))
			return;
		memcpy(&eps->req, eps->dma, rxlen);
		printf("SETUP %x len %d  type %x  req %x\r\n", ep, req->wLength, req->bRequestType, req->bRequest);

#define REQ(u, l) (((u) << 8) | (l))
		switch (REQ(req->bRequest, req->bRequestType)) {
		case REQ(USB_REQ_GET_DESCRIPTOR, USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE):
		case REQ(USB_REQ_GET_DESCRIPTOR, USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE):
			usb_get_descriptor(eps);
			break;
		case REQ(USB_REQ_SET_ADDRESS, USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE):
		case REQ(USB_REQ_SET_CONFIGURATION, USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE):
			break;
			/* wValue -> report type and report ID, wIndex -> interface index, wLength len */
			printf("GET REPORT\r\n");
			break;
		case REQ(USB_HID_REQ_SET_IDLE, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE):
			/* wValue & 0x00ff : report number (0 for all) */
			/* wValue & 0xff00 : minimal duration between report, 0 for only report on change */
			printf("SET IDLE %d %d ms for rep %d\r\n", req->wIndex, 4* (req->wValue >> 8), req->wValue & 0xff);
			break;
		case REQ(USB_HID_REQ_SET_REPORT, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE):
			printf("SET REPORT\r\n");
			break;
		default:
			printf("unsupported request\r\n");
			break;
		}
		if (req->bRequestType & USB_DIR_IN) {
			if (req->wLength != 0) {
				txlen = MIN(USB_MAXPACKET_SIZE, eps->rem);
				memcpy(eps->dma, eps->buf, txlen);
				eps->buf += txlen;
				eps->rem -= txlen;

				write8(USBHD_BASE + USBHD_EP_TXLEN(ep), txlen);
				usb_ep_ctrl(ep, USBHD_EP_RX_NAK | USBHD_EP_TX_ACK | USBHD_EP_TX_TOG);
			} else {
				usb_ep_ctrl(ep, USBHD_EP_RX_ACK | USBHD_EP_TX_ACK
					    | USBHD_EP_RX_TOG | USBHD_EP_TX_TOG);
			}
		} else {
			if (req->wLength != 0) {
				eps->rem = eps->req.wLength;
			} else {
				/* status always use PID DATA1 */
				write8(USBHD_BASE + USBHD_EP_TXLEN(ep), 0); /* zlp */
				usb_ep_ctrl(ep, USBHD_EP_RX_ACK | USBHD_EP_TX_ACK
					    | USBHD_EP_RX_TOG | USBHD_EP_TX_TOG);
			}
		}
		break;
	case USBHD_PID_IN:
		if (ep == 1) {
			usb_ep_toggle(ep, USBHD_EP_TX_ACK | USBHD_EP_TX_NAK | USBHD_EP_TX_TOG);
			break;
		}
		printf("IN %x\r\n", ep);
		if (eps->req.bRequestType & USB_DIR_IN) { /* data in */
			if (eps->rem != 0) {
				txlen = MIN(USB_MAXPACKET_SIZE, eps->rem);
				memcpy(eps->dma, eps->buf, txlen);
				eps->buf += txlen;
				eps->rem -= txlen;
				usb_ep_toggle(ep, USBHD_EP_TX_TOG);
				write8(USBHD_BASE + USBHD_EP_TXLEN(ep), txlen);
			} else if (eps->rem == 0) {
				/* goto OUT STATUS */
				write8(USBHD_BASE + USBHD_EP_TXLEN(ep), 0); /* zlp */
				usb_ep_ctrl(ep, USBHD_EP_RX_ACK | USBHD_EP_TX_NAK
					    | USBHD_EP_RX_TOG);
			}
		} else { /* status phase */
			/* transaction ends here */
			usb_ep_ctrl(ep, USBHD_EP_RX_ACK | USBHD_EP_TX_NAK);
			if (ep != 0)
				break;
			switch (REQ(req->bRequest, req->bRequestType)) {
			case REQ(USB_REQ_SET_ADDRESS, USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE):
				printf("set addr %d\r\n", eps->req.wValue);
				usb_set_addr(eps->req.wValue);
				break;
			case REQ(USB_REQ_SET_CONFIGURATION, USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE):
				printf("set conf %d\r\n", eps->req.wValue);
				if (eps->req.wValue == 0) {
					/* unconfigure */
					usb_ep_ctrl(1, USBHD_EP_RX_ERR | USBHD_EP_TX_ERR);
				} else {
					/* configure */
					usb_ep_ctrl(1, USBHD_EP_RX_ACK | USBHD_EP_TX_ACK);
				}
				break;
			case REQ(USB_HID_REQ_SET_REPORT, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE):
				printf("set report %d\r\n", eps->req.wValue);
				break;
			}
		}
		break;
	case USBHD_PID_OUT:
		if (!(eps->req.bRequestType & USB_DIR_IN)) {
			if (eps->rem != 0 && rxlen > 0) {
				rxlen = MIN(eps->rem, rxlen);
				eps->buf += rxlen;
				eps->rem -= rxlen;
				usb_ep_toggle(ep, USBHD_EP_RX_TOG);
			}
			if (eps->rem == 0) {
				write8(USBHD_BASE + USBHD_EP_TXLEN(ep), 0); /* zlp */
				usb_ep_ctrl(ep, USBHD_EP_RX_ACK | USBHD_EP_TX_ACK
					    | USBHD_EP_RX_TOG | USBHD_EP_TX_TOG);
			}
		} else {
			/* STATUS out */
			usb_ep_ctrl(ep, USBHD_EP_RX_ACK | USBHD_EP_TX_NAK);
		}
		break;
	}
}

void
usb_poll(void)
{
	u8 itflag;
	u8 itstat;

	itflag = read8(USBHD_BASE + USBHD_ITFG);
	itstat = read8(USBHD_BASE + USBHD_ITST);

	/* clear interrupt */
	write8(USBHD_BASE + USBHD_ITFG, itflag);

	if (itflag & USBHD_ITFG_XFER) {
		u8 pid = USBHD_ITST_TOKEN(itstat);
		u8 ep = USBHD_ITST_ENDP(itstat);

		usb_xfer(pid, ep);
	}
	if (itflag & USBHD_ITFG_BUSRST) {
//		puts("bus rst!\r\n");
//write8(USBHD_BASE + USBHD_CTRL, USBHD_CTR_CLEAR_ALL | USBHD_CTR_RESET_SIE);
		usb_set_addr(0);
	}
	if (itflag & USBHD_ITFG_WAKEUP) {
//		puts("wakeup / suspend!\r\n");
	}
}

void usb_init(void)
{
	/* does require GPIOA enabled ? */
	u8 ep;
	u32 reg;

#define USBHD_PU BIT(1)
#define USBHD_IO BIT(2)
#define USBHD_5V BIT(3)

#define EXTEND_CTR IOMEM(0x40023800)

	reg = read32(EXTEND_CTR);
	reg |= USBHD_IO | USBHD_PU;
	reg |= USBHD_5V;
	write32(EXTEND_CTR, reg);

	reg = USBHD_CTR_CLEAR_ALL | USBHD_CTR_RESET_SIE;
	write8(USBHD_BASE + USBHD_CTRL, reg);
	reg = USBHD_CTR_DEFCONFIG | USBHD_CTR_DMA_EN;
	write8(USBHD_BASE + USBHD_CTRL, reg);

	/* clear all interrupts */
	write8(USBHD_BASE + USBHD_ITFG, 0xff);

	usb_set_addr(0);
	/* setup endpoints */
	for (ep = 0; ep < 8; ep++) {
		usb_ep_ctrl(ep, USBHD_EP_RX_ERR | USBHD_EP_TX_ERR);
		usb_ep_dma(ep, ep_state[ep].dma);
	}

	usb_ep_ctrl(0, USBHD_EP_RX_ACK | USBHD_EP_TX_NAK);

	write8(USBHD_BASE + USBHD_EP1_MOD, USBHD_EP1_TX_EN);

	/* enable usb device */
	reg = USBHD_DEV_PORT_EN;
#ifdef CONFIG_USB_LOWSPEED
	reg |= USBHD_DEV_LOW_SPEED;
#endif
	write8(USBHD_BASE + USBHD_UDEV, reg);
}

void
usb_ep_xfer(int ep, void *buf, size_t len)
{
	struct ep_state *eps = &ep_state[ep];

	memcpy(eps->dma, buf, len);
	write8(USBHD_BASE + USBHD_EP_TXLEN(ep), len);
	usb_ep_toggle(ep, USBHD_EP_TX_ACK | USBHD_EP_TX_NAK);
}
