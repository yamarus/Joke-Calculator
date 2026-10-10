#include "app.h"
#include "app_power.h"
#include "debug.h"
#include "lcd.h"
#include "gui.h"
#include "input.h"
#include "power.h"

#include "mode_normal.h"
#include "mode_econ.h"
#include "mode_error.h"
#include "settings.h"

#define BATTERY_UPDATE_TICKS 500

static GuiStatus status={
    .battery=0,
    .mode=GUI_MODE_NORMAL,
    .angle=GUI_ANGLE_DEG,
    .fn=0
};

static AppMode current_mode=APP_MODE_NORMAL;
static uint16_t battery_timer=0,idle_seconds;
static uint8_t idle_fraction;
static uint16_t battery_mv=0;

static void App_UpdateBattery(void){
    battery_mv=Power_ReadBatteryMv();
    status.battery=Power_BatteryBars(battery_mv);
    GUI_DrawStatus(&status);
}

static void App_EnterMode(AppMode mode){
    status.fn=0;
    status.mode=mode==APP_MODE_ECON?GUI_MODE_ECO:
                mode==APP_MODE_ERROR?GUI_MODE_ERROR:mode==APP_MODE_SETTINGS?GUI_MODE_SETTINGS:GUI_MODE_NORMAL;

    switch(mode){
        case APP_MODE_NORMAL:
            ModeNormal_Enter(&status);
            break;

        case APP_MODE_ECON:
            ModeEcon_Enter(&status);
            break;

        case APP_MODE_ERROR:
            ModeError_Enter(&status);
            break;

        case APP_MODE_SETTINGS:
            Settings_Enter(&status);
            break;

        default:
            current_mode=APP_MODE_NORMAL;
            ModeNormal_Enter(&status);
            break;
    }
}

static void App_NextMode(void){
    current_mode=(AppMode)(current_mode+1);

    if(current_mode>=APP_MODE_COUNT)
        current_mode=APP_MODE_NORMAL;

    App_EnterMode(current_mode);
}

static void App_UpdateMode(void){
    switch(current_mode){
        case APP_MODE_NORMAL:
            ModeNormal_Update(&status);
            break;

        case APP_MODE_ECON:
            ModeEcon_Update(&status);
            break;

        case APP_MODE_ERROR:
            ModeError_Update(&status);
            break;

        case APP_MODE_SETTINGS:
            Settings_Update(&status);
            break;

        default:
            current_mode=APP_MODE_NORMAL;
            App_EnterMode(current_mode);
            break;
    }
}

void App_Init(void){
    battery_timer=idle_seconds=idle_fraction=0;
    Settings_Init();
    current_mode=APP_MODE_NORMAL;

    Input_Init();
    AppPower_Init();

    LCD_Init();
    GUI_Init();
    Power_Init();

    status.battery=0;
    status.mode=GUI_MODE_NORMAL;
    status.angle=GUI_ANGLE_DEG;
    status.fn=0;

    App_UpdateBattery();
    App_EnterMode(current_mode);
}

void App_Update(void){
    Input_Update();
    /* Existing loop has a 10-ms delay; keep policy in seconds, no new timer IRQ. */
    if(Input_Down()||Input_Pressed()||Input_Released())idle_seconds=idle_fraction=0;
    else if(++idle_fraction>=100){
        uint16_t timeout=Settings_IdleSeconds();idle_fraction=0;
        if(idle_seconds<65535)idle_seconds++;
        if(timeout&&idle_seconds>=timeout){AppPower_Standby();return;}
    }

    if(Power_TakeShortPress()){
        AppPower_Standby();
        return;
    }

    if(Input_WasPressed(KEY_SHIFT)&&current_mode!=APP_MODE_SETTINGS){
        status.fn=!status.fn;
        GUI_DrawStatus(&status);
        return;
    }

    if(Input_WasPressed(KEY_MODE)){
        App_NextMode();
        return;
    }

    App_UpdateMode();
    GUI_Tick();

    if(++battery_timer>=BATTERY_UPDATE_TICKS){
        battery_timer=0;
        App_UpdateBattery();
    }
}

AppMode App_GetMode(void){
    return current_mode;
}

GuiStatus *App_GetStatus(void){
    return &status;
}

void App_DrawStatus(void){
    GUI_DrawStatus(&status);
}