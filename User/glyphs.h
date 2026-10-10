#ifndef __GLYPHS_H
#define __GLYPHS_H
#include <stdint.h>
#define GLYPH_PLUS_MINUS ((char)0x7f)
#define GLYPH_PI ((char)0x80)
#define GLYPH_DEGREE ((char)0x81)
enum{ FONT_SMALL=1,FONT_MATH,FONT_RESULT };
void Glyph_DrawChar(int16_t x,int16_t y,char c,uint8_t font,uint16_t color);
void Glyph_DrawTextRow(int16_t x,int16_t y,int16_t w,const char *s,int16_t offset,uint8_t row,uint8_t font,uint16_t color);
void Glyph_DrawText(int16_t x,int16_t y,const char *s,uint8_t font,uint16_t color);
int16_t Glyph_TextWidth(const char *s,uint8_t font);
uint8_t Glyph_FontHeight(uint8_t font);
void Glyph_SetBackground(uint16_t color);
void Glyph_TextInk(const char *s,uint8_t font,uint8_t *top,uint8_t *bottom);
void Glyph_SetClip(int16_t x,int16_t y,int16_t w,int16_t h);
void Glyph_ClearClip(void);
void Glyph_DrawPi(int16_t x,int16_t y,uint8_t font,uint16_t color);
void Glyph_DrawPlusMinus(int16_t x,int16_t y,uint8_t font,uint16_t color);
void Glyph_DrawDegree(int16_t x,int16_t y,uint8_t font,uint16_t color);
#endif
