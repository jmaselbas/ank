#include "common.h"
#include "string.h"
#include "usb.h"
#include "hid.h"

void usb_init(void);
void usb_poll(void);
void usb_ep_xfer(int ep, void *buf, size_t len);

static u32 clock_get_rate(void);

#define EXTEND_CTR IOMEM(0x40023800)
#define EXTEND_CTR_USBHDIO	BIT(2)
#define EXTEND_CTR_USB5VSEL	BIT(3)
#define EXTEND_CTR_HSIPRE	BIT(4)

#define RCC_BASE IOMEM(0x40021000)
#define RCC_CTLR 0x00
#define RCC_AHBPCENR 0x14
#define RCC_APB2PCENR 0x18
#define RCC_APB1PCENR 0x1c

#define RCC_APB2_GPIOA_EN	BIT(2)
#define RCC_APB2_GPIOB_EN	BIT(3)
#define RCC_APB2_GPIOC_EN	BIT(4)
#define RCC_APB2_GPIOD_EN	BIT(5)
#define RCC_APB2_USART1_EN	BIT(14)

#define GPIOA_BASE IOMEM(0x40010800)
#define GPIOB_BASE IOMEM(0x40010c00)

#define GPIO_CFGLR	0x00
#define GPIO_CFGHR	0x04
#define GPIO_INDR	0x08
#define GPIO_OUTR	0x0c
#define GPIO_BSR	0x10
#define GPIO_BCR	0x14
#define GPIO_LCKR	0x18

static void rcc_enable(u32 reg, u32 en)
{
	u32 val = read32(RCC_BASE + reg);
	write32(RCC_BASE + reg, val | en);
}

static void
ext_conf_usb_gpio(void)
{
	u32 reg = read32(EXTEND_CTR);
	write32(EXTEND_CTR, reg | EXTEND_CTR_USBHDIO);
}

static void
ext_conf_usb_vbus(int vbus)
{
	u32 reg = read32(EXTEND_CTR);

	if (vbus == 5)
		reg |= EXTEND_CTR_USB5VSEL;
	else
		reg &= ~EXTEND_CTR_USB5VSEL;
	write32(EXTEND_CTR, reg);
}


/* combine config and mode bits */
#define GPIO_CFG_ALTFUN 0b1000
#define GPIO_CFG_ODRAIN 0b0100
#define GPIO_CFG_FLOAT  0b0100
#define GPIO_CFG_ANALOG 0b0000
#define GPIO_CFG_INPUT  0b0000
#define GPIO_CFG_INPUT_PUPD  0b1000
#define GPIO_CFG_OUTPUT_10MH 0b0001
#define GPIO_CFG_OUTPUT_2MH  0b0010
#define GPIO_CFG_OUTPUT_50MH 0b0011

#define GPIO_PA9_USART1  (GPIO_CFG_ALTFUN | GPIO_CFG_ODRAIN | GPIO_CFG_OUTPUT_10MH)
#define GPIO_PA10_USART1 (GPIO_CFG_ALTFUN | GPIO_CFG_ODRAIN | GPIO_CFG_INPUT)

#define GPIO_PB6_USBD (GPIO_CFG_ALTFUN | GPIO_CFG_ODRAIN | GPIO_CFG_INPUT)
#define GPIO_PB7_USBD (GPIO_CFG_ALTFUN | GPIO_CFG_ODRAIN | GPIO_CFG_INPUT)

static void gpio_cfg_pin(void *base, u8 pin, u8 cfg)
{
	u32 reg;
	if (pin < 8)
		reg = read32(base + GPIO_CFGLR);
	else
		reg = read32(base + GPIO_CFGHR);
	cfg &= 0b1111;
	reg &= ~(0b1111 << (pin % 8) * 4);
	reg |= (cfg << (pin % 8) * 4);
	if (pin < 8)
		write32(base + GPIO_CFGLR, reg);
	else
		write32(base + GPIO_CFGHR, reg);
}

static void gpio_set_val(void *base, u8 pin, u8 val)
{
	if (val)
		write32(base + GPIO_BSR, BIT(pin)); /* set */
	else
		write32(base + GPIO_BCR, BIT(pin)); /* clear */
}

static u8 gpio_get_val(void *base, u8 pin)
{
	return !!(read32(base + GPIO_INDR) & BIT(pin));
}

#define USART1_BASE IOMEM(0x40013800)

#define USART_STATR 0x00
#define  USART_TXE       BIT(7)
#define  USART_RXNE      BIT(5)
#define USART_DATAR 0x04
#define USART_BRR   0x08
#define USART_CTLR1 0x0c
#define  USART_RE        BIT(2) /* RX enable */
#define  USART_TE        BIT(3) /* TX enable */
#define  USART_UE        BIT(13) /* uart enable */

