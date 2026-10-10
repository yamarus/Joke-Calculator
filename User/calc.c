#include "calc.h"
#include <math.h>
#include <string.h>
#define CALC_MAX_DEPTH 12

static CalcToken tokens[CALC_MAX_TOKENS];
static CalcNode nodes[CALC_MAX_NODES];
static uint8_t token_count=0,cursor=0,node_count=0,parse_pos=0;
static int8_t root=CALC_INVALID;
static CalcError last_error=CALC_OK;
static const char *domain_reason;
static CalcError domain(const char *reason){domain_reason=reason;return CALC_ERR_DOMAIN;}
static CalcUncertain answer={0,0},expression_answer={0,0};
static uint8_t evaluated=0,answer_captured=0,editing=0,repeat_op=0;
static CalcUncertain repeat_rhs;
static uint32_t random_state=0x91e10da5u;
/* Postorder AST indices allow one visit per node without recursive evaluation. */
static CalcUncertain values[CALC_MAX_NODES];

static uint8_t is_digit(CalcToken t){ return t>='0'&&t<='9'; }
static uint8_t is_number_char(CalcToken t){ return is_digit(t)||t==CALC_T_DOT; }
static void set_error(CalcError e){ if(last_error==CALC_OK) last_error=e; }

void Calc_Init(void){ answer=repeat_rhs=(CalcUncertain){0,0}; random_state=0x91e10da5u; Calc_Clear(); }

void Calc_Clear(void){
    evaluated=answer_captured=repeat_op=0;
    token_count=0;
    cursor=0;
    node_count=0;
    root=CALC_INVALID;
    last_error=CALC_OK;
}

static uint8_t insert_at(uint8_t pos,CalcToken token){
    uint8_t i;
    if(token_count>=CALC_MAX_TOKENS||pos>token_count) return 0;
    for(i=token_count;i>pos;i--) tokens[i]=tokens[i-1];
    tokens[pos]=token;
    token_count++;
    return 1;
}

void Calc_Edit(void){ evaluated=repeat_op=0; }
uint8_t Calc_IsEvaluated(void){ return evaluated; }
CalcUncertain Calc_Ans(void){ return answer; }

static uint8_t continues_result(CalcToken t){
    return t=='+'||t=='-'||t=='*'||t=='/'||t=='^'||t=='%'||
           t=='!'||t==CALC_T_INV||t==CALC_T_DELTAPERCENT||t=='~';
}
uint8_t Calc_InsertSequence(const CalcToken *seq,uint8_t count){
    uint8_t i,continuing;
    if(!seq||!count) return 0;
    continuing=evaluated&&cursor==token_count&&continues_result(seq[0]);
    uint8_t replace=cursor<token_count&&tokens[cursor]==CALC_T_HOLE;
    uint8_t base=evaluated&&cursor==token_count?(continuing?1:0):token_count-replace;
    if(!count||base+count>CALC_MAX_TOKENS){ last_error=CALC_ERR_FULL; return 0; }
    if(evaluated&&cursor==token_count){
        Calc_Clear();
        if(continuing){ tokens[0]=CALC_T_ANS; token_count=cursor=1;
            expression_answer=answer; answer_captured=1; }
    }
    if(replace&&!evaluated){
        memmove(tokens+cursor,tokens+cursor+1,token_count-cursor-1); token_count--;
    }
    evaluated=repeat_op=0; last_error=CALC_OK;
    for(i=0;i<count;i++){
        if(seq[i]==CALC_T_ANS&&!answer_captured){
            expression_answer=answer; answer_captured=1;
        }
        insert_at(cursor++,seq[i]);
    }
    return 1;
}
uint8_t Calc_InsertToken(CalcToken token){
    if(token==')'&&cursor<token_count){
        if(tokens[cursor]==','){ cursor++; Calc_Edit(); return 1; }
        if(tokens[cursor]==')'){ cursor++; Calc_Edit(); return 1; }
    }
    return Calc_InsertSequence(&token,1);
}
uint8_t Calc_InsertFunction(CalcToken token){
    CalcToken unary[]={'*',token,'(',CALC_T_HOLE,')'};
    CalcToken binary[]={'*',token,'(',CALC_T_HOLE,',',CALC_T_HOLE,')'};
    uint8_t two=token==CALC_T_LOG||token==CALC_T_RND;
    uint8_t adjacent=!evaluated&&cursor&&(is_digit(tokens[cursor-1])||tokens[cursor-1]==')');
    if(!Calc_InsertSequence((two?binary:unary)+!adjacent,(two?6:4)+adjacent)) return 0;
    cursor-=two?4:2;
    return 1;
}
uint8_t Calc_InsertBinary(CalcToken op){
    uint8_t start=cursor,i,depth=0,has_left;
    if(evaluated&&cursor==token_count){
        Calc_Clear(); Calc_InsertToken(CALC_T_ANS);
        start=cursor;
    }
    while(start){
        CalcToken t=tokens[start-1];
        if(t==')') depth++;
        else if(t=='('){ if(!depth) break; depth--; }
        if(!depth&&(t=='+'||t=='-'||t=='*'||t=='/'||t=='^'||t==','||t==CALC_T_DELTAPERCENT)){
            uint8_t exponent_sign=(t=='+'||t=='-')&&start>1&&(tokens[start-2]==CALC_T_EE||tokens[start-2]=='E');
            uint8_t unary_minus=t=='-'&&(start==1||tokens[start-2]=='('||tokens[start-2]=='+'||tokens[start-2]=='-'||tokens[start-2]=='*'||tokens[start-2]=='/'||tokens[start-2]=='^'||tokens[start-2]==',');
            if(!exponent_sign&&!unary_minus) break;
        }
        start--;
    }
    if(op==CALC_T_POW&&start<cursor&&tokens[start]=='-')start++;
    has_left=start<cursor;
    if(!has_left){
        CalcToken seq[]={'(',CALC_T_HOLE,')',op,'(',CALC_T_HOLE,')'};
        if(!Calc_InsertSequence(seq,7)) return 0;
        cursor-=6; return 1;
    }
    if(token_count+6>CALC_MAX_TOKENS){ last_error=CALC_ERR_FULL; return 0; }
    /* Wrap just the preceding operand; a+b/c retains its conventional precedence. */
    insert_at(start,'('); cursor++;
    i=cursor;
    { CalcToken seq[]={')',op,'(',CALC_T_HOLE,')'};
      if(!Calc_InsertSequence(seq,5)) return 0; }
    cursor=i+3;
    return 1;
}
uint8_t Calc_NextField(void){
    uint8_t i;
    for(i=cursor+1;i<token_count;i++) if(tokens[i]==CALC_T_HOLE){ cursor=i; Calc_Edit(); return 1; }
    if(cursor<token_count){ cursor++; Calc_Edit(); return 1; }
    return 0;
}

