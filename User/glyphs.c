#include "glyphs.h"
#include "lcd.h"
#include "font_data.h"
static int16_t clip_x=0,clip_y=0,clip_w=240,clip_h=240;
static uint16_t background=0xffff;
static const char result_chars[]="0123456789.+-Ee ~";
static uint8_t index_for(char c,uint8_t *font){
    uint8_t u=(uint8_t)c,i;if(u==127)c='~';
    if(*font>=FONT_RESULT){
        for(i=0;result_chars[i];i++)if(c==result_chars[i])return i;
        *font=FONT_MATH;
    }
    if(c=='~')return 95;
    if(u>=32&&u<=129)return u-32;
    return '?'-32;
}
uint8_t Glyph_FontHeight(uint8_t font){return font==FONT_SMALL?FONT0_HEIGHT:font==FONT_MATH?FONT1_HEIGHT:FONT2_HEIGHT;}
static uint8_t width(char c,uint8_t font){
    uint8_t i=index_for(c,&font);return font==FONT_SMALL?font0_width[i]:font==FONT_MATH?font1_width[i]:font2_width[i];
}
static uint8_t glyph_top(uint8_t i,uint8_t font){return font==FONT_MATH?font1_tops[font1_bounds[i]>>4]:(font==FONT_SMALL?font0_bounds[i]:font2_bounds[i])>>4;}
static uint8_t inkrows(uint8_t i,uint8_t font){return font==FONT_MATH?font1_rows[font1_bounds[i]&15]:(font==FONT_SMALL?font0_bounds[i]:font2_bounds[i])&15;}
/* Four-glyph checkpoints preserve the original bitmap without a full offset table. */
static const uint8_t *data(uint8_t i,uint8_t font){
    const uint8_t *widths=font==FONT_SMALL?font0_width:font==FONT_MATH?font1_width:font2_width;
    uint16_t offset=font==FONT_SMALL?font0_offsets[i/4]:font==FONT_MATH?font1_offsets[i/4]:font2_offsets[i/4];
    for(uint8_t j=i&~3;j<i;j++)offset+=(widths[j]*inkrows(j,font)+3)/4;
    return (font==FONT_SMALL?font0_data:font==FONT_MATH?font1_data:font2_data)+offset;
}
static uint8_t coverage(const uint8_t *p,uint16_t pixel){return (p[pixel/4]>>((pixel%4)*2))&3;}
void Glyph_SetBackground(uint16_t c){background=c;}
void Glyph_SetClip(int16_t x,int16_t y,int16_t w,int16_t h){clip_x=x;clip_y=y;clip_w=w;clip_h=h;}
void Glyph_ClearClip(void){Glyph_SetClip(0,0,240,240);}
static uint16_t blend(uint16_t fg,uint8_t level){
    uint8_t r=(((fg>>11)&31)*level+((background>>11)&31)*(3-level)+1)/3;
    uint8_t g=(((fg>>5)&63)*level+((background>>5)&63)*(3-level)+1)/3;
    uint8_t b=((fg&31)*level+(background&31)*(3-level)+1)/3;
    return (r<<11)|(g<<5)|b;
}
void Glyph_DrawChar(int16_t x,int16_t y,char c,uint8_t font,uint16_t color){
    uint8_t i=index_for(c,&font),first=glyph_top(i,font),h=first+inkrows(i,font),w=width(c,font),r,k,start,level;
    const uint8_t *p=data(i,font);uint16_t colors[4]={background,blend(color,1),blend(color,2),color};
    for(r=first;r<h;r++){
        if(y+r<clip_y||y+r>=clip_y+clip_h)continue;
        for(k=0;k<w;){
            level=coverage(p,(r-first)*w+k);start=k++;
            while(k<w&&coverage(p,(r-first)*w+k)==level)k++;
            if(level){
                int16_t left=x+start,right=x+k;
                if(left<clip_x)left=clip_x;if(right>clip_x+clip_w)right=clip_x+clip_w;
                if(right>left)LCD_FillRect(left,y+r,right-left,1,colors[level]);
            }
        }
    }
}
void Glyph_DrawText(int16_t x,int16_t y,const char *s,uint8_t font,uint16_t color){while(*s){Glyph_DrawChar(x,y,*s,font,color);x+=width(*s++,font);}}
int16_t Glyph_TextWidth(const char *s,uint8_t font){int16_t w=0;while(*s)w+=width(*s++,font);return w;}
void Glyph_TextInk(const char *s,uint8_t font,uint8_t *top,uint8_t *bottom){
    *top=255;*bottom=0;
    while(*s){
        uint8_t f=font,i=index_for(*s++,&f),first=glyph_top(i,f);
        uint8_t h=inkrows(i,f);if(!h)continue;
        if(first<*top)*top=first;if(first+h-1>*bottom)*bottom=first+h-1;
    }if(*top==255)*top=0;
}
void Glyph_DrawPi(int16_t x,int16_t y,uint8_t font,uint16_t c){Glyph_DrawChar(x,y,GLYPH_PI,font,c);}
void Glyph_DrawPlusMinus(int16_t x,int16_t y,uint8_t font,uint16_t c){Glyph_DrawChar(x,y,GLYPH_PLUS_MINUS,font,c);}
void Glyph_DrawDegree(int16_t x,int16_t y,uint8_t font,uint16_t c){Glyph_DrawChar(x,y,GLYPH_DEGREE,font,c);}

/* Opaque scanline: every pixel is replaced once, never clear the text first. */
void Glyph_DrawTextRow(int16_t x,int16_t y,int16_t w,const char *s,int16_t offset,uint8_t row,uint8_t font,uint16_t color){
    uint16_t colors[4]={background,blend(color,1),blend(color,2),color};
    int16_t pos=0,start=0;uint8_t old=255;
    while(*s&&offset>=width(*s,font)){offset-=width(*s++,font);}
    while(pos<w){
        uint8_t level=0;
        if(*s){
            uint8_t f=font,i=index_for(*s,&f),first=glyph_top(i,f),h=inkrows(i,f),cw=width(*s,font);
            if(row>=first&&row<first+h)level=coverage(data(i,f),(row-first)*cw+offset);
            if(++offset>=cw){offset=0;s++;}
        }
        if(level!=old){if(pos>start)LCD_FillRect(x+start,y,pos-start,1,colors[old]);start=pos;old=level;}
        pos++;
    }
    if(pos>start)LCD_FillRect(x+start,y,pos-start,1,colors[old]);
}