#define USART_CTLR2 0x10
// bit 15 in CTTLR1 for 8 time oversampling ?

static void usart_init(void)
{
	void *base = USART1_BASE;
	u32 baud = CONFIG_UART_BAUDRATE;
	u32 freq = clock_get_rate();
	u32 reg, div;

	gpio_cfg_pin(GPIOA_BASE, 9, GPIO_PA9_USART1);
	gpio_cfg_pin(GPIOA_BASE, 10, GPIO_PA10_USART1);

	/* enable USART1 */
	reg = read32(RCC_BASE + RCC_APB2PCENR);
	write32(RCC_BASE + RCC_APB2PCENR, reg | BIT(14));

	/* reset USART1 */
	reg = read32(RCC_BASE + 0xc);
	write32(RCC_BASE + 0xc, reg | BIT(14));
	reg = read32(RCC_BASE + 0xc);
	write32(RCC_BASE + 0xc, reg & ~BIT(14));

	/* From the datasheet:
	 *   UARTDIV = DIV_M+(DIV_F/16)
	 *   baudrate = Fclk / (16 * USARTDIV)
	 * UARTDIV is actually a fixed point number, with 4-bits for the
	 * fractional part, ie the number in the BRR register can be directly
	 * stored as if divided by 16.
	 * This makes calculating the uart divisor very easy, by simply doing:
	 */
	div = freq / baud;
	if (div >= BIT(16))
		div = BIT(16) - 1;
	write32(base + USART_BRR, div);

	/* now enable for RX and TX */
	write32(base + USART_CTLR1, USART_RE | USART_TE | USART_UE);
}

static u32
tstc(void)
{
	void *base = USART1_BASE;
	return read32(base + USART_STATR) & USART_RXNE;
}

void
putc_ll(u8 c)
{
	void *base = USART1_BASE;
	while (!(read32(base + USART_STATR) & USART_TXE));
	write32(base + USART_DATAR, c);
}

static
u32 clock_get_rate(void)
{
	return 48 * 1000000; /* 48 MHz */
}

static void
systick_init(void)
{
#define STK_CTLR IOMEM(0xe000f000)
	write32(STK_CTLR, BIT(0)); /* enable */
}

static
u64 get_cycles64(void)
{
#define STK_CNTL IOMEM(0xe000f004)
#define STK_CNTH IOMEM(0xe000f008)
	u32 hi, lo;
	/* CSR timer are not available */
	do {
		hi = read32(STK_CNTH);
		lo = read32(STK_CNTL);
	} while (hi != read32(STK_CNTH));
	return (((u64)hi << 32) | lo) * 8;
}

static
void udelay(int us)
{
	u32 freq = clock_get_rate();
	u64 cyc = us * (freq / 1000000);
	u64 end = get_cycles64() + cyc;

	while (get_cycles64() < end);
}

static void
clock_init(void)
{
#define RCC_CTLR 0x00
#define RCC_CTLR_PLLRDY BIT(25)
#define RCC_CTLR_PLLON BIT(24)
#define RCC_CFGR0 0x04
#define RCC_CFGR0_USBPRE_1X BIT(22) /* 0: PLLclk / 1.5 ; 1: PLLclk / 1.0 */
#define RCC_CFGR0_PLLMUL_MASK (0b1111UL << 18)
#define RCC_CFGR0_PLLMUL(x) ((x - 2UL) << 18) /* 0 -> 16 */
#define RCC_CFGR0_PLLSRC_HSE BIT(16)
#define RCC_CFGR0_SW_MASK (0b11UL)
#define RCC_CFGR0_SW_HSI (0b00UL)
#define RCC_CFGR0_SW_HSE (0b01UL)
#define RCC_CFGR0_SW_PLL (0b10UL)
	void *base = RCC_BASE;
	u32 reg;

	/* switch back to HSI just in case */
	reg = read32(base + RCC_CFGR0);
	reg &= ~RCC_CFGR0_SW_MASK;
	reg |= RCC_CFGR0_SW_HSI;
	write32(base + RCC_CFGR0, reg);

	reg = read32(base + RCC_CTLR);
	write32(base + RCC_CTLR, reg & ~RCC_CTLR_PLLON); /* turn PLL off */

	/* directly use HSI (without division by 2) as PLL input */
	reg = read32(EXTEND_CTR);
	reg |= EXTEND_CTR_HSIPRE;
	write32(EXTEND_CTR, reg);

	reg = read32(base + RCC_CFGR0);
	reg &= ~RCC_CFGR0_PLLMUL_MASK;
	reg |= RCC_CFGR0_PLLMUL(6); /* HSI @ 8MHz * 6 gives 48MHz */
	reg |= RCC_CFGR0_USBPRE_1X;
	write32(base + RCC_CFGR0, reg);

	reg = read32(base + RCC_CTLR);
	write32(base + RCC_CTLR, reg | RCC_CTLR_PLLON);
	while (!(read32(base + RCC_CTLR) & RCC_CTLR_PLLRDY));

	/* switch to PLL */
	reg = read32(base + RCC_CFGR0);
	reg &= ~RCC_CFGR0_SW_MASK;
	reg |= RCC_CFGR0_SW_PLL;
	write32(base + RCC_CFGR0, reg);
}

