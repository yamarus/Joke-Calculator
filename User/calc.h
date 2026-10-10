#ifndef __CALC_H
#define __CALC_H

#include <stdint.h>

#define CALC_MAX_TOKENS 64
#define CALC_MAX_NODES  32
#define CALC_INVALID     (-1)

typedef float CalcValue;
typedef uint8_t CalcToken;
typedef struct{ float value,sigma; } CalcUncertain;
enum{
    CALC_RISK_CANCEL=1, CALC_RISK_DIVISOR=2,
    CALC_RISK_GROWTH=4, CALC_RISK_PERCENT=8
};
typedef struct{
    uint32_t hash;
    uint8_t operations,flags;
    float cancellation,denominator_relative;
} CalcFeatures;

/* ASCII tokens where possible */
enum{
    CALC_T_ADD='+',
    CALC_T_SUB='-',
    CALC_T_MUL='*',
    CALC_T_DIV='/',
    CALC_T_POW='^',
    CALC_T_DOT='.',
    CALC_T_LPAREN='(',
    CALC_T_RPAREN=')',
    CALC_T_PERCENT='%',
    CALC_T_FACT='!',
    CALC_T_UNCERTAIN='~',
    CALC_T_HOLE='?',
    CALC_T_COMMA=',',

    /* Non-ASCII tokens */
    CALC_T_EE=0x80,
    CALC_T_SQRT,
    CALC_T_INV,
    CALC_T_LOG,
    CALC_T_LN,
    CALC_T_EXP,
    CALC_T_SIN,
    CALC_T_COS,
    CALC_T_TAN,
    CALC_T_ASIN,
    CALC_T_ACOS,
    CALC_T_ATAN,
    CALC_T_ABS,
    CALC_T_PI,
    CALC_T_E,
    CALC_T_DELTAPERCENT,
    CALC_T_RND,
    CALC_T_ANS,
    CALC_T_CAPTURED
};

enum{
    CALC_NODE_NUMBER=0,
    CALC_NODE_ADD,
    CALC_NODE_SUB,
    CALC_NODE_MUL,
    CALC_NODE_DIV,
    CALC_NODE_POW,
    CALC_NODE_NEG,
    CALC_NODE_SQRT,
    CALC_NODE_INV,
    CALC_NODE_PERCENT,
    CALC_NODE_FACT,
    CALC_NODE_LOG,
    CALC_NODE_LN,
    CALC_NODE_EXP,
    CALC_NODE_SIN,
    CALC_NODE_COS,
    CALC_NODE_TAN,
    CALC_NODE_ASIN,
    CALC_NODE_ACOS,
    CALC_NODE_ATAN,
    CALC_NODE_ABS,
    CALC_NODE_PI,
    CALC_NODE_E,
    CALC_NODE_ANS,
    CALC_NODE_RND,
    CALC_NODE_UNCERTAIN,
    CALC_NODE_DELTAPERCENT,
    CALC_NODE_HOLE,
    CALC_NODE_CAPTURED
};

typedef enum{
    CALC_OK=0,
    CALC_ERR_FULL,
    CALC_ERR_SYNTAX,
    CALC_ERR_DIV_ZERO,
    CALC_ERR_DOMAIN,
    CALC_ERR_UNSUPPORTED,
    CALC_ERR_DEPTH,
    CALC_ERR_MISSING
} CalcError;

typedef struct{
    uint8_t type;
    int8_t a,b;
    uint8_t start,len,from,to;
} CalcNode;

/* Optional input model; NULL preserves the ordinary evaluator and repeated-equals UX. */
typedef struct{
    float (*sigma)(uint8_t id,float value,uint32_t hash,void *context);
    /* Model may replace a node's outcome; ordinary evaluators have no hook. */
    void (*observe)(uint8_t id,CalcError *error,CalcUncertain *value,
                    CalcUncertain a,CalcUncertain b,void *context);
    void *context;
} CalcModel;
CalcError Calc_EvaluateModel(CalcUncertain *result,uint8_t degrees,
                            CalcFeatures *features,const CalcModel *model);

void Calc_Init(void);
void Calc_Clear(void);
void Calc_Edit(void);
uint8_t Calc_IsEvaluated(void);
uint8_t Calc_InsertFunction(CalcToken token);
uint8_t Calc_InsertBinary(CalcToken token);
uint8_t Calc_NextField(void);
uint8_t Calc_InsertSequence(const CalcToken *seq,uint8_t count);
/* Degrees is supplied from the authoritative application status. */
CalcError Calc_EvaluateUncertain(CalcUncertain *result,uint8_t degrees,
                                uint8_t uncertainty,CalcFeatures *features);
CalcUncertain Calc_Ans(void);
/* Scaled root-sum-square; avoids overflow from squaring large sigmas. */
float Calc_CombineSigma(float a,float b);

uint8_t Calc_InsertToken(CalcToken token);
uint8_t Calc_InsertDigit(uint8_t digit);
uint8_t Calc_Backspace(void);
uint8_t Calc_Delete(void);
uint8_t Calc_ToggleSign(void);

void Calc_MoveLeft(void);
void Calc_MoveRight(void);
void Calc_SetCursor(uint8_t pos);

uint8_t Calc_Count(void);
uint8_t Calc_Cursor(void);
const CalcToken *Calc_Tokens(void);

CalcError Calc_Parse(void);
CalcError Calc_ParseEditing(void);
CalcError Calc_Evaluate(CalcValue *result);
CalcError Calc_LastError(void);
const char *Calc_ErrorText(CalcError error);

int8_t Calc_Root(void);
uint8_t Calc_NodeCount(void);
const CalcNode *Calc_Nodes(void);
uint8_t Calc_CopyNodeText(int8_t id,char *dst,uint8_t size);
uint8_t Calc_FormatInput(char *dst,uint8_t size,uint8_t *cursor_char);
void Calc_FormatValueDigits(CalcValue value,char *dst,uint8_t size,uint8_t significant);
void Calc_FormatDisplay(CalcValue value,char *dst,uint8_t size,uint8_t significant,uint8_t scientific);
void Calc_FormatValue(CalcValue value,char *dst,uint8_t size);
void Calc_FormatFixed(CalcValue value,char *dst,uint8_t size,uint8_t places);
#endif
