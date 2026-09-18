# ==============================================================
#  Makefile (top-level only)
#  Target: Raspberry Pi 4B / AArch64 baremetal
#  Supports: Linux / macOS / WSL2
#
#  Usage:
#    make
#    make run
#    make clean
#    make dump
# ==============================================================

ARCH ?= arm64
TARGET := kernel8
STORAGE_WRITE_TEST ?= 0

# --------------------------------------------------------------
# OS detection
# --------------------------------------------------------------
HOST_OS := $(shell uname -s)

# --------------------------------------------------------------
# Toolchain
# --------------------------------------------------------------
CROSS_COMPILE ?= aarch64-elf-

CC      := $(CROSS_COMPILE)gcc
AS      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
NM      := $(CROSS_COMPILE)nm

HOSTCC := cc
HOST_TEST_CFLAGS := -std=c11 -Wall -Wextra -Werror -Iinclude

# --------------------------------------------------------------
# Directories
# --------------------------------------------------------------
BUILD_DIR := build/$(ARCH)
IMAGE_DIR := image/$(ARCH)

SOURCE_DIRS := \
	boot/$(ARCH) \
	arch/$(ARCH) \
	kernel \
	block \
	drivers \
	fs \
	usr/shell \
	lib

INCLUDE_DIRS := \
	include \
	kernel \
	arch/$(ARCH) \
	drivers \
	fs \
	lib

LINKER_SCRIPT := arch/$(ARCH)/boot/linker.ld

# --------------------------------------------------------------
# Flags
# --------------------------------------------------------------
INCLUDES := $(addprefix -I,$(INCLUDE_DIRS))

CFLAGS := \
	-std=gnu11 \
	-Wall \
	-Wextra \
	-Werror=implicit-function-declaration \
	-ffreestanding \
	-fno-builtin \
	-fno-stack-protector \
	-fno-pic \
	-mgeneral-regs-only \
	-mstrict-align \
	-O2 \
	-g \
	-DSTORAGE_WRITE_TEST=$(STORAGE_WRITE_TEST) \
	$(INCLUDES)

ASFLAGS := \
	-ffreestanding \
	-g \
	$(INCLUDES)

LDFLAGS := \
	-T $(LINKER_SCRIPT) \
	-nostdlib

# --------------------------------------------------------------
# Source discovery
# --------------------------------------------------------------
C_SRCS := $(shell find $(SOURCE_DIRS) -type f -name '*.c' 2>/dev/null)
C_SRCS += usr/bin/demo.c
C_SRCS += usr/lib/fs_commands.c
C_SRCS += \
	usr/bin/basic.c \
	usr/bin/editor.c \
	usr/lib/apps_common.c \
	usr/lib/apps_runtime.c
S_SRCS := $(shell find $(SOURCE_DIRS) -type f -name '*.S' 2>/dev/null)

C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.c.o,$(C_SRCS))
S_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.S.o,$(S_SRCS))

OBJS := $(C_OBJS) $(S_OBJS)
DEPS := $(OBJS:.o=.d)

ELF := $(BUILD_DIR)/$(TARGET).elf
IMG := $(BUILD_DIR)/$(TARGET).img

# --------------------------------------------------------------
# Targets
# --------------------------------------------------------------
.PHONY: all
all: image
	@echo "\033[1;34m==== build done [ARCH=$(ARCH) HOST_OS=$(HOST_OS)] ====\033[0m"

.PHONY: image
image: $(IMG)
	@mkdir -p $(IMAGE_DIR)
	cp $(IMG) $(IMAGE_DIR)/kernel8.img

$(IMG): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(ELF): $(OBJS) $(LINKER_SCRIPT)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.S.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -MMD -MP -c $< -o $@

# --------------------------------------------------------------
# Debug
# --------------------------------------------------------------
.PHONY: dump
dump: $(ELF)
	$(OBJDUMP) -D $(ELF) > $(BUILD_DIR)/$(TARGET).dump

.PHONY: symbols
symbols: $(ELF)
	$(NM) -n $(ELF) > $(BUILD_DIR)/$(TARGET).sym

# --------------------------------------------------------------
# Run
# --------------------------------------------------------------
.PHONY: run
SD_IMAGE ?= $(IMAGE_DIR)/sd.img

run: all
	qemu-system-aarch64 \
		-M raspi4b \
		-m 2g \
		-kernel $(IMAGE_DIR)/kernel8.img \
		-drive if=sd,file=$(SD_IMAGE),format=raw \
		-dtb $(IMAGE_DIR)/bcm2711-rpi-4-b.dtb \
		-serial null \
		-serial stdio

# --------------------------------------------------------------
# Clean
# --------------------------------------------------------------
.PHONY: clean
clean:
	rm -rf build
	@echo "\033[1;34m==== clean done [ARCH=$(ARCH)] ====\033[0m"

.PHONY: test-block
test-block: build/host/block_test
	$<

build/host/block_test: tests/block_test.c block/block.c include/block/block.h
	@mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_TEST_CFLAGS) tests/block_test.c block/block.c -o $@

.PHONY: test-fat32
test-fat32: build/host/fat32_test
	@test -n "$(FAT32_TEST_IMAGE)" -a -n "$(FAT32_MBR_TEST_IMAGE)" || \
		(echo "set FAT32_TEST_IMAGE and FAT32_MBR_TEST_IMAGE"; exit 2)
	$< $(FAT32_TEST_IMAGE) $(FAT32_MBR_TEST_IMAGE)

build/host/fat32_test: tests/fat32_test.c block/block.c fs/fat32/fat32.c \
		include/block/block.h include/fs/fat32.h
	@mkdir -p $(dir $@)
	$(HOSTCC) $(HOST_TEST_CFLAGS) tests/fat32_test.c block/block.c \
		fs/fat32/fat32.c -o $@

-include $(DEPS)
