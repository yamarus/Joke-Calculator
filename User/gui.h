#ifndef __GUI_H
#define __GUI_H

#include <stdint.h>
#include "calc.h"

typedef enum{
    GUI_MODE_NORMAL=0,
    GUI_MODE_ECO,
    GUI_MODE_ERROR,
    GUI_MODE_SETTINGS
} GuiMode;

typedef enum{
    GUI_ANGLE_DEG=0,
    GUI_ANGLE_RAD
} GuiAngle;

typedef struct{
    uint8_t battery;
    uint8_t mode,angle;
    uint8_t fn;
} GuiStatus;

void GUI_DrawSettings(const char *const *labels,const char *const *values,uint8_t selected);
void GUI_DrawBlackSwan(const char *message);
enum{ ECON_VIS_NUMBER=1,ECON_VIS_IMAGINARY,ECON_VIS_MODEL,ECON_VIS_CRASH,
      ECON_VIS_QUESTION,ECON_VIS_HISTORY,ECON_VIS_TV,ECON_VIS_REVIEW,ECON_VIS_REJECT,ECON_VIS_INFERNAL };
void GUI_DrawEconVisual(uint8_t event,const char *text);
void GUI_Init(void);
void GUI_Tick(void);
void GUI_ClearMain(void);

void GUI_DrawStatus(const GuiStatus *status);
void GUI_DrawError(CalcError error);

void GUI_ClearExpression(void);
void GUI_DrawExpression(int8_t root);

void GUI_ClearResult(void);
void GUI_DrawResult(const char *result);
void GUI_DrawUncertain(float value,float sigma);

void GUI_ClearMessage(void);
void GUI_DrawMessage(const char *text);

void GUI_DrawEditor(const char *text,uint8_t cursor);
#endif