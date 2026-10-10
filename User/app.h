#ifndef __APP_H
#define __APP_H

#include <stdint.h>
#include "gui.h"

typedef enum{
    APP_MODE_NORMAL=0,
    APP_MODE_ECON,
    APP_MODE_ERROR,
    APP_MODE_SETTINGS,
    APP_MODE_COUNT
} AppMode;

void App_Init(void);
void App_Update(void);

AppMode App_GetMode(void);
GuiStatus *App_GetStatus(void);
void App_DrawStatus(void);

#endif