uint8_t Calc_InsertDigit(uint8_t digit){
    if(digit>9) return 0;
    return Calc_InsertToken('0'+digit);
}

uint8_t Calc_Backspace(void){
    uint8_t i;
    Calc_Edit();
    if(cursor==0) return 0;
    for(i=cursor-1;i+1<token_count;i++) tokens[i]=tokens[i+1];
    token_count--;
    cursor--;
    return 1;
}

uint8_t Calc_Delete(void){
    uint8_t i;
    Calc_Edit();
    if(cursor>=token_count) return 0;
    for(i=cursor;i+1<token_count;i++) tokens[i]=tokens[i+1];
    token_count--;
    return 1;
}

void Calc_MoveLeft(void){ Calc_Edit(); if(cursor>0) cursor--; }
void Calc_MoveRight(void){
    Calc_Edit();
    if(cursor<token_count){
        if(tokens[cursor]==CALC_T_HOLE||tokens[cursor]==')'||tokens[cursor]==',') Calc_NextField();
        else cursor++;
    }
}

void Calc_SetCursor(uint8_t pos){
    Calc_Edit();
    if(pos<=token_count) cursor=pos;
}

uint8_t Calc_Count(void){ return token_count; }
uint8_t Calc_Cursor(void){ return cursor; }
const CalcToken *Calc_Tokens(void){ return tokens; }

uint8_t Calc_ToggleSign(void){
    uint8_t start=cursor;
    if(evaluated&&cursor==token_count){
        Calc_Clear();
        expression_answer=answer; answer_captured=1;
        tokens[0]=CALC_T_SUB; tokens[1]=CALC_T_ANS; cursor=token_count=2;
        return 1;
    }
    evaluated=0;
    while(start>0&&(tokens[start-1]==CALC_T_PERCENT||tokens[start-1]==CALC_T_FACT||
                   tokens[start-1]==CALC_T_INV)) start--;
    if(start&&tokens[start-1]==CALC_T_RPAREN){
        uint8_t depth=1;
        start--;
        while(start&&depth){
            CalcToken t=tokens[--start];
            if(t==CALC_T_RPAREN) depth++;
            else if(t==CALC_T_LPAREN) depth--;
        }
    }else if(start&&(tokens[start-1]==CALC_T_PI||tokens[start-1]==CALC_T_E||
                    tokens[start-1]==CALC_T_ANS)) start--;
    else{
        while(start>0&&is_number_char(tokens[start-1])) start--;
        if(start&&((tokens[start-1]==CALC_T_EE)||
           ((tokens[start-1]=='+'||tokens[start-1]=='-')&&start>1&&tokens[start-2]==CALC_T_EE))){
            start-=tokens[start-1]==CALC_T_EE?1:2;
            while(start&&is_number_char(tokens[start-1])) start--;
        }
    }

    if(start>0&&tokens[start-1]==CALC_T_SUB){
        uint8_t unary=(start-1==0);
        CalcToken before=0;

        if(start>=2){
            before=tokens[start-2];
            if(before==CALC_T_ADD||before==CALC_T_SUB||before==CALC_T_MUL||
               before==CALC_T_DIV||before==CALC_T_POW||before==CALC_T_LPAREN)
                unary=1;
        }

        if(unary){
            uint8_t i;
            for(i=start-1;i+1<token_count;i++) tokens[i]=tokens[i+1];
            token_count--;
            cursor--;
            return 1;
        }
    }

    if(!insert_at(start,CALC_T_SUB)){
        last_error=CALC_ERR_FULL;
        return 0;
    }

    if(start<=cursor) cursor++;
    return 1;
}

static int8_t new_node(uint8_t type,int8_t a,int8_t b,uint8_t start,uint8_t len){
    CalcNode *n;

    if(node_count>=CALC_MAX_NODES){
        set_error(CALC_ERR_FULL);
        return CALC_INVALID;
    }

    n=&nodes[node_count];
    n->type=type;
    n->a=a;
    n->b=b;
    n->start=start;
    n->len=len;
    n->from=a>=0?nodes[(uint8_t)a].from:start;
    n->to=a>=0?parse_pos:start+len;

    return node_count++;
}

