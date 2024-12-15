APP ?= blinky
TARGET := build/$(APP).elf

MEMMAP := util/memmap.ld

CC := riscv32-unknown-elf-gcc
AS := riscv32-unknown-elf-as
LD := riscv32-unknown-elf-ld
GDB := riscv32-unknown-elf-gdb

ARCHFLAGS = -mabi=ilp32 -misa-spec=20191213 \
		 -march=rv32ima_zicsr_zifencei_zba_zbb_zbkb_zbs_zca_zcb_zcmp

CFLAGS = $(ARCHFLAGS) -g -nostdlib -nodefaultlibs
ASFLAGS = $(ARCHFLAGS) -g -mpriv-spec=1.12
LDFLAGS = -T $(MEMMAP) -e _entry_point -Wl,--no-warn-rwx-segments

BUILD_DIR := build
DOCS_DIR := docs

KERNEL_DIR := kernel
KERNEL_SRCS := $(wildcard $(KERNEL_DIR)/*.c $(KERNEL_DIR)/*.S)
KERNEL_OBJ := $(BUILD_DIR)/kernel.o

USER_DIR := user/$(APP)
USER_SRCS := $(wildcard $(USER_DIR)/*.c $(USER_DIR)/*.S)
USER_OBJ := $(BUILD_DIR)/user_$(APP).o

GDB_TEMPLATE := util/gdb_template

.DEFAULT_GOAL := compile

run: $(TARGET) $(GDB_TEMPLATE)
	sed "s|<PROGRAM>|$(TARGET)|" $(GDB_TEMPLATE) > init.gdb
	$(GDB) $(TARGET) -x init.gdb

compile: $(TARGET)

$(TARGET): $(KERNEL_OBJ) $(USER_OBJ) $(MEMMAP) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET)  $(KERNEL_OBJ) $(USER_OBJ)

$(KERNEL_OBJ): $(KERNEL_SRCS) $(KERNEL_INCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I $(KERNEL_DIR) -c -o $@ $(KERNEL_SRCS)

$(USER_OBJ): $(USER_SRCS) $(USER_INCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I $(USER_DIR) -c -o $@ $(USER_SRCS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

console:
	openocd -s tcl \
		-f interface/cmsis-dap.cfg \
		-f target/rp2350-riscv.cfg \
		-c "adapter speed 5000"

check: $(TARGET)
	@echo Using openocd to flash and verify $(TARGET)...
	openocd -s tcl -f interface/cmsis-dap.cfg -f target/rp2350-riscv.cfg \
		-c "adapter speed 5000" -c "program $(TARGET) verify reset exit"

docs:
	@mkdir -p docs
	doxygen util/Doxyfile

clean:
	rm -rf build/* docs/html/* docs/latex/* init.gdb

.PHONY: docs run compile console check clean
