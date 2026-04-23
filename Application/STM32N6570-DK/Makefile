######################################
# PupilCenter_N6 - classical CV bring-up build
#
# No NPU / NN / stai runtime. Classical CV pupil detection only. All
# pupil-specific code lives under Core/. The ST reference example's
# camera + LCD infrastructure is reused from Src/ and STM32Cube_FW_N6/.
######################################

V = 0
ifeq ($(V), 0)
  quiet = quiet_
else
  quiet =
endif
quiet_CC  = @echo "  CC $@"; $(CC)
quiet_LD  = @echo "  LD $@"; $(CC)
quiet_AS  = @echo "  AS $@"; $(AS)
quiet_SZ  = @echo "  SZ $@"; $(SZ)
quiet_HEX = @echo "  HEX $@"; $(HEX)
quiet_BIN = @echo "  BIN $@"; $(BIN)

rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

######################################
# target
######################################
TARGET      = PupilCenter_N6
BINARY_DIR  = Binary

OPT         = -Os -g3
BUILD_TOP   = build
BUILD_DIR   = $(BUILD_TOP)/Application/STM32N6570-DK

######################################
# source - application + board bring-up
######################################
C_SOURCES += Src/main.c
C_SOURCES += Src/syscalls.c
C_SOURCES += Src/app_fuseprogramming.c
C_SOURCES += Src/stm32_lcd_ex.c
C_SOURCES += Src/stm32n6xx_it.c
C_SOURCES += Src/crop_img.c
C_SOURCES += Src/app_camerapipeline.c
C_SOURCES += Src/bsp_uart.c
C_SOURCES += Src/bsp_lcd.c

# Our Core/ pupil-detection pipeline
C_SOURCES += Core/Src/app_main.c
C_SOURCES += Core/Src/camera_if.c
C_SOURCES += Core/Src/frame_mgr.c
C_SOURCES += Core/Src/roi_mgr.c
C_SOURCES += Core/Src/pupil_detect.c
C_SOURCES += Core/Src/pupil_filter.c
C_SOURCES += Core/Src/tracking_sm.c
C_SOURCES += Core/Src/result_output.c
C_SOURCES += Core/Src/perf_monitor.c
C_SOURCES += Core/Src/debug_overlay.c

# STM32Cube_FW_N6 HAL drivers - keep only what we actually use
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/CMSIS/Device/ST/STM32N6xx/Source/Templates/system_stm32n6xx_fsbl.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_cortex.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dcmipp.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma2d.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_gpio.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_i2c.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_i2c_ex.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ltdc.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ltdc_ex.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rif.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ramcfg.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_cacheaxi.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_pwr.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_pwr_ex.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rcc.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rcc_ex.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_xspi.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_bsec.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/BSP/STM32N6570-DK/stm32n6570_discovery.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/BSP/STM32N6570-DK/stm32n6570_discovery_bus.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/BSP/STM32N6570-DK/stm32n6570_discovery_lcd.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/BSP/STM32N6570-DK/stm32n6570_discovery_xspi.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/BSP/Components/aps256xx/aps256xx.c
C_SOURCES += ../../STM32Cube_FW_N6/Drivers/BSP/Components/mx66uw1g45g/mx66uw1g45g.c
C_SOURCES += ../../STM32Cube_FW_N6/Utilities/lcd/stm32_lcd.c

# Camera middleware
C_SOURCES += ../../Middlewares/stm32-mw-camera/cmw_camera.c
C_SOURCES += ../../Middlewares/stm32-mw-camera/cmw_utils.c
C_SOURCES += ../../Middlewares/stm32-mw-camera/sensors/imx335/imx335.c
C_SOURCES += ../../Middlewares/stm32-mw-camera/sensors/imx335/imx335_reg.c
C_SOURCES += ../../Middlewares/stm32-mw-camera/sensors/cmw_imx335.c
C_SOURCES += ../../Middlewares/stm32-mw-camera/ISP_Library/isp/Src/isp_algo.c
C_SOURCES += ../../Middlewares/stm32-mw-camera/ISP_Library/isp/Src/isp_awb_algo.c
C_SOURCES += ../../Middlewares/stm32-mw-camera/ISP_Library/isp/Src/isp_ae_algo.c
C_SOURCES += ../../Middlewares/stm32-mw-camera/ISP_Library/isp/Src/isp_cmd_parser.c
C_SOURCES += ../../Middlewares/stm32-mw-camera/ISP_Library/isp/Src/isp_core.c
C_SOURCES += ../../Middlewares/stm32-mw-camera/ISP_Library/isp/Src/isp_services.c
C_SOURCES += ../../Middlewares/stm32-mw-camera/ISP_Library/isp/Src/isp_tool_com.c

