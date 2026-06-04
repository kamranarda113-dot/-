#include "dac.h"
#include "math.h"

DAC_HandleTypeDef DAC1_Handler;
TIM_HandleTypeDef TIM6_Handler;
DMA_HandleTypeDef DMA1_Stream5_Handler;

#define WAVE_BUF_SIZE 1024
uint32_t sine_wave[WAVE_BUF_SIZE];
uint32_t triangle_wave[WAVE_BUF_SIZE];
uint32_t square_wave[WAVE_BUF_SIZE];

void GenerateWaveforms(void)
{
    for(uint16_t i = 0; i < WAVE_BUF_SIZE; i++)
    {
        float angle = 2.0f * 3.1415926f * i / WAVE_BUF_SIZE;
        sine_wave[i] = (uint32_t)((sin(angle) + 1.0f) * 2047.5f);
        
        if(i < WAVE_BUF_SIZE / 2)
        {
            triangle_wave[i] = (uint32_t)(i * 4095.0f / (WAVE_BUF_SIZE / 2));
        }
        else
        {
            triangle_wave[i] = (uint32_t)(4095 - (i - WAVE_BUF_SIZE / 2) * 4095.0f / (WAVE_BUF_SIZE / 2));
        }
        
        square_wave[i] = (i < WAVE_BUF_SIZE / 2) ? 4095 : 0;
    }
}

void MX_TIM6_Init(uint32_t freq)
{
    TIM_MasterConfigTypeDef master_config;
    uint32_t update_freq = freq * WAVE_BUF_SIZE;
    uint32_t period = 84000000UL / update_freq;

    if(period == 0)
    {
        period = 1;
    }

    TIM6_Handler.Instance = TIM6;
    TIM6_Handler.Init.Prescaler = 1 - 1;
    TIM6_Handler.Init.CounterMode = TIM_COUNTERMODE_UP;
    TIM6_Handler.Init.Period = period - 1;
    TIM6_Handler.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&TIM6_Handler);

    master_config.MasterOutputTrigger = TIM_TRGO_UPDATE;
    master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&TIM6_Handler, &master_config);
}

void MY_DAC_Init(uint32_t freq)
{
    GenerateWaveforms();
    
    MX_TIM6_Init(freq);
    
    DAC1_Handler.Instance = DAC;
    HAL_DAC_Init(&DAC1_Handler);
    
    DAC_ChannelConfTypeDef sConfig;
    sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
    sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    HAL_DAC_ConfigChannel(&DAC1_Handler, &sConfig, DAC_CHANNEL_1);
}

void HAL_DAC_MspInit(DAC_HandleTypeDef* hdac)
{
    GPIO_InitTypeDef GPIO_Initure;
    __HAL_RCC_DAC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    
    GPIO_Initure.Pin = GPIO_PIN_4;
    GPIO_Initure.Mode = GPIO_MODE_ANALOG;
    GPIO_Initure.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_Initure);

    DMA1_Stream5_Handler.Instance = DMA1_Stream5;
    DMA1_Stream5_Handler.Init.Channel = DMA_CHANNEL_7;
    DMA1_Stream5_Handler.Init.Direction = DMA_MEMORY_TO_PERIPH;
    DMA1_Stream5_Handler.Init.PeriphInc = DMA_PINC_DISABLE;
    DMA1_Stream5_Handler.Init.MemInc = DMA_MINC_ENABLE;
    DMA1_Stream5_Handler.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    DMA1_Stream5_Handler.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    DMA1_Stream5_Handler.Init.Mode = DMA_CIRCULAR;
    DMA1_Stream5_Handler.Init.Priority = DMA_PRIORITY_HIGH;
    DMA1_Stream5_Handler.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&DMA1_Stream5_Handler);

    __HAL_LINKDMA(hdac, DMA_Handle1, DMA1_Stream5_Handler);

    HAL_NVIC_DisableIRQ(DMA1_Stream5_IRQn);
}

void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&DMA1_Stream5_Handler);
}

void DAC_StartWave(WaveType type)
{
    uint32_t *buf_addr;
    switch(type)
    {
        case WAVE_SINE:
            buf_addr = (uint32_t*)sine_wave;
            break;
        case WAVE_TRIANGLE:
            buf_addr = (uint32_t*)triangle_wave;
            break;
        case WAVE_SQUARE:
            buf_addr = (uint32_t*)square_wave;
            break;
        default:
            buf_addr = (uint32_t*)sine_wave;
    }

    HAL_TIM_Base_Stop(&TIM6_Handler);
    HAL_DAC_Stop_DMA(&DAC1_Handler, DAC_CHANNEL_1);
    HAL_DAC_Start_DMA(&DAC1_Handler, DAC_CHANNEL_1, buf_addr, WAVE_BUF_SIZE, DAC_ALIGN_12B_R);
    HAL_TIM_Base_Start(&TIM6_Handler);
}

void DAC_StopWave(void)
{
    HAL_DAC_Stop_DMA(&DAC1_Handler, DAC_CHANNEL_1);
    HAL_TIM_Base_Stop(&TIM6_Handler);
}
