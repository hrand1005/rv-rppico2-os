CC = riscv32-unknown-elf-gcc
AS = riscv32-unknown-elf-as
LD = riscv32-unknown-elf-ld
GDB = riscv32-unknown-elf-gdb

TARGET = main.elf
SRCS = startup.S main.c
OBJS = startup.o
MEMMAP = memmap.ld

ARCHFLAGS = -mabi=ilp32 -misa-spec=20191213 \
		 -march=rv32ima_zicsr_zifencei_zba_zbb_zbkb_zbs_zca_zcb_zcmp

CFLAGS = $(ARCHFLAGS) -g -nostdlib -nodefaultlibs
ASFLAGS = $(ARCHFLAGS) -g -mpriv-spec=1.12
LDFLAGS = -T $(MEMMAP) -e _entry_point -Wl,--no-warn-rwx-segments

.DEFAULT_GOAL := build

all: build

run: $(TARGET)
	$(GDB) $(TARGET) -x init.gdb

console:
	openocd -s tcl \
		-f interface/cmsis-dap.cfg \
		-f target/rp2350-riscv.cfg \
		-c "adapter speed 5000"

check: $(TARGET)
	@echo Using openocd to flash and verify $(TARGET)...
	openocd -s tcl -f interface/cmsis-dap.cfg -f target/rp2350-riscv.cfg \
		-c "adapter speed 5000" -c "program $(TARGET) verify reset exit"

build: $(TARGET)

$(TARGET): $(OBJS) $(MEMMAP) main.c
	@echo final cmd: $(CC) $(LDFLAGS) -o $(TARGET) $(OBJS) main.c

	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) main.c

%.o: %.S
	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all run check build clean