# ASM
ASM_SOURCES += ../../STM32Cube_FW_N6/Drivers/CMSIS/Device/ST/STM32N6xx/Source/Templates/gcc/startup_stm32n657xx_fsbl.s

######################################
# toolchain
######################################
PREFIX = arm-none-eabi-
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif
HEX     = $(CP) -O ihex
BIN     = $(CP) -O binary
FLASHER = STM32_Programmer_CLI
SIGNER  = STM32_SigningTool_CLI
EL      = "$(shell dirname "$(shell which $(FLASHER))")/ExternalLoader/MX66UW1G45G_STM32N6570-DK.stldr"

######################################
# CFLAGS
######################################
CPU = -mcpu=cortex-m55 -mcmse -mthumb
FPU = -mfpu=fpv5-d16 -mfloat-abi=hard
MCU = $(CPU) $(FPU)

C_DEFS += -DSTM32N657xx
C_DEFS += -DUSE_FULL_ASSERT
C_DEFS += -DUSE_FULL_LL_DRIVER
C_DEFS += -DVECT_TAB_SRAM

C_INCLUDES += -IInc
C_INCLUDES += -ICore/Inc
C_INCLUDES += -I../../Middlewares/stm32-mw-camera
C_INCLUDES += -I../../Middlewares/stm32-mw-camera/sensors
C_INCLUDES += -I../../Middlewares/stm32-mw-camera/sensors/imx335
C_INCLUDES += -I../../Middlewares/stm32-mw-camera/ISP_Library/isp/Inc
C_INCLUDES += -I../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Inc
C_INCLUDES += -I../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Inc/Legacy
C_INCLUDES += -I../../STM32Cube_FW_N6/Drivers/CMSIS/Device/ST/STM32N6xx/Include
C_INCLUDES += -I../../STM32Cube_FW_N6/Drivers/CMSIS/Include
C_INCLUDES += -I../../STM32Cube_FW_N6/Drivers/CMSIS/DSP/Include
C_INCLUDES += -I../../STM32Cube_FW_N6/Drivers/BSP/Components/Common
C_INCLUDES += -I../../STM32Cube_FW_N6/Drivers/BSP/STM32N6570-DK
C_INCLUDES += -I../../STM32Cube_FW_N6/Drivers/BSP/Components/aps256xx
C_INCLUDES += -I../../STM32Cube_FW_N6/Utilities/lcd

ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fstack-usage -fdata-sections -ffunction-sections -fcyclomatic-complexity
CFLAGS  = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fstack-usage -fdata-sections -ffunction-sections -fcyclomatic-complexity
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"
CFLAGS += -std=gnu11

######################################
# LDFLAGS
######################################
LDSCRIPT = STM32CubeIDE/STM32N657xx.ld
LIBS     = -lc -lm -lnosys
LDFLAGS  = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections
LDFLAGS += -u _printf_float
LDFLAGS += -Wl,--print-memory-usage
LDFLAGS += -Wl,--no-warn-rwx-segments

######################################
# rules
######################################
.PHONY: all
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin

.PHONY: sign
sign: $(BUILD_DIR)/$(TARGET)_sign.hex

OBJECTS  = $(addprefix $(BUILD_DIR)/, $(C_SOURCES:.c=.o))
OBJECTS += $(addprefix $(BUILD_DIR)/, $(ASM_SOURCES:.s=.o))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$($(quiet)CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$($(quiet)AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).list: $(OBJECTS)
	$(file > $@, $(OBJECTS))

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile $(BUILD_DIR)/$(TARGET).list
	$($(quiet)LD) @$(BUILD_DIR)/$(TARGET).list $(LDFLAGS) -o $@
	$($(quiet)SZ) $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR)/$(TARGET)_sign.bin: $(BUILD_DIR)/$(TARGET).bin
	$(SIGNER) -s -bin $< -nk -t ssbl -hv 2.3 -o $(BUILD_DIR)/$(TARGET)_sign.bin

$(BUILD_DIR)/$(TARGET)_sign.hex: $(BUILD_DIR)/$(TARGET)_sign.bin
	$(CP) -I binary $< --change-addresses 0x70100000 -O ihex $@

$(BUILD_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	-rm -fR $(BUILD_TOP)

.PHONY: flash
flash: $(BUILD_DIR)/$(TARGET)_sign.bin
	$(FLASHER) -c port=SWD mode=HOTPLUG -el $(EL) -hardRst -w $< 0x70100000
	@echo FLASH $<

-include $(call rwildcard,$(BUILD_TOP),*.d)
