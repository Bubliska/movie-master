#include "writer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define IMPORT_LIMIT (8L*1024*1024)
#define LINE_LIMIT 65536

/* Fountain syntax: https://fountain.io/syntax/. Import is transactional: the
   caller's document is replaced only after the complete file is validated. */
static char *decode_source(const unsigned char *src,size_t size,char *error,int *warnings){
    char *out=mem(size+1);size_t at=0,n=0;int line=1;
    if(size>=3&&!memcmp(src,"\xef\xbb\xbf",3))at=3;
    while(at<size){
        unsigned int c=src[at++];int more=0,v,k;const char *replacement=NULL;
        if(c>=0xc2&&c<=0xdf){c&=31;more=1;}
        else if(c>=0xe0&&c<=0xef){c&=15;more=2;}
        else if(c>=0xf0&&c<=0xf4){c&=7;more=3;}
        else if(c>=128)goto invalid;
        if(at+more>size)goto invalid;
        for(k=0;k<more;k++){if((src[at]&192)!=128)goto invalid;c=(c<<6)|(src[at++]&63);}
        if((more==1&&c<128)||(more==2&&(c<2048||(c>=0xd800&&c<=0xdfff)))||(more==3&&(c<65536||c>0x10ffff)))goto invalid;
        if(c=='\r'){if(at<size&&src[at]=='\n')at++;out[n++]='\n';line++;continue;}
        if(c=='\n'){out[n++]='\n';line++;continue;}
        if(c=='\t'){out[n++]='\t';continue;}
        if(c==0x2018||c==0x2019||c==0x201a)replacement="'";
        else if(c==0x201c||c==0x201d||c==0x201e)replacement="\"";
        else if(c==0x2013||c==0x2014||c==0x2212)replacement="-";
        else if(c==0x2026)replacement="...";
        else if(c==0x2022)replacement="*";
        else if(c==0x2009||c==0x202f||c==0x00a0)replacement=" ";
        if(replacement){size_t length=strlen(replacement);memcpy(out+n,replacement,length);n+=length;(*warnings)++;continue;}
        v=from_unicode(c);
        if(v<32){snprintf(error,256,"Unsupported character U+%04X at line %d. Import canceled.",c,line);free(out);return NULL;}
        out[n++]=(char)v;
    }
    out[n]=0;return out;
invalid:
    snprintf(error,256,"Invalid UTF-8 at line %d. Save the source as UTF-8 Fountain.",line);free(out);return NULL;
}
static int hide_comments(char *source,char *error,int *warnings){
    char *p=source;
    while(*p){
        char *end;int length;
        if(*p=='\\'&&p[1]){p+=2;continue;}
        if(!strncmp(p,"/*",2)){end=strstr(p+2,"*/");length=2;}
        else if(!strncmp(p,"[[",2)){end=strstr(p+2,"]]");length=2;}
        else{p++;continue;}
        if(!end){strcpy(error,"An unclosed Fountain note/comment was found. Import canceled.");return 0;}
        end+=length;while(p<end){if(*p!='\n')*p=' ';p++;}(*warnings)++;
    }
    return 1;
}
static char *trim(char *text){char *end;while(*text==' '||*text=='\t')text++;end=text+strlen(text);while(end>text&&(end[-1]==' '||end[-1]=='\t'))*--end=0;return text;}
static int empty_line(const char *text){while(*text==' '||*text=='\t')text++;return !*text;}
static int ascii_equal(const char *a,const char *b){while(*a&&*b){if(tolower((unsigned char)*a++)!=tolower((unsigned char)*b++))return 0;}return !*a&&!*b;}
static int starts(const char *text,const char *prefix){while(*prefix){if(toupper((unsigned char)*text++)!=*prefix++)return 0;}return 1;}
static int title_key(const char *text,size_t *length){
    const char *colon=strchr(text,':');char key[40];size_t n;
    if(!colon||(n=(size_t)(colon-text))>=sizeof(key))return 0;
    memcpy(key,text,n);key[n]=0;*length=n+1;
    if(ascii_equal(key,"Title"))return 1;
    if(ascii_equal(key,"Author")||ascii_equal(key,"Authors"))return 2;
    if(ascii_equal(key,"Credit")||ascii_equal(key,"Source")||ascii_equal(key,"Draft date")||ascii_equal(key,"Contact")||ascii_equal(key,"Copyright")||ascii_equal(key,"Notes"))return 3;
    return 0;
}
/* Keep literal/escaped punctuation, removing matched emphasis delimiters only. */
static int plain_text(const char *src,char *out){
    size_t n=strlen(src),i,j,w=0;unsigned char *hide=mem(n+1);
    for(i=0;i<n;i++){
        size_t run=1;
        if(src[i]=='\\'&&src[i+1]&&strchr("*_\\.!@>~#[]",src[i+1])){hide[i]=1;i++;continue;}
        if(hide[i]||(src[i]!='*'&&src[i]!='_'))continue;
        if(src[i]=='*')while(src[i+run]=='*'&&run<3)run++;
        if(i+run>=n||isspace((unsigned char)src[i+run]))continue;
        for(j=i+run;j<n;j++){
            size_t k;if(src[j]=='\\'&&src[j+1]){j++;continue;}
            if(src[j]!=src[i]||j==i+run||isspace((unsigned char)src[j-1]))continue;
            for(k=0;k<run&&src[j+k]==src[i];k++){}
            if(k==run){memset(hide+i,1,run);memset(hide+j,1,run);break;}
        }
        if(j<n)i+=run-1;
    }
    for(i=0;i<n;i++)if(!hide[i])out[w++]=src[i]=='\t'?' ':src[i];
    out[w]=0;free(hide);return strcmp(src,out)!=0;
}
static int metadata_add(char *dest,const char *text,char *error,int *warnings){
    char value[MAX_TEXT+1];size_t a=strlen(dest),b;
    if(plain_text(text,value))(*warnings)++;
    b=strlen(value);
    if(a+b+(a&&b?1:0)>127){strcpy(error,"Title/author exceeds 127 characters. Import canceled.");return 0;}
    if(a&&b)strcat(dest," ");
    strcat(dest,value);return 1;
}
static int uppercase_cue(const char *text){
    int letters=0;const unsigned char *p=(const unsigned char*)text;
    while(*p&&*p!='('){int c=*p++;
        if(upper857(c)!=c)return 0;
        if(lower857(c)!=c)letters=1;
    }return letters;
}
static int scene_heading(const char *text){
    static const char *prefixes[]={"INT./EXT","INT/EXT","EXT./INT","EXT/INT","INT","EXT","EST","I/E",NULL};int i;
    for(i=0;prefixes[i];i++)if(starts(text,prefixes[i])){char c=text[strlen(prefixes[i])];if(c=='.'||c==' '||c=='\t')return 1;}
    /* Common Turkish scene prefixes, in the document's CP857 encoding. */
    return (upper857((unsigned char)text[0])==152&&upper857((unsigned char)text[1])==128&&(text[2]=='.'||text[2]==' '))||
           (upper857((unsigned char)text[0])=='D'&&upper857((unsigned char)text[1])=='I'&&upper857((unsigned char)text[2])==158&&(text[3]=='.'||text[3]==' '));
}
static int append_element(Doc *doc,int type,const char *raw,int join,char *error,int *warnings){
    char text[MAX_TEXT+1];size_t n=strlen(raw);
    if(n>MAX_TEXT){strcpy(error,"A Fountain line exceeds 8190 characters. Import canceled.");return 0;}
    if(plain_text(raw,text))(*warnings)++;
    n=strlen(text);
    if(join&&doc->n&&doc->p[doc->n-1].type==type){
        Para *p=doc->p+doc->n-1;size_t a=strlen(p->text);
        if(a+n+1<=MAX_TEXT){char *combined=mem(a+n+2);strcpy(combined,p->text);if(a&&n)strcat(combined," ");strcat(combined,text);free(p->text);p->text=combined;return 1;}
    }
    if(!doc_insert(doc,doc->n,type,text)){strcpy(error,"The document exceeds the paragraph limit. Import canceled.");return 0;}return 1;
}
int import_fountain(Doc *doc,const char *path,char *error,int *warnings){
    FILE *f;long size;unsigned char *bytes;char *source=NULL,**lines=NULL,*p;Doc tmp={0};int n=0,i=0,blank=1,dialogue=0,paren=0,ok=0,notes=0;
    *warnings=0;
    if(path_type(path)!=PATH_FILE||(f=file_open(path,"rb"))==NULL){strcpy(error,"Cannot open this Fountain file.");return 0;}
    if(fseek(f,0,SEEK_END)||(size=ftell(f))<0||size>IMPORT_LIMIT||fseek(f,0,SEEK_SET)){fclose(f);strcpy(error,"Cannot read the file, or it exceeds the 8 MB import limit.");return 0;}
    bytes=mem((size_t)size+1);if(fread(bytes,1,size,f)!=(size_t)size||ferror(f)){free(bytes);fclose(f);strcpy(error,"The Fountain file could not be read completely.");return 0;}fclose(f);
    source=decode_source(bytes,(size_t)size,error,&notes);free(bytes);if(!source)return 0;
    if(!hide_comments(source,error,&notes))goto done;
    n=1;for(p=source;*p;p++)if(*p=='\n')n++;
    if(n>LINE_LIMIT){strcpy(error,"The file has more than 65536 lines. Import canceled.");goto done;}
    lines=mem(n*sizeof(char*));lines[0]=source;n=1;
    for(p=source;*p;p++)if(*p=='\n'){*p=0;lines[n++]=p+1;}
    for(i=0;i<n;i++)if(strlen(lines[i])>MAX_TEXT){strcpy(error,"A Fountain line exceeds 8190 characters. Import canceled.");goto done;}
    i=0;while(i<n&&!*trim(lines[i]))i++;
    if(i<n){size_t keylen=0;int field=title_key(trim(lines[i]),&keylen);
        if(field){for(;i<n;i++){
            char *raw=lines[i],*line=trim(raw);int next;
            if(!*line){i++;break;}
            next=title_key(line,&keylen);
            if(next){field=next;line=trim(line+keylen);if(field==3)notes++;}
            else if(raw[0]!=' '&&raw[0]!='\t')break;
            if(field==1&&!metadata_add(tmp.title,line,error,&notes))goto done;
            if(field==2&&!metadata_add(tmp.author,line,error,&notes))goto done;
        }}
    }
    for(;i<n;i++){
        size_t rawlen=strlen(lines[i]);int trailing=rawlen&&(lines[i][rawlen-1]==' '||lines[i][rawlen-1]=='\t');
        char *line=trim(lines[i]);size_t len=strlen(line);int type=ACTION,forced=0,number=0;
        if(!len){blank=1;dialogue=paren=0;continue;}
        if(line[0]=='#'||line[0]=='='){notes++;blank=1;dialogue=paren=0;continue;}
        if(line[0]=='\\'&&line[1]&&strchr(".!@>~#",line[1])){type=ACTION;forced=1;}
        else if(line[0]=='!'||line[0]=='@'||(line[0]=='.'&&(isalnum((unsigned char)line[1])||(unsigned char)line[1]>=128))){
            type=line[0]=='!'?ACTION:line[0]=='@'?CHARACTER:SCENE;line=trim(line+1);forced=1;
        }
        else if(line[0]=='>'&&len>1){
            if(line[len-1]=='<'){line[len-1]=0;type=ACTION;notes++;}else type=TRANSITION;
            line=trim(line+1);forced=1;
        }
        else if(line[0]=='~'){line=trim(line+1);type=ACTION;forced=1;notes++;}
        else if(scene_heading(line))type=SCENE;
        else if(blank&&!trailing&&len>=3&&!strcmp(line+len-3,"TO:")&&uppercase_cue(line)&&(i+1==n||empty_line(lines[i+1])))type=TRANSITION;
        else if(blank&&uppercase_cue(line)&&i+1<n&&!empty_line(lines[i+1]))type=CHARACTER;
        else if(dialogue)type=(paren||line[0]=='(')?PAREN:DIALOGUE;
        if(type==SCENE){char *end=line+strlen(line);if(end>line&&end[-1]=='#'){char *hash=end-1;while(hash>line&&hash[-1]!='#')hash--;if(hash>line&&hash[-1]=='#'){char label[16];int base,suffix;size_t length=(size_t)((end-1)-hash);
            if(length>=sizeof(label)){strcpy(error,"Scene number is too long. Import canceled.");goto done;}
            memcpy(label,hash,length);label[length]=0;
            if(!scene_parse(label,&base,&suffix)||(number=scene_register(&tmp,base,suffix))==0){strcpy(error,"Invalid or duplicate scene number. Import canceled.");goto done;}
            hash[-1]=0;line=trim(line);}}}
        if(type==CHARACTER){size_t l=strlen(line);if(l&&line[l-1]=='^'){line[l-1]=0;line=trim(line);notes++;}dialogue=1;paren=0;}
        else if(type==PAREN){size_t l=strlen(line);paren=!(l&&line[l-1]==')');}
        else if(type!=DIALOGUE){dialogue=paren=0;}
        if(!append_element(&tmp,type,line,!blank&&!forced&&(type==ACTION||type==DIALOGUE||type==PAREN),error,&notes))goto done;
        if(number)tmp.p[tmp.n-1].scene_id=number;
        blank=0;
    }
    if(!tmp.n)doc_insert(&tmp,0,ACTION,"");
    if(tmp.nn){tmp.locked=tmp.numbering=1;for(i=0;i<tmp.n;i++)if(!scene_assign(&tmp,i)){strcpy(error,"Scene number limit exceeded. Import canceled.");goto done;}}
    doc_free(doc);*doc=tmp;memset(&tmp,0,sizeof(tmp));*warnings=notes;
    snprintf(error,256,"Imported %d paragraphs%s",doc->n,notes?"; some formatting was simplified.":".");ok=1;
done:
    free(lines);free(source);doc_free(&tmp);return ok;
}
