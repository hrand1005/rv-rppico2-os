CC := riscv32-unknown-elf-gcc
AS := riscv32-unknown-elf-as
LD := riscv32-unknown-elf-ld
GDB := riscv32-unknown-elf-gdb

MEMMAP := memmap.ld

BUILD_DIR := build
DOCS_DIR := docs
# NOTE: only for definitions common to kernel and user code
INCLUDE_DIR := include
TARGET_DIR := $(BUILD_DIR)/bin

APP ?= $(if $(TEST),,blinky)
TARGET := $(if $(TEST),$(TARGET_DIR)/$(TEST).elf,$(TARGET_DIR)/$(APP).elf)

KERNEL_DIR := kernel
KERNEL_C_SRCS := $(wildcard $(KERNEL_DIR)/*.c)
KERNEL_ASM_SRCS := $(wildcard $(KERNEL_DIR)/*.S)
# NOTE: compile separately for tests due to CPP directives 
KERNEL_BUILD_DIR := $(BUILD_DIR)/kernel$(if $(TEST),_test,)
KERNEL_OBJS := $(KERNEL_C_SRCS:$(KERNEL_DIR)/%.c=$(KERNEL_BUILD_DIR)/%.o) \
			   $(KERNEL_ASM_SRCS:$(KERNEL_DIR)/%.S=$(KERNEL_BUILD_DIR)/%.o)

USER_DIR := user
USER_C_SRCS := $(wildcard $(USER_DIR)/*.c)
USER_ASM_SRCS := $(wildcard $(USER_DIR)/*.S)
USER_BUILD_DIR := $(BUILD_DIR)/$(USER_DIR)
USER_OBJS := $(USER_C_SRCS:$(USER_DIR)/%.c=$(USER_BUILD_DIR)/%.o) \
			 $(USER_ASM_SRCS:$(USER_DIR)/%.S=$(USER_BUILD_DIR)/%.o) 

# NOTE: The compiled program may be a test or an application
PROGRAM_DIR := $(if $(TEST),test/$(TEST),apps/$(APP))
PROGRAM_C_SRCS := $(wildcard $(PROGRAM_DIR)/*.c)
PROGRAM_ASM_SRCS := $(wildcard $(PROGRAM_DIR)/*.S)
PROGRAM_BUILD_DIR := $(BUILD_DIR)/$(PROGRAM_DIR)
PROGRAM_OBJS := $(PROGRAM_C_SRCS:$(PROGRAM_DIR)/%.c=$(PROGRAM_BUILD_DIR)/%.o) \
			 $(PROGRAM_ASM_SRCS:$(PROGRAM_DIR)/%.S=$(PROGRAM_BUILD_DIR)/%.o) 

GDB_TEMPLATE := util/gdb_template
MEMMAP_TEMPLATE := util/memmap_template

ARCHFLAGS = -mabi=ilp32 -misa-spec=20191213 \
		 -march=rv32ima_zicsr_zifencei_zba_zbb_zbkb_zbs_zca_zcb_zcmp

# tests get IS_TEST flag and kernel libraries
CFLAGS = $(ARCHFLAGS) -g -nostdlib -nodefaultlibs -I $(INCLUDE_DIR) \
		 $(if $(TEST),-DIS_TEST -I $(KERNEL_DIR),)
ASFLAGS = $(ARCHFLAGS) -g -mpriv-spec=1.12
LDFLAGS = -T $(MEMMAP) -e _entry_point -Wl,--no-warn-rwx-segments

.DEFAULT_GOAL := run

run: $(GDB_TEMPLATE) $(MEMMAP_TEMPLATE)
	@if [ -n "$(TEST)" ] && [ -n "$(APP)" ]; then \
		echo "Error: Cannot specify both APP and TEST"; \
		exit 1; \
	fi
	@echo "Running $(if $(TEST),test $(TEST),application $(APP))..."
	@sed "s|<PROGRAM>|$(TARGET)|" $(GDB_TEMPLATE) > init.gdb
	make compile
	$(GDB) $(TARGET) -x init.gdb

compile: $(TARGET)

$(TARGET): $(KERNEL_OBJS) $(USER_OBJS) $(PROGRAM_OBJS)
	@mkdir -p $(TARGET_DIR)
	@sed -e "s|<KERNEL_BUILD_DIR>|$(KERNEL_BUILD_DIR)|g" \
		 -e "s|<USER_BUILD_DIR>|$(USER_BUILD_DIR)|g" \
		 -e "s|<PROGRAM_BUILD_DIR>|$(PROGRAM_BUILD_DIR)|g" \
		$(MEMMAP_TEMPLATE) > $(MEMMAP)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(KERNEL_OBJS) $(USER_OBJS) $(PROGRAM_OBJS)

$(PROGRAM_BUILD_DIR)/%.o: $(PROGRAM_DIR)/%.S
	@mkdir -p $(PROGRAM_BUILD_DIR)
	$(CC) $(CFLAGS) -I $(PROGRAM_DIR) -I $(USER_DIR) -c $< -o $@

$(PROGRAM_BUILD_DIR)/%.o: $(PROGRAM_DIR)/%.c
	@mkdir -p $(PROGRAM_BUILD_DIR)
	$(CC) $(CFLAGS) -I $(PROGRAM_DIR) -I $(USER_DIR) -c $< -o $@

$(USER_BUILD_DIR)/%.o: $(USER_DIR)/%.S
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(CFLAGS) -I $(USER_DIR) -c $< -o $@

$(USER_BUILD_DIR)/%.o: $(USER_DIR)/%.c
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(CFLAGS) -I $(USER_DIR) -c $< -o $@

$(KERNEL_BUILD_DIR)/%.o: $(KERNEL_DIR)/%.S
	@mkdir -p $(KERNEL_BUILD_DIR)
	$(CC) $(CFLAGS) -I $(KERNEL_DIR) -c $< -o $@

$(KERNEL_BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
	@mkdir -p $(KERNEL_BUILD_DIR)
	$(CC) $(CFLAGS) -I $(KERNEL_DIR) -c $< -o $@

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
	rm -rf $(BUILD_DIR)/* $(DOCS_DIR)/html/* $(DOCS_DIR)/latex/* \
		init.gdb $(MEMMAP)

.PHONY: run compile console check docs format clean
