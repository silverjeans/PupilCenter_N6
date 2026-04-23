/**
 * @file    main.c (board entry)
 * @brief   Thin board entry. Responsible only for:
 *            - clocks / caches / XSPI / security / console UART init
 *            - handing control to app_main_run(), which wires the
 *              pupil-detection pipeline.
 *
 *          All classical-CV, filtering, state-machine and logging logic
 *          lives in Core/. If a reader wants to understand behaviour, they
 *          should read Core/Src/app_main.c, not this file.
 *
 *          The hardware-init bodies are copied verbatim from the ST
 *          ObjectDetection example (kept as main_original_objdetect.c.bak
 *          for reference).
 */
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "stm32n6570_discovery_bus.h"
#include "stm32n6570_discovery_xspi.h"
#include "stm32n6570_discovery.h"
#include "stm32n6xx_hal.h"

#include "app_fuseprogramming.h"
#include "main.h"

/* Declared by the ST example's USART1 console config below. */
UART_HandleTypeDef huart1;

/* Pipeline entry from Core/. */
void app_main_run(void);

static void SystemClock_Config(void);
static void CONSOLE_Config(void);
static void Security_Config(void);
static void set_clk_sleep_mode(void);
static void IAC_Config(void);
static void Hardware_init(void);

int main(void)
{
    Hardware_init();

    printf("\r\n========================================\r\n");
    printf("PupilCenter_N6 - classical CV skeleton\r\n");
    printf("Build: %s %s\r\n", __DATE__, __TIME__);
    printf("========================================\r\n");

    /* Never returns. */
    app_main_run();

    while (1) {}
}

static void Hardware_init(void)
{
    MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_ICACTIVE_Msk;
    __HAL_RCC_CPUCLK_CONFIG(RCC_CPUCLKSOURCE_HSI);
    __HAL_RCC_SYSCLK_CONFIG(RCC_SYSCLKSOURCE_HSI);
    HAL_Init();
    SCB_EnableICache();
#if defined(USE_DCACHE)
    MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_DCACTIVE_Msk;
    SCB_EnableDCache();
#endif
    SystemClock_Config();
    CONSOLE_Config();
    Fuse_Programming();
    /* NPU init intentionally omitted - classical CV pipeline does not
     * use the neural processor. */

    BSP_XSPI_RAM_Init(0);
    BSP_XSPI_RAM_EnableMemoryMappedMode(0);

    BSP_XSPI_NOR_Init_t NOR_Init;
    NOR_Init.InterfaceMode = BSP_XSPI_NOR_OPI_MODE;
    NOR_Init.TransferRate  = BSP_XSPI_NOR_DTR_TRANSFER;
    BSP_XSPI_NOR_Init(0, &NOR_Init);
    BSP_XSPI_NOR_EnableMemoryMappedMode(0);

    Security_Config();
    IAC_Config();
    set_clk_sleep_mode();
}

static void set_clk_sleep_mode(void)
{
    __HAL_RCC_XSPI1_CLK_SLEEP_ENABLE();
    __HAL_RCC_XSPI2_CLK_SLEEP_ENABLE();
    __HAL_RCC_LTDC_CLK_SLEEP_ENABLE();
    __HAL_RCC_DMA2D_CLK_SLEEP_ENABLE();
    __HAL_RCC_DCMIPP_CLK_SLEEP_ENABLE();
    __HAL_RCC_CSI_CLK_SLEEP_ENABLE();
    __HAL_RCC_FLEXRAM_MEM_CLK_SLEEP_ENABLE();
    __HAL_RCC_AXISRAM1_MEM_CLK_SLEEP_ENABLE();
    __HAL_RCC_AXISRAM2_MEM_CLK_SLEEP_ENABLE();
}

static void Security_Config(void)
{
    __HAL_RCC_RIFSC_CLK_ENABLE();
    RIMC_MasterConfig_t m = {0};
    m.MasterCID = RIF_CID_1;
    m.SecPriv   = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU,    &m);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DMA2D,  &m);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &m);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC1,  &m);
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC2,  &m);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_NPU,    RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DMA2D,  RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_CSI,    RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DCMIPP, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDC,   RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL1, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL2, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
}

static void IAC_Config(void)
{
    __HAL_RCC_IAC_CLK_ENABLE();
    __HAL_RCC_IAC_FORCE_RESET();
    __HAL_RCC_IAC_RELEASE_RESET();
}

void IAC_IRQHandler(void) { while (1) {} }

HAL_StatusTypeDef MX_DCMIPP_ClockConfig(DCMIPP_HandleTypeDef *hdcmipp)
{
    RCC_PeriphCLKInitTypeDef s = {0};
    HAL_StatusTypeDef ret;
    s.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP;
    s.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
    s.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL2;
    s.ICSelection[RCC_IC17].ClockDivider   = 3;
    ret = HAL_RCCEx_PeriphCLKConfig(&s);
    if (ret) return ret;

    s.PeriphClockSelection = RCC_PERIPHCLK_CSI;
    s.ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL1;
    s.ICSelection[RCC_IC18].ClockDivider   = 40;
    return HAL_RCCEx_PeriphCLKConfig(&s);
}

