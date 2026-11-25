# Optional command-line verification build using the toolchain's standard C runtime.
# For normal development, generate a CubeIDE project and copy the five user modules.
TARGET := hbridge_f303ze
BUILD := build
CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE := $(CROSS_COMPILE)size
PYTHON ?= python3
NEWLIB_INCLUDE ?=
CPU := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS := -DSTM32F303xE -DUSE_HAL_DRIVER
INCLUDES := -ICore/Inc -IDrivers/STM32F3xx_HAL_Driver/Inc \
 -IDrivers/CMSIS/Device/ST/STM32F3xx/Include -IDrivers/CMSIS/Include
ifneq ($(NEWLIB_INCLUDE),)
INCLUDES += -isystem $(NEWLIB_INCLUDE)
endif
CFLAGS := $(CPU) $(DEFS) $(INCLUDES) -std=c11 -Os -g3 -Wall -Wextra \
 -ffunction-sections -fdata-sections -MMD -MP
ASFLAGS := $(CPU) $(DEFS) $(INCLUDES) -x assembler-with-cpp -MMD -MP
LDFLAGS := $(CPU) --specs=nano.specs --specs=nosys.specs -static -TSTM32F303ZETX_FLASH.ld \
 -Wl,--gc-sections,-Map=$(BUILD)/$(TARGET).map -Wl,--print-memory-usage
HAL_SRC := $(addprefix Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_,\
 hal.c hal_adc.c hal_adc_ex.c hal_cortex.c hal_dma.c hal_gpio.c \
 hal_flash.c hal_flash_ex.c hal_pwr.c hal_pwr_ex.c hal_rcc.c hal_rcc_ex.c \
 hal_tim.c hal_tim_ex.c)
APP_SRC := $(sort $(wildcard Core/Src/*.c) Core/Src/system_stm32f3xx.c)
C_SRC := $(APP_SRC) $(HAL_SRC)
ASM_SRC := Core/Startup/startup_stm32f303xe.s
OBJECTS := $(addprefix $(BUILD)/obj/,$(C_SRC:.c=.o) $(ASM_SRC:.s=.o))

.PHONY: all vendor clean size
all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin
# Rebuild when compiler/runtime flags change, including updates from older builds.
$(OBJECTS): Makefile
vendor:
	$(PYTHON) scripts/fetch_vendor.py
$(HAL_SRC) Core/Src/system_stm32f3xx.c $(ASM_SRC): | vendor
	@test -f $@
$(BUILD)/obj/%.o: %.c | vendor
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/obj/%.o: %.s | vendor
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -c $< -o $@
$(BUILD)/$(TARGET).elf: $(OBJECTS) STM32F303ZETX_FLASH.ld Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -Wl,--start-group -lc -lm -Wl,--end-group -o $@
	$(SIZE) $@
$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@
$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
size: $(BUILD)/$(TARGET).elf
	$(SIZE) $<
clean:
	rm -rf $(BUILD)
-include $(OBJECTS:.o=.d)
