# SPDX-License-Identifier: GPL-2.0-only
-include config.mk

ifneq ($(CROSS_COMPILE),)
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
OBJCOPY=$(CROSS_COMPILE)objcopy
OBJDUMP=$(CROSS_COMPILE)objdump
SIZE=$(CROSS_COMPILE)size
endif

ARCH_CFLAGS=-march=rv32imac_zicsr -mabi=ilp32
CFLAGS=$(ARCH_CFLAGS) -fno-pie -fno-pic -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -Os -flto

hdr=common.h config.h layout.h hid.h hid_key.h usb.h
obj=start.o init.o string.o usb.o

all: ank.bin

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c $(hdr)
	$(CC) $(CFLAGS) -c -o $@ $<

ank.elf: $(obj) link.ld
	$(CC) -static -T link.ld $(CFLAGS) $(LDFLAGS) -o $@ $(obj)

ank.bin: ank.elf
	@$(SIZE) -G $^
	$(OBJCOPY) -O binary $^ $@
	@wc -c $@

flash: ank.bin
	wch-isp -rp flash ank.bin

clean:
	rm -f $(obj)

.PHONY: all clean flash
