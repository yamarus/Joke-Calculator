#ifndef __LCD_H
#define __LCD_H

#include <stdint.h>

#define LCD_WIDTH  240
#define LCD_HEIGHT 240
#define LCD_RGB565(r,g,b) ((((uint16_t)(r)&0xF8)<<8)|(((uint16_t)(g)&0xFC)<<3)|((uint16_t)(b)>>3))

void LCD_Init(void);
void LCD_Power(uint8_t on);
void LCD_Fill(uint16_t color);
void LCD_Pixel(int16_t x,int16_t y,uint16_t color);
void LCD_FillRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t color);
void LCD_HLine(int16_t x,int16_t y,int16_t w,uint16_t color);
void LCD_VLine(int16_t x,int16_t y,int16_t h,uint16_t color);
void LCD_Shutdown(void);

#endif