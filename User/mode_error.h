#ifndef __MODE_ERROR_H
#define __MODE_ERROR_H

#include "gui.h"
#include "calc.h"

void ModeError_Result(CalcUncertain result);
void ModeError_Enter(GuiStatus *status);
void ModeError_Update(GuiStatus *status);

#endif