static uint8_t prefix_node(CalcToken t){
    switch(t){
        case CALC_T_SQRT: return CALC_NODE_SQRT;
        case CALC_T_LN:   return CALC_NODE_LN;
        case CALC_T_EXP:  return CALC_NODE_EXP;
        case CALC_T_SIN:  return CALC_NODE_SIN;
        case CALC_T_COS:  return CALC_NODE_COS;
        case CALC_T_TAN:  return CALC_NODE_TAN;
        case CALC_T_ASIN: return CALC_NODE_ASIN;
        case CALC_T_ACOS: return CALC_NODE_ACOS;
        case CALC_T_ATAN: return CALC_NODE_ATAN;
        case CALC_T_ABS:  return CALC_NODE_ABS;
        default: return 0xFF;
    }
}

static int8_t parse_expression(uint8_t depth);
static int8_t parse_unary(uint8_t depth);

static int8_t parse_number(void){
    uint8_t start=parse_pos,saw_digit=0,saw_dot=0;

    while(parse_pos<token_count){
        CalcToken t=tokens[parse_pos];

        if(is_digit(t)){
            saw_digit=1;
            parse_pos++;
            continue;
        }

        if(t==CALC_T_DOT&&!saw_dot){
            saw_dot=1;
            parse_pos++;
            continue;
        }

        break;
    }

    if(!saw_digit){
        set_error(CALC_ERR_SYNTAX);
        return CALC_INVALID;
    }

    if(parse_pos<token_count&&tokens[parse_pos]==CALC_T_EE){
        parse_pos++;

        if(parse_pos<token_count&&(tokens[parse_pos]==CALC_T_ADD||tokens[parse_pos]==CALC_T_SUB))
            parse_pos++;

        if(parse_pos>=token_count||!is_digit(tokens[parse_pos])){
            set_error(CALC_ERR_SYNTAX);
            return CALC_INVALID;
        }

        while(parse_pos<token_count&&is_digit(tokens[parse_pos])) parse_pos++;
    }

    return new_node(CALC_NODE_NUMBER,CALC_INVALID,CALC_INVALID,start,parse_pos-start);
}

static int8_t parse_primary(uint8_t depth){
    int8_t n;

    if(depth>CALC_MAX_DEPTH){
        set_error(CALC_ERR_DEPTH);
        return CALC_INVALID;
    }

    if(parse_pos>=token_count||(editing&&(tokens[parse_pos]==')'||tokens[parse_pos]==','))){
        if(editing) return new_node(CALC_NODE_HOLE,CALC_INVALID,CALC_INVALID,parse_pos,0);
        set_error(CALC_ERR_SYNTAX); return CALC_INVALID;
    }
    if(tokens[parse_pos]==CALC_T_HOLE){
        uint8_t pos=parse_pos++;
        if(!editing){ cursor=pos;set_error(CALC_ERR_MISSING); return CALC_INVALID; }
        return new_node(CALC_NODE_HOLE,CALC_INVALID,CALC_INVALID,pos,1);
    }

    if(is_number_char(tokens[parse_pos]))
        return parse_number();

    if(tokens[parse_pos]==CALC_T_CAPTURED){
        uint8_t pos=parse_pos++;
        return new_node(CALC_NODE_CAPTURED,CALC_INVALID,CALC_INVALID,pos,1);
    }
    if(tokens[parse_pos]==CALC_T_ANS){
        uint8_t pos=parse_pos++;
        return new_node(CALC_NODE_ANS,CALC_INVALID,CALC_INVALID,pos,1);
    }
    if(tokens[parse_pos]==CALC_T_PI){
        uint8_t pos=parse_pos++;
        return new_node(CALC_NODE_PI,CALC_INVALID,CALC_INVALID,pos,1);
    }

    if(tokens[parse_pos]==CALC_T_E){
        uint8_t pos=parse_pos++;
        return new_node(CALC_NODE_E,CALC_INVALID,CALC_INVALID,pos,1);
    }

    {
        CalcToken token=tokens[parse_pos];uint8_t type=prefix_node(token);
        if((type!=0xFF||token==CALC_T_LOG||token==CALC_T_RND)&&parse_pos+1<token_count&&tokens[parse_pos+1]=='('){
            uint8_t start=parse_pos;int8_t a,b=CALC_INVALID;
            parse_pos+=2;a=parse_expression(depth+1);if(a<0)return a;
            if(token==CALC_T_LOG||token==CALC_T_RND){
                if(parse_pos>=token_count||tokens[parse_pos++]!=','){set_error(CALC_ERR_SYNTAX);return CALC_INVALID;}
                b=parse_expression(depth+1);if(b<0)return b;
                type=token==CALC_T_LOG?CALC_NODE_LOG:CALC_NODE_RND;
            }
            if(parse_pos<token_count&&tokens[parse_pos]==')')parse_pos++;
            else if(!editing||parse_pos<token_count){set_error(CALC_ERR_SYNTAX);return CALC_INVALID;}
            n=new_node(type,a,b,0,0);
            if(n>=0){nodes[(uint8_t)n].from=start;nodes[(uint8_t)n].to=parse_pos;}
            return n;
        }
    }

    if(tokens[parse_pos]==CALC_T_LPAREN){
        uint8_t start=parse_pos++;
        n=parse_expression(depth+1);

        if(n==CALC_INVALID) return CALC_INVALID;

        if(parse_pos>=token_count&&editing){nodes[(uint8_t)n].from=start;nodes[(uint8_t)n].to=parse_pos;return n;}
        if(parse_pos>=token_count||tokens[parse_pos]!=CALC_T_RPAREN){
            set_error(CALC_ERR_SYNTAX);
            return CALC_INVALID;
        }

        parse_pos++;
        nodes[(uint8_t)n].from=start; nodes[(uint8_t)n].to=parse_pos;
        return n;
    }

    set_error(CALC_ERR_SYNTAX);
    return CALC_INVALID;
}

