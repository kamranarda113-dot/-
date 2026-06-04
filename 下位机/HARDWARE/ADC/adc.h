#ifndef _ADC_H
#define _ADC_H

#include "sys.h"

extern uint16_t ADC_ConvertedValue[];
extern uint8_t ADC_TransferComplete;

void MY_ADC_Init(uint32_t sample_freq);
void ADC_Start(void);
void ADC_Stop(void);
uint16_t* ADC_GetBuffer(void);
uint16_t ADC_GetBufferSize(void);
uint8_t ADC_IsTransferComplete(void);
void ADC_ClearTransferFlag(void);

#endif