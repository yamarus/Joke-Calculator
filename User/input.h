#ifndef __INPUT_H
#define __INPUT_H

#include <stdint.h>

typedef enum{
    KEY_0=0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    KEY_DOT,
    KEY_PLUSMINUS,

    KEY_ADD,
    KEY_SUB,
    KEY_MUL,
    KEY_DIV,

    KEY_PERCENT,
    KEY_SQRT,
    KEY_POW,
    KEY_LOG,

    KEY_LPAREN,
    KEY_RPAREN,

    KEY_AC,
    KEY_DEL,
    KEY_ENTER,

    KEY_LEFT,
    KEY_RIGHT,

    KEY_SHIFT,
    KEY_MODE,

    KEY_POWER,

    KEY_COUNT
} KeyCode;

#define KEY_BIT(k) (1UL<<(k))

void Input_Init(void);
void Input_Update(void);

uint32_t Input_Down(void);
uint32_t Input_Pressed(void);
uint32_t Input_Released(void);

uint8_t Input_IsDown(KeyCode key);
uint8_t Input_WasPressed(KeyCode key);
uint8_t Input_WasReleased(KeyCode key);


#endif