static int8_t parse_postfix(uint8_t depth){
    int8_t n=parse_primary(depth);

    if(n==CALC_INVALID) return CALC_INVALID;

    while(parse_pos<token_count){
        uint8_t type;

        if(tokens[parse_pos]==CALC_T_PERCENT) type=CALC_NODE_PERCENT;
        else if(tokens[parse_pos]==CALC_T_FACT) type=CALC_NODE_FACT;
        else if(tokens[parse_pos]==CALC_T_INV) type=CALC_NODE_INV;
        else if(tokens[parse_pos]==CALC_T_UNCERTAIN){
            int8_t sigma;
            parse_pos++;
            sigma=parse_primary(depth+1);
            if(sigma==CALC_INVALID) return CALC_INVALID;
            n=new_node(CALC_NODE_UNCERTAIN,n,sigma,0,0);
            if(n==CALC_INVALID) return n;
            continue;
        }else break;

        parse_pos++;
        n=new_node(type,n,CALC_INVALID,0,0);

        if(n==CALC_INVALID) return CALC_INVALID;
    }

    return n;
}

static int8_t parse_power(uint8_t depth){
    int8_t left=parse_postfix(depth);

    if(left==CALC_INVALID) return CALC_INVALID;

    if(parse_pos<token_count&&tokens[parse_pos]==CALC_T_POW){
        int8_t right;

        parse_pos++;
        right=parse_unary(depth+1);

        if(right==CALC_INVALID) return CALC_INVALID;
        left=new_node(CALC_NODE_POW,left,right,0,0);
    }

    return left;
}

static int8_t parse_unary(uint8_t depth){
    uint8_t type;

    if(depth>CALC_MAX_DEPTH){
        set_error(CALC_ERR_DEPTH);
        return CALC_INVALID;
    }

    if(parse_pos<token_count&&tokens[parse_pos]==CALC_T_ADD){
        parse_pos++;
        return parse_unary(depth+1);
    }

    if(parse_pos<token_count&&tokens[parse_pos]==CALC_T_SUB){
        int8_t n,result;uint8_t start=parse_pos++;
        n=parse_unary(depth+1);
        if(n==CALC_INVALID) return CALC_INVALID;
        result=new_node(CALC_NODE_NEG,n,CALC_INVALID,0,0);
        if(result>=0)nodes[(uint8_t)result].from=start;
        return result;
    }

    if(parse_pos<token_count&&(type=prefix_node(tokens[parse_pos]))!=0xFF&&
       !(parse_pos+1<token_count&&tokens[parse_pos+1]=='(')){
        int8_t n,result; uint8_t start=parse_pos++;
        n=parse_unary(depth+1);
        if(n==CALC_INVALID) return CALC_INVALID;
        result=new_node(type,n,CALC_INVALID,0,0);
        if(result>=0){ nodes[(uint8_t)result].from=start; nodes[(uint8_t)result].to=parse_pos; }
        return result;
    }

    return parse_power(depth);
}

static int8_t parse_term(uint8_t depth){
    int8_t left=parse_unary(depth);

    if(left==CALC_INVALID) return CALC_INVALID;

    while(parse_pos<token_count&&(tokens[parse_pos]==CALC_T_MUL||tokens[parse_pos]==CALC_T_DIV)){
        CalcToken op=tokens[parse_pos++];
        int8_t right=parse_unary(depth);

        if(right==CALC_INVALID) return CALC_INVALID;

        left=new_node(op==CALC_T_MUL?CALC_NODE_MUL:CALC_NODE_DIV,left,right,0,0);
        if(left==CALC_INVALID) return CALC_INVALID;
    }

    return left;
}

static int8_t parse_expression(uint8_t depth){
    int8_t left=parse_term(depth);

    if(left==CALC_INVALID) return CALC_INVALID;

    while(parse_pos<token_count&&(tokens[parse_pos]==CALC_T_ADD||tokens[parse_pos]==CALC_T_SUB||tokens[parse_pos]==CALC_T_DELTAPERCENT)){
        CalcToken op=tokens[parse_pos++];
        int8_t right=parse_term(depth);

        if(right==CALC_INVALID) return CALC_INVALID;

        left=new_node(op==CALC_T_ADD?CALC_NODE_ADD:op==CALC_T_SUB?CALC_NODE_SUB:CALC_NODE_DELTAPERCENT,left,right,0,0);
        if(left==CALC_INVALID) return CALC_INVALID;
    }

    return left;
}

static CalcError parse(uint8_t edit){
    editing=edit;
    node_count=0;
    parse_pos=0;
    root=CALC_INVALID;
    last_error=CALC_OK;

    if(token_count==0){
        last_error=CALC_ERR_SYNTAX;
        return last_error;
    }

    root=parse_expression(0);

    if(root==CALC_INVALID) return last_error;

    if(parse_pos!=token_count){
        root=CALC_INVALID;
        last_error=CALC_ERR_SYNTAX;
    }

    return last_error;
}

