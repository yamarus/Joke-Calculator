#include "power.h"
#include "debug.h"

#define VREF_MV 1200UL
static volatile uint8_t power_short_press=0;

void Power_ButtonIRQInit(void){
    EXTI_InitTypeDef exti={0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_AFIO|RCC_PB2Periph_GPIOD,ENABLE);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOD,GPIO_PinSource5);

    EXTI_ClearITPendingBit(EXTI_Line5);

    exti.EXTI_Line=EXTI_Line5;
    exti.EXTI_Mode=EXTI_Mode_Interrupt;
    exti.EXTI_Trigger=EXTI_Trigger_Falling;
    exti.EXTI_LineCmd=ENABLE;
    EXTI_Init(&exti);

    NVIC_EnableIRQ(EXTI7_0_IRQn);
}

void Power_ButtonIRQHandler(void){
    uint16_t hold=0;

    /* Clear immediately so button bounce cannot leave a stale IRQ. */
    EXTI_ClearITPendingBit(EXTI_Line5);

    while(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_5)==Bit_RESET){
        Delay_Ms(10);

        if(++hold>=200){
            SystemReset_StartMode(Start_Mode_USER);
            NVIC_SystemReset();
            while(1);
        }
    }

    /* Ignore very short glitches/bounce. */
    if(hold>=2)
        power_short_press=1;
}

uint8_t Power_TakeShortPress(void){
    if(!power_short_press)
        return 0;

    power_short_press=0;
    return 1;
}


void Power_Init(void){
    ADC_InitTypeDef adc={0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_ADC1,ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    ADC_DeInit(ADC1);

    adc.ADC_Mode=ADC_Mode_Independent;
    adc.ADC_ScanConvMode=DISABLE;
    adc.ADC_ContinuousConvMode=DISABLE;
    adc.ADC_ExternalTrigConv=ADC_ExternalTrigConv_None;
    adc.ADC_DataAlign=ADC_DataAlign_Right;
    adc.ADC_NbrOfChannel=1;

    ADC_Init(ADC1,&adc);
    ADC_Cmd(ADC1,ENABLE);

    Delay_Ms(2);
}

static uint16_t read_vref(void){
    ADC_RegularChannelConfig(
        ADC1,
        ADC_Channel_Vrefint,
        1,
        ADC_SampleTime_CyclesMode7
    );

    ADC_SoftwareStartConvCmd(ADC1,ENABLE);

    while(ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC)==RESET);

    return ADC_GetConversionValue(ADC1);
}

uint16_t Power_ReadBatteryMv(void){
    uint32_t sum=0;
    uint16_t raw;
    uint8_t i;

    /* First conversion after channel selection can be discarded. */
    read_vref();

    for(i=0;i<16;i++)
        sum+=read_vref();

    raw=(uint16_t)((sum+8)/16);

    if(raw==0)
        return 0;

    return (uint16_t)((VREF_MV*4095UL+raw/2)/raw);
}

uint8_t Power_BatteryBars(uint16_t mv){
    if(mv>=3900) return 4;
    if(mv>=3700) return 3;
    if(mv>=3500) return 2;
    if(mv>=3300) return 1;
    return 0;
}

void Power_EnterStandby(void){
    GPIO_InitTypeDef gpio={0};
    EXTI_InitTypeDef exti={0};

    RCC_PB1PeriphClockCmd(RCC_PB1Periph_PWR,ENABLE);
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_AFIO|RCC_PB2Periph_GPIOD,ENABLE);

    gpio.GPIO_Pin=GPIO_Pin_5;
    gpio.GPIO_Mode=GPIO_Mode_IPU;
    GPIO_Init(GPIOD,&gpio);

    /*
     * While awake PD5 is an interrupt.
     * In standby it becomes a WFE wake event.
     */
    NVIC_DisableIRQ(EXTI7_0_IRQn);
    EXTI_ClearITPendingBit(EXTI_Line5);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOD,GPIO_PinSource5);

    exti.EXTI_Line=EXTI_Line5;
    exti.EXTI_Mode=EXTI_Mode_Event;
    exti.EXTI_Trigger=EXTI_Trigger_Falling;
    exti.EXTI_LineCmd=ENABLE;
    EXTI_Init(&exti);

    EXTI_ClearFlag(EXTI_Line5);

    ADC_Cmd(ADC1,DISABLE);
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_ADC1,DISABLE);

    SystemReset_StartMode(Start_Mode_USER);

    PWR_EnterSTANDBYMode(PWR_STANDBYEntry_WFE);

    /* Wake on next POWER press. */
    while(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_5)==Bit_RESET);

    SystemReset_StartMode(Start_Mode_USER);
    NVIC_SystemReset();
    while(1);
}