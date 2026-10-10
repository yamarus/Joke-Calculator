#include "settings.h"
#include "input.h"
#ifdef SETTINGS_FLASH_TEST
#include "ch32v00X_flash.h"
#include <string.h>
#endif
static AppSettings settings={2,1,0,ECON_ECONOMIST};
static uint8_t selected;
static const uint8_t counts[]={5,3,2,3};
static const char *const labels[]={"Auto-off","Digits","Notation","ECON style"};
static const char *const idle_names[]={"30 sec","1 min","2 min","5 min","Never"};
static const char *const digit_names[]={"2","4","6"};
static const char *const styles[]={"Economist","TV expert","Referee 2"};
#ifdef SETTINGS_FLASH_TEST
/* One reserved 256-byte page; SPL addresses use the FLASH_BASE alias. */
#ifdef SETTINGS_STORAGE_HOST
extern uint32_t settings_test_page[64];
#define STORE_ADDRESS ((uintptr_t)settings_test_page)
#else
#define STORE_ADDRESS (FLASH_BASE+0xf700u)
#endif
#define STORE_CHECK 0x53543101u
static uint8_t valid(AppSettings s){return s.idle<5&&s.digits<4&&s.scientific<2&&s.style<3;}
static void load(void){
    const volatile uint32_t *p=(const volatile uint32_t *)STORE_ADDRESS;
    union{uint32_t word;AppSettings value;}record;
    record.word=p[0];
    if(p[1]==(record.word^STORE_CHECK)&&valid(record.value)){
        settings=record.value;
        /* Accept the previous 8-digit index without rewriting flash at boot. */
        settings.digits-=settings.digits==3;
    }
}
static uint8_t save(void){
    uint32_t page[64];const volatile uint32_t *stored=(const volatile uint32_t *)STORE_ADDRESS;
    memcpy(&page[0],&settings,sizeof(settings));
    page[1]=page[0]^STORE_CHECK;
    memset(page+2,0xff,sizeof(page)-8);
    FLASH_Unlock_Fast();FLASH_ErasePage_Fast(STORE_ADDRESS);
    if(FLASH_ROM_WRITE(STORE_ADDRESS,page,sizeof(page))!=FLASH_COMPLETE)return 0;
    return stored[0]==page[0]&&stored[1]==page[1];
}
#endif
static void draw(void){
    const char *values[]={idle_names[settings.idle],digit_names[settings.digits],
                        settings.scientific?"Scientific":"Standard",styles[settings.style]};
    GUI_DrawSettings(labels,values,selected);
}
void Settings_Init(void){
    settings=(AppSettings){2,1,0,ECON_ECONOMIST};selected=0;
#ifdef SETTINGS_FLASH_TEST
    load();
#endif
}
const AppSettings *Settings_Get(void){return &settings;}
const char *Settings_StyleName(void){return styles[settings.style];}
uint8_t Settings_Digits(void){return 2+2*settings.digits;}
uint16_t Settings_IdleSeconds(void){static const uint16_t seconds[]={30,60,120,300,0};return seconds[settings.idle];}
void Settings_Format(float value,char *dst,uint8_t size,uint8_t digits){
    Calc_FormatDisplay(value,dst,size,digits?digits:Settings_Digits(),settings.scientific);
}
void Settings_Enter(GuiStatus *status){status->fn=0;GUI_DrawStatus(status);GUI_ClearMain();draw();}
void Settings_Update(GuiStatus *status){
    uint8_t *value=0,count=0;int8_t step=1;
    (void)status;
    if(Input_WasPressed(KEY_LEFT)){selected=(selected+3)%4;draw();return;}
    if(Input_WasPressed(KEY_RIGHT)){selected=(selected+1)%4;draw();return;}
    if(Input_WasPressed(KEY_SUB))step=-1;
    else if(!Input_WasPressed(KEY_ENTER)&&!Input_WasPressed(KEY_ADD))return;
    value=(uint8_t *)&settings+selected;count=counts[selected];
    *value=(*value+count+step)%count;draw();
#ifdef SETTINGS_FLASH_TEST
    if(!save())GUI_DrawMessage("Settings not saved");
#endif
}