CalcError Calc_Parse(void){ return parse(0); }
CalcError Calc_ParseEditing(void){
    CalcError previous=last_error,result=parse(1);
    last_error=previous;
    return result;
}

static CalcError eval_number(const CalcNode *n,CalcValue *out){
    uint8_t i=n->start,end=n->start+n->len;
    CalcValue value=0.0f,frac=0.1f;
    uint8_t after_dot=0;
    int16_t exponent=0;
    int8_t exp_sign=1;

    while(i<end&&tokens[i]!=CALC_T_EE){
        if(tokens[i]==CALC_T_DOT){
            after_dot=1;
            i++;
            continue;
        }

        if(is_digit(tokens[i])){
            if(after_dot){
                value+=(CalcValue)(tokens[i]-'0')*frac;
                frac*=0.1f;
            }else value=value*10.0f+(CalcValue)(tokens[i]-'0');
        }

        i++;
    }

    if(i<end&&tokens[i]==CALC_T_EE){
        i++;

        if(i<end&&tokens[i]==CALC_T_SUB){
            exp_sign=-1;
            i++;
        }else if(i<end&&tokens[i]==CALC_T_ADD) i++;

        while(i<end&&is_digit(tokens[i])){
            exponent=exponent*10+(tokens[i]-'0');
            if(exponent>99) return CALC_ERR_DOMAIN;
            i++;
        }

        exponent*=exp_sign;

        while(exponent>0){
            value*=10.0f;
            exponent--;
        }

        while(exponent<0){
            value/=10.0f;
            exponent++;
        }
    }

    if(!isfinite(value)) return CALC_ERR_DOMAIN;
    *out=value;
    return CALC_OK;
}

