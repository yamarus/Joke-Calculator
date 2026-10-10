#include "gui.h"
#include "lcd.h"
#include "glyphs.h"
#include "calc.h"
#include "settings.h"
#include <string.h>
#define COL_MAIN LCD_RGB565(243,246,249)
#define COL_PANEL LCD_RGB565(255,255,255)
#define COL_TEXT LCD_RGB565(25,35,49)
#define COL_DIM LCD_RGB565(82,96,111)
#define COL_LINE LCD_RGB565(206,214,222)
#define COL_EMPTY LCD_RGB565(239,242,245)
#define COL_ERROR LCD_RGB565(190,57,62)
#define VIEW_X 30
#define VIEW_Y 76
#define VIEW_W 180
#define VIEW_H 80
static GuiMode screen_mode;
static uint16_t previous_status;
static uint8_t fn_active,error_active;
static uint16_t accent;
static int16_t scroll_x,scroll_y;
typedef struct{ int16_t w,h,base; } Metrics;
typedef struct{ int8_t id; uint8_t font,framed; int16_t x,y; } DrawTask;
static Metrics metrics[2][CALC_MAX_NODES];
static const CalcNode *tree;
static int8_t active;
static uint8_t edit_view;
static int16_t active_x,active_y,active_h,caret_rank;
static char message_text[96];
static int16_t message_width,message_offset;
static uint16_t message_ticks;
static uint8_t message_scroll;

