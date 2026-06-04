#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"
#include "adc.h"
#include "dac.h"

#define SAMPLE_FREQ 10000
#define PACKET_SIZE 128
#define WAVE_FREQ 100
#define LED_ON  0
#define LED_OFF 1

uint8_t g_osc_running = 0;
uint8_t g_signal_source = 0;
uint16_t g_sample_index = 0;
uint8_t g_wave_type = WAVE_SINE;
uint8_t g_tx_packet[PACKET_SIZE * 2 + 6];

void SendStatus(uint8_t status);

void Scope_SetRunning(uint8_t running)
{
    if(running)
    {
        if(!g_osc_running)
        {
            g_osc_running = 1;
            g_sample_index = 0;
            ADC_Start();
            LED0 = LED_ON;
        }
    }
    else
    {
        if(g_osc_running)
        {
            g_osc_running = 0;
            ADC_Stop();
            LED0 = LED_OFF;
        }
    }
    SendStatus(g_osc_running);
}

void Signal_SetWave(WaveType type)
{
    g_wave_type = type;
    g_signal_source = 1;
    DAC_StartWave(type);
    LED1 = LED_ON;
}

void Signal_SetRunning(uint8_t running)
{
    if(running)
    {
        Signal_SetWave((WaveType)g_wave_type);
    }
    else
    {
        g_signal_source = 0;
        DAC_StopWave();
        LED1 = LED_OFF;
    }
}

void SendDataPacket(uint16_t* data, uint16_t len)
{
    if(len > PACKET_SIZE)
    {
        len = PACKET_SIZE;
    }

    g_tx_packet[0] = 0xAA;
    g_tx_packet[1] = 0xBB;
    g_tx_packet[2] = (len >> 8) & 0xFF;
    g_tx_packet[3] = len & 0xFF;
    
    for(uint16_t i = 0; i < len; i++)
    {
        uint16_t idx = 4 + i * 2;
        g_tx_packet[idx] = (data[i] >> 8) & 0xFF;
        g_tx_packet[idx + 1] = data[i] & 0xFF;
    }

    g_tx_packet[4 + len * 2] = 0xCC;
    g_tx_packet[5 + len * 2] = 0xDD;
    HAL_UART_Transmit(&UART1_Handler, g_tx_packet, len * 2 + 6, 50);
}

void SendStatus(uint8_t status)
{
    uint8_t pkt[4] = {0xAA, 0xCC, 0x01, status};
    HAL_UART_Transmit(&UART1_Handler, pkt, 4, 100);
}

void ProcessCommand(uint8_t cmd)
{
    switch(cmd)
    {
        case 0x01:
            Scope_SetRunning(1);
            break;
        case 0x02:
            Scope_SetRunning(0);
            break;
        case 0x03:
            SendStatus(g_osc_running);
            break;
        case 0x10:
            Signal_SetWave(WAVE_SINE);
            break;
        case 0x11:
            Signal_SetWave(WAVE_TRIANGLE);
            break;
        case 0x12:
            Signal_SetWave(WAVE_SQUARE);
            break;
        case 0x13:
            Signal_SetRunning(0);
            break;
    }
}

int main(void)
{
    HAL_Init();
    Stm32_Clock_Init(336, 8, 2, 7);
    delay_init(168);
    uart_init(115200);
    LED_Init();
    KEY_Init();
    MY_ADC_Init(SAMPLE_FREQ);
    MY_DAC_Init(WAVE_FREQ);
    
    uint8_t key_val;
    uint16_t* adc_buf = ADC_GetBuffer();
    uint16_t buf_size = ADC_GetBufferSize();
    uint32_t last_packet_tick = 0;
    
    while(1)
    {
        if(USART_RX_STA & 0x8000)
        {
            if((USART_RX_STA & 0x3FFF) > 0)
            {
                ProcessCommand(USART_RX_BUF[0]);
            }
            USART_RX_STA = 0;
        }

        key_val = KEY_Scan(0);
        
        if(key_val == KEY0_PRES)
        {
            Scope_SetRunning(!g_osc_running);
        }
        
        if(key_val == WKUP_PRES)
        {
            Signal_SetRunning(!g_signal_source);
        }
        
        if(g_osc_running && (HAL_GetTick() - last_packet_tick >= 200))
        {
            last_packet_tick = HAL_GetTick();
            if(buf_size >= PACKET_SIZE)
            {
                SendDataPacket(adc_buf, PACKET_SIZE);
            }
            else
            {
                SendDataPacket(adc_buf, buf_size);
            }
        }

        delay_ms(1);
    }
}