float Calc_CombineSigma(float a,float b){
    a=fabsf(a); b=fabsf(b);
    if(a<b){ float t=a; a=b; b=t; }
    if(!a) return 0;
    b/=a;
    return a*sqrtf(1.0f+b*b);
}
static CalcError eval_node(uint8_t id,uint8_t degrees,uint8_t uncertain,CalcFeatures *f,const CalcModel *model){
    const CalcNode *n=&nodes[id];
    CalcUncertain a={0,0},b={0,0};
    float v=0,da=0,db=0,k=degrees?0.0174532925199433f:1.0f;
    CalcError e;
    if(n->a>=0) a=values[(uint8_t)n->a];
    if(n->b>=0) b=values[(uint8_t)n->b];
    switch(n->type){
        case CALC_NODE_NUMBER:
            e=eval_number(n,&v); if(e) return e;
            if(model&&model->sigma){a.sigma=model->sigma(id,v,f->hash,model->context);da=1;}
            break;
        case CALC_NODE_PI: v=3.141592653589793f; break;
        case CALC_NODE_E: v=2.718281828459045f; break;
        case CALC_NODE_CAPTURED:
            if(!uncertain&&repeat_rhs.sigma) return CALC_ERR_UNSUPPORTED;
            v=repeat_rhs.value; da=1; a.sigma=uncertain?repeat_rhs.sigma:0; break;
        case CALC_NODE_ANS:
            if(!uncertain&&expression_answer.sigma) return CALC_ERR_UNSUPPORTED;
            v=expression_answer.value; da=1; a.sigma=uncertain?expression_answer.sigma:0; break;
        case CALC_NODE_UNCERTAIN:
            if(!uncertain) return CALC_ERR_UNSUPPORTED;
            if(b.value<0)return domain("Uncertainty cannot be negative");
            if(b.sigma!=0)return domain("Uncertainty must be exact");
            if(a.sigma!=0)return domain("Value already has uncertainty");
            v=a.value; a.sigma=b.value; da=1; break;
        case CALC_NODE_NEG: v=-a.value; da=-1; break;
        case CALC_NODE_ABS:
            if(a.value==0&&a.sigma>0)return domain("Uncertainty undefined at zero");
            v=fabsf(a.value); da=1; break;
        case CALC_NODE_SQRT:
            if(a.value<0)return domain("No real square root");
            if(a.value==0&&a.sigma>0)return domain("Uncertainty undefined at zero");
            v=sqrtf(a.value); da=v?0.5f/v:0; break;
        case CALC_NODE_INV:
            if(a.value==0) return CALC_ERR_DIV_ZERO;
            v=1/a.value; da=-v/a.value;
            f->flags|=CALC_RISK_DIVISOR;
            f->denominator_relative=fmaxf(f->denominator_relative,a.sigma/fabsf(a.value)); break;
        case CALC_NODE_PERCENT:
            v=a.value*0.01f; da=0.01f; f->flags|=CALC_RISK_PERCENT; break;
        case CALC_NODE_FACT:{
            uint8_t j;
            if(a.value<0||a.value>34||truncf(a.value)!=a.value)return domain("Factorial needs integer 0..34");
            if(a.sigma>0) return CALC_ERR_UNSUPPORTED;
            v=1; for(j=2;j<=(uint8_t)a.value;j++) v*=j;
            break;
        }
        case CALC_NODE_RND:{
            float u;
            if(a.sigma||b.sigma) return CALC_ERR_UNSUPPORTED;
            if(a.value>b.value)return domain("RND minimum exceeds maximum");
            if(a.value==b.value){ v=a.value; break; }
            random_state^=random_state<<13; random_state^=random_state>>17; random_state^=random_state<<5;
            u=(random_state>>8)*(1.0f/16777216.0f);
            v=a.value*(1-u)+b.value*u;
            break;
        }
        case CALC_NODE_LOG:{
            float l;
            if(a.value<=0||a.value==1)return domain("Log base must be >0 and !=1");
            if(b.value<=0)return domain("Log needs a positive value");
            l=logf(a.value); v=logf(b.value)/l;
            da=-v/(a.value*l); db=1/(b.value*l); break;
        }
        case CALC_NODE_LN:
            if(a.value<=0)return domain("Log needs a positive value");
            v=logf(a.value); da=1/a.value; break;
        case CALC_NODE_EXP:
            v=expf(a.value); da=v; f->flags|=CALC_RISK_GROWTH; break;
        case CALC_NODE_SIN: v=sinf(a.value*k); da=k*cosf(a.value*k); break;
        case CALC_NODE_COS: v=cosf(a.value*k); da=-k*sinf(a.value*k); break;
        case CALC_NODE_TAN:{
            float c=cosf(a.value*k);
            if(fabsf(c)<1e-6f)return domain("Tangent undefined here");
            v=tanf(a.value*k); da=k/(c*c); break;
        }
        case CALC_NODE_ASIN:
        case CALC_NODE_ACOS:
            if(fabsf(a.value)>1)return domain("Input must be between -1 and 1");
            if(fabsf(a.value)==1&&a.sigma>0)return domain("Uncertainty at trig boundary");
            v=(n->type==CALC_NODE_ASIN?asinf(a.value):acosf(a.value))/k;
            if(a.sigma>0) da=(n->type==CALC_NODE_ASIN?1:-1)/(k*sqrtf(1-a.value*a.value));
            break;
        case CALC_NODE_ATAN:
            v=atanf(a.value)/k; da=(1/k)/(1+a.value*a.value); break;
        case CALC_NODE_ADD:
        case CALC_NODE_SUB:
            db=n->type==CALC_NODE_ADD?1:-1;da=1;
            v=n->type==CALC_NODE_ADD?a.value+b.value:a.value-b.value;
            if((n->type==CALC_NODE_SUB&&((a.value>0&&b.value>0)||(a.value<0&&b.value<0)))||
               (n->type==CALC_NODE_ADD&&((a.value>0&&b.value<0)||(a.value<0&&b.value>0)))){
                float scale=fmaxf(fabsf(a.value),fabsf(b.value));
                float c=1-fminf(1,fabsf(v)/scale);
                f->cancellation=fmaxf(f->cancellation,c);
                if(c>0.9f) f->flags|=CALC_RISK_CANCEL;
            } break;
        case CALC_NODE_MUL: v=a.value*b.value; da=b.value; db=a.value; break;
        case CALC_NODE_DIV:
            if(b.value==0) return CALC_ERR_DIV_ZERO;
            v=a.value/b.value; da=1/b.value; db=-v/b.value;
            f->flags|=CALC_RISK_DIVISOR;
            f->denominator_relative=fmaxf(f->denominator_relative,b.sigma/fabsf(b.value)); break;
        case CALC_NODE_DELTAPERCENT:
            if(a.value==0) return CALC_ERR_DIV_ZERO;
            v=100*(b.value-a.value)/fabsf(a.value);
            da=-100*(b.value/fabsf(a.value))/a.value; db=100/fabsf(a.value);
            f->flags|=CALC_RISK_PERCENT; break;
        case CALC_NODE_POW:
            if(a.value==0&&b.value<0) return CALC_ERR_DIV_ZERO;
            if(a.value<0&&(truncf(b.value)!=b.value||b.sigma>0))return domain("Negative base: integer power only");
            if(a.value==0&&b.value==0)return domain("0^0 is undefined");
            if(a.value==0&&(b.sigma>0||(a.sigma>0&&b.value<1)))return domain("Uncertain power at zero");
            v=powf(a.value,b.value);
            if(a.sigma>0) da=a.value?b.value*(v/a.value):(b.value==1?1:0);
            if(b.sigma>0) db=v*logf(a.value);
            f->flags|=CALC_RISK_GROWTH; break;
        default: return CALC_ERR_SYNTAX;
    }
    /* A singular derivative cannot contaminate an exact input through 0*Inf. */
    values[id].value=v;
    values[id].sigma=Calc_CombineSigma(a.sigma?da*a.sigma:0,b.sigma?db*b.sigma:0);
    if(!isfinite(v))return domain("Result exceeds number range");
    if(!isfinite(values[id].sigma))return domain("Uncertainty exceeds range");
    if(n->a>=0&&n->type!=CALC_NODE_UNCERTAIN) f->operations++;
    return CALC_OK;
}
static CalcToken repeat_token(uint8_t type){
    switch(type){
        case CALC_NODE_ADD: return '+'; case CALC_NODE_SUB: return '-';
        case CALC_NODE_MUL: return '*'; case CALC_NODE_DIV: return '/';
        case CALC_NODE_POW: return '^'; case CALC_NODE_DELTAPERCENT: return CALC_T_DELTAPERCENT;
        default: return 0;
    }
}
static CalcError evaluate(CalcUncertain *result,uint8_t degrees,
                          uint8_t uncertain,CalcFeatures *features,const CalcModel *model){
    CalcFeatures f={2166136261u,0,0,0,0};
    uint8_t i,repeating=!model&&evaluated&&repeat_op;
    CalcError e;
    if(!result) return CALC_ERR_SYNTAX;
    domain_reason=0;
    if(repeating){
        expression_answer=answer; answer_captured=1;
        tokens[0]=CALC_T_ANS; tokens[1]=repeat_op; tokens[2]=CALC_T_CAPTURED;
        token_count=cursor=3;
    }
    e=Calc_Parse();
    for(i=0;i<token_count;i++) f.hash=(f.hash^tokens[i])*16777619u;
    for(i=0;e==CALC_OK&&i<node_count;i++){
        e=eval_node(i,degrees,uncertain,&f,model);
        if(model&&model->observe){
            const CalcNode *n=&nodes[i];CalcUncertain zero={0,0};
            model->observe(i,&e,&values[i],
                           n->a>=0?values[(uint8_t)n->a]:zero,n->b>=0?values[(uint8_t)n->b]:zero,model->context);
        }
    }
    if(e==CALC_OK){
        if(!repeating){
            const CalcNode *r=&nodes[(uint8_t)root];
            repeat_op=repeat_token(r->type);
            if(repeat_op) repeat_rhs=values[(uint8_t)r->b];
        }
        *result=values[(uint8_t)root];
        answer=*result; evaluated=1; cursor=token_count;
        if(features) *features=f;
    }else evaluated=repeat_op=0;
    last_error=e;
    return e;
}
CalcError Calc_EvaluateUncertain(CalcUncertain *result,uint8_t degrees,
                                uint8_t uncertain,CalcFeatures *features){
    return evaluate(result,degrees,uncertain,features,0);
}
CalcError Calc_EvaluateModel(CalcUncertain *result,uint8_t degrees,
                            CalcFeatures *features,const CalcModel *model){
    return evaluate(result,degrees,1,features,model);
}
CalcError Calc_Evaluate(CalcValue *result){
    CalcUncertain r;
    CalcError e;
    if(!result) return CALC_ERR_SYNTAX;
    e=Calc_EvaluateUncertain(&r,0,0,0);
    if(!e) *result=r.value;
    return e;
}

