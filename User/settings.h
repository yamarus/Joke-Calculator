#ifndef SETTINGS_H
#define SETTINGS_H
#include "gui.h"
/* Device-tested storage is enabled in target builds; host tests opt in explicitly. */
#if defined(__riscv) && !defined(SETTINGS_FLASH_TEST)
#define SETTINGS_FLASH_TEST
#endif
enum{ ECON_ECONOMIST,ECON_TV_EXPERT,ECON_REFEREE };
typedef struct{ uint8_t idle,digits,scientific,style; } AppSettings;
void Settings_Init(void);
const AppSettings *Settings_Get(void);
uint16_t Settings_IdleSeconds(void);
uint8_t Settings_Digits(void);
const char *Settings_StyleName(void);
void Settings_Format(float value,char *dst,uint8_t size,uint8_t digits);
void Settings_Enter(GuiStatus *status);
void Settings_Update(GuiStatus *status);
#endif
