#include "moviemst.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include "encoding.h"

const char *type_names[TYPES]={"Scene", "Action", "Character", "Dialogue", "Parenthetical", "Transition"};
void *mem(size_t n) { void *p=calloc(1,n?n:1); if(!p){fputs("Out of memory. Check your recovery file.\n",stderr);exit(2);}return p; }
char *dupstr(const char *s){char *p=mem(strlen(s)+1);strcpy(p,s);return p;}
void doc_init(Doc *d){memset(d,0,sizeof(*d));doc_insert(d,0,ACTION,"");}
void doc_free(Doc *d){int i;for(i=0;i<d->n;i++)free(d->p[i].text);free(d->p);free(d->numbers);memset(d,0,sizeof(*d));}
int doc_insert(Doc *d,int at,int type,const char *text){
    if(d->n>=MAX_PARAS||strlen(text)>MAX_TEXT||at<0||at>d->n||type<0||type>=TYPES)return 0;
    if(d->n==d->cap){int cap=d->cap?d->cap*2:16;Para *p=mem(cap*sizeof(*p));if(d->n)memcpy(p,d->p,d->n*sizeof(*p));free(d->p);d->p=p;d->cap=cap;}
    memmove(d->p+at+1,d->p+at,(d->n-at)*sizeof(Para));d->p[at].type=type;d->p[at].text=dupstr(text);d->p[at].scene_id=0;d->n++;
    if(d->locked&&type==SCENE&&!scene_assign(d,at)){doc_remove(d,at);return 0;}return 1;
}
void doc_remove(Doc *d,int at){free(d->p[at].text);memmove(d->p+at,d->p+at+1,(d->n-at-1)*sizeof(Para));d->n--;}
void doc_clone(Doc *dst,const Doc *src){int i;memset(dst,0,sizeof(*dst));strcpy(dst->title,src->title);strcpy(dst->author,src->author);for(i=0;i<src->n;i++){doc_insert(dst,dst->n,src->p[i].type,src->p[i].text);dst->p[i].scene_id=src->p[i].scene_id;}dst->numbering=src->numbering;dst->locked=src->locked;dst->nn=dst->nc=src->nn;if(src->nn){dst->numbers=mem(src->nn*sizeof(SceneNumber));memcpy(dst->numbers,src->numbers,src->nn*sizeof(SceneNumber));}}
void state_free(State *s){doc_free(&s->d);memset(s,0,sizeof(*s));}
void state_clone(State *dst,const State *src){*dst=*src;doc_clone(&dst->d,&src->d);}
static size_t state_size(const State *s){size_t n=sizeof(*s)+s->d.cap*sizeof(Para)+s->d.nc*sizeof(SceneNumber);int i;for(i=0;i<s->d.n;i++)n+=strlen(s->d.p[i].text)+1;return n;}
void history_clear(History *h){int i;for(i=0;i<h->un;i++)state_free(h->undo+i);for(i=0;i<h->rn;i++)state_free(h->redo+i);memset(h,0,sizeof(*h));}
void checkpoint(History *h,const State *s){
    int i;size_t n=state_size(s);for(i=0;i<h->rn;i++)state_free(h->redo+i);h->rn=0;
    while(h->un&&(h->un==32||h->bytes+n>16UL*1024*1024)){h->bytes-=state_size(h->undo);state_free(h->undo);memmove(h->undo,h->undo+1,(--h->un)*sizeof(State));}
    state_clone(h->undo+h->un++,s);h->bytes+=n;
}
int undo(History *h,State *s,int redo){
    State *from=redo?h->redo:h->undo,*to=redo?h->undo:h->redo;int *fn=redo?&h->rn:&h->un,*tn=redo?&h->un:&h->rn;
    if(!*fn)return 0;
    state_clone(to+(*tn)++,s);if(redo)h->bytes+=state_size(s);else h->bytes-=state_size(from+*fn-1);
    state_free(s);*s=from[--*fn];memset(from+*fn,0,sizeof(State));return 1;
}
int pos_cmp(Pos a,Pos b){if(a.p!=b.p)return a.p<b.p?-1:1;return a.o==b.o?0:(a.o<b.o?-1:1);}
int selection(const State *s,Pos *a,Pos *b){if(!s->selecting||!pos_cmp(s->cur,s->anchor))return 0;if(pos_cmp(s->cur,s->anchor)<0){*a=s->cur;*b=s->anchor;}else{*a=s->anchor;*b=s->cur;}return 1;}
int delete_selection(State *s){
    Pos a,b;char *text;size_t n;if(!selection(s,&a,&b)){s->selecting=0;return 0;}
    n=a.o+strlen(s->d.p[b.p].text+b.o);if(n>MAX_TEXT)return 0;
    text=mem(n+1);memcpy(text,s->d.p[a.p].text,a.o);strcpy(text+a.o,s->d.p[b.p].text+b.o);
    free(s->d.p[a.p].text);s->d.p[a.p].text=text;while(b.p>a.p){doc_remove(&s->d,a.p+1);b.p--;}
    s->cur=a;s->selecting=0;return 1;
}
int insert_text(State *s,const char *text){
    char *old,*newtext;size_t len=strlen(text),n;Pos a,b;
    if(selection(s,&a,&b)){n=a.o+strlen(s->d.p[b.p].text+b.o);}else n=strlen(s->d.p[s->cur.p].text);
    if(n+len>MAX_TEXT)return 0;
    delete_selection(s);old=s->d.p[s->cur.p].text;n=strlen(old);newtext=mem(n+len+1);
    memcpy(newtext,old,s->cur.o);memcpy(newtext+s->cur.o,text,len);strcpy(newtext+s->cur.o+len,old+s->cur.o);
    free(old);s->d.p[s->cur.p].text=newtext;s->cur.o+=(int)len;return 1;
}
int split_para(State *s){
    Para *p;char *tail;int type;if(s->d.n>=MAX_PARAS)return 0;
    if(s->selecting&&!delete_selection(s)&&pos_cmp(s->cur,s->anchor))return 0;
    p=s->d.p+s->cur.p;type=p->type;
    if(!p->text[0]&&type!=ACTION){set_type(s,ACTION);return 1;}
    tail=dupstr(p->text+s->cur.o);p->text[s->cur.o]=0;
    type=type==CHARACTER||type==PAREN?DIALOGUE:ACTION;
    doc_insert(&s->d,s->cur.p+1,type,tail);free(tail);s->cur.p++;s->cur.o=0;return 1;
}
int backspace(State *s,int forward){
    Para *p=s->d.p+s->cur.p;int n=(int)strlen(p->text);char *joined;
    if(s->selecting){Pos a,b;if(selection(s,&a,&b))return delete_selection(s);s->selecting=0;}
    if(forward&&s->cur.o<n){memmove(p->text+s->cur.o,p->text+s->cur.o+1,n-s->cur.o);return 1;}
    if(!forward&&s->cur.o){memmove(p->text+s->cur.o-1,p->text+s->cur.o,n-s->cur.o+1);s->cur.o--;return 1;}
    if(forward){if(s->cur.p+1>=s->d.n)return 0;}else{if(!s->cur.p)return 0;s->cur.p--;s->cur.o=(int)strlen(s->d.p[s->cur.p].text);}
    p=s->d.p+s->cur.p;n=(int)strlen(p->text);if(n+strlen(p[1].text)>MAX_TEXT){if(!forward){s->cur.p++;s->cur.o=0;}return 0;}
    joined=mem(n+strlen(p[1].text)+1);strcpy(joined,p->text);strcat(joined,p[1].text);free(p->text);p->text=joined;doc_remove(&s->d,s->cur.p+1);return 1;
}
int upper857(int c){return letter_case(c,1);}
int lower857(int c){return letter_case(c,0);}
/* Capitals are element formatting; keep the writer's original spelling. */
int element_char(int type,int c){return type==SCENE||type==CHARACTER||type==TRANSITION?upper857(c):c;}
const char *element_find(int type,const char *text,const char *needle){
    size_t i;if(type!=SCENE&&type!=CHARACTER&&type!=TRANSITION)return strstr(text,needle);
    if(!*needle)return text;
    for(;*text;text++){for(i=0;needle[i]&&text[i]&&element_char(type,(unsigned char)text[i])==element_char(type,(unsigned char)needle[i]);i++){}if(!needle[i])return text;}
    return NULL;
}
int set_type(State *s,int type){Para *p=s->d.p+s->cur.p;int old=p->type;if(type==old)return 1;p->type=type;if(type==SCENE&&s->d.locked&&!scene_assign(&s->d,s->cur.p)){p->type=old;return 0;}if(type!=SCENE)p->scene_id=0;return 1;}
int copy_selection(State *s,Doc *clip){Pos a,b;int i;if(!selection(s,&a,&b))return 0;doc_free(clip);for(i=a.p;i<=b.p;i++){int start=i==a.p?a.o:0,end=i==b.p?b.o:(int)strlen(s->d.p[i].text);char *text=mem(end-start+1);memcpy(text,s->d.p[i].text+start,end-start);doc_insert(clip,clip->n,s->d.p[i].type,text);free(text);}return 1;}
int paste(State *s,const Doc *clip){
    State tmp;int i,start;char *tail;if(!clip->n||(s->d.locked&&s->d.nn+scene_count(clip)>MAX_SCENE_LABELS))return 0;
    state_clone(&tmp,s);if(tmp.selecting){Pos a,b;if(selection(&tmp,&a,&b)&&!delete_selection(&tmp)){state_free(&tmp);return 0;}}
    if(clip->n==1){int ok=insert_text(&tmp,clip->p[0].text);if(!ok){state_free(&tmp);return 0;}}
    else{
        if(tmp.d.n+clip->n-1>MAX_PARAS||tmp.cur.o+strlen(clip->p[0].text)>MAX_TEXT||strlen(tmp.d.p[tmp.cur.p].text+tmp.cur.o)+strlen(clip->p[clip->n-1].text)>MAX_TEXT){state_free(&tmp);return 0;}
        tail=dupstr(tmp.d.p[tmp.cur.p].text+tmp.cur.o);tmp.d.p[tmp.cur.p].text[tmp.cur.o]=0;insert_text(&tmp,clip->p[0].text);start=tmp.cur.p;
        for(i=1;i<clip->n;i++)if(!doc_insert(&tmp.d,start+i,clip->p[i].type,clip->p[i].text)){free(tail);state_free(&tmp);return 0;}
        tmp.cur.p=start+clip->n-1;tmp.cur.o=(int)strlen(clip->p[clip->n-1].text);insert_text(&tmp,tail);tmp.cur.o-=strlen(tail);free(tail);
    }
    state_free(s);*s=tmp;s->selecting=0;return 1;
}
int from_unicode(unsigned int c){int i;for(i=32;i<256;i++)if(unicode857[i]==c)return i;return c=='\t'?' ':-1;}
int utf8_decode(const char *in,char *out,size_t cap){
    const unsigned char *p=(const unsigned char*)in;size_t n=0;while(*p){unsigned int c=*p++;int extra=0,v;if(c>=0xc2&&c<=0xdf){c&=31;extra=1;}else if(c>=0xe0&&c<=0xef){c&=15;extra=2;}else if(c>=0xf0&&c<=0xf4){c&=7;extra=3;}else if(c>=128)return 0;
        {int k;for(k=0;k<extra;k++){if((*p&192)!=128)return 0;c=(c<<6)|(*p++&63);}}
        if((extra==1&&c<128)||(extra==2&&(c<2048||(c>=0xd800&&c<=0xdfff)))||(extra==3&&(c<65536||c>0x10ffff)))return 0;
        v=from_unicode(c);if(v<32||n+1>=cap)return 0;out[n++]=(char)v;
    }out[n]=0;return 1;
}
char *utf8_encode(const char *in){char *out=mem(strlen(in)*3+1),*p=out;while(*in){unsigned int c=unicode857[(unsigned char)*in++];if(c<128)*p++=(char)c;else if(c<2048){*p++=(char)(192|(c>>6));*p++=(char)(128|(c&63));}else{*p++=(char)(224|(c>>12));*p++=(char)(128|((c>>6)&63));*p++=(char)(128|(c&63));}}return out;}
unsigned long doc_hash(const Doc *d){uint32_t h=2166136261u;int i;const unsigned char *p;for(i=-2;i<d->n;i++){p=(const unsigned char*)(i==-2?d->title:i==-1?d->author:d->p[i].text);if(i>=0)h=(h^(d->p[i].type+1))*16777619u;while(*p)h=(h^*p++)*16777619u;h=(h^255)*16777619u;}
    h=(h^d->numbering)*16777619u;h=(h^d->locked)*16777619u;
    for(i=0;i<d->nn;i++){h=(h^d->numbers[i].base)*16777619u;h=(h^d->numbers[i].suffix)*16777619u;}
    for(i=0;i<d->n;i++)h=(h^d->p[i].scene_id)*16777619u;
    return h;
}

