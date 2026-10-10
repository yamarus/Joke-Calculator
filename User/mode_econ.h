#ifndef __MODE_ECON_H
#define __MODE_ECON_H

#include "gui.h"
#include "calc.h"
enum{ ECON_HIDE_SIGMA=1,ECON_REFUSED=2,ECON_BLACK_SWAN=4 };
typedef struct{ CalcUncertain result; const char *message; uint8_t absurdity,flags; } EconForecast;
void ModeEcon_StartSession(uint32_t seed);
CalcError ModeEcon_Evaluate(CalcUncertain *result,uint8_t degrees,CalcFeatures *features);
const char *ModeEcon_ErrorMessage(CalcError error);
EconForecast ModeEcon_Forecast(CalcUncertain result,const CalcFeatures *features);
void ModeEcon_Result(CalcUncertain result,const CalcFeatures *features);

void ModeEcon_Enter(GuiStatus *status);
void ModeEcon_Update(GuiStatus *status);

#endif