#include "layout.h"

static void
key_init(void)
{
	struct gpio *g;
	u8 i;

	for (i = 0, g = gpio_row; i < LEN(gpio_row); i++, g++) {
		gpio_cfg_pin(g->port, g->pin_nr, GPIO_CFG_INPUT); /* hi-z */
		gpio_set_val(g->port, g->pin_nr, 1);
	}

	for (i = 0, g = gpio_col; i < LEN(gpio_col); i++, g++) {
		gpio_cfg_pin(g->port, g->pin_nr, GPIO_CFG_INPUT_PUPD);
		gpio_set_val(g->port, g->pin_nr, 0);
	}
}

static void
led_init(void)
{
	struct gpio *g;
	u8 i;

	for (i = 0, g = gpio_col; i < LEN(gpio_led); i++, g++) {
		gpio_cfg_pin(g->port, g->pin_nr, GPIO_CFG_INPUT);
		gpio_set_val(g->port, g->pin_nr, 0);
	}
}

static void
key_scan(void)
{
	struct gpio *r;
	struct gpio *c;
	u8 val[LEN(gpio_col)];
	u8 i, j, n;

	for (i = 0, r = gpio_row; i < LEN(gpio_row); i++, r++) {
		/* drive high */
		gpio_cfg_pin(r->port, r->pin_nr, GPIO_CFG_OUTPUT_10MH);

		memset(val, 0, sizeof(val));

		for (n = 0; n < 4; n++) /* debounce */
			for (j = 0, c = gpio_col; j < LEN(gpio_col); j++, c++)
				val[j] += gpio_get_val(c->port, c->pin_nr);

		for (j = 0, c = gpio_col; j < LEN(gpio_col); j++, c++)
			key[i][j] = val[j] == 4;

		/* hi-z */
		gpio_cfg_pin(r->port, r->pin_nr, GPIO_CFG_INPUT);

		/* drive cols to low */
		for (j = 0, c = gpio_col; j < LEN(gpio_col); j++, c++)
			gpio_cfg_pin(c->port, c->pin_nr, GPIO_CFG_OUTPUT_10MH);
		for (j = 0, c = gpio_col; j < LEN(gpio_col); j++, c++)
			gpio_cfg_pin(c->port, c->pin_nr, GPIO_CFG_INPUT_PUPD);
	}
}

static void
fill_report(struct keyboard_boot_report *r)
{
	u8 i, j;
	u8 n = 0;
	u8 f = 0;
	u8 k;

	memset(r, 0, sizeof(r));

	for (i = 0; i < LEN(key); i++) {
		for (j = 0; j < LEN(key[0]); j++) {
			if (layer_base[i][j] >= LAY_FUNC && key[i][j])
				f = 1;
		}
	}

	for (i = 0; i < LEN(key); i++) {
		for (j = 0; j < LEN(key[0]); j++) {
			if (key[i][j] == 0)
				continue;
			k = 0;
			if (f == 1)
				k = layer_func[i][j];
			if (k == 0)
				k = layer_base[i][j];
			if (k < KEY_LCTRL) {
				if (n == LEN(r->keycodes)) {
					memset(r->keycodes, KEY_ERR, LEN(r->keycodes));
					break;
				}

				r->keycodes[n++] = k;
			} else {
				u8 off = k - KEY_LCTRL;
				r->modifier |= BIT(off);
			}
		}
	}
}

void main(void)
{
	struct keyboard_boot_report rep = { 0 };
	struct keyboard_boot_report new;

	clock_init();
	systick_init();
	rcc_enable(RCC_APB2PCENR, RCC_APB2_GPIOA_EN | RCC_APB2_GPIOB_EN);
	key_init();
	led_init();
	usart_init();

	puts("--------------------------------\r\n");

	ext_conf_usb_gpio();
	ext_conf_usb_vbus(5);

	rcc_enable(RCC_AHBPCENR, BIT(12));
	usb_init();
	while (1) {
		key_scan();

		fill_report(&new);
		if (memcmp(&rep, &new, sizeof(rep))) {
			memcpy(&rep, &new, sizeof(rep));
			usb_ep_xfer(1, &rep, sizeof(rep));
			printf("report %x [%x %x %x %x %x %x]\r\n", rep.modifier,
			       rep.keycodes[0], rep.keycodes[1], rep.keycodes[2],
			       rep.keycodes[3], rep.keycodes[4], rep.keycodes[5]);
		}

		usb_poll();
	}
}
