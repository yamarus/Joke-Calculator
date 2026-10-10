#include "mode_normal.h"
#include "settings.h"
#include "input.h"
#include "calc.h"
#include "gui.h"
#include "mode_error.h"
#include "mode_econ.h"

static uint8_t initialized=0;
static char result_text[24];


static void redraw_expression(uint8_t force_editor){
    if(!Calc_Count()){ GUI_ClearExpression(); return; }
    if(force_editor) GUI_DrawEditor(0,0);
    else GUI_DrawExpression(Calc_Root());
}

static void expression_changed(void){
    GUI_ClearResult();
    GUI_ClearMessage();
    redraw_expression(1);
}

static void fn_clear(GuiStatus *status){
    if(status->fn){
        status->fn=0;
        GUI_DrawStatus(status);
    }
}

static uint8_t insert_suffix(const CalcToken *tail,uint8_t count){
    uint8_t cursor=Calc_Cursor(),missing=!cursor;
    if(cursor){
        CalcToken t=Calc_Tokens()[cursor-1];
        missing=t=='('||t==','||t=='+'||t=='-'||t=='*'||t=='/'||t=='^'||t==CALC_T_UNCERTAIN||t==CALC_T_DELTAPERCENT;
    }
    if(cursor<Calc_Count()&&Calc_Tokens()[cursor]==CALC_T_HOLE)missing=1;
    if(missing){
        CalcToken seq[7]={'(',CALC_T_HOLE,')'};uint8_t i;
        if(count>4)return 0;
        for(i=0;i<count;i++)seq[i+3]=tail[i];
        if(!Calc_InsertSequence(seq,count+3))return 0;
        Calc_SetCursor(Calc_Cursor()-count-2);return 1;
    }
    return Calc_InsertSequence(tail,count);
}
static uint8_t insert_postfix(CalcToken token){return insert_suffix(&token,1);}
static uint8_t insert_square(void){
    const CalcToken seq[]={CALC_T_POW,'(','2',')'};
    return insert_suffix(seq,4);
}
static uint8_t insert_ten_power(void){
    const CalcToken seq[]={'1','0',CALC_T_POW,'(',CALC_T_HOLE,')'};
    if(!Calc_InsertSequence(seq,6)) return 0;
    Calc_SetCursor(Calc_Cursor()-2); return 1;
}

static uint8_t handle_fn(GuiStatus *status){
    uint8_t changed=0,full;

    if(!status->fn) return 0;
    if(Input_WasPressed(KEY_LEFT)||Input_WasPressed(KEY_RIGHT)){
        if(Input_WasPressed(KEY_LEFT)) Calc_MoveLeft(); else Calc_MoveRight();
        redraw_expression(1);
        return 1;
    }

    if(Input_WasPressed(KEY_PERCENT)){
        changed=Calc_InsertBinary(CALC_T_DELTAPERCENT);
    }else if(Input_WasPressed(KEY_SQRT)){
        changed=insert_square();
    }else if(Input_WasPressed(KEY_POW)){
        changed=Calc_InsertFunction(CALC_T_EXP);
    }else if(Input_WasPressed(KEY_LOG)){
        changed=Calc_InsertFunction(CALC_T_LOG);
    }else if(Input_WasPressed(KEY_4)){
        changed=Calc_InsertFunction(CALC_T_SIN);
    }else if(Input_WasPressed(KEY_5)){
        changed=Calc_InsertFunction(CALC_T_COS);
    }else if(Input_WasPressed(KEY_6)){
        changed=Calc_InsertFunction(CALC_T_TAN);
    }else if(Input_WasPressed(KEY_7)){
        changed=Calc_InsertFunction(CALC_T_ASIN);
    }else if(Input_WasPressed(KEY_8)){
        changed=Calc_InsertFunction(CALC_T_ACOS);
    }else if(Input_WasPressed(KEY_9)){
        changed=Calc_InsertFunction(CALC_T_ATAN);

    }else if(Input_WasPressed(KEY_LPAREN)){
        status->angle=status->angle==GUI_ANGLE_RAD?GUI_ANGLE_DEG:GUI_ANGLE_RAD;
        Calc_Edit();
        GUI_ClearResult(); GUI_ClearMessage();
    }else if(Input_WasPressed(KEY_RPAREN)){
        changed=Calc_InsertFunction(CALC_T_ABS);
    }else if(Input_WasPressed(KEY_MUL)){
        changed=insert_postfix(CALC_T_FACT);
    }else if(Input_WasPressed(KEY_DIV)){
        changed=insert_postfix(CALC_T_INV);

    }else if(Input_WasPressed(KEY_1)){
        changed=insert_ten_power();
    }else if(Input_WasPressed(KEY_2)){
        changed=Calc_InsertToken(CALC_T_E);
    }else if(Input_WasPressed(KEY_3)){
        changed=Calc_InsertToken(CALC_T_PI);

    }else if(Input_WasPressed(KEY_ADD)){
        changed=Calc_InsertToken(CALC_T_EE);
    }else if(Input_WasPressed(KEY_SUB)){
        changed=Calc_InsertFunction(CALC_T_RND);
    }else if(Input_WasPressed(KEY_DEL)){
        changed=Calc_InsertToken(CALC_T_ANS);
    }else if(Input_WasPressed(KEY_PLUSMINUS)&&status->mode!=GUI_MODE_NORMAL){
        changed=status->mode==GUI_MODE_ECO?Calc_InsertBinary(CALC_T_UNCERTAIN):Calc_ToggleSign();
    }else if(Input_WasPressed(KEY_ENTER)){
        fn_clear(status);
        if(Calc_NextField()) redraw_expression(1);
        else GUI_DrawMessage("OK");
        return 1;
    }else{
        if(Input_WasPressed(KEY_0)||Input_WasPressed(KEY_DOT)||Input_WasPressed(KEY_PLUSMINUS))
            GUI_DrawMessage("No FN function on this key");
        return 1;
    }

    full=Calc_LastError()==CALC_ERR_FULL;
    fn_clear(status);

    if(changed) expression_changed();
    else if(full) GUI_DrawMessage(Calc_ErrorText(CALC_ERR_FULL));

    return 1;
}

