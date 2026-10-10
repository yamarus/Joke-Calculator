#include "lcd.h"
#include "debug.h"

#define LCD_DC_LOW()  GPIO_ResetBits(GPIOC,GPIO_Pin_7)
#define LCD_DC_HIGH() GPIO_SetBits(GPIOC,GPIO_Pin_7)
#define LCD_PWR_ON()  GPIO_ResetBits(GPIOD,GPIO_Pin_4)
#define LCD_PWR_OFF() GPIO_SetBits(GPIOD,GPIO_Pin_4)

static void spi_tx(uint8_t b){
    while(SPI_I2S_GetFlagStatus(SPI1,SPI_I2S_FLAG_TXE)==RESET);
    SPI_I2S_SendData(SPI1,b);
}

static void spi_wait(void){
    while(SPI_I2S_GetFlagStatus(SPI1,SPI_I2S_FLAG_BSY)==SET);
}

static void lcd_cmd(uint8_t c){
    spi_wait();
    LCD_DC_LOW();
    spi_tx(c);
    spi_wait();
}

static void lcd_cmd_data(uint8_t cmd,const uint8_t *data,uint8_t n){
    uint8_t i;
    lcd_cmd(cmd);
    LCD_DC_HIGH();
    for(i=0;i<n;i++) spi_tx(data[i]);
    spi_wait();
}

static const uint8_t init_seq[]={
    0xEF,0,0xEB,1,0x14,0xFE,0,0xEF,0,0xEB,1,0x14,
    0x84,1,0x40,0x85,1,0xFF,0x86,1,0xFF,0x87,1,0xFF,
    0x88,1,0x0A,0x89,1,0x21,0x8A,1,0x00,0x8B,1,0x80,
    0x8C,1,0x01,0x8D,1,0x01,0x8E,1,0xFF,0x8F,1,0xFF,
    0xB6,2,0x00,0x00,0x36,1,0x48,0x3A,1,0x05,
    0x90,4,0x08,0x08,0x08,0x08,0xBD,1,0x06,0xBC,1,0x00,
    0xFF,3,0x60,0x01,0x04,0xC3,1,0x13,0xC4,1,0x13,0xC9,1,0x22,
    0xBE,1,0x11,0xE1,2,0x10,0x0E,0xDF,3,0x21,0x0C,0x02,
    0xF0,6,0x45,0x09,0x08,0x08,0x26,0x2A,
    0xF1,6,0x43,0x70,0x72,0x36,0x37,0x6F,
    0xF2,6,0x45,0x09,0x08,0x08,0x26,0x2A,
    0xF3,6,0x43,0x70,0x72,0x36,0x37,0x6F,
    0xED,2,0x1B,0x0B,0xAE,1,0x77,0xCD,1,0x63,0xE8,1,0x34,
    0x62,12,0x18,0x0D,0x71,0xED,0x70,0x70,0x18,0x0F,0x71,0xEF,0x70,0x70,
    0x63,12,0x18,0x11,0x71,0xF1,0x70,0x70,0x18,0x13,0x71,0xF3,0x70,0x70,
    0x64,7,0x28,0x29,0xF1,0x01,0xF1,0x00,0x07,
    0x66,10,0x3C,0x00,0xCD,0x67,0x45,0x45,0x10,0x00,0x00,0x00,
    0x67,10,0x00,0x3C,0x00,0x00,0x00,0x01,0x54,0x10,0x32,0x98,
    0x74,7,0x10,0x85,0x80,0x00,0x00,0x4E,0x00,
    0x98,2,0x3E,0x07,0x35,0,0x21,0,0
};

static void lcd_window(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1){
    uint8_t d[4];
    d[0]=x0>>8; d[1]=x0; d[2]=x1>>8; d[3]=x1;
    lcd_cmd_data(0x2A,d,4);
    d[0]=y0>>8; d[1]=y0; d[2]=y1>>8; d[3]=y1;
    lcd_cmd_data(0x2B,d,4);
    lcd_cmd(0x2C);
}

void LCD_Power(uint8_t on){
    if(on) LCD_PWR_ON();
    else LCD_PWR_OFF();
}