static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef       C = {0};
    RCC_OscInitTypeDef       O = {0};
    RCC_PeriphCLKInitTypeDef P = {0};

    BSP_SMPS_Init(SMPS_VOLTAGE_OVERDRIVE);

    O.OscillatorType = RCC_OSCILLATORTYPE_NONE;
    O.PLL1.PLLState = RCC_PLL_ON; O.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
    O.PLL1.PLLM = 2;  O.PLL1.PLLN = 25;  O.PLL1.PLLFractional = 0;
    O.PLL1.PLLP1 = 1; O.PLL1.PLLP2 = 1;
    O.PLL2.PLLState = RCC_PLL_ON; O.PLL2.PLLSource = RCC_PLLSOURCE_HSI;
    O.PLL2.PLLM = 8;  O.PLL2.PLLFractional = 0; O.PLL2.PLLN = 125;
    O.PLL2.PLLP1 = 1; O.PLL2.PLLP2 = 1;
    O.PLL3.PLLState = RCC_PLL_ON; O.PLL3.PLLSource = RCC_PLLSOURCE_HSI;
    O.PLL3.PLLM = 8;  O.PLL3.PLLN = 225; O.PLL3.PLLFractional = 0;
    O.PLL3.PLLP1 = 1; O.PLL3.PLLP2 = 2;
    O.PLL4.PLLState = RCC_PLL_ON; O.PLL4.PLLSource = RCC_PLLSOURCE_HSI;
    O.PLL4.PLLM = 8;  O.PLL4.PLLFractional = 0; O.PLL4.PLLN = 225;
    O.PLL4.PLLP1 = 6; O.PLL4.PLLP2 = 6;
    if (HAL_RCC_OscConfig(&O) != HAL_OK) while (1);

    C.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK |
                   RCC_CLOCKTYPE_HCLK   | RCC_CLOCKTYPE_PCLK1  |
                   RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_PCLK4  |
                   RCC_CLOCKTYPE_PCLK5);
    C.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
    C.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
    C.IC1Selection.ClockSelection  = RCC_ICCLKSOURCE_PLL1; C.IC1Selection.ClockDivider  = 1;
    C.IC2Selection.ClockSelection  = RCC_ICCLKSOURCE_PLL1; C.IC2Selection.ClockDivider  = 2;
    C.IC6Selection.ClockSelection  = RCC_ICCLKSOURCE_PLL2; C.IC6Selection.ClockDivider  = 1;
    C.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL3; C.IC11Selection.ClockDivider = 1;
    C.AHBCLKDivider = RCC_HCLK_DIV2;
    C.APB1CLKDivider = RCC_APB1_DIV1;
    C.APB2CLKDivider = RCC_APB2_DIV1;
    C.APB4CLKDivider = RCC_APB4_DIV1;
    C.APB5CLKDivider = RCC_APB5_DIV1;
    if (HAL_RCC_ClockConfig(&C) != HAL_OK) while (1);

    P.PeriphClockSelection = 0;
    P.PeriphClockSelection |= RCC_PERIPHCLK_XSPI1;
    P.Xspi1ClockSelection   = RCC_XSPI1CLKSOURCE_HCLK;
    P.PeriphClockSelection |= RCC_PERIPHCLK_XSPI2;
    P.Xspi2ClockSelection   = RCC_XSPI2CLKSOURCE_HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&P) != HAL_OK) while (1);
}

static void CONSOLE_Config(void)
{
    GPIO_InitTypeDef g;
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Pin       = GPIO_PIN_5 | GPIO_PIN_6;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOE, &g);
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_8;
    if (HAL_UART_Init(&huart1) != HAL_OK) while (1);
}

int _write(int file, char *ptr, int len)
{
    if ((file != STDOUT_FILENO) && (file != STDERR_FILENO)) { errno = EBADF; return -1; }
    HAL_StatusTypeDef s = HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, ~0);
    return (s == HAL_OK ? len : 0);
}

void npu_cache_enable_clocks_and_reset(void)
{
    __HAL_RCC_CACHEAXIRAM_MEM_CLK_ENABLE();
    __HAL_RCC_CACHEAXI_CLK_ENABLE();
    __HAL_RCC_CACHEAXI_FORCE_RESET();
    __HAL_RCC_CACHEAXI_RELEASE_RESET();
}

void npu_cache_disable_clocks_and_reset(void)
{
    __HAL_RCC_CACHEAXIRAM_MEM_CLK_DISABLE();
    __HAL_RCC_CACHEAXI_CLK_DISABLE();
    __HAL_RCC_CACHEAXI_FORCE_RESET();
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line) { UNUSED(file); UNUSED(line); __BKPT(0); while (1) {} }
#endif
