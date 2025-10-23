CC      := gcc
LD      := ld

CFLAGS  := -std=gnu11 -ffreestanding -fno-stack-protector -fno-stack-check \
           -fno-pic -fno-pie -m64 -mabi=sysv -mno-red-zone -mcmodel=kernel \
           -O2 -Wall -Wextra

LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -T linker.ld

BIN     := bin/myos

all: $(BIN)

obj/kernel.o: src/kernel.c src/limine.h
	mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): obj/kernel.o linker.ld
	mkdir -p bin
	$(LD) $(LDFLAGS) -o $@ $<

iso: all
	# 准备 ISO 目录
	mkdir -p iso_root/boot/limine iso_root/EFI/BOOT

	# 拷贝内核与配置
	cp -v $(BIN) iso_root/boot/myos
	cp -v limine.conf iso_root/

	# 拷贝 Limine 所需文件（UEFI+BIOS 均拷贝，兼容面更广）
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

	# 安装 BIOS 阶段（可选，主要为传统 BIOS 启动兼容；UEFI 不依赖这步）
	./limine/limine bios-install image.iso

run: iso
	# 复制一份可写的 VARS（不复制也能跑，只是不保存 NVRAM）
	cp -f /usr/share/OVMF/OVMF_VARS_4M.fd ./OVMF_VARS.fd
	qemu-system-x86_64 \
	  -machine q35 -cpu max -m 512 \
	  -drive if=pflash,format=raw,unit=0,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
	  -drive if=pflash,format=raw,unit=1,file=./OVMF_VARS.fd \
	  -cdrom image.iso \
	  -boot d \
	  -debugcon stdio \
	  -display gtk,gl=off

clean:
	rm -rf obj bin iso_root/image.iso image.iso OVMF_VARS.fd

.PHONY: all iso run clean
