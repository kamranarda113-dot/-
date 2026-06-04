#ifndef _DAC_H
#define _DAC_H

#include "sys.h"

typedef enum {
    WAVE_SINE,
    WAVE_TRIANGLE,
    WAVE_SQUARE
} WaveType;

void MY_DAC_Init(uint32_t freq);
void DAC_StartWave(WaveType type);
void DAC_StopWave(void);

#endif