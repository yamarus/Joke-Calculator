#include "mode_error.h"
#include "mode_normal.h"
#include "gui.h"

void ModeError_Enter(GuiStatus *status){ ModeNormal_Enter(status); }
void ModeError_Update(GuiStatus *status){ ModeNormal_Update(status); }
void ModeError_Result(CalcUncertain result){
    GUI_DrawUncertain(result.value,result.sigma);
    GUI_DrawMessage("Value +/- uncertainty");
}