CalcError Calc_LastError(void){ return last_error; }

const char *Calc_ErrorText(CalcError e){
    switch(e){
        case CALC_OK: return "OK";
        case CALC_ERR_FULL: return "Expression full";
        case CALC_ERR_SYNTAX: return "Syntax error";
        case CALC_ERR_DIV_ZERO: return "Division by zero";
        case CALC_ERR_DOMAIN:return domain_reason?domain_reason:"Number exceeds range";
        case CALC_ERR_UNSUPPORTED: return "Not implemented";
        case CALC_ERR_DEPTH: return "Expression too deep";
        case CALC_ERR_MISSING: return "Fill the empty field";
        default: return "Error";
    }
}

int8_t Calc_Root(void){ return root; }
uint8_t Calc_NodeCount(void){ return node_count; }
const CalcNode *Calc_Nodes(void){ return nodes; }

uint8_t Calc_CopyNodeText(int8_t id,char *dst,uint8_t size){
    const CalcNode *n;
    uint8_t i,j=0;

    if(!dst||size==0||id<0||id>=node_count) return 0;

    n=&nodes[id];

    if(n->type==CALC_NODE_CAPTURED){
        Calc_FormatValue(repeat_rhs.value,dst,size);
        if(repeat_rhs.sigma){
            char sigma[24];Calc_FormatValue(repeat_rhs.sigma,sigma,sizeof(sigma));
            j=strlen(dst);if(j+1<size){dst[j++]='~';dst[j]=0;}
            for(i=0;sigma[i]&&j+1<size;i++)dst[j++]=sigma[i];dst[j]=0;
        }
        return strlen(dst);
    }
    if(n->type==CALC_NODE_HOLE){ if(size) dst[0]=0; return 0; }
    if(n->type==CALC_NODE_ANS){
        if(size<4) return 0;
        dst[0]='A'; dst[1]='n'; dst[2]='s'; dst[3]=0;
        return 3;
    }
    if(n->type==CALC_NODE_PI){
        if(size<3) return 0;
        dst[0]='p';
        dst[1]='i';
        dst[2]=0;
        return 2;
    }

    if(n->type==CALC_NODE_E){
        if(size<2) return 0;
        dst[0]='e';
        dst[1]=0;
        return 1;
    }

    if(n->type!=CALC_NODE_NUMBER){
        dst[0]=0;
        return 0;
    }

    for(i=0;i<n->len&&j+1<size;i++){
        CalcToken t=tokens[n->start+i];
        dst[j++]=t==CALC_T_EE?'E':(char)t;
    }

    dst[j]=0;
    return j;
}
static const char *token_name(CalcToken t){
    switch(t){
        case CALC_T_EE: return "E";
        case CALC_T_SQRT: return "sqrt";
        case CALC_T_INV: return "inv";
        case CALC_T_LOG: return "log";
        case CALC_T_LN: return "ln";
        case CALC_T_EXP: return "exp";
        case CALC_T_SIN: return "sin";
        case CALC_T_COS: return "cos";
        case CALC_T_TAN: return "tan";
        case CALC_T_ASIN: return "asin";
        case CALC_T_ACOS: return "acos";
        case CALC_T_ATAN: return "atan";
        case CALC_T_ABS: return "abs";
        case CALC_T_PI: return "pi";
        case CALC_T_E: return "e";
        case CALC_T_DELTAPERCENT: return "d%";
        case CALC_T_RND: return "RND";
        case CALC_T_ANS: return "Ans";
        case CALC_T_CAPTURED: return "last";
        default: return 0;
    }
}

