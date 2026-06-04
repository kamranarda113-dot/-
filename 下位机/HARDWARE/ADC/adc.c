#include "adc.h"
#include "delay.h"

ADC_HandleTypeDef ADC1_Handler;
TIM_HandleTypeDef TIM3_Handler;
DMA_HandleTypeDef DMA2_Stream0_Handler;

#define ADC_BUF_SIZE 1024
uint16_t ADC_ConvertedValue[ADC_BUF_SIZE];
uint8_t ADC_TransferComplete = 0;

void MX_TIM3_Init(uint32_t Period)
{
    TIM_MasterConfigTypeDef master_config;

    TIM3_Handler.Instance = TIM3;
    TIM3_Handler.Init.Prescaler = 84 - 1;
    TIM3_Handler.Init.CounterMode = TIM_COUNTERMODE_UP;
    TIM3_Handler.Init.Period = Period - 1;
    TIM3_Handler.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&TIM3_Handler);

    master_config.MasterOutputTrigger = TIM_TRGO_UPDATE;
    master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&TIM3_Handler, &master_config);
    
    HAL_TIM_Base_Stop(&TIM3_Handler);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM3)
    {
        __HAL_RCC_TIM3_CLK_ENABLE();
    }
    else if(htim->Instance == TIM6)
    {
        __HAL_RCC_TIM6_CLK_ENABLE();
    }
}

void MY_ADC_Init(uint32_t sample_freq)
{
    uint32_t period = 1000000 / sample_freq;
    ADC_ChannelConfTypeDef channel_config;
    
    MX_TIM3_Init(period);
    
    ADC1_Handler.Instance = ADC1;
    ADC1_Handler.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    ADC1_Handler.Init.Resolution = ADC_RESOLUTION_12B;
    ADC1_Handler.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    ADC1_Handler.Init.ScanConvMode = DISABLE;
    ADC1_Handler.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    ADC1_Handler.Init.ContinuousConvMode = DISABLE;
    ADC1_Handler.Init.NbrOfConversion = 1;
    ADC1_Handler.Init.DiscontinuousConvMode = DISABLE;
    ADC1_Handler.Init.NbrOfDiscConversion = 0;
    ADC1_Handler.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
    ADC1_Handler.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    ADC1_Handler.Init.DMAContinuousRequests = ENABLE;
    HAL_ADC_Init(&ADC1_Handler);

    channel_config.Channel = ADC_CHANNEL_5;
    channel_config.Rank = 1;
    channel_config.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    channel_config.Offset = 0;
    HAL_ADC_ConfigChannel(&ADC1_Handler, &channel_config);
    
    DMA2_Stream0_Handler.Instance = DMA2_Stream0;
    DMA2_Stream0_Handler.Init.Channel = DMA_CHANNEL_0;
    DMA2_Stream0_Handler.Init.Direction = DMA_PERIPH_TO_MEMORY;
    DMA2_Stream0_Handler.Init.PeriphInc = DMA_PINC_DISABLE;
    DMA2_Stream0_Handler.Init.MemInc = DMA_MINC_ENABLE;
    DMA2_Stream0_Handler.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    DMA2_Stream0_Handler.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    DMA2_Stream0_Handler.Init.Mode = DMA_CIRCULAR;
    DMA2_Stream0_Handler.Init.Priority = DMA_PRIORITY_HIGH;
    DMA2_Stream0_Handler.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&DMA2_Stream0_Handler);
    
    __HAL_LINKDMA(&ADC1_Handler, DMA_Handle, DMA2_Stream0_Handler);
    
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
    GPIO_InitTypeDef GPIO_Initure;
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    
    GPIO_Initure.Pin = GPIO_PIN_5;
    GPIO_Initure.Mode = GPIO_MODE_ANALOG;
    GPIO_Initure.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_Initure);
}

void HAL_DMA_ConvCpltCallback(DMA_HandleTypeDef *hdma)
{
    if(hdma->Instance == DMA2_Stream0)
    {
        ADC_TransferComplete = 1;
    }
}

void ADC_Start(void)
{
    HAL_TIM_Base_Stop(&TIM3_Handler);
    HAL_ADC_Stop_DMA(&ADC1_Handler);
    HAL_ADC_Start_DMA(&ADC1_Handler, (uint32_t*)ADC_ConvertedValue, ADC_BUF_SIZE);
    HAL_TIM_Base_Start(&TIM3_Handler);
}

void ADC_Stop(void)
{
    HAL_TIM_Base_Stop(&TIM3_Handler);
    HAL_ADC_Stop_DMA(&ADC1_Handler);
}

uint16_t* ADC_GetBuffer(void)
{
    return ADC_ConvertedValue;
}

uint16_t ADC_GetBufferSize(void)
{
    return ADC_BUF_SIZE;
}

uint8_t ADC_IsTransferComplete(void)
{
    return ADC_TransferComplete;
}

void ADC_ClearTransferFlag(void)
{
    ADC_TransferComplete = 0;
}