void LCD_Init(void){
    GPIO_InitTypeDef gpio={0};
    SPI_InitTypeDef spi={0};
    const uint8_t *p=init_seq;
    uint8_t cmd,n;

    RCC_PB2PeriphClockCmd(
        RCC_PB2Periph_GPIOC|
        RCC_PB2Periph_GPIOD|
        RCC_PB2Periph_SPI1,
        ENABLE
    );

    /* Preload OFF before PD4 becomes an output. */
    GPIO_SetBits(GPIOD,GPIO_Pin_4);

    gpio.GPIO_Pin=GPIO_Pin_4;
    gpio.GPIO_Mode=GPIO_Mode_Out_PP;
    gpio.GPIO_Speed=GPIO_Speed_30MHz;
    GPIO_Init(GPIOD,&gpio);

    /* Guarantee a real LCD power-off interval on every reboot. */
    LCD_PWR_OFF();
    Delay_Ms(50);

    GPIO_ResetBits(GPIOC,GPIO_Pin_7);
    gpio.GPIO_Pin=GPIO_Pin_7;
    GPIO_Init(GPIOC,&gpio);

    gpio.GPIO_Pin=GPIO_Pin_5|GPIO_Pin_6;
    gpio.GPIO_Mode=GPIO_Mode_AF_PP;
    GPIO_Init(GPIOC,&gpio);

    SPI_I2S_DeInit(SPI1);

    spi.SPI_Direction=SPI_Direction_1Line_Tx;
    spi.SPI_Mode=SPI_Mode_Master;
    spi.SPI_DataSize=SPI_DataSize_8b;
    spi.SPI_CPOL=SPI_CPOL_Low;
    spi.SPI_CPHA=SPI_CPHA_1Edge;
    spi.SPI_NSS=SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler=SPI_BaudRatePrescaler_4;
    spi.SPI_FirstBit=SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial=7;

    SPI_Init(SPI1,&spi);
    SPI_NSSInternalSoftwareConfig(SPI1,SPI_NSSInternalSoft_Set);
    SPI_Cmd(SPI1,ENABLE);

    LCD_PWR_ON();
    Delay_Ms(50);

    lcd_cmd(0x01);
    Delay_Ms(150);

    while((cmd=*p++)!=0){
        n=*p++;
        lcd_cmd_data(cmd,p,n);
        p+=n;
    }

    lcd_cmd(0x11);
    Delay_Ms(120);
    lcd_cmd(0x29);
    Delay_Ms(20);
}

void LCD_FillRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t color){
    uint32_t i,n;
    uint8_t hi,lo;

    if(x<0){ w+=x; x=0; }
    if(y<0){ h+=y; y=0; }
    if(x+w>LCD_WIDTH) w=LCD_WIDTH-x;
    if(y+h>LCD_HEIGHT) h=LCD_HEIGHT-y;
    if(w<=0||h<=0) return;

    hi=color>>8;
    lo=color;
    n=(uint32_t)w*h;

    lcd_window(x,y,x+w-1,y+h-1);
    LCD_DC_HIGH();
    for(i=0;i<n;i++){
        spi_tx(hi);
        spi_tx(lo);
    }
    spi_wait();
}

void LCD_Shutdown(void){
    GPIO_InitTypeDef gpio={0};

    SPI_Cmd(SPI1,DISABLE);

    gpio.GPIO_Pin=GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7;
    gpio.GPIO_Mode=GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC,&gpio);

    LCD_PWR_OFF();
}

void LCD_Fill(uint16_t color){ LCD_FillRect(0,0,LCD_WIDTH,LCD_HEIGHT,color); }
void LCD_Pixel(int16_t x,int16_t y,uint16_t color){ LCD_FillRect(x,y,1,1,color); }
void LCD_HLine(int16_t x,int16_t y,int16_t w,uint16_t color){ LCD_FillRect(x,y,w,1,color); }
void LCD_VLine(int16_t x,int16_t y,int16_t h,uint16_t color){ LCD_FillRect(x,y,1,h,color); }