static int write_field(FILE *f,const char *text){char *u=utf8_encode(text),*p=u;int ok=1;while(*p&&ok){if(*p=='\\')ok=fputs("\\\\",f)>=0;else ok=fputc(*p,f)!=EOF;p++;}free(u);return ok&&fputc('\n',f)!=EOF;}
static int write_native(const Doc *d,FILE *f){
    int i,v2=d->numbering||d->locked||d->nn;
    if(fprintf(f,"MOVIEMASTER %d\nT\t",v2?2:1)<0||!write_field(f,d->title)||fputs("Y\t",f)<0||!write_field(f,d->author))return 0;
    if(v2){
        if(fprintf(f,"N\t%d\t%d\t%d\n",d->numbering,d->locked,d->nn)<0)return 0;
        for(i=0;i<d->nn;i++)if(fprintf(f,"S\t%d\t%d\n",d->numbers[i].base,d->numbers[i].suffix)<0)return 0;
    }
    for(i=0;i<d->n;i++){
        if(fprintf(f,"%d\t",d->p[i].type)<0||(v2&&fprintf(f,"%d\t",d->p[i].scene_id)<0)||!write_field(f,d->p[i].text))return 0;
    }
    return !ferror(f);
}
static void side_path(const char *path,const char *ext,char *out){const char *sep1=strrchr(path,'/'),*sep2=strrchr(path,'\\'),*dot=strrchr(path,'.');const char *sep=sep1;if(!sep||(sep2&&sep2>sep))sep=sep2;if(dot&&(!sep||dot>sep))snprintf(out,512,"%.*s.%s",(int)(dot-path),path,ext);else snprintf(out,512,"%s.%s",path,ext);}
int save_doc(const Doc *d,const char *path,char *error){
    FILE *f;char tmp[512],old[512],bak[512];int exists,ok,type;
    if(strlen(path)>490){strcpy(error,"File path is too long.");return 0;}
    side_path(path,"TMP",tmp);side_path(path,"OLD",old);side_path(path,"BAK",bak);
    if(!strcmp(path,tmp)||!strcmp(path,old)||!strcmp(path,bak)){strcpy(error,"Use the .SEN document extension.");return 0;}
    /* Never overwrite a leftover transaction: it may be the only recovery copy. */
    if(path_type(tmp)>=0||errno!=ENOENT){strcpy(error,"TMP exists or cannot be accessed. Preserve it before saving.");return 0;}
    if(path_type(old)>=0||errno!=ENOENT){strcpy(error,"OLD exists or cannot be accessed. Preserve it before saving.");return 0;}
    type=path_type(path);exists=type>=0;
    if((exists&&type!=PATH_FILE)||(!exists&&errno!=ENOENT)){strcpy(error,"Target is not a regular file or cannot be accessed.");return 0;}
    f=file_open(tmp,"wb");if(!f){strcpy(error,"Cannot create a temporary file.");return 0;}
    ok=write_native(d,f);if(fflush(f))ok=0;if(fclose(f))ok=0;if(!ok){file_remove(tmp);strcpy(error,"Write failed. The original file was preserved.");return 0;}
    if(exists&&file_rename(path,old)){file_remove(tmp);strcpy(error,"Cannot back up the original file.");return 0;}
    if(file_rename(tmp,path)){if(exists)file_rename(old,path);strcpy(error,"Save failed. Keep the TMP/OLD files for recovery.");return 0;}
    if(exists){if(file_remove(bak)&&errno!=ENOENT){strcpy(error,"Saved; the previous version remains in OLD.");return 1;}if(file_rename(old,bak)){strcpy(error,"Saved; the previous version remains in OLD.");return 1;}}
    strcpy(error,"Saved.");return 1;
}
static int native_int(const char **cursor,int *value,int terminal,int limit){
    const char *p=*cursor;long n;char *end;
    if(*p<'0'||*p>'9')return 0;
    errno=0;n=strtol(p,&end,10);
    if(errno||n>limit||*end!=terminal)return 0;
    *value=(int)n;*cursor=end+(terminal!=0);return 1;
}
int load_doc(Doc *d,const char *path,char *error){
    FILE *f=file_open(path,"rb");Doc tmp={0};
    char *line=mem(MAX_TEXT*4+64),*raw=mem(MAX_TEXT*4+64),*decoded=mem(MAX_TEXT+1);
    int count=0,ok=1,v2=0,show=0,locked=0,expected=0,*used=mem(MAX_SCENE_LABELS*sizeof(int));
    if(!f){strcpy(error,"Cannot open the file.");ok=0;goto done;}
    while(fgets(line,MAX_TEXT*4+64,f)){
        size_t n=strlen(line),i=2,j=0;int id=0;const char *cursor=line+2;
        if(!n||line[n-1]!='\n'){ok=0;break;}line[--n]=0;if(n&&line[n-1]=='\r')line[--n]=0;
        if(count++==0){v2=!strcmp(line,"MOVIEMASTER 2")||!strcmp(line,"WRITER 2");if(!v2&&strcmp(line,"MOVIEMASTER 1")&&strcmp(line,"WRITER 1")){ok=0;break;}continue;}
        if(n<2||line[1]!='\t'){ok=0;break;}
        if(v2&&count==4){
            if(line[0]!='N'||!native_int(&cursor,&show,'\t',1)||!native_int(&cursor,&locked,'\t',1)||!native_int(&cursor,&expected,0,MAX_SCENE_LABELS)||(!locked&&expected)){ok=0;break;}
            continue;
        }
        if(v2&&count>4&&tmp.nn<expected){
            int base,suffix;
            if(line[0]!='S'||!native_int(&cursor,&base,'\t',1000000)||!native_int(&cursor,&suffix,0,1000000)||!scene_register(&tmp,base,suffix)){ok=0;break;}
            continue;
        }
        if(v2&&count>4){
            if(!native_int(&cursor,&id,'\t',tmp.nn)){ok=0;break;}
            i=(size_t)(cursor-line);
        }
        for(;i<n;i++){if(line[i]=='\\'){if(++i>=n||line[i]!='\\'){ok=0;break;}}raw[j++]=line[i];}raw[j]=0;
        if(!ok||!utf8_decode(raw,decoded,MAX_TEXT+1)){ok=0;break;}
        if(count==2&&line[0]=='T'){if(strlen(decoded)>127){ok=0;break;}strcpy(tmp.title,decoded);}
        else if(count==3&&line[0]=='Y'){if(strlen(decoded)>127){ok=0;break;}strcpy(tmp.author,decoded);}
        else if(count>(v2?4:3)&&line[0]>='0'&&line[0]<'0'+TYPES){
            int type=line[0]-'0';
            if((id&&(type!=SCENE||used[id-1]))||(locked&&type==SCENE&&!id)||!doc_insert(&tmp,tmp.n,type,decoded)){ok=0;break;}
            tmp.p[tmp.n-1].scene_id=id;if(id)used[id-1]=1;
        }else{ok=0;break;}
    }
    if(ferror(f)||count<(v2?5:4)||tmp.nn!=expected||!tmp.n)ok=0;
    fclose(f);
 done:
    free(line);free(raw);free(decoded);free(used);
    if(!ok){doc_free(&tmp);strcpy(error,"Invalid/incomplete SEN file. Current document was preserved.");return 0;}
    tmp.numbering=show;tmp.locked=locked;doc_free(d);*d=tmp;strcpy(error,"Opened.");return 1;
}
static void row_add(Layout *l,int p,int start,int end,int indent,int *page,int *line){Row *r;if(*line>=54){(*page)++;*line=0;}if(l->n==l->cap){Row *newrows;int cap=l->cap?l->cap*2:128;newrows=mem(cap*sizeof(Row));if(l->n)memcpy(newrows,l->r,l->n*sizeof(Row));free(l->r);l->r=newrows;l->cap=cap;}r=l->r+l->n++;r->p=p;r->start=start;r->end=end;r->indent=indent;r->page=*page;r->line=(*line)++;}
void layout_build(const Doc *d,Layout *l){
    int i,page=1,line=0;memset(l,0,sizeof(*l));for(i=0;i<d->n;i++){
        const Para *p=d->p+i;int pos=0,len=(int)strlen(p->text),left=0,width=60,gap=i>0&&p->type!=DIALOGUE&&p->type!=PAREN;
        if(p->type==CHARACTER){left=22;width=38;}if(p->type==DIALOGUE){left=10;width=40;}if(p->type==PAREN){left=16;width=30;}
        if((p->type==SCENE||p->type==CHARACTER)&&line+gap+3>54){page++;line=0;gap=0;}
        if(gap){row_add(l,-1,0,0,0,&page,&line);}
        do{int end=pos+width,next;if(end>len)end=len;next=end;
            if(end<len){int k=end;while(k>pos&&p->text[k]!=' ')k--;if(k>pos){end=k;next=k+1;}}
            row_add(l,i,pos,end,p->type==TRANSITION?60-(end-pos):left,&page,&line);pos=next;
        }while(pos<len);
    }l->pages=page;
}
void layout_free(Layout *l){free(l->r);memset(l,0,sizeof(*l));}
int scene_end(const Doc *d,int start){int i;for(i=start+1;i<d->n&&d->p[i].type!=SCENE;i++){}return i;}
static int scene_start(const State *s){int i;for(i=s->cur.p;i>=0;i--)if(s->d.p[i].type==SCENE)return i;return -1;}
int scene_duplicate(State *s){int start=scene_start(s),end,i;Doc block;if(start<0)return 0;end=scene_end(&s->d,start);if((s->d.locked&&s->d.nn==MAX_SCENE_LABELS)||s->d.n+end-start>MAX_PARAS)return 0;memset(&block,0,sizeof(block));for(i=start;i<end;i++)doc_insert(&block,block.n,s->d.p[i].type,s->d.p[i].text);for(i=0;i<block.n;i++)if(!doc_insert(&s->d,end+i,block.p[i].type,block.p[i].text)){while(i)doc_remove(&s->d,end+--i);doc_free(&block);return 0;}doc_free(&block);s->cur.p=end;s->cur.o=0;s->selecting=0;return 1;}
int scene_move(State *s,int direction){
    int start=scene_start(s),end,other,count;Para *tmp;if(start<0)return 0;end=scene_end(&s->d,start);count=end-start;
    if(direction<0){other=start-1;while(other>=0&&s->d.p[other].type!=SCENE)other--;if(other<0)return 0;tmp=mem(count*sizeof(Para));memcpy(tmp,s->d.p+start,count*sizeof(Para));memmove(s->d.p+other+count,s->d.p+other,(start-other)*sizeof(Para));memcpy(s->d.p+other,tmp,count*sizeof(Para));s->cur.p=other;}
    else{if(end>=s->d.n)return 0;other=scene_end(&s->d,end);tmp=mem(count*sizeof(Para));memcpy(tmp,s->d.p+start,count*sizeof(Para));memmove(s->d.p+start,s->d.p+end,(other-end)*sizeof(Para));memcpy(s->d.p+other-count,tmp,count*sizeof(Para));s->cur.p=other-count;}
    free(tmp);s->cur.o=0;s->selecting=0;return 1;
}
int export_text(const Doc *d,const char *path,int fountain,char *error){
    FILE *f=file_open(path,"wb");int i,ok=1;Layout l;if(!f){strcpy(error,"Cannot open the export file.");return 0;}
    if(d->title[0]){char *t=utf8_encode(d->title),*a=utf8_encode(d->author);fprintf(f,fountain?"Title: %s\nAuthor: %s\n\n":"%s\n%s\n\n",t,a);free(t);free(a);}
    if(fountain){for(i=0;i<d->n;i++){char *t=utf8_encode(d->p[i].text);int type=d->p[i].type;if(i&&type!=DIALOGUE&&type!=PAREN)fputc('\n',f);if(type==SCENE)fputc('.',f);if(type==CHARACTER)fputc('@',f);if(type==ACTION)fputc('!',f);if(type==TRANSITION)fputc('>',f);if(type==PAREN&&t[0]!='(')fprintf(f,"(%s)\n",t);else if(type==SCENE&&d->numbering){char number[16];scene_label(d,i,number);fprintf(f,"%s #%s#\n",t,number);}else fprintf(f,"%s\n",t);free(t);}}
    else{layout_build(d,&l);for(i=0;i<l.n;i++){Row *r=l.r+i;int k;if(i&&r->page!=l.r[i-1].page)fprintf(f,"\n--- %d ---\n\n",r->page);for(k=0;k<r->indent;k++)fputc(' ',f);if(r->p>=0){char part[61],*u;if(d->numbering&&d->p[r->p].type==SCENE&&r->start==0){char number[16];scene_label(d,r->p,number);fprintf(f,"%s  ",number);}for(k=r->start;k<r->end;k++)part[k-r->start]=(char)element_char(d->p[r->p].type,(unsigned char)d->p[r->p].text[k]);part[r->end-r->start]=0;u=utf8_encode(part);fputs(u,f);free(u);}fputc('\n',f);}layout_free(&l);}
    if(ferror(f)||fflush(f))ok=0;
    if(fclose(f))ok=0;
    strcpy(error,ok?"Export complete.":"Export failed.");return ok;
}
