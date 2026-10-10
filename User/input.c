#include "input.h"
#include "debug.h"

#define NO_KEY 0xFF
#define DEBOUNCE_COUNT 3

static GPIO_TypeDef *row_port[6]={GPIOD,GPIOA,GPIOC,GPIOD,GPIOA,GPIOC};
static const uint16_t row_pin[6]={
    GPIO_Pin_6,GPIO_Pin_1,GPIO_Pin_0,
    GPIO_Pin_0,GPIO_Pin_2,GPIO_Pin_1
};

static GPIO_TypeDef *col_port[5]={GPIOC,GPIOC,GPIOC,GPIOD,GPIOD};
static const uint16_t col_pin[5]={
    GPIO_Pin_2,GPIO_Pin_3,GPIO_Pin_4,GPIO_Pin_2,GPIO_Pin_3
};

/* Physical matrix as actually wired on the rebuilt PCB. */
static const uint8_t keymap[6][5]={
    {KEY_MODE,KEY_RIGHT,KEY_LEFT,KEY_AC,NO_KEY},
    {KEY_SHIFT,KEY_LOG,KEY_POW,KEY_SQRT,KEY_PERCENT},
    {KEY_RPAREN,KEY_LPAREN,KEY_9,KEY_8,KEY_7},
    {KEY_DIV,KEY_MUL,KEY_6,KEY_5,KEY_4},
    {KEY_SUB,KEY_ADD,KEY_3,KEY_2,KEY_1},
    {KEY_ENTER,KEY_DEL,KEY_PLUSMINUS,KEY_DOT,KEY_0}
};

static uint32_t stable_mask=0;
static uint32_t candidate_mask=0;
static uint32_t pressed_mask=0;
static uint32_t released_mask=0;
static uint8_t debounce_counter=0;

static uint32_t scan_matrix(void){
    uint32_t mask=0;
    uint8_t r,c,i;

    for(r=0;r<6;r++){
        for(i=0;i<6;i++)
            GPIO_SetBits(row_port[i],row_pin[i]);

        GPIO_ResetBits(row_port[r],row_pin[r]);

        for(volatile uint16_t d=0;d<100;d++);

        for(c=0;c<5;c++){
            if(GPIO_ReadInputDataBit(col_port[c],col_pin[c])==Bit_RESET){
                uint8_t key=keymap[r][c];
                if(key!=NO_KEY)
                    mask|=KEY_BIT(key);
            }
        }
    }

    for(i=0;i<6;i++)
        GPIO_SetBits(row_port[i],row_pin[i]);

    /* Separate power button, PD5 active LOW. */
    if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_5)==Bit_RESET)
        mask|=KEY_BIT(KEY_POWER);

    return mask;
}

void Input_Init(void){
    GPIO_InitTypeDef gpio={0};
    uint8_t i;

    RCC_PB2PeriphClockCmd(
        RCC_PB2Periph_GPIOA|
        RCC_PB2Periph_GPIOC|
        RCC_PB2Periph_GPIOD,
        ENABLE
    );

    gpio.GPIO_Mode=GPIO_Mode_Out_OD;
    gpio.GPIO_Speed=GPIO_Speed_30MHz;

    for(i=0;i<6;i++){
        gpio.GPIO_Pin=row_pin[i];
        GPIO_Init(row_port[i],&gpio);
        GPIO_SetBits(row_port[i],row_pin[i]);
    }

    gpio.GPIO_Mode=GPIO_Mode_IPU;

    for(i=0;i<5;i++){
        gpio.GPIO_Pin=col_pin[i];
        GPIO_Init(col_port[i],&gpio);
    }

    gpio.GPIO_Pin=GPIO_Pin_5;
    GPIO_Init(GPIOD,&gpio);

    stable_mask=0;
    candidate_mask=0;
    pressed_mask=0;
    released_mask=0;
    debounce_counter=0;
}

void Input_Update(void){
    uint32_t raw=scan_matrix();

    pressed_mask=0;
    released_mask=0;

    if(raw!=candidate_mask){
        candidate_mask=raw;
        debounce_counter=0;
        return;
    }

    if(debounce_counter<DEBOUNCE_COUNT){
        debounce_counter++;
        if(debounce_counter<DEBOUNCE_COUNT)
            return;
    }

    if(candidate_mask!=stable_mask){
        pressed_mask=candidate_mask&~stable_mask;
        released_mask=stable_mask&~candidate_mask;
        stable_mask=candidate_mask;
    }
}

uint32_t Input_Down(void){return stable_mask;}
uint32_t Input_Pressed(void){return pressed_mask;}
uint32_t Input_Released(void){return released_mask;}
uint8_t Input_IsDown(KeyCode key){return !!(stable_mask&KEY_BIT(key));}
uint8_t Input_WasPressed(KeyCode key){return !!(pressed_mask&KEY_BIT(key));}
uint8_t Input_WasReleased(KeyCode key){return !!(released_mask&KEY_BIT(key));}