#ifndef __POWER_H
#define __POWER_H

#include <stdint.h>

void Power_Init(void);
uint16_t Power_ReadBatteryMv(void);
uint8_t Power_BatteryBars(uint16_t mv);

void Power_ButtonIRQInit(void);
void Power_ButtonIRQHandler(void);
uint8_t Power_TakeShortPress(void);

void Power_EnterStandby(void);

#endif