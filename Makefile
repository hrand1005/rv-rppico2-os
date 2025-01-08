CC := riscv32-unknown-elf-gcc
AS := riscv32-unknown-elf-as
LD := riscv32-unknown-elf-ld
GDB := riscv32-unknown-elf-gdb

MEMMAP := util/memmap.ld

APP ?= $(if $(TEST),,blinky)
TARGET := $(if $(TEST),build/$(TEST).elf,build/$(APP).elf)

BUILD_DIR := build
DOCS_DIR := docs
# NOTE: only for definitions common to kernel and user code
INCLUDE_DIR := include

KERNEL_DIR := kernel
KERNEL_SRCS := $(wildcard $(KERNEL_DIR)/*.c $(KERNEL_DIR)/*.S)
# NOTE: compile separately for tests due to CPP directives 
KERNEL_OBJ := $(BUILD_DIR)/kernel$(if $(TEST),_test,).o

USER_DIR := user
USER_SRCS := $(wildcard $(USER_DIR)/*.c $(USER_DIR)/*.S)

# NOTE: The compiled program may be a test or an application
PROGRAM_DIR := $(if $(TEST),test/$(TEST),apps/$(APP))
PROGRAM_SRCS := $(wildcard $(PROGRAM_DIR)/*.c $(PROGRAM_DIR)/*.S)
PROGRAM_OBJ := $(BUILD_DIR)/$(if $(TEST),test_$(TEST),user_$(APP)).o

GDB_TEMPLATE := util/gdb_template

ARCHFLAGS = -mabi=ilp32 -misa-spec=20191213 \
		 -march=rv32ima_zicsr_zifencei_zba_zbb_zbkb_zbs_zca_zcb_zcmp

# tests get IS_TEST flag and kernel libraries
CFLAGS = $(ARCHFLAGS) -g -nostdlib -nodefaultlibs -I $(INCLUDE_DIR) \
		 $(if $(TEST),-DIS_TEST -I $(KERNEL_DIR),)
ASFLAGS = $(ARCHFLAGS) -g -mpriv-spec=1.12
LDFLAGS = -T $(MEMMAP) -e _entry_point -Wl,--no-warn-rwx-segments

.DEFAULT_GOAL := run

run: $(GDB_TEMPLATE)
	@if [ -n "$(TEST)" ] && [ -n "$(APP)" ]; then \
		echo "Error: Cannot specify both APP and TEST"; \
		exit 1; \
	fi
	@echo "Running $(if $(TEST),test $(TEST),application $(APP))..."
	make compile
	sed "s|<PROGRAM>|$(TARGET)|" $(GDB_TEMPLATE) > init.gdb
	$(GDB) $(TARGET) -x init.gdb

compile: $(TARGET)

$(TARGET): $(KERNEL_OBJ) $(PROGRAM_OBJ) $(MEMMAP) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(KERNEL_OBJ) $(PROGRAM_OBJ)

$(KERNEL_OBJ): $(KERNEL_SRCS) | $(BUILD_DIR)
	# TODO: is '-r' what we want here?
	$(CC) $(CFLAGS) -I $(KERNEL_DIR) -r -o $@ $(KERNEL_SRCS)

$(PROGRAM_OBJ): $(PROGRAM_SRCS) $(USER_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I $(PROGRAM_DIR) -I $(USER_DIR) -r -o $@ $(PROGRAM_SRCS) $(USER_SRCS)

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
	@mkdir -p $(DOCS_DIR)
	doxygen util/Doxyfile

format:
	find . -name "*.c" -o -name "*.h" | xargs clang-format -i

clean:
	rm -rf $(BUILD_DIR)/* $(DOCS_DIR)/html/* $(DOCS_DIR)/latex/* init.gdb

.PHONY: run compile console check docs format clean
