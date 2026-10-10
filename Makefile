# WCH GCC12, as supplied with MounRiver Studio. WCH_BIN may contain spaces.
WCH_BIN ?=
TOOL_PREFIX = $(if $(WCH_BIN),$(WCH_BIN)/,)riscv-wch-elf
TARGET = build/calculator
C_SOURCES = $(sort $(wildcard User/*.c)) $(sort $(wildcard Peripheral/src/*.c)) Debug/debug.c Core/core_riscv.c
OBJECTS = $(patsubst %.c,build/%.o,$(sort $(wildcard User/*.c))) build/Startup/startup_ch32v00X.o $(patsubst %.c,build/%.o,$(sort $(wildcard Peripheral/src/*.c))) build/Debug/debug.o build/Core/core_riscv.o
ARCH = -march=rv32ec_zmmul_xw -mabi=ilp32e -msmall-data-limit=0 -msave-restore
CFLAGS = $(ARCH) -Os -g -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -std=gnu99
INCLUDES = -IDebug -ICore -IUser -IPeripheral/inc
LDFLAGS = $(ARCH) -Os -g -T Ld/Link.ld -nostartfiles -Wl,--gc-sections,-Map,$(TARGET).map --specs=nano.specs --specs=nosys.specs
.PHONY: all size
all: $(TARGET).bin $(TARGET).hex
build/%.o: %.c
	@mkdir -p "$(@D)"
	"$(TOOL_PREFIX)-gcc" $(CFLAGS) $(INCLUDES) -MMD -MP -c "$<" -o "$@"
build/%.o: %.S
	@mkdir -p "$(@D)"
	"$(TOOL_PREFIX)-gcc" $(ARCH) -Os -g -x assembler-with-cpp -MMD -MP -c "$<" -o "$@"
$(TARGET).elf: $(OBJECTS) Ld/Link.ld Makefile
	"$(TOOL_PREFIX)-gcc" $(LDFLAGS) -o "$@" $(OBJECTS) -lm -lprintf
$(TARGET).bin: $(TARGET).elf
	"$(TOOL_PREFIX)-objcopy" -O binary "$<" "$@"
$(TARGET).hex: $(TARGET).elf
	"$(TOOL_PREFIX)-objcopy" -O ihex "$<" "$@"
size: $(TARGET).elf
	"$(TOOL_PREFIX)-size" "$<"
-include $(OBJECTS:.o=.d)
