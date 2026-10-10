#include "debug.h"
#include "app.h"

int main(void){
    SystemCoreClockUpdate();
    Delay_Init();
    SystemReset_StartMode(Start_Mode_USER);

    App_Init();

    while(1){
        App_Update();
        Delay_Ms(10);
    }
}