static uint8_t handle_digits(void){
    /* KEY_0..KEY_9 are contiguous; retain lowest-digit priority. */
    for(uint8_t digit=0;digit<10;digit++)if(Input_WasPressed((KeyCode)(KEY_0+digit))){
        Calc_InsertDigit(digit);return 1;
    }
    return 0;
}

static void evaluate(GuiStatus *status){
    CalcUncertain result;
    CalcFeatures features;
    CalcError err=Calc_EvaluateUncertain(&result,status->angle==GUI_ANGLE_DEG,
                                        status->mode!=GUI_MODE_NORMAL,&features);

    if(err!=CALC_OK){
        GUI_ClearResult();
        GUI_DrawError(err);
        redraw_expression(1);
        return;
    }

    redraw_expression(0);
    if(status->mode==GUI_MODE_ERROR) ModeError_Result(result);
    else if(status->mode==GUI_MODE_ECO) ModeEcon_Result(result,&features);
    else{
        if(Settings_Get()->scientific)Settings_Format(result.value,result_text,sizeof(result_text),0);
        else Calc_FormatFixed(result.value,result_text,sizeof(result_text),Settings_Digits());
        GUI_DrawResult(result_text);
        GUI_ClearMessage();
    }
}

void ModeNormal_Enter(GuiStatus *status){
    if(!initialized){
        Calc_Init();
        initialized=1;
    }

    status->fn=0;
    Calc_Edit();
    GUI_DrawStatus(status);
    GUI_ClearMessage();
    GUI_ClearExpression();
    GUI_ClearResult();

    if(Calc_Count()) redraw_expression(1);
    GUI_ClearMessage();
}

void ModeNormal_Update(GuiStatus *status){
    uint8_t changed=0;

    if(Input_WasPressed(KEY_AC)){
        Calc_Clear();
        status->fn=0;
        GUI_DrawStatus(status);
        GUI_ClearExpression();
        GUI_ClearResult();
        GUI_ClearMessage();
        return;
    }

    if(handle_fn(status)) return;

    if(handle_digits()) changed=1;
    else if(Input_WasPressed(KEY_DOT)){
        Calc_InsertToken(CALC_T_DOT); changed=1;
    }else if(Input_WasPressed(KEY_PLUSMINUS)){
        if(status->mode==GUI_MODE_ERROR) Calc_InsertBinary(CALC_T_UNCERTAIN);
        else Calc_ToggleSign();
        changed=1;
    }else if(Input_WasPressed(KEY_ADD)){
        Calc_InsertToken(CALC_T_ADD); changed=1;
    }else if(Input_WasPressed(KEY_SUB)){
        Calc_InsertToken(CALC_T_SUB); changed=1;
    }else if(Input_WasPressed(KEY_MUL)){
        Calc_InsertToken(CALC_T_MUL); changed=1;
    }else if(Input_WasPressed(KEY_DIV)){
        Calc_InsertBinary(CALC_T_DIV); changed=1;
    }else if(Input_WasPressed(KEY_LPAREN)){
        Calc_InsertToken(CALC_T_LPAREN); changed=1;
    }else if(Input_WasPressed(KEY_RPAREN)){
        Calc_InsertToken(CALC_T_RPAREN); changed=1;
    }else if(Input_WasPressed(KEY_PERCENT)){
        insert_postfix(CALC_T_PERCENT); changed=1;
    }else if(Input_WasPressed(KEY_SQRT)){
        Calc_InsertFunction(CALC_T_SQRT); changed=1;
    }else if(Input_WasPressed(KEY_POW)){
        Calc_InsertBinary(CALC_T_POW); changed=1;
    }else if(Input_WasPressed(KEY_LOG)){
        Calc_InsertFunction(CALC_T_LN); changed=1;
    }else if(Input_WasPressed(KEY_DEL)){
        Calc_Backspace(); changed=1;
    }else if(Input_WasPressed(KEY_LEFT)){
        Calc_MoveLeft();
        redraw_expression(1);
        return;
    }else if(Input_WasPressed(KEY_RIGHT)){
        Calc_MoveRight();
        redraw_expression(1);
        return;
    }else if(Input_WasPressed(KEY_ENTER)){
        evaluate(status);
        return;
    }

    if(changed){
        uint8_t full=Calc_LastError()==CALC_ERR_FULL;
        expression_changed();
        if(full) GUI_DrawMessage(Calc_ErrorText(CALC_ERR_FULL));
    }
}
