#include "app_power.h"
#include "debug.h"
#include "lcd.h"
#include "gui.h"
#include "power.h"

void AppPower_Init(void){
    Power_ButtonIRQInit();
}

void AppPower_Reset(void){
    SystemReset_StartMode(Start_Mode_USER);
    NVIC_SystemReset();
    while(1);
}

void AppPower_Standby(void){
    GUI_DrawMessage("OFF");
    Delay_Ms(100);

    LCD_Shutdown();
    Power_EnterStandby();

    while(1);
}