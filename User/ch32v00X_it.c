#include <ch32v00X_it.h>
#include "power.h"

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void NMI_Handler(void){
    while(1);
}

void HardFault_Handler(void){
    SystemReset_StartMode(Start_Mode_USER);
    NVIC_SystemReset();
    while(1);
}

void EXTI7_0_IRQHandler(void){
    if(EXTI_GetITStatus(EXTI_Line5)!=RESET)
        Power_ButtonIRQHandler();
}