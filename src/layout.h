struct gpio {
	void *port;
	u32 pin_nr;
};

enum {
	ROW_A,
	ROW_B,
	ROW_C,
	ROW_D,
	ROW_E,
};

struct gpio gpio_row[] = {
	[ROW_E] = { GPIOA_BASE, 0 },
	[ROW_D] = { GPIOB_BASE, 8 },
	[ROW_C] = { GPIOB_BASE, 7 },
	[ROW_B] = { GPIOB_BASE, 6 },
	[ROW_A] = { GPIOB_BASE, 5 },
};

struct gpio gpio_col[] = {
	[0]  = { GPIOA_BASE, 14 },
	[1]  = { GPIOA_BASE, 15 },
	[2]  = { GPIOB_BASE,  3 },
	[3]  = { GPIOB_BASE,  4 },
	[4]  = { GPIOA_BASE,  1 },
	[5]  = { GPIOA_BASE,  5 },
	[6]  = { GPIOA_BASE,  6 },
	[7]  = { GPIOA_BASE,  7 },
	[8]  = { GPIOB_BASE,  0 },
	[9]  = { GPIOB_BASE,  1 },
	[10] = { GPIOB_BASE,  2 },
	[11] = { GPIOB_BASE, 10 },
	[12] = { GPIOB_BASE, 11 },
};

struct gpio gpio_led[] = {
	[0]  = { GPIOA_BASE,  8 },
	[1]  = { GPIOA_BASE,  9 },
	[2]  = { GPIOA_BASE, 10 },
};

static u8 key[LEN(gpio_row)][LEN(gpio_col)];
static u8 layer_base[LEN(gpio_row)][LEN(gpio_col)] = {
	[ROW_E] = {KEY_GRAVE,  KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, 0, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS},
	[ROW_D] = {KEY_TAB,    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_MINUS, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEY_ENTER},
	[ROW_C] = {0,          KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_EQUAL, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_ENTER},
	[ROW_B] = {KEY_LSHIFT, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_APOSTROPHE, KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_RSHIFT},
	[ROW_A] = {KEY_LCTRL,  LAY_FUNC, KEY_LGUI, KEY_LALT, KEY_SPACE, KEY_SPACE, LAY_FUNC, KEY_BACKSPACE, KEY_BACKSPACE, KEY_RALT, KEY_RGUI, LAY_FUNC, KEY_RCTRL},
};

static u8 layer_func[LEN(gpio_row)][LEN(gpio_col)] = {
	[ROW_E] = {KEY_ESCAPE, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,          0, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_BACKSPACE},
	[ROW_D] = {         0,      0,      0,      0,      0,      0, KEY_INSERT, KEY_HOME, KEY_PAGEDOWN, KEY_PAGEUP, KEY_END, 0, 0},
	[ROW_C] = {         0,      0,      0,      0,      0,      0, KEY_DELETE, KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT, 0, 0},
	[ROW_B] = {         0,      0,      0,      0,      0,      0, KEY_ESCAPE, },
	[ROW_A] = {         0,      0,      0,      0,      0, KEY_INSERT,      0, KEY_DELETE},
};
