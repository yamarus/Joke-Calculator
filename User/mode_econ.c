#include "mode_econ.h"
#include "mode_normal.h"
#include "input.h"
#include "settings.h"
#include <math.h>
#include <float.h>
#include <string.h>

/* Tuning: input assumptions, score weights, then maximum shift in sigma units. */
#define ECON_SEED 0x6a09e667u
#define REL_MIN .05f
#define REL_SPREAD .15f
#define PRECISION_CAP .005f
#define ZERO_SIGMA .02f
#define SCORE_UNCERTAINTY 70.f
#define SCORE_CANCEL 30.f
#define SCORE_INSTABILITY 24.f
#define SCORE_NONLINEAR 3.f
#define SMALL_DIVISOR .01f
#define SCORE_SMALL_DIVISOR 12.f
#define SHIFT_CALM .18f
#define SHIFT_MILD .65f
#define SHIFT_HIGH 1.4f
#define SHIFT_EXTREME 2.2f

enum{ CONFIDENT,MILD,UNCERTAIN,VERY_UNCERTAIN,CANCELLATION,
      NEGATIVE,STRONG_NEGATIVE,UNSTABLE,GROWTH,PRECISE,EXTREME,GENERIC };
#define C(k) (1u<<(k))
typedef struct{ const char *text; uint16_t classes; uint8_t weight; } Joke;
static const Joke jokes[]={
    {"Probably.",C(CONFIDENT)|C(GENERIC),4},
    {"Within economist error.",C(CONFIDENT)|C(MILD)|C(GENERIC),3},
    {"Best estimate. For now.",C(CONFIDENT),2},
    {"Good enough.",C(CONFIDENT),2},
    {"Approximately exactly.",C(PRECISE),2},
    {"Hm.",0x0fff,3},
    {"Subject to reality.",C(MILD)|C(UNCERTAIN),2},
    {"Model says yes.",C(MILD)|C(UNCERTAIN),3},
    {"Numbers happened.",C(MILD)|C(EXTREME),2},
    {"That's a number.",0x0fff,2},
    {"Data may disagree.",C(VERY_UNCERTAIN)|C(UNSTABLE),3},
    {"This looks wrong.",C(VERY_UNCERTAIN)|C(STRONG_NEGATIVE),3},
    {"Things missing.",C(VERY_UNCERTAIN)|C(UNSTABLE),2},
    {"Don't quote me.",C(VERY_UNCERTAIN),2},
    {"Equilibrium detected.",C(CANCELLATION),1},
    {"Model incomplete.",C(CANCELLATION),2},
    {"Fine.",0x0fff,2},
    {"Anyway.",0x0fff,2},
    {"Sign subject to change.",C(CANCELLATION),2},
    {"Assuming infinite growth.",C(GROWTH),4},
    {"Future not in sample.",C(GROWTH),2},
    {"Extrapolation. Good luck.",C(GROWTH),1},
    {"Unfortunately, yes.",C(NEGATIVE)|C(STRONG_NEGATIVE),4},
    {"I don't like this.",C(NEGATIVE)|C(STRONG_NEGATIVE),2},
    {"Anxiety is not included in sigma.",C(STRONG_NEGATIVE)|C(VERY_UNCERTAIN),3},
    {"Kierkegaard approves negativity.",C(STRONG_NEGATIVE),1},
    {"Reality may differ.",C(VERY_UNCERTAIN),3},
    {"Precision isn't knowledge.",C(PRECISE),1}
};
#define JOKE_COUNT (sizeof(jokes)/sizeof(jokes[0]))
enum{ METHOD_DIMENSION=1,METHOD_TRIG=2,METHOD_POWER=4,METHOD_RANDOM=8 };
typedef struct{
    float instability;
    uint8_t nonlinear,growth,precise,history,imaginary,methods,pathological;
} Traits;
typedef struct{
    uint32_t seed,entropy,hash,seen,count,repeat_hash,presentation;
    Traits traits;
    uint8_t quiet,last_kind,last_joke,repeats,cooldown,misses;
    uint16_t eggs_seen;
} EconState;
static EconState state={.last_kind=255,.last_joke=255};
static const struct{float value;const char *text;uint8_t visual;} eggs[]={
    {80085,"Very mature.",ECON_VIS_NUMBER},{58008,"Very mature.",ECON_VIS_NUMBER},
    {5318008,"Finally.",ECON_VIS_NUMBER},
    {69,"Nice.",0},{420,"Highly significant.",0},{404,"Result not found.",ECON_VIS_NUMBER},
    {666,"Infernal confidence.",ECON_VIS_INFERNAL},
    {42,"Question missing.",0},
    {2008,"Strong fundamentals.",ECON_VIS_CRASH},{1929,"Temporary correction.",ECON_VIS_CRASH}
};
static uint8_t number_egg(float value){
    for(uint8_t i=0;i<sizeof(eggs)/sizeof(eggs[0]);i++)if(value==eggs[i].value)return i+1;
    return 0;
}
static uint32_t mix(uint32_t x){
    if(!x)x=ECON_SEED;
    x^=x<<13;x^=x>>17;x^=x<<5;return x;
}
static float unit(uint32_t x){return (mix(x)>>8)*(1.f/16777216.f);}
static uint32_t value_bits(float x){uint32_t bits;memcpy(&bits,&x,sizeof(bits));return bits;}
void ModeEcon_StartSession(uint32_t seed){
    memset(&state,0,sizeof(state));state.seed=seed?seed:ECON_SEED;
    state.last_kind=state.last_joke=255;
}
static void ensure_seed(void){
    /* Main-loop timing provides variation without touching clocks or hardware. */
    if(!state.seed){uint32_t seed=mix(ECON_SEED^state.entropy);ModeEcon_StartSession(seed);}
}
static uint8_t swan_try(uint32_t random){
    static const uint8_t chance[]={20,40,70,100};
    if(mix(random)%100<chance[state.misses]){state.misses=0;return 1;}
    if(state.misses<3)state.misses++;return 0;
}
static uint8_t explicit_measurement(uint8_t id){
    const CalcNode *nodes=Calc_Nodes(),*n=&nodes[id];uint8_t i;
    for(i=id+1;i<Calc_NodeCount();i++)if(nodes[i].type==CALC_NODE_UNCERTAIN&&
        n->from>=nodes[i].from&&n->to<=nodes[i].to)return 1;
    return 0;
}
static float literal_sigma(uint8_t id,float value,uint32_t hash,void *context){
    Traits *traits=context;const CalcNode *n=&Calc_Nodes()[id];const CalcToken *tokens=Calc_Tokens();
    uint8_t i,dot=0,fraction=0,exponent_part=0;int exponent=0,sign=1;
    float magnitude=fabsf(value),step,precision,r;
    /* An explicit measurement supplies both its own value and an exact error. */
    if(explicit_measurement(id))return 0;
    for(i=n->start;i<n->start+n->len;i++){
        CalcToken t=tokens[i];
        if(t==CALC_T_EE){exponent_part=1;continue;}
        if(exponent_part){if(t=='-')sign=-1;else if(t>='0'&&t<='9'&&exponent<1000)exponent=exponent*10+t-'0';}
        else if(t=='.')dot=1;else if(dot)fraction++;
    }
    if(fraction>=4)traits->precise=1;
    step=powf(10.f,sign*exponent-(int)fraction);
    precision=fminf(step*.288675135f,magnitude?magnitude*PRECISION_CAP:.03f);
    r=unit(state.seed^hash^((uint32_t)(n->start+1)*0x9e3779b9u));
    r=REL_MIN+REL_SPREAD*r*r;
    return Calc_CombineSigma(precision,magnitude?magnitude*r:ZERO_SIGMA)*
           (Settings_Get()->style==ECON_TV_EXPERT?1.f:.5f);
}
static void observe(uint8_t id,CalcError *error,CalcUncertain *value,
                    CalcUncertain a,CalcUncertain b,void *context){
    Traits *t=context;uint8_t type=Calc_Nodes()[id].type;
    if(type==CALC_NODE_LN||type==CALC_NODE_LOG||type==CALC_NODE_EXP)t->methods|=METHOD_DIMENSION;
    if(type>=CALC_NODE_SIN&&type<=CALC_NODE_ATAN)t->methods|=METHOD_TRIG;
    if(type==CALC_NODE_RND){
        t->methods|=METHOD_RANDOM;*value=(CalcUncertain){1,0};*error=CALC_OK;
    }
    if(type==CALC_NODE_POW&&truncf(b.value)!=b.value)t->methods|=METHOD_POWER;
    if(*error){
        if(*error==CALC_ERR_DIV_ZERO||(*error==CALC_ERR_DOMAIN&&(!strcmp(Calc_ErrorText(*error),"Number exceeds range")||
           !strcmp(Calc_ErrorText(*error),"Result exceeds number range")||
           !strcmp(Calc_ErrorText(*error),"Uncertainty exceeds range"))))t->pathological=1;
        if(*error==CALC_ERR_DOMAIN&&a.value<0&&(type==CALC_NODE_SQRT||(type==CALC_NODE_POW&&(t->methods&METHOD_POWER))))t->imaginary=1;
        return;
    }
    if(type==CALC_NODE_NUMBER){
        if(value->value==1841)t->history|=1;if(value->value==1871)t->history|=2;
    }
    if(type==CALC_NODE_DIV||type==CALC_NODE_INV){
        float divisor=fabsf(type==CALC_NODE_DIV?b.value:a.value);
        t->instability=fmaxf(t->instability,SMALL_DIVISOR/(SMALL_DIVISOR+divisor));
    }
    if(type==CALC_NODE_POW||type==CALC_NODE_EXP)t->growth++;
    if(type==CALC_NODE_POW||type==CALC_NODE_EXP||type==CALC_NODE_SQRT||type==CALC_NODE_INV||
       type==CALC_NODE_LOG||type==CALC_NODE_LN||(type>=CALC_NODE_SIN&&type<=CALC_NODE_ATAN))t->nonlinear++;

}
CalcError ModeEcon_Evaluate(CalcUncertain *result,uint8_t degrees,CalcFeatures *features){
    CalcFeatures local;CalcError error;CalcModel model={literal_sigma,observe,&state.traits};
    ensure_seed();memset(&state.traits,0,sizeof(state.traits));state.hash=0;
    error=Calc_EvaluateModel(result,degrees,&local,&model);
    if(!error){state.hash=local.hash;if(features)*features=local;}
    else{uint8_t i;state.hash=2166136261u;for(i=0;i<Calc_Count();i++)state.hash=(state.hash^Calc_Tokens()[i])*16777619u;}
    return error;
}
/* sigma/(|mu|+sigma) is bounded even at zero; scaling avoids overflow. */
static float uncertainty_fraction(CalcUncertain r){
    float scale=fmaxf(fabsf(r.value),r.sigma);
    if(!scale)return 0;
    return (r.sigma/scale)/(fabsf(r.value)/scale+r.sigma/scale);
}
static uint8_t classify(CalcUncertain r,const CalcFeatures *f,const Traits *t,float q,float cancel){
    if(f->denominator_relative>=.5f||(t->instability>.9f&&q>.05f))return UNSTABLE;
    if(cancel>=.9f)return CANCELLATION;
    if(r.value<0)return r.sigma>=fabsf(r.value)||q>.3f?STRONG_NEGATIVE:NEGATIVE;
    if(q>=.5f)return VERY_UNCERTAIN;
    if(t->growth||(f->flags&CALC_RISK_GROWTH))return GROWTH;
    if(fabsf(r.value)>1e12f)return EXTREME;
    if(q>.12f)return UNCERTAIN;
    if(q<.002f||(t->precise&&q<.05f))return PRECISE;
    return q<.05f?CONFIDENT:MILD;
}
static const char *personal(CalcUncertain r,const Traits *t,uint8_t *egg){
    uint8_t h=t->history;
    if(r.value==1841)h|=1;if(r.value==1871)h|=2;
    if(h==3){*egg=2;return "30 years of equilibrium.";}
    if(h==1){*egg=0;return "Journal des Economistes initialized.";}
    if(h==2){*egg=1;return "End of historical sample.";}
    return 0;
}
static uint8_t choose(uint8_t kind,uint32_t hash,uint8_t remember){
    uint8_t i,index=0;uint16_t total=0,pick;uint32_t eligible=0;
    for(i=0;i<JOKE_COUNT;i++)if(jokes[i].classes&C(kind))eligible|=1u<<i;
    if(kind==STRONG_NEGATIVE&&mix(hash)%16)eligible&=~(1u<<25);
    if(!eligible)eligible=3;
    if(remember){
        if(!(eligible&~state.seen))state.seen&=~eligible;
        eligible&=~state.seen;
        if(state.last_joke<JOKE_COUNT&&(eligible&~(1u<<state.last_joke)))eligible&=~(1u<<state.last_joke);
    }
    for(i=0;i<JOKE_COUNT;i++)if(eligible&(1u<<i))total+=jokes[i].weight;
    pick=mix(hash)%total;
    for(i=0;i<JOKE_COUNT;i++)if(eligible&(1u<<i)){
        index=i;if(pick<jokes[i].weight)break;pick-=jokes[i].weight;
    }
    if(remember){state.seen|=1u<<index;state.last_joke=index;}
    return index;
}
static EconForecast forecast(CalcUncertain r,const CalcFeatures *f,uint8_t *kind,uint32_t presentation){
    Traits empty={0},*t=state.hash==f->hash?&state.traits:&empty;
    EconForecast out;float q,cancel,score,k,limit,room;uint32_t hash;uint8_t egg=0;
    q=uncertainty_fraction(r);cancel=f->cancellation;
    hash=mix((state.seed?state.seed:ECON_SEED)^f->hash^value_bits(r.value)^value_bits(r.sigma));
    score=SCORE_UNCERTAINTY*q+SCORE_CANCEL*cancel+SCORE_INSTABILITY*fminf(f->denominator_relative,1)+
          SCORE_SMALL_DIVISOR*t->instability+SCORE_NONLINEAR*fminf(t->nonlinear,4)+((f->flags&CALC_RISK_GROWTH)?5:0)+
          fminf(f->operations,12)*.5f+(fabsf(r.value)>1e12f?5:0)+(unit(hash)-.5f)*4;
    out.flags=0;out.absurdity=(uint8_t)fmaxf(0,fminf(100,score));
    *kind=classify(r,f,t,q,cancel);
    limit=score<20?SHIFT_CALM:score<50?SHIFT_MILD:score<80?SHIFT_HIGH:SHIFT_EXTREME;
    if(Settings_Get()->style==ECON_TV_EXPERT)limit=fminf(3,limit+.35f+q);
    k=Settings_Get()->style==ECON_REFEREE?0:(2*unit(hash^presentation^0xb7e15162u)-1)*limit;
    /* Stay finite by reducing the coefficient, never inventing an unrelated value. */
    room=fmaxf(0,FLT_MAX*.95f-fabsf(r.value));
    if(r.sigma&&fabsf(k)>room/r.sigma)k=copysignf(room/r.sigma,k);
    out.result.value=r.value+k*r.sigma;
    out.result.sigma=r.sigma;
    out.message=personal(r,t,&egg);
    if(!out.message){
        if(Settings_Get()->style==ECON_TV_EXPERT){
            static const char *const claims[]={"100% CONFIDENT","VERY CLEAR","Definitely.","Trust me."};
            if(unit(hash^0x85ebca6bu)<.25f+.7f*q)out.flags|=ECON_HIDE_SIGMA;
            out.message=q>.25f?claims[mix(hash)%2]:claims[mix(hash)%4];
        }else if(Settings_Get()->style==ECON_REFEREE){
            if(f->denominator_relative>=.5f)out.message="Denominator may be endogenous.";
            else if(cancel>.9f)out.message="Identification is weak.";
            else if(q>.65f)out.message="Standard errors are concerning.";
            else if(q>.5f)out.message="Sign is not robust.";
            else if(f->operations>=8)out.message="Model appears overparameterized.";
            else if(t->methods&METHOD_POWER)out.message="Functional form unjustified.";
            else if(t->growth)out.message="Out-of-sample validity not established.";
            else if(t->methods&METHOD_DIMENSION)out.message="Dimensionless? Please clarify.";
            else if(t->methods&METHOD_TRIG)out.message="Economic meaning unclear.";
            else if(q<.002f)out.message="Standard errors are suspiciously small.";
            else if(Settings_Digits()>=6&&q>.05f)out.message="Precision exceeds evidence.";
            else out.message=0;
            if(out.message&&(q>.65f||cancel>.95f)&&mix(hash^0xc2b2ae35u)%29==0)out.flags|=ECON_REFUSED;
        }else out.message=jokes[choose(*kind,hash,0)].text;
    }
    return out;
}
EconForecast ModeEcon_Forecast(CalcUncertain result,const CalcFeatures *features){
    uint8_t kind;return forecast(result,features,&kind,0);
}
void ModeEcon_Result(CalcUncertain result,const CalcFeatures *features){
    EconForecast out;uint8_t kind,egg=0,strong,show,revised=0,style=Settings_Get()->style;const char *message;
    Traits empty={0},*t=state.hash==features->hash?&state.traits:&empty;
    uint32_t random=features->hash^state.seed^mix(++state.count);
    float q=uncertainty_fraction(result);
    if(state.repeat_hash==features->hash){if(state.repeats<255)state.repeats++;}
    else{state.repeat_hash=features->hash;state.repeats=0;state.presentation=0;}
    /* Fresh interpretation on every repeat, without reseeding the input model. */
    if(style!=ECON_REFEREE&&result.sigma>0&&state.repeats){state.presentation=mix(random);revised=1;}
    out=forecast(result,features,&kind,state.presentation);
    message=personal(result,t,&egg);
    if(message&&(state.seen&(1u<<(28+egg)))){message=0;out.message=0;}
    if(message){state.seen|=1u<<(28+egg);out.flags&=~(ECON_BLACK_SWAN|ECON_REFUSED);}
    uint8_t visual=0,number=number_egg(result.value);
    if(state.cooldown)state.cooldown--;
    if(message)visual=ECON_VIS_HISTORY;
    else if(number&&!(state.eggs_seen&(1u<<(number-1)))){
        state.eggs_seen|=1u<<(number-1);message=eggs[number-1].text;visual=eggs[number-1].visual;
        out.flags&=~ECON_REFUSED;
    }else if(!state.cooldown){
        if(features->denominator_relative>=1||q>.85f||out.absurdity>95){
            if(swan_try(random)){out.flags=ECON_BLACK_SWAN;message="Not in the model.";}
        }
        if(!(out.flags&ECON_BLACK_SWAN)&&(mix(random)%4==0||state.count%7==0)){
            if(out.absurdity>85){visual=ECON_VIS_MODEL+(mix(random)>>8)%3;message=visual==ECON_VIS_CRASH?"Strong fundamentals.":"Subject to reality.";}
            else if(style==ECON_TV_EXPERT){visual=ECON_VIS_TV;message=out.message;}
            else if(style==ECON_REFEREE){visual=out.flags&ECON_REFUSED?ECON_VIS_REJECT:ECON_VIS_REVIEW;message=out.message?out.message:"Please clarify.";}
            else if(state.count%7==0){visual=ECON_VIS_REVIEW;message="Best estimate. For now.";}
        }
    }
    if(!message&&state.repeats){
        static const char *const story[]={"Still.","Consensus achieved.","Robustness confirmed.","Please stop.","You asked already.","Nothing changed.","Fine."};
        message=story[state.repeats<5?state.repeats-1:4+mix(random)%3];
        if(style==ECON_TV_EXPERT&&state.repeats!=2)message="Still definitely.";
        if(style==ECON_REFEREE&&state.repeats!=2)message="Please clarify.";
        if(revised&&mix(random)%4==0)message=state.repeats&1?"Revised estimate.":"Same model. New answer.";
    }
    strong=kind==CANCELLATION||kind==VERY_UNCERTAIN||kind==UNSTABLE||kind==STRONG_NEGATIVE;
    show=message||(out.flags&ECON_REFUSED)||(style==ECON_REFEREE&&out.message&&state.last_kind!=kind)||state.quiet>=3||(strong&&state.last_kind!=kind)||
         unit(random)<(style==ECON_TV_EXPERT?.55f+.3f*q:.15f+.2f*state.quiet+(strong?.25f:0));
    if(show){
        if(!message){
            if(style!=ECON_ECONOMIST)message=out.message?out.message:"Please clarify.";
            else if(Settings_Digits()>=6&&q>.1f&&mix(random)%7==0){
                message="More digits for credibility.";
            }
            else message=jokes[choose(kind,random,1)].text;
        }
        state.quiet=0;
    }else{message="";state.quiet++;}
    state.last_kind=kind;
    if((t->methods&METHOD_RANDOM)&&!result.sigma){message="";visual=out.flags=0;}
    if(out.flags&ECON_BLACK_SWAN){state.cooldown=4;GUI_DrawBlackSwan(message);return;}
    if(visual){
        char text[24];
        /* Numeric gags identify a number, rather than report an estimate. */
        if(fabsf(result.value)<1e7f&&truncf(result.value)==result.value)Calc_FormatFixed(result.value,text,sizeof(text),0);
        else Calc_FormatDisplay(result.value,text,sizeof(text),6,0);
        GUI_DrawEconVisual(visual,visual==ECON_VIS_TV?out.message:visual==ECON_VIS_REVIEW?(style==ECON_REFEREE?"REVISION":"ESTIMATE"):visual==ECON_VIS_REJECT?"REJECTED":text);
        state.cooldown=4;GUI_DrawMessage(message);return;
    }
    if(out.flags&ECON_REFUSED){GUI_ClearResult();GUI_DrawMessage(message);return;}
    if(out.flags&ECON_HIDE_SIGMA){char text[24];Settings_Format(out.result.value,text,sizeof(text),Settings_Digits()<4?4:0);GUI_DrawResult(text);}
    /* Cosmetic error-bar revision; propagation, scoring and Ans retain sigma. */
    else GUI_DrawUncertain(out.result.value,fminf(FLT_MAX,out.result.sigma*(.9f+.2f*unit(random))));
    GUI_DrawMessage(message);
}
const char *ModeEcon_ErrorMessage(CalcError error){
    uint8_t egg=0;const char *message=personal((CalcUncertain){0,0},&state.traits,&egg);
    if(message&&!(state.seen&(1u<<(28+egg)))){state.seen|=1u<<(28+egg);return message;}
    if(state.traits.imaginary&&error==CALC_ERR_DOMAIN)return "2002 Ig Nobel got there first.";
    if(error==CALC_ERR_DOMAIN||error==CALC_ERR_DIV_ZERO)return "Reality did not cooperate.";
    return Calc_ErrorText(error);
}
void ModeEcon_Enter(GuiStatus *status){ModeNormal_Enter(status);}
void ModeEcon_Update(GuiStatus *status){
    if(!state.seed)state.entropy++;
    if(Input_WasPressed(KEY_ENTER)&&!status->fn&&!Input_WasPressed(KEY_AC)){
        CalcUncertain result;CalcFeatures features;CalcError error;
        error=ModeEcon_Evaluate(&result,status->angle==GUI_ANGLE_DEG,&features);
        if(error){
            if(state.cooldown)state.cooldown--;
            const char *message=ModeEcon_ErrorMessage(error);
            GUI_ClearResult();GUI_DrawError(error);GUI_DrawEditor(0,0);
            if(state.traits.imaginary&&!state.traits.history){GUI_DrawEconVisual(ECON_VIS_IMAGINARY,"i");GUI_DrawMessage(message);}
            else if(state.traits.pathological&&!state.traits.history&&!state.cooldown&&swan_try(state.hash^state.seed^++state.count)){
                state.cooldown=4;GUI_DrawBlackSwan("Not in the model.");
            }
            else GUI_DrawMessage(message);
        }
        else{GUI_DrawExpression(Calc_Root());ModeEcon_Result(result,&features);}
        return;
    }
    ModeNormal_Update(status);
}
