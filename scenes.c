#include "moviemst.h"
#include <stdlib.h>
#include <string.h>

int scene_count(const Doc *d){int i,n=0;for(i=0;i<d->n;i++)n+=d->p[i].type==SCENE;return n;}

int scene_parse(const char *label,int *base,int *suffix){
    const unsigned char *p=(const unsigned char*)label;int b=0,s=0;
    if(*p<'0'||*p>'9')return 0;
    while(*p>='0'&&*p<='9'){b=b*10+*p++-'0';if(b>1000000)return 0;}
    while(*p>='A'&&*p<='Z'){s=s*26+*p++-'A'+1;if(s>1000000)return 0;}
    if(*p||(!b&&!s))return 0;
    *base=b;*suffix=s;return 1;
}
void scene_label(const Doc *d,int para,char out[16]){
    int i,n=0,id=d->p[para].scene_id;out[0]=0;
    if(d->p[para].type!=SCENE)return;
    if(d->locked&&id>0&&id<=d->nn){
        SceneNumber number=d->numbers[id-1];char suffix[8];int len=0;
        while(number.suffix){number.suffix--;suffix[len++]=(char)('A'+number.suffix%26);number.suffix/=26;}
        n=snprintf(out,16,"%d",number.base);
        while(len&&n<15)out[n++]=suffix[--len];
        out[n]=0;
    }else{for(i=0;i<=para;i++)n+=d->p[i].type==SCENE;snprintf(out,16,"%d",n);}
}
/* The registry includes deleted scenes, so a production number is never reused. */
int scene_register(Doc *d,int base,int suffix){
    int i;SceneNumber *p;
    if(base<0||base>1000000||suffix<0||suffix>1000000||(!base&&!suffix)||d->nn==MAX_SCENE_LABELS)return 0;
    for(i=0;i<d->nn;i++)if(d->numbers[i].base==base&&d->numbers[i].suffix==suffix)return 0;
    if(d->nn==d->nc){int cap=d->nc?d->nc*2:16;p=mem(cap*sizeof(*p));if(d->nn)memcpy(p,d->numbers,d->nn*sizeof(*p));free(d->numbers);d->numbers=p;d->nc=cap;}
    d->numbers[d->nn++]=(SceneNumber){base,suffix};return d->nn;
}
int scene_assign(Doc *d,int para){
    int i,base=0,suffix=0,next=0;
    if(d->p[para].type!=SCENE||d->p[para].scene_id)return 1;
    for(i=para-1;i>=0;i--)if(d->p[i].type==SCENE&&d->p[i].scene_id){base=d->numbers[d->p[i].scene_id-1].base;break;}
    for(i=para+1;i<d->n;i++)if(d->p[i].type==SCENE&&d->p[i].scene_id){next=1;break;}
    if(next){
        for(i=0;i<d->nn;i++)if(d->numbers[i].base==base&&d->numbers[i].suffix>suffix)suffix=d->numbers[i].suffix;
        suffix++;
    }else{for(i=0;i<d->nn;i++)if(d->numbers[i].base>base)base=d->numbers[i].base;base++;}
    d->p[para].scene_id=scene_register(d,base,suffix);return d->p[para].scene_id!=0;
}
void scene_lock(Doc *d,int locked){
    int i;
    if(d->locked==locked)return;
    free(d->numbers);d->numbers=NULL;d->nn=d->nc=0;
    if(locked){d->nc=scene_count(d);if(d->nc)d->numbers=mem(d->nc*sizeof(SceneNumber));}
    for(i=0;i<d->n;i++){
        d->p[i].scene_id=0;
        if(locked&&d->p[i].type==SCENE){d->numbers[d->nn]=(SceneNumber){d->nn+1,0};d->p[i].scene_id=++d->nn;}
    }
    d->locked=locked;if(locked)d->numbering=1;
}