uint8_t Calc_FormatInput(char *dst,uint8_t size,uint8_t *cursor_char){
    uint16_t total=0,caret=0,start=0,offset=0;
    uint8_t i,j=0,k;
    const char *name;
    char ascii[2]={0,0};
    if(!dst||!size) return 0;
    for(i=0;i<token_count;i++){
        if(i==cursor) caret=total;
        name=token_name(tokens[i]);
        if(name){ for(k=0;name[k];k++) total++; }
        else if(tokens[i]>=32&&tokens[i]<=126) total++;
    }
    if(cursor==token_count) caret=total;
    /* Window the expanded tokens so a late cursor never disappears in a full editor. */
    if(total>=size){
        start=caret>(size-1)/2?caret-(size-1)/2:0;
        if(start>total-(size-1)) start=total-(size-1);
    }
    for(i=0;i<token_count;i++){
        name=token_name(tokens[i]);
        if(!name){
            if(tokens[i]<32||tokens[i]>126) continue;
            ascii[0]=(char)tokens[i]; name=ascii;
        }
        for(k=0;name[k];k++,offset++)
            if(offset>=start&&j+1<size) dst[j++]=name[k];
    }
    dst[j]=0;
    if(cursor_char) *cursor_char=(uint8_t)(caret-start);
    return j;
}

static uint8_t append_char(char *dst,uint8_t size,uint8_t pos,char c){
    if(pos+1>=size) return pos;
    dst[pos++]=c;
    dst[pos]=0;
    return pos;
}

static uint8_t append_uint(char *dst,uint8_t size,uint8_t pos,uint32_t v){
    char tmp[10];
    uint8_t n=0;

    do{
        tmp[n++]='0'+v%10;
        v/=10;
    }while(v);

    while(n&&pos+1<size)
        dst[pos++]=tmp[--n];

    dst[pos]=0;
    return pos;
}

static void format_digits(CalcValue value,char *dst,uint8_t size,uint8_t significant,uint8_t notation){
    float v;
    uint32_t digits,divisor;
    int exponent=0,before;
    uint8_t pos=0,i,scientific,places=significant;
    char decimal[10];
    if(!dst||!size)return;dst[0]=0;
    if(significant<1||significant>6)significant=6;
    if(!isfinite(value)){pos=append_char(dst,size,pos,'E');pos=append_char(dst,size,pos,'r');append_char(dst,size,pos,'r');return;}
    if(value==0){append_char(dst,size,0,'0');return;}
    if(value<0)pos=append_char(dst,size,pos,'-');
    v=fabsf(value);
    if(notation==3&&v<0x1p32f&&v==(uint32_t)v){
        append_uint(dst,size,pos,(uint32_t)v);return;
    }
    while(v>=10){v/=10;exponent++;}
    while(v<1){v*=10;exponent--;}
    if(notation==3){
        if(places>6)places=6;
        int count=exponent+places+1;
        if(count<1){v=count==0&&v>=4.999999f?1:0;if(!v)pos=0;exponent=-(int)places;count=1;}
        significant=count>7?7:count;
    }
    divisor=1;for(i=1;i<significant;i++)divisor*=10;
    digits=(uint32_t)(v*divisor+.5f);
    if(digits>=divisor*10){digits=divisor;exponent++;}
    append_uint(decimal,sizeof(decimal),0,digits);
    scientific=notation==1||(notation==0?(exponent>=6||exponent<-4):
        (exponent>=0?exponent+2+pos>=size:-exponent+significant+2+pos>=size));
    before=scientific?1:exponent+1;
    if(before<=0){pos=append_char(dst,size,pos,'0');pos=append_char(dst,size,pos,'.');for(i=0;i<-before;i++)pos=append_char(dst,size,pos,'0');}
    for(i=0;i<significant||i<before;i++){
        if(before>0&&i==before)pos=append_char(dst,size,pos,'.');
        pos=append_char(dst,size,pos,i<significant?decimal[i]:'0');
    }
    if(before<significant){
        while(pos&&dst[pos-1]=='0')dst[--pos]=0;
        if(pos&&dst[pos-1]=='.'){
            if(notation==3&&!scientific)for(i=0;i<places;i++)pos=append_char(dst,size,pos,'0');
            else dst[--pos]=0;
        }
    }
    if(scientific){pos=append_char(dst,size,pos,'E');pos=append_char(dst,size,pos,exponent<0?'-':'+');append_uint(dst,size,pos,(uint32_t)(exponent<0?-exponent:exponent));}
}

void Calc_FormatValueDigits(CalcValue value,char *dst,uint8_t size,uint8_t significant){format_digits(value,dst,size,significant,0);}
void Calc_FormatDisplay(CalcValue value,char *dst,uint8_t size,uint8_t significant,uint8_t scientific){format_digits(value,dst,size,significant,scientific?1:2);}
void Calc_FormatValue(CalcValue value,char *dst,uint8_t size){ Calc_FormatValueDigits(value,dst,size,6); }
/* Fixed decimals share normalization, rounding and buffer handling above. */
void Calc_FormatFixed(CalcValue value,char *dst,uint8_t size,uint8_t places){format_digits(value,dst,size,places,3);}
