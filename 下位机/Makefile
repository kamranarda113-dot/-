TARGET = oscilloscope
CPU = cortex-m4
MCU = STM32F407xx

PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
LD = $(PREFIX)ld

OPENOCD = openocd
OPENOCD_SCRIPTS ?= C:/Users/Rose/Downloads/openocd-20250710/OpenOCD-20250710-0.12.0/share/openocd/scripts
OPENOCD_CFG = stm32f407.cfg

CFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += -D$(MCU) -DUSE_HAL_DRIVER
CFLAGS += -I./CORE -I./USER -I./HALLIB/STM32F4xx_HAL_Driver/Inc -I./HALLIB/STM32F4xx_HAL_Driver/Inc/Legacy -I./SYSTEM/sys -I./SYSTEM/delay -I./SYSTEM/usart -I./HARDWARE/KEY -I./HARDWARE/LED -I./HARDWARE/ADC -I./HARDWARE/DAC
CFLAGS += -O0 -g3 -Wall -fmessage-length=0 -ffunction-sections -fdata-sections
CFLAGS += -MMD -MP

LDFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
LDFLAGS += -T./STM32F407ZGTX_FLASH.ld -Wl,--gc-sections -Wl,-Map=$(TARGET).map

SRCS = \
    CORE/startup_stm32f407xx_gcc.s \
    USER/main.c \
    USER/stm32f4xx_it.c \
    USER/system_stm32f4xx.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dac.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dac_ex.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
    HALLIB/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
    SYSTEM/sys/sys.c \
    SYSTEM/delay/delay.c \
    SYSTEM/usart/usart.c \
    HARDWARE/LED/led.c \
    HARDWARE/KEY/key.c \
    HARDWARE/ADC/adc.c \
    HARDWARE/DAC/dac.c

OBJS := $(SRCS:.s=.o)
OBJS := $(OBJS:.c=.o)

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(AS) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@
	$(SZ) $@

$(TARGET).hex: $(TARGET).elf
	$(CP) -O ihex $< $@

$(TARGET).bin: $(TARGET).elf
	$(CP) -O binary $< $@

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET).elf $(TARGET).hex $(TARGET).bin $(TARGET).map

flash: $(TARGET).hex
	$(OPENOCD) -s "$(OPENOCD_SCRIPTS)" -f $(OPENOCD_CFG) -c "program $(TARGET).hex verify reset exit"

debug:
	$(OPENOCD) -s "$(OPENOCD_SCRIPTS)" -f $(OPENOCD_CFG)
