# ==== toolchain ====
CC      := gcc
LD      := ld

CFLAGS  := -std=gnu11 -ffreestanding -fno-stack-protector -fno-stack-check \
           -fno-pic -fno-pie -m64 -mabi=sysv -mno-red-zone -mcmodel=kernel \
           -O2 -Wall -Wextra

LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -T linker.ld

# ==== outputs ====
BIN     := bin/myos
OBJDIR  := obj
SRCDIR  := src

# 把 fb_console.c 编进来！
OBJS    := $(OBJDIR)/kernel.o $(OBJDIR)/fb_console.o

all: $(BIN)

# ==== compile ====
$(OBJDIR)/kernel.o: $(SRCDIR)/kernel.c $(SRCDIR)/limine.h $(SRCDIR)/fb_console.h
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/fb_console.o: $(SRCDIR)/fb_console.c $(SRCDIR)/fb_console.h $(SRCDIR)/limine.h
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ==== link ====
$(BIN): $(OBJS) linker.ld
	mkdir -p bin
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# ==== make ISO ====
iso: $(BIN)
	# 准备 ISO 根目录
	mkdir -p iso_root/boot/limine iso_root/EFI/BOOT

	# 拷贝内核与 limine.conf
	cp -v $(BIN) iso_root/boot/myos
	cp -v limine.conf iso_root/

	# 拷贝 Limine 所需文件
	cp -v limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin \
	      iso_root/boot/limine/
	cp -v limine/BOOTX64.EFI iso_root/EFI/BOOT/

	# 生成支持 UEFI 的可启动 ISO
	xorriso -as mkisofs -R -r -J \
	  -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
	  -hfsplus -apm-block-size 2048 \
	  --efi-boot boot/limine/limine-uefi-cd.bin \
	  -efi-boot-part --efi-boot-image --protective-msdos-label \
	  iso_root -o image.iso

	# 安装 BIOS 阶段（UEFI 启动不必须）
	./limine/limine bios-install image.iso

# ==== run with QEMU (WSLg) ====
run: iso
	cp -f /usr/share/OVMF/OVMF_VARS_4M.fd ./OVMF_VARS.fd
	LIBGL_ALWAYS_SOFTWARE=1 \
	qemu-system-x86_64 \
	  -machine q35 -cpu max -m 512 \
	  -drive if=pflash,format=raw,unit=0,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
	  -drive if=pflash,format=raw,unit=1,file=./OVMF_VARS.fd \
	  -cdrom image.iso \
	  -boot d \
	  -no-reboot -no-shutdown \
	  -debugcon stdio \
	  -display gtk,gl=off

# ==== clean ====
clean:
	rm -rf $(OBJDIR) bin image.iso OVMF_VARS.fd
	rm -rf iso_root/*

.PHONY: all iso run clean