static void centered(const char *s,int16_t y,uint8_t font,uint16_t c){ Glyph_DrawText(120-Glyph_TextWidth(s,font)/2,y,s,font,c); }
static void right_text(const char *s,int16_t right,int16_t y,uint8_t font,uint16_t c){ Glyph_DrawText(right-Glyph_TextWidth(s,font),y,s,font,c); }
static void round_rect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c){
    LCD_FillRect(x+4,y,w-8,1,c); LCD_FillRect(x+2,y+1,w-4,2,c);
    LCD_FillRect(x+1,y+3,w-2,2,c); LCD_FillRect(x,y+5,w,h-10,c);
    LCD_FillRect(x+1,y+h-5,w-2,2,c); LCD_FillRect(x+2,y+h-3,w-4,2,c);
    LCD_FillRect(x+4,y+h-1,w-8,1,c);
}
static void card(void){
    LCD_FillRect(17,54,206,110,COL_MAIN); round_rect(17,54,206,110,COL_PANEL);
    Glyph_SetBackground(COL_PANEL);
    if(error_active)Glyph_DrawText(26,54,"CHECK INPUT",FONT_SMALL,COL_ERROR);
    if(error_active)LCD_FillRect(26,73,188,1,COL_ERROR);
}
static void rect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c){
    if(x<VIEW_X){ w-=VIEW_X-x;x=VIEW_X; } if(y<VIEW_Y){ h-=VIEW_Y-y;y=VIEW_Y; }
    if(x+w>VIEW_X+VIEW_W) w=VIEW_X+VIEW_W-x;
    if(y+h>VIEW_Y+VIEW_H) h=VIEW_Y+VIEW_H-y;
    if(w>0&&h>0) LCD_FillRect(x,y,w,h,c);
}
static void line(int16_t x,int16_t y,int16_t to_x,int16_t to_y){
    int16_t dx=to_x>x?to_x-x:x-to_x,dy=to_y>y?y-to_y:to_y-y;
    int16_t sx=x<to_x?1:-1,sy=y<to_y?1:-1,err=dx+dy;
    for(;;){int16_t e=2*err;rect(x,y,2,1,COL_TEXT);if(x==to_x&&y==to_y)break;
        if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
}
static Metrics metric(int8_t id,uint8_t font){ Metrics empty={16,Glyph_FontHeight(font),Glyph_FontHeight(font)-4}; return id>=0?metrics[font-1][(uint8_t)id]:empty; }
static Metrics field(int8_t id,uint8_t font){ Metrics m=metric(id,font); m.w+=8;m.h+=6;m.base+=3; return m; }
static const char *name(uint8_t type){
    switch(type){
        case CALC_NODE_LN:return "ln";case CALC_NODE_EXP:return "exp";
        case CALC_NODE_SIN:return "sin";case CALC_NODE_COS:return "cos";case CALC_NODE_TAN:return "tan";
        case CALC_NODE_ASIN:return "asin";case CALC_NODE_ACOS:return "acos";case CALC_NODE_ATAN:return "atan";
        case CALC_NODE_RND:return "RND";default:return "";
    }
}
static uint8_t leaf(uint8_t type){ return type==CALC_NODE_NUMBER||type==CALC_NODE_PI||type==CALC_NODE_E||type==CALC_NODE_ANS||type==CALC_NODE_HOLE||type==CALC_NODE_CAPTURED; }
static void leaf_text(int8_t id,char *s){
    if(tree[(uint8_t)id].type==CALC_NODE_PI){ s[0]=GLYPH_PI;s[1]=0; }
    else Calc_CopyNodeText(id,s,65);
}
static uint8_t precedence(uint8_t type){
    if(type==CALC_NODE_ADD||type==CALC_NODE_SUB||type==CALC_NODE_DELTAPERCENT) return 1;
    if(type==CALC_NODE_MUL||type==CALC_NODE_DIV) return 2;
    if(type==CALC_NODE_NEG) return 3;
    if(type==CALC_NODE_POW) return 4;
    return 5;
}
static uint8_t parens(int8_t child,uint8_t parent,uint8_t right){
    uint8_t t=tree[(uint8_t)child].type;
    return precedence(t)<precedence(parent)||(parent==CALC_NODE_SUB&&right&&(t==CALC_NODE_ADD||t==CALC_NODE_SUB));
}
static Metrics wrapped(int8_t child,uint8_t font,uint8_t parent,uint8_t right){
    Metrics m=metric(child,font); if(parens(child,parent,right)) m.w+=Glyph_TextWidth("()",font); return m;
}
static void measure(void){
    char text[65]; uint8_t font,i;
    for(font=FONT_SMALL;font<=FONT_MATH;font++) for(i=0;i<Calc_NodeCount();i++){
        const CalcNode *n=&tree[i]; Metrics m={0,0,0},a=field(n->a,font),b=field(n->b,font);
        int16_t fh=Glyph_FontHeight(font),op=Glyph_TextWidth("+",font)+8;
        if(leaf(n->type)){
            leaf_text(i,text);m.w=n->type==CALC_NODE_HOLE?16:Glyph_TextWidth(text,font); m.h=fh;m.base=fh-4;
        }else switch(n->type){
            case CALC_NODE_DIV: m.w=(a.w>b.w?a.w:b.w)+4;m.h=a.h+b.h+5;m.base=a.h+2;break;
            case CALC_NODE_INV: m.w=a.w+4;m.h=fh+a.h+5;m.base=fh+2;break;
            case CALC_NODE_POW:
                b=field(n->b,FONT_SMALL);if(parens(n->a,n->type,0)){a.w+=Glyph_TextWidth("()",font);}
                m.w=a.w+b.w+3;m.h=a.h+b.h/2;m.base=a.base+b.h/2;break;
            case CALC_NODE_SQRT: m=a;m.w+=16;m.h+=3;m.base+=3;break;
            case CALC_NODE_LOG:
                a=field(n->a,FONT_SMALL);m.w=Glyph_TextWidth("log",font)+a.w+b.w+7+Glyph_TextWidth("()",font);
                m.h=b.h>fh/2+a.h?b.h:fh/2+a.h;m.base=b.base;break;
            case CALC_NODE_ABS: m=a;m.w+=10;break;
            case CALC_NODE_NEG: m=wrapped(n->a,font,n->type,0);m.w+=Glyph_TextWidth("-",font);break;
            case CALC_NODE_FACT:case CALC_NODE_PERCENT: m=a;if(parens(n->a,n->type,0))m.w+=Glyph_TextWidth("()",font);m.w+=Glyph_TextWidth(n->type==CALC_NODE_FACT?"!":"%",font);break;
            case CALC_NODE_ADD:case CALC_NODE_SUB:case CALC_NODE_MUL:
                a=wrapped(n->a,font,n->type,0);b=wrapped(n->b,font,n->type,1);
                m.base=a.base>b.base?a.base:b.base;m.h=m.base+((a.h-a.base)>(b.h-b.base)?a.h-a.base:b.h-b.base);m.w=a.w+b.w+op;break;
            case CALC_NODE_UNCERTAIN:case CALC_NODE_DELTAPERCENT:
                m.base=a.base>b.base?a.base:b.base;m.h=m.base+((a.h-a.base)>(b.h-b.base)?a.h-a.base:b.h-b.base);
                m.w=a.w+b.w+(n->type==CALC_NODE_UNCERTAIN?Glyph_TextWidth("~",font):Glyph_TextWidth("d%",font))+8;break;
            case CALC_NODE_RND:
                m.w=Glyph_TextWidth("RND",font)+a.w+b.w+7+Glyph_TextWidth("(,)",font);m.base=a.base>b.base?a.base:b.base;
                m.h=m.base+((a.h-a.base)>(b.h-b.base)?a.h-a.base:b.h-b.base);break;
            default:m=a;m.w+=Glyph_TextWidth(name(n->type),font)+Glyph_TextWidth("()",font)+5;break;
        }
        metrics[font-1][i]=m;
    }
}
static uint8_t inside(int8_t id){
    const CalcNode *n=&tree[(uint8_t)id];uint8_t lo=n->from,hi=n->to;
    if(lo<hi&&Calc_Tokens()[lo]=='('){lo++;hi--;}
    return edit_view&&(n->type!=CALC_NODE_HOLE||Calc_Cursor()==n->start)&&Calc_Cursor()>=lo&&Calc_Cursor()<=hi;
}
static uint8_t empty_field(int8_t id){
    const CalcNode *n=&tree[(uint8_t)id];uint8_t i;
    if(n->type==CALC_NODE_HOLE)return 1;
    for(i=n->from;i<n->to;i++)if(Calc_Tokens()[i]==CALC_T_HOLE)return 1;
    return 0;
}
static void box_extra(int8_t id,uint8_t font,int16_t x,int16_t y,int16_t extra){
    Metrics m=field(id,font);uint8_t selected=inside(id),empty=empty_field(id);m.w+=extra;
    uint16_t color=error_active&&(empty||selected)?COL_ERROR:selected?accent:COL_LINE;
    if(!empty&&!selected)return;
    if(tree[(uint8_t)id].type==CALC_NODE_HOLE)rect(x+1,y+1,m.w-2,m.h-2,COL_EMPTY);
    rect(x,y,m.w,1,color);rect(x,y+m.h-1,m.w,1,color);rect(x,y,1,m.h,color);rect(x+m.w-1,y,1,m.h,color);
}
static void caret(int16_t rank,int16_t x,int16_t y,int16_t height){
    if(rank>caret_rank){caret_rank=rank;active_x=x;active_y=y;active_h=height;}
}
static void box(int8_t id,uint8_t font,int16_t x,int16_t y){box_extra(id,font,x,y,0);}
static void push(DrawTask *tasks,uint8_t *count,int8_t id,uint8_t font,int16_t x,int16_t y){
    if(id>=0&&*count<CALC_MAX_NODES) tasks[(*count)++]=(DrawTask){id,font,0,x,y};
}
static void argument(DrawTask *tasks,uint8_t *count,int8_t id,uint8_t font,int16_t x,int16_t y,uint8_t paint){
    if(paint)box(id,font,x,y);push(tasks,count,id,font,x+4,y+3);if(id>=0&&*count)tasks[*count-1].framed=1;
}
static void function_argument(DrawTask *tasks,uint8_t *count,int8_t id,uint8_t font,int16_t x,int16_t y,uint8_t paint){
    Metrics a=field(id,font);int16_t cw=Glyph_TextWidth("(",font);
    if(paint){Glyph_DrawChar(x,y+a.base-Glyph_FontHeight(font)+4,'(',font,COL_TEXT);Glyph_DrawChar(x+cw+a.w+2,y+a.base-Glyph_FontHeight(font)+4,')',font,COL_TEXT);}
    argument(tasks,count,id,font,x+cw,y,paint);
}
static void wrap_task(DrawTask *tasks,uint8_t *count,int8_t id,uint8_t font,int16_t x,int16_t y,uint8_t parent,uint8_t right,uint8_t paint){
    if(parens(id,parent,right)){
        Metrics m=metric(id,font);int16_t cw=Glyph_TextWidth("(",font);
        if(paint){Glyph_DrawChar(x,y+m.base-Glyph_FontHeight(font)+4,'(',font,COL_TEXT);Glyph_DrawChar(x+cw+m.w,y+m.base-Glyph_FontHeight(font)+4,')',font,COL_TEXT);}
        x+=cw;
    }push(tasks,count,id,font,x,y);
}
static void wrapped_argument(DrawTask *tasks,uint8_t *count,int8_t id,uint8_t font,int16_t x,int16_t y,uint8_t parent,uint8_t paint){
    int16_t extra=parens(id,parent,0)?Glyph_TextWidth("()",font):0;
    if(paint)box_extra(id,font,x,y,extra);
    wrap_task(tasks,count,id,font,x+4,y+3,parent,0,paint);if(*count)tasks[*count-1].framed=1;
}
/* Iterative layout/drawing keeps nested field rendering off the MCU call stack. */
static void layout(int8_t root,int16_t origin_x,int16_t origin_y,uint8_t paint){
    DrawTask tasks[CALC_MAX_NODES]; uint8_t count=0;
    char text[65];push(tasks,&count,root,FONT_MATH,origin_x,origin_y);
    while(count){
        DrawTask t=tasks[--count];const CalcNode *n=&tree[(uint8_t)t.id];
        Metrics m=metric(t.id,t.font),a=field(n->a,t.font),b=field(n->b,t.font);
        int16_t x=t.x,y=t.y,fh=Glyph_FontHeight(t.font),lw;
        uint8_t pos=Calc_Cursor();
        if(pos==n->from)caret(200+n->to-n->from,x-3,y+m.base-fh+4,fh);
        if(pos==n->to&&(!leaf(n->type)||pos!=n->start+n->len))
            caret(300+n->to-n->from,x+m.w+5,y+m.base-fh+4,fh);
        if(leaf(n->type)){
            leaf_text(t.id,text);
            if(n->type==CALC_NODE_HOLE&&pos==n->start)caret(1000,x,y,m.h);
            else if(n->type==CALC_NODE_HOLE&&n->len&&pos==n->start+n->len)caret(600,x+m.w+5,y,m.h);
            else if(n->type!=CALC_NODE_HOLE&&pos>=n->start&&pos<=n->start+n->len){
                uint8_t chars=n->type==CALC_NODE_NUMBER?pos-n->start:pos>n->start?strlen(text):0;
                if(chars>strlen(text))chars=strlen(text);
                char saved=text[chars];text[chars]=0;
                caret(500,x+Glyph_TextWidth(text,t.font),y,m.h);text[chars]=saved;
            }
            if(paint&&n->type!=CALC_NODE_HOLE)Glyph_DrawText(x,y,text,t.font,COL_TEXT);
            if(paint&&n->type==CALC_NODE_HOLE&&!t.framed)box(t.id,t.font,x-4,y-3);
            continue;
        }
        if(pos>n->from&&pos<n->to&&n->a>=0&&pos<tree[(uint8_t)n->a].from){
            int16_t offset=n->type==CALC_NODE_SQRT?16:n->type==CALC_NODE_ABS?5:Glyph_TextWidth(name(n->type),t.font)+3;
            caret(100-(n->to-n->from),x+offset-2,y+m.base-fh+4,fh);
        }
        switch(n->type){
            case CALC_NODE_DIV:
                argument(tasks,&count,n->a,t.font,x+(m.w-a.w)/2,y,paint);
                argument(tasks,&count,n->b,t.font,x+(m.w-b.w)/2,y+a.h+5,paint);
                if(paint) rect(x,y+a.h+2,m.w,1,COL_TEXT);break;
            case CALC_NODE_INV:
                if(paint){Glyph_DrawText(x+(m.w-Glyph_TextWidth("1",t.font))/2,y,"1",t.font,COL_TEXT);rect(x,y+fh+2,m.w,1,COL_TEXT);}
                argument(tasks,&count,n->a,t.font,x+(m.w-a.w)/2,y+fh+5,paint);break;
            case CALC_NODE_POW:
                b=field(n->b,FONT_SMALL);
                if(parens(n->a,n->type,0))a.w+=Glyph_TextWidth("()",t.font);
                wrapped_argument(tasks,&count,n->a,t.font,x,y+b.h/2,n->type,paint);
                argument(tasks,&count,n->b,FONT_SMALL,x+a.w+3,y,paint);break;
            case CALC_NODE_SQRT:
                argument(tasks,&count,n->a,t.font,x+16,y+3,paint);
                if(paint){line(x,y+m.h/2,x+3,y+m.h/2-2);line(x+3,y+m.h/2-2,x+7,y+m.h-2);line(x+7,y+m.h-2,x+13,y);rect(x+13,y,m.w-13,1,COL_TEXT);}break;
            case CALC_NODE_LOG:
                a=field(n->a,FONT_SMALL);lw=Glyph_TextWidth("log",t.font);
                if(paint) Glyph_DrawText(x,y+m.base-fh+4,"log",t.font,COL_TEXT);
                argument(tasks,&count,n->a,FONT_SMALL,x+lw,y+fh/2,paint);
                function_argument(tasks,&count,n->b,t.font,x+lw+a.w+5,y+m.base-b.base,paint);break;
            case CALC_NODE_ABS:
                if(paint){rect(x,y,2,a.h,COL_TEXT);rect(x+a.w+8,y,2,a.h,COL_TEXT);}
                argument(tasks,&count,n->a,t.font,x+5,y,paint);break;
            case CALC_NODE_NEG:
                if(paint) Glyph_DrawText(x,y+m.base-fh+4,"-",t.font,COL_TEXT);
                wrap_task(tasks,&count,n->a,t.font,x+Glyph_TextWidth("-",t.font),y,n->type,0,paint);break;
            case CALC_NODE_FACT:case CALC_NODE_PERCENT:
                if(parens(n->a,n->type,0))a.w+=Glyph_TextWidth("()",t.font);
                wrapped_argument(tasks,&count,n->a,t.font,x,y,n->type,paint);
                if(paint) Glyph_DrawChar(x+a.w,y+m.base-fh+4,n->type==CALC_NODE_FACT?'!':'%',t.font,COL_TEXT);break;
            case CALC_NODE_ADD:case CALC_NODE_SUB:case CALC_NODE_MUL:
                a=wrapped(n->a,t.font,n->type,0);b=wrapped(n->b,t.font,n->type,1);
                wrap_task(tasks,&count,n->a,t.font,x,y+m.base-a.base,n->type,0,paint);
                wrap_task(tasks,&count,n->b,t.font,x+a.w+Glyph_TextWidth("+",t.font)+8,y+m.base-b.base,n->type,1,paint);
                if(paint) Glyph_DrawChar(x+a.w+4,y+m.base-fh+4,n->type==CALC_NODE_ADD?'+':n->type==CALC_NODE_SUB?'-':'*',t.font,COL_TEXT);break;
            case CALC_NODE_UNCERTAIN:case CALC_NODE_DELTAPERCENT:
                lw=Glyph_TextWidth(n->type==CALC_NODE_UNCERTAIN?"~":"d%",t.font);
                argument(tasks,&count,n->a,t.font,x,y+m.base-a.base,paint);
                argument(tasks,&count,n->b,t.font,x+a.w+lw+8,y+m.base-b.base,paint);
                if(paint) Glyph_DrawText(x+a.w+4,y+m.base-fh+4,n->type==CALC_NODE_UNCERTAIN?"~":"d%",t.font,COL_TEXT);break;
            case CALC_NODE_RND:{
                int16_t cw=Glyph_TextWidth("(",t.font),comma=Glyph_TextWidth(",",t.font);
                lw=Glyph_TextWidth("RND",t.font);
                if(paint){
                    Glyph_DrawText(x,y+m.base-fh+4,"RND",t.font,COL_TEXT);
                    Glyph_DrawChar(x+lw+3,y+m.base-fh+4,'(',t.font,COL_TEXT);
                    Glyph_DrawChar(x+lw+3+cw+a.w,y+m.base-fh+4,',',t.font,COL_TEXT);
                    Glyph_DrawChar(x+lw+7+cw+a.w+comma+b.w,y+m.base-fh+4,')',t.font,COL_TEXT);
                }
                argument(tasks,&count,n->a,t.font,x+lw+3+cw,y+m.base-a.base,paint);
                argument(tasks,&count,n->b,t.font,x+lw+5+cw+a.w+comma,y+m.base-b.base,paint);break;
            }
            default:
                lw=Glyph_TextWidth(name(n->type),t.font);
                if(paint) Glyph_DrawText(x,y+m.base-fh+4,name(n->type),t.font,COL_TEXT);
                function_argument(tasks,&count,n->a,t.font,x+lw+3,y,paint);break;
        }
    }
}
static void arrow(int16_t x,int16_t y,uint8_t right){
    uint8_t i;for(i=0;i<5;i++) LCD_FillRect(x+(right?i:4-i),y+i,3,11-2*i,COL_TEXT);
}
static void vertical_arrow(int16_t x,int16_t y,uint8_t down){
    uint8_t i;for(i=0;i<5;i++)LCD_FillRect(x+i,y+(down?i:4-i),11-2*i,3,COL_TEXT);
}
static void expression(void){
    uint8_t i;int16_t x,y; Metrics root;
    edit_view=!Calc_IsEvaluated();card();
    if(!Calc_Count()){centered("Enter calculation",99,FONT_MATH,COL_DIM);Glyph_SetBackground(COL_MAIN);return;}
    if(Calc_ParseEditing()!=CALC_OK){
        char text[96];uint8_t c=0;Calc_FormatInput(text,sizeof(text),&c);
        /* Malformed sequences retain a usable cursor instead of guessing a formula. */
        Glyph_SetClip(VIEW_X,VIEW_Y,VIEW_W,VIEW_H);
        char saved=text[c];text[c]=0;int16_t caret=Glyph_TextWidth(text,FONT_MATH);text[c]=saved;
        int16_t left=VIEW_X+(caret>VIEW_W-12?VIEW_W-12-caret:0);
        Glyph_DrawText(left,108,text,FONT_MATH,COL_TEXT);rect(left+caret,109,2,Glyph_FontHeight(FONT_MATH)-2,accent);
        Glyph_ClearClip();Glyph_SetBackground(COL_MAIN);if(left<VIEW_X)arrow(17,116,0);
        if(left+Glyph_TextWidth(text,FONT_MATH)>VIEW_X+VIEW_W)arrow(215,116,1);return;
    }
    tree=Calc_Nodes();active=-1;
    for(i=0;i<Calc_NodeCount();i++)if(leaf(tree[i].type)&&inside(i))active=i;
    if(edit_view&&active>=0){
        const char *hint=0;
        for(i=0;i<Calc_NodeCount();i++){
            uint8_t a=tree[i].a==active,b=tree[i].b==active;
            if(!a&&!b)continue;
            if(tree[i].type==CALC_NODE_LOG)hint=a?"BASE":"VALUE";
            else if(tree[i].type==CALC_NODE_RND)hint=a?"MIN":"MAX";
            else if(tree[i].type==CALC_NODE_DIV)hint=a?"NUMERATOR":"DENOMINATOR";
            else if(tree[i].type==CALC_NODE_UNCERTAIN)hint=a?"VALUE":"UNCERTAINTY";
            else if(tree[i].type==CALC_NODE_POW)hint=a?"BASE":"POWER";
            else if(tree[i].type==CALC_NODE_SQRT||tree[i].type==CALC_NODE_ABS||*name(tree[i].type))hint="VALUE";
            if(hint)break;
        }
        if(hint)right_text(hint,209,54,FONT_SMALL,accent);
    }
    measure();root=metric(Calc_Root(),FONT_MATH);
    active_x=active_y=0;active_h=23;caret_rank=-32767;layout(Calc_Root(),0,0,0);
    root.w+=10;
    if(root.w<=VIEW_W){scroll_x=0;x=120-root.w/2;}
    else{
        if(active_x<scroll_x+12)scroll_x=active_x-12;
        if(active_x>scroll_x+VIEW_W-12)scroll_x=active_x-VIEW_W+12;
        if(scroll_x>root.w-VIEW_W)scroll_x=root.w-VIEW_W;if(scroll_x<0)scroll_x=0;
        x=VIEW_X-scroll_x;
    }
    if(root.h<=VIEW_H){scroll_y=0;y=VIEW_Y+(VIEW_H-root.h)/2;}
    else{
        if(active_y<scroll_y+4)scroll_y=active_y-4;
        if(active_y+active_h>scroll_y+VIEW_H-4)scroll_y=active_y+active_h-VIEW_H+4;
        if(scroll_y>root.h-VIEW_H)scroll_y=root.h-VIEW_H;if(scroll_y<0)scroll_y=0;
        y=VIEW_Y-scroll_y;
    }
    caret_rank=-32767;Glyph_SetClip(VIEW_X,VIEW_Y,VIEW_W,VIEW_H);layout(Calc_Root(),x,y,1);if(edit_view)rect(active_x,active_y+1,2,active_h-2,error_active?COL_ERROR:accent);Glyph_ClearClip();Glyph_SetBackground(COL_MAIN);
    if(scroll_x>0)arrow(17,116,0);
    if(scroll_x+VIEW_W<root.w)arrow(215,116,1);
    if(scroll_y>0)vertical_arrow(116,70,0);
    if(scroll_y+VIEW_H<root.h)vertical_arrow(116,157,1);
}
void GUI_Init(void){
    previous_status=0xffff;screen_mode=GUI_MODE_NORMAL;fn_active=error_active=0;accent=LCD_RGB565(22,126,117);
    scroll_x=scroll_y=0;Glyph_ClearClip();Glyph_SetBackground(COL_MAIN);LCD_Fill(COL_MAIN);
    GUI_ClearExpression();GUI_ClearResult();GUI_ClearMessage();
}
void GUI_ClearMain(void){LCD_FillRect(12,51,216,183,COL_MAIN);GUI_ClearExpression();GUI_ClearResult();GUI_ClearMessage();}
void GUI_DrawStatus(const GuiStatus *s){
    uint16_t key=s->battery|(s->mode<<3)|(s->angle<<5)|(s->fn<<6)|
                 ((s->mode==GUI_MODE_ECO?Settings_Get()->style:0)<<7);
    if(key==previous_status)return;
    previous_status=key;
    uint8_t i,top,bottom;uint16_t battery=s->battery>=3?LCD_RGB565(42,145,83):s->battery==2?LCD_RGB565(200,144,25):LCD_RGB565(202,57,55);
    const char *mode=s->mode==GUI_MODE_ECO?"ECONOMICS":s->mode==GUI_MODE_ERROR?"UNCERTAINTY":s->mode==GUI_MODE_SETTINGS?"SETTINGS":"NORMAL";
    screen_mode=s->mode;fn_active=s->fn;
    accent=s->mode==GUI_MODE_ECO?LCD_RGB565(200,83,36):s->mode==GUI_MODE_ERROR?LCD_RGB565(53,104,181):LCD_RGB565(22,126,117);
    Glyph_SetBackground(COL_MAIN);LCD_FillRect(37,12,166,39,COL_MAIN);
    round_rect(60,14,120,22,accent);Glyph_TextInk(mode,FONT_SMALL,&top,&bottom);
    Glyph_SetBackground(accent);centered(mode,14+(22-(bottom-top+1))/2-top,FONT_SMALL,COL_PANEL);Glyph_SetBackground(COL_MAIN);
    LCD_FillRect(48,42,20,8,COL_DIM);LCD_FillRect(68,44,2,4,COL_DIM);LCD_FillRect(49,43,18,6,COL_MAIN);
    for(i=0;i<s->battery&&i<4;i++)LCD_FillRect(50+i*4,44,3,4,battery);
    if(!s->battery)LCD_FillRect(50,44,2,4,battery);
    if(fn_active){
        Glyph_DrawText(167,34,"FN",FONT_SMALL,accent);Glyph_DrawText(168,34,"FN",FONT_SMALL,accent);
    }else Glyph_DrawText(167,34,s->angle==GUI_ANGLE_DEG?"DEG":"RAD",FONT_SMALL,COL_DIM);
    if(s->mode==GUI_MODE_ECO)centered(Settings_StyleName(),34,FONT_SMALL,COL_DIM);
}
void GUI_ClearExpression(void){error_active=0;edit_view=1;card();centered("Enter calculation",99,FONT_MATH,COL_DIM);Glyph_SetBackground(COL_MAIN);}
void GUI_DrawExpression(int8_t root){(void)root;expression();}
void GUI_DrawEditor(const char *text,uint8_t cursor){(void)text;(void)cursor;expression();}
void GUI_ClearResult(void){
    error_active=0;Glyph_SetBackground(COL_MAIN);LCD_FillRect(28,164,184,27,COL_MAIN);
    right_text(Calc_Count()?"--":"0",198,165,FONT_RESULT,COL_DIM);
}
void GUI_DrawResult(const char *text){
    uint8_t font=FONT_RESULT;GUI_ClearResult();LCD_FillRect(28,164,184,27,COL_MAIN);
    if(Glyph_TextWidth(text,font)>168)font=FONT_SMALL;
    right_text(text,198,165,font,COL_TEXT);
}
void GUI_DrawUncertain(float value,float sigma){
    char v[24],s[24],text[51];uint8_t i,j,font=FONT_RESULT,digits=Settings_Digits(),compact=0;
    /* Keep small ECON revisions visible even when the global precision is two. */
    if(screen_mode==GUI_MODE_ECO&&digits<4)digits=4;
    do{
        if(compact){Calc_FormatDisplay(value,v,sizeof(v),digits,1);Calc_FormatDisplay(sigma,s,sizeof(s),digits,1);}
        else if(screen_mode!=GUI_MODE_ECO&&!Settings_Get()->scientific){
            Calc_FormatFixed(value,v,sizeof(v),digits);Calc_FormatFixed(sigma,s,sizeof(s),digits);
        }else{Settings_Format(value,v,sizeof(v),digits);Settings_Format(sigma,s,sizeof(s),digits);}
        i=0;for(j=0;v[j];j++)text[i++]=v[j];text[i++]=' ';text[i++]=GLYPH_PLUS_MINUS;text[i++]=' ';
        for(j=0;s[j];j++)text[i++]=s[j];text[i]=0;
        if(Glyph_TextWidth(text,font)<=168)break;
        if(font==FONT_RESULT)font=FONT_SMALL;else if(digits>2)digits--;else if(!compact)compact=1;else break;
    }while(1);
    GUI_ClearResult();LCD_FillRect(28,164,184,27,COL_MAIN);right_text(text,198,165,font,COL_TEXT);
}
static uint8_t break_line(const char *s,int16_t max){
    char glyph[2]={0,0};uint8_t i=0,space=0;int16_t width=0;
    while(s[i]&&i<95){glyph[0]=s[i];width+=Glyph_TextWidth(glyph,FONT_SMALL);if(width>max)break;if(s[i]==' ')space=i;i++;}
    return s[i]&&space?space:i;
}
static void paint_scroll(void){
    uint8_t row;Glyph_SetBackground(COL_MAIN);
    for(row=0;row<16;row++)Glyph_DrawTextRow(44,195+row,152,message_text,message_offset,row,FONT_SMALL,error_active?COL_ERROR:COL_DIM);
}
static void paint_message(void){
    char line[96];uint8_t first=break_line(message_text,164);const char *rest=message_text+first;
    Glyph_SetBackground(COL_MAIN);LCD_FillRect(30,191,180,41,COL_MAIN);
    if(message_scroll){
        paint_scroll();
    }else{
        memcpy(line,message_text,first);line[first]=0;centered(line,191,FONT_SMALL,error_active?COL_ERROR:COL_DIM);
        if(*rest==' ')rest++;centered(rest,208,FONT_SMALL,error_active?COL_ERROR:COL_DIM);
    }
}
void GUI_DrawMessage(const char *text){
    uint8_t first;const char *rest;
    strncpy(message_text,text,sizeof(message_text)-1);message_text[sizeof(message_text)-1]=0;
    first=break_line(message_text,164);rest=message_text+first;if(*rest==' ')rest++;
    message_width=Glyph_TextWidth(message_text,FONT_SMALL);
    message_scroll=Glyph_TextWidth(rest,FONT_SMALL)>112;
    message_offset=0;message_ticks=0;paint_message();
}
void GUI_Tick(void){
    if(!message_scroll)return;
    if(++message_ticks<=20)return;
    if(message_offset<message_width-152){
        message_offset+=2;
        message_ticks=20;
        paint_scroll();
    }else if(message_ticks>50){
        message_offset=message_ticks=0;
        paint_scroll();
    }
}
void GUI_ClearMessage(void){
    uint8_t i;
    if(!Calc_IsEvaluated())for(i=0;i<Calc_Count();i++)if(Calc_Tokens()[i]==CALC_T_HOLE){GUI_DrawMessage("RIGHT: next field");return;}
    GUI_DrawMessage(screen_mode==GUI_MODE_ERROR?"Enter value +/- uncertainty":"");
}
void GUI_DrawError(CalcError error){
    error_active=1;Glyph_SetBackground(COL_MAIN);LCD_FillRect(28,164,184,27,COL_MAIN);
    GUI_DrawMessage(error==CALC_ERR_MISSING?"Fill the empty field":Calc_ErrorText(error));
}

void GUI_DrawSettings(const char *const *labels,const char *const *values,uint8_t selected){
    uint8_t i;message_scroll=0;Glyph_SetBackground(COL_MAIN);
    LCD_FillRect(12,51,216,183,COL_MAIN);
    for(i=0;i<4;i++){
        int16_t y=66+32*i;uint16_t bg=i==selected?COL_LINE:COL_MAIN;
        round_rect(36,y-1,168,29,bg);Glyph_SetBackground(bg);
        Glyph_DrawText(43,y,labels[i],FONT_SMALL,COL_DIM);
        right_text(values[i],197,y+11,FONT_SMALL,i==selected?accent:COL_TEXT);
    }
    Glyph_SetBackground(COL_MAIN);GUI_DrawMessage("< > select   = change");
}
void GUI_DrawBlackSwan(const char *message){
    /* One-bit silhouette, 24 x 16; drawing primitives stay unchanged. */
    static const uint8_t swan[16][3]={
        {0,0x1c,0},{0,0x3e,0},{0,0x36,0},{0,0x30,0},{0,0x30,0},
        {0,0x18,0},{0,0x0c,0},{0,0x06,0},{0x0f,0xff,0},
        {0x3f,0xff,0x80},{0x7f,0xff,0xc0},{0xff,0xff,0xe0},
        {0x7f,0xff,0xc0},{0x3f,0xff,0x80},{0x1f,0xfe,0},{0,0,0}};
    uint8_t x,y;GUI_ClearResult();LCD_FillRect(28,164,184,27,COL_MAIN);
    for(y=0;y<16;y++)for(x=0;x<24;x++)if(swan[y][x/8]&(0x80>>(x%8)))LCD_FillRect(108+x,167+y,1,1,0x0000);
    GUI_DrawMessage(message);
}

/* Temporary ECON cards; the next ordinary result restores its own background. */
void GUI_DrawEconVisual(uint8_t event,const char *text){
    static const uint16_t colors[]={0xf81f,0x6010,0x05ff,0xd900,0x6010,0xd4a3,0x0668,0xfbe0,0x8800,0x8800};
    uint16_t color=event==ECON_VIS_MODEL||event==ECON_VIS_HISTORY||event==ECON_VIS_TV||event==ECON_VIS_REVIEW?COL_TEXT:COL_PANEL;
    GUI_ClearResult();LCD_FillRect(28,164,184,27,colors[event-1]);Glyph_SetBackground(colors[event-1]);
    if(event==ECON_VIS_MODEL){
        Glyph_DrawText(54,169,"MODEL",FONT_SMALL,color);Glyph_DrawText(134,169,"REALITY",FONT_SMALL,color);
        LCD_FillRect(110,174,14,2,color);LCD_FillRect(110,179,14,2,color);
        for(uint8_t i=0;i<10;i++)LCD_FillRect(112+i,182-i,2,1,color);
    }else if(event==ECON_VIS_CRASH){
        for(uint8_t i=0;i<5;i++){LCD_FillRect(57+9*i,166+3*i,10,2,color);if(i<4)LCD_FillRect(65+9*i,168+3*i,2,3,color);}
        right_text("CRASH",198,166,FONT_MATH,color);
    }else centered(event==ECON_VIS_QUESTION?"???":text,166,FONT_MATH,color);
    Glyph_SetBackground(COL_MAIN);
}
