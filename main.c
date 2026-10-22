#include "moviemst.h"
#include "screen.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

static State s;
static History history;
static Doc clipboard;
static Layout layout;
static char filename[512]="",status[256]="Alt+H: Help",search[128]="";
static int top=0,typing=0,running=1,overwrite=0,marking=0,active_menu=-1;
static const char *menu_names[]={"File","Edit","Element","Find","Format","Scenes","Export","Settings","Help","Exit"};
static const int element_order[]={SCENE,ACTION,CHARACTER,PAREN,DIALOGUE,TRANSITION};
static int menu_x(int at){int i,x=1;for(i=0;i<at;i++)x+=(int)strlen(menu_names[i])+2;return x;}
static unsigned long saved_hash;
static time_t last_edit,last_recovery;
static int dirty(void){return doc_hash(&s.d)!=saved_hash;}
static void text(int x,int y,const char *cp,int attr){while(*cp&&x<80)cell(x++,y,(unsigned char)*cp++,attr);}
static void ut(int x,int y,const char *u,int attr){char b[512];if(utf8_decode(u,b,sizeof(b)))text(x,y,b,attr);else text(x,y,u,attr);}
static void fill(int x,int y,int w,int h,int attr){int i,j;for(j=0;j<h;j++)for(i=0;i<w;i++)cell(x+i,y+j,' ',attr);}
static void frame(int x,int y,int w,int h,const char *title,int attr){
    int i;fill(x,y,w,h,attr);
    for(i=1;i<w-1;i++){cell(x+i,y,196,attr);cell(x+i,y+h-1,196,attr);}
    for(i=1;i<h-1;i++){cell(x,y+i,179,attr);cell(x+w-1,y+i,179,attr);}
    cell(x,y,218,attr);cell(x+w-1,y,191,attr);cell(x,y+h-1,192,attr);cell(x+w-1,y+h-1,217,attr);
    if(title&&*title)ut(x+2,y,title,attr);
}
static void box(int x,int y,int w,int h,const char *title){frame(x,y,w,h,title,0x70);}
static int current_row(void){int i,found=0;for(i=0;i<layout.n;i++)if(layout.r[i].p==s.cur.p&&s.cur.o>=layout.r[i].start){found=i;if(s.cur.o<layout.r[i].end)break;}return found;}
static const char *suggestion(void){int i;Para *p=s.d.p+s.cur.p;size_t len=strlen(p->text),j;if((p->type!=SCENE&&p->type!=CHARACTER)||!len||s.cur.o!=(int)len||s.selecting)return NULL;for(i=s.cur.p-1;i>=0;i--)if(s.d.p[i].type==p->type&&strlen(s.d.p[i].text)>len){for(j=0;j<len;j++)if(element_char(p->type,(unsigned char)s.d.p[i].text[j])!=element_char(p->type,(unsigned char)p->text[j]))break;if(j==len)return s.d.p[i].text+len;}return NULL;}
static void rebuild(void){layout_free(&layout);layout_build(&s.d,&layout);}
static void draw(void){
    int row=current_row(),i,j,cx,cy;char b[256],displayfile[512];const char *base;
    Pos a,bpos;int sel=selection(&s,&a,&bpos);const char *suggest=suggestion();
    static const char *keys[]={"SLUG","ACTION","CHAR","PAREN","DIALOG","TRANS","INS/OT","SELECT","MENU","SAVE"};
    if(row<top)top=row;
    if(row>=top+21)top=row-20;
    if(top<0)top=0;
    screen_clear(0x07);
    for(i=0;i<10;i++){
        int x=menu_x(i),attr=i==active_menu?0x70:0x07;
        fill(x-1,0,(int)strlen(menu_names[i])+2,1,attr);text(x,0,menu_names[i],attr);
    }
    for(i=0;i<80;i++)cell(i,1,205,0x07);
    for(i=0;i<21;i++){
        int ri=top+i;Row *r;if(ri>=layout.n)break;r=layout.r+ri;
        if(r->p<0)continue;
        if(s.d.numbering&&s.d.p[r->p].type==SCENE&&r->start==0){char number[16];scene_label(&s.d,r->p,number);snprintf(b,sizeof(b),"%8.8s",number);text(0,i+2,b,0x07);}
        for(j=r->start;j<r->end;j++){Pos pos={r->p,j};int attr=sel&&pos_cmp(pos,a)>=0&&pos_cmp(pos,bpos)<0?0x70:0x07;cell(10+r->indent+j-r->start,i+2,element_char(s.d.p[r->p].type,(unsigned char)s.d.p[r->p].text[j]),attr);}
        if(sel&&r->p>=a.p&&r->p<bpos.p&&r->end==(int)strlen(s.d.p[r->p].text))cell(10+r->indent+r->end-r->start,i+2,' ',0x70);
    }
    for(i=0;i<10;i++){
        int x=i*8,n=i==9?2:1,attr=(i<6&&element_order[i]==s.d.p[s.cur.p].type)?0x07:0x70;
        snprintf(b,sizeof(b),"%d",i+1);text(x,23,b,0x07);fill(x+n,23,8-n,1,attr);
        text(x+n,23,i==8&&active_menu>=0?"OK":keys[i],attr);
    }
    path_display(filename[0]?filename:"NEW.SEN",displayfile,sizeof(displayfile));base=displayfile+strlen(displayfile);
    while(base>displayfile&&base[-1]!='/'&&base[-1]!='\\')base--;
    snprintf(b,sizeof(b),"File:%.12s%s",base,dirty()?"*":"");text(0,24,b,0x07);
    snprintf(b,sizeof(b),"Page:%d/%d",layout.r[row].page,layout.pages);text(20,24,b,0x07);
    snprintf(b,sizeof(b),"Sc:%d%s",scene_count(&s.d),s.d.locked?"L":"");text(33,24,b,0x07);
    snprintf(b,sizeof(b),"Col:%d",layout.r[row].indent+s.cur.o-layout.r[row].start+1);text(43,24,b,0x07);
    text(53,24,overwrite?"OT":"INS",0x07);text(57,24,keyboard_tags[keyboard_layout],0x07);
    if(marking)text(62,24,"Selecting",0x07);
    else if(suggest)text(62,24,"Tab: complete",0x07);
    else{snprintf(b,sizeof(b),"%.17s",status);text(62,24,b,0x07);}
    cx=10+layout.r[row].indent+s.cur.o-layout.r[row].start;cy=2+row-top;if(cx>79)cx=79;
    screen_flush(cx,cy);
}
/* Negative results let the top menu switch columns without accepting an item. */
static int popup(const char *title,const char **items,int n,int x,int choice){
    int i,w=24,y=x<0?4:1,h=n+2;Key k;
    for(i=0;i<n;i++)if((int)strlen(items[i])+4>w)w=(int)strlen(items[i])+4;
    if((int)strlen(title)+4>w)w=(int)strlen(title)+4;
    if(w>76)w=76;
    if(x<0)x=(80-w)/2;
    if(x+w>80)x=80-w;
    for(;;){
        draw();frame(x,y,w,h,title,0x07);
        for(i=0;i<n;i++){int attr=i==choice?0x70:0x07;fill(x+1,y+1+i,w-2,1,attr);ut(x+2,y+1+i,items[i],attr);}
        screen_flush(-1,-1);k=key_read(keyboard_layout,1);
        if(k.code==K_ESC)return -1;
        if(k.code==K_LEFT)return -2;
        if(k.code==K_RIGHT)return -3;
        if(k.code==K_UP)choice=(choice+n-1)%n;
        if(k.code==K_DOWN)choice=(choice+1)%n;
        if(k.code==K_ENTER||k.code==K_F9)return choice;
        if(k.code==K_CHAR&&k.ch>='1'&&k.ch<='9'&&k.ch<'1'+n)return k.ch-'1';
    }
}
static int menu(const char *title,const char **items,int n){int result;do{result=popup(title,items,n,-1,0);}while(result<-1);return result;}
static void notice(const char *msg){
    Key k;char value[256];size_t at=0,len;int y;
    if(!utf8_decode(msg,value,sizeof(value))){strncpy(value,msg,sizeof(value)-1);value[sizeof(value)-1]=0;}
    len=strlen(value);draw();box(5,7,70,10,"Information");
    for(y=9;y<=13&&at<len;y++){
        char line[65];size_t end=at+64,next;if(end>len)end=len;next=end;
        if(end<len){size_t space=end;while(space>at&&value[space]!=' ')space--;if(space>at){end=space;next=space+1;}}
        memcpy(line,value+at,end-at);line[end-at]=0;text(8,y,line,0x70);at=next;
    }
    ut(8,15,"Enter / Esc: continue",0x70);screen_flush(-1,-1);do{k=key_read(keyboard_layout,1);}while(k.code!=K_ENTER&&k.code!=K_ESC&&k.code!=K_F9);
}
static int prompt(const char *title,char *out,size_t cap,int path){
    char value[512];int pos,len,start;Key k;if(cap>sizeof(value))cap=sizeof(value);strncpy(value,out,cap-1);value[cap-1]=0;len=(int)strlen(value);pos=len;
    for(;;){draw();box(5,7,70,8,title);ut(8,9,path?"File name (DOS: 8.3, e.g. SCRIPT.SEN)":"Enter text:",0x70);fill(8,11,64,1,0x07);start=pos>60?pos-60:0;text(8,11,value+start,0x07);ut(8,13,"Enter: confirm   Esc: cancel   Ctrl+A: clear",0x70);screen_flush(8+pos-start,11);k=key_read(keyboard_layout,1);
        if(k.code==K_ESC)return 0;
        if(k.code==K_ENTER||k.code==K_F9){strcpy(out,value);return 1;}
        if(k.ctrl&&lower857(k.ch)=='a'){value[0]=0;len=pos=0;}
        else if(k.code==K_LEFT&&pos)pos--;else if(k.code==K_RIGHT&&pos<len)pos++;else if(k.code==K_HOME)pos=0;else if(k.code==K_END)pos=len;
        else if(k.code==K_BACK&&pos){memmove(value+pos-1,value+pos,len-pos+1);pos--;len--;}
        else if(k.code==K_DEL&&pos<len){memmove(value+pos,value+pos+1,len-pos);len--;}
        else if(k.code==K_CHAR&&!k.ctrl&&!k.alt&&k.ch>=32&&k.ch<=255&&len+1<(int)cap){int ch=k.ch;if(path){if(ch>126||strchr("*?<>|\"",ch))continue;ch=toupper(ch);}memmove(value+pos+1,value+pos,len-pos+1);value[pos++]=(char)ch;len++;}
    }
}
enum {BROWSE_OPEN,BROWSE_IMPORT,BROWSE_SAVE,BROWSE_PDF,BROWSE_TEXT,BROWSE_FOUNTAIN};
static int browse_destination(char *out,const char *path,const char *extension,char *error){
    char folder[512],name[512];path_split(path,folder,name);return destination_path(out,folder[0]?folder:".",name,extension,error);
}
static int browse_file(char *selected,int mode){
    static const char *titles[]={"Open Document","Import Fountain","Save As","Export PDF","Export Plain Text","Export Fountain"};
    static const char *extensions[]={".SEN",".PDF",".TXT",".FTN"};
    char folder[512],desktop[512],project[512],name[512]="",error[256],display[512];FileEntry *entries=NULL;
    int saving=mode>=BROWSE_SAVE,count=0,choice=0,start=0,default_filter=mode<2?mode:mode+1,filter=default_filter,drives=0,reload=1;
    const char *extension=saving?extensions[mode-BROWSE_SAVE]:"";
    desktop_path(desktop);path_split(filename,project,display);if(!project[0]||path_type(project)!=PATH_DIR)strcpy(project,desktop);strcpy(folder,desktop);
    if(saving){path_split(selected,folder,name);if(!folder[0]||path_type(folder)!=PATH_DIR)strcpy(folder,project);}
    for(;;){
        Key key;int i,total;
        if(reload){
            files_free(entries,count);entries=NULL;count=0;
            if(drives){char roots[26][4];int n=drive_roots(roots);entries=mem(n*sizeof(FileEntry));for(i=0;i<n;i++){entries[i].name=dupstr(roots[i]);entries[i].directory=1;}count=n;}
            else if(!files_list(folder,&entries,&count,filter,error)){notice(error);files_free(entries,count);return 0;}
            choice=start=0;reload=0;
        }
        total=count+1;
        if(choice<start)start=choice;
        if(choice>=start+13)start=choice-12;
        draw();box(1,2,78,22,titles[mode]);
        path_display(drives?"Computer / Drives":folder,display,sizeof(display));
        text(3,4,!drives&&!strcmp(folder,desktop)?"Desktop:":"Folder: ",0x70);text(11,4,display+(strlen(display)>65?strlen(display)-65:0),0x70);
        for(i=0;i<13&&start+i<total;i++){
            int at=start+i,attr=at==choice?0x07:0x70;char name[512],label[73];
            if(!at)strcpy(label,drives?"[Desktop]":"[..] Parent folder");
            else{FileEntry *e=entries+at-1;path_display(e->name,name,sizeof(name));
                if(strlen(name)>64){name[61]='.';name[62]='.';name[63]='.';name[64]=0;}
                snprintf(label,sizeof(label),e->directory?"[%.64s]":"  %.64s",name);
            }
            fill(3,6+i,74,1,attr);text(4,6+i,label,attr);
        }
        if(saving){snprintf(display,sizeof(display),"Filter: %s%s",extension,filter==2?" (showing all files)":"");text(3,19,display,0x70);}
        else text(3,19,filter==2?"Filter: All files":mode==BROWSE_IMPORT?"Filter: Fountain / FTN / FOU / TXT":"Filter: SEN / BAK",0x70);
        text(3,20,"Enter Open  Backspace Parent  F1 Desktop  F2 Drives",0x70);
        text(3,21,saving?"F3 Path  F4 Filter  F5 Save Here  F6 Project  Esc Cancel":"F3 Path  F4 Filter  F6 Project  Esc Cancel",0x70);
        if(saving){path_display(name,display,sizeof(display));text(3,22,"Name: ",0x70);text(9,22,display,0x70);}
        screen_flush(3,5);key=key_read(keyboard_layout,1);
        if(key.code==K_ESC){files_free(entries,count);return 0;}
        if(key.code==K_UP&&choice)choice--;
        if(key.code==K_DOWN&&choice+1<total)choice++;
        if(key.code==K_PGUP){choice-=12;if(choice<0)choice=0;}
        if(key.code==K_PGDN){choice+=12;if(choice>=total)choice=total-1;}
        if(key.code==K_HOME)choice=0;
        if(key.code==K_END)choice=total-1;
        if(key.code==K_F1){desktop_path(folder);drives=0;reload=1;}
        if(key.code==K_F2){drives=!drives;reload=1;}
        if(key.code==K_F4){filter=filter==2?default_filter:2;reload=1;}
        if(key.code==K_F6){strcpy(folder,project);drives=0;reload=1;}
        if(key.code==K_F5&&saving&&drives){notice("Open a drive or folder before choosing Save Here.");continue;}
        if(key.code==K_F5&&saving&&!drives){
            char typed[512],before[512],native[512],path[512];path_display(name,typed,sizeof(typed));strcpy(before,typed);
            if(prompt("File name - Enter saves in the selected folder",typed,sizeof(typed),0)){
#ifdef __DJGPP__
                strcpy(native,typed);
#else
                if(!strcmp(before,typed))strcpy(native,name);else{char *utf=utf8_encode(typed);if(strlen(utf)>=sizeof(native)){free(utf);notice("File name is too long.");continue;}strcpy(native,utf);free(utf);}
#endif
                if(destination_path(path,folder,native,extension,error)){strcpy(selected,path);files_free(entries,count);return 1;}
                notice(error);
            }
        }
        if(key.code==K_BACK){if(drives){desktop_path(folder);drives=0;}else path_parent(folder);reload=1;}
        if(key.code==K_F3){
            char typed[512],path[512];path_display(folder,typed,sizeof(typed));
            if(prompt("Go to a folder or file path",typed,sizeof(typed),0)){
#ifdef __DJGPP__
                strcpy(path,typed);
#else
                char *utf=utf8_encode(typed);if(strlen(utf)>=sizeof(path)){free(utf);notice("Path is too long.");continue;}strcpy(path,utf);free(utf);
#endif
                if(path[0]!='/'&&path[0]!='\\'&&!(path[0]&&path[1]==':')){
                    char absolute[512];if(!path_join(absolute,folder,path)){notice("Path is too long.");continue;}strcpy(path,absolute);
                }
                if(path_type(path)==PATH_DIR){strcpy(folder,path);drives=0;reload=1;}
                else if(saving){char destination[512];if(browse_destination(destination,path,extension,error)){strcpy(selected,destination);files_free(entries,count);return 1;}notice(error);}
                else if(path_type(path)==PATH_FILE){strcpy(selected,path);files_free(entries,count);return 1;}
                else notice("That path does not exist or cannot be read.");
            }
        }
        if(key.code==K_ENTER){
            if(!choice){if(drives){desktop_path(folder);drives=0;}else path_parent(folder);reload=1;}
            else if(drives){strcpy(folder,entries[choice-1].name);drives=0;reload=1;}
            else{
                char path[512];FileEntry *entry=entries+choice-1;
                if(!path_join(path,folder,entry->name)){notice("Path is too long.");continue;}
                if(entry->directory){strcpy(folder,path);reload=1;}
                else{if(saving){char destination[512];if(!browse_destination(destination,path,extension,error)){notice(error);continue;}strcpy(path,destination);}strcpy(selected,path);files_free(entries,count);return 1;}
            }
        }
    }
}
static int confirm_replace(const char *path){FILE *f=file_open(path,"rb");const char *items[]={"Overwrite","Cancel"};if(!f)return 1;fclose(f);return menu("File exists. Overwrite?",items,2)==0;}
static int save(int as){char path[512],error[256];if(!as&&filename[0])strcpy(path,filename);else{strcpy(path,filename[0]?filename:"NEW.SEN");if(!browse_file(path,BROWSE_SAVE))return 0;if(strcmp(path,filename)&&!confirm_replace(path))return 0;}
    if(!save_doc(&s.d,path,error)){notice(error);return 0;}strcpy(filename,path);saved_hash=doc_hash(&s.d);strcpy(status,error);file_remove("KURTAR.SEN");file_remove("KURTAR.BAK");return 1;
}
static int unsaved(void){const char *items[]={"Save and continue","Continue without saving","Cancel"};int a;if(!dirty())return 1;a=menu("Unsaved changes",items,3);return a==0?save(0):a==1;}
static void reset_history(void){marking=0;history_clear(&history);s.cur.p=s.cur.o=0;s.selecting=0;top=0;typing=0;saved_hash=doc_hash(&s.d);rebuild();}
static void new_file(void){if(!unsaved())return;state_free(&s);doc_init(&s.d);filename[0]=0;reset_history();file_remove("KURTAR.SEN");strcpy(status,"New. F1-F6: types");}
static void open_file(void){
    char path[512],error[256];Doc opened={0};const char *base;
    if(!browse_file(path,BROWSE_OPEN))return;
    if(!load_doc(&opened,path,error)){notice(error);return;}
    if(!unsaved()){doc_free(&opened);return;}
    state_free(&s);s.d=opened;base=path+strlen(path);while(base>path&&base[-1]!='/'&&base[-1]!='\\')base--;
    strcpy(filename,!strcmp(base,"KURTAR.SEN")?"":path);reset_history();
    if(!filename[0])saved_hash=doc_hash(&s.d)^1;
    strcpy(status,"Opened. F10: Save");
}
static int mac_import_file(char *path,const char *bridge){
    static unsigned long serial;unsigned long identifier=((unsigned long)time(NULL)<<8)^++serial;
    char request[512],response[512];FILE *f;time_t started=time(NULL);int cancel=0;
    if(!path_join(request,bridge,"REQUEST.TXT")||!path_join(response,bridge,"RESPONSE.TXT")||!path_join(path,bridge,"IMPORT.FTN")){notice("Mac import path is too long.");return 0;}
    f=file_open(request,"w");if(!f){notice("Cannot contact the Mac file chooser. Restart Movie Master.");return 0;}
    {int ok=fprintf(f,"I %lu\n",identifier)>=0;if(fclose(f))ok=0;if(!ok){notice("Cannot request the Mac file chooser.");return 0;}}
    draw();box(4,8,72,7,"Import Fountain");ut(7,10,"Choose your file in the macOS window.",0x70);ut(7,12,"Esc: cancel",0x70);screen_flush(-1,-1);
    while(time(NULL)-started<600){
        char outcome=0;unsigned long reply=0;Key k=key_read(keyboard_layout,0);
        if(k.code==K_ESC&&!cancel){f=file_open(request,"w");if(f){fprintf(f,"C %lu\n",identifier);fclose(f);}cancel=1;}
        f=file_open(response,"r");if(f){int n=fscanf(f,"%c %lu",&outcome,&reply);fclose(f);if(n==2&&reply==identifier){if(cancel||outcome=='C'){file_remove(path);return 0;}if(outcome=='O')return 1;notice("The selected file could not be read. Your draft is unchanged.");return 0;}}
        idle();
    }
    f=file_open(request,"w");if(f){fprintf(f,"C %lu\n",identifier);fclose(f);}notice("Mac file chooser timed out. Your draft is unchanged.");return 0;
}
static void import_file(void){
    char path[512],error[256];Doc imported={0};int warnings;
    const char *bridge=getenv("MMMAC");
    if(bridge){if(!mac_import_file(path,bridge))return;}else if(!browse_file(path,BROWSE_IMPORT))return;
    {int ok=import_fountain(&imported,path,error,&warnings);if(bridge)file_remove(path);if(!ok){notice(error);return;}}
    if(!unsaved()){doc_free(&imported);return;}
    state_free(&s);s.d=imported;filename[0]=0;reset_history();saved_hash=doc_hash(&s.d)^1;
    snprintf(status,sizeof(status),"Imported %d paragraphs. F10: Save as SEN%s",s.d.n,warnings?" (simplified formatting)":"");
    if(warnings)notice("Imported. Some formatting was simplified; source unchanged.");
}
static void change(void){checkpoint(&history,&s);typing=0;last_edit=time(NULL);}
static int copy(void){int ok=copy_selection(&s,&clipboard);if(ok)system_copy(&clipboard);return ok;}
static void paste_clipboard(void){if(system_paste(&clipboard)<0){notice("Clipboard text is too long or contains unsupported characters.");return;}if(!clipboard.n)return;change();if(!paste(&s,&clipboard))strcpy(status,"Cannot paste: check the text limits.");}
static void use_type(int type,int add){
    change();marking=0;
    if(add&&s.d.p[s.cur.p].text[0]&&!split_para(&s)){strcpy(status,"Cannot add element.");return;}
    if(!set_type(&s,type))strcpy(status,"Scene number limit reached. Unlock to renumber.");else snprintf(status,sizeof(status),"%s",type_names[type]);rebuild();
}
static void edit_type(int add){
    const char *items[]={"Slugline","Action","Character Name","Parenthetical","Dialog","Transition"};int i,choice=0,a;
    for(i=0;i<6;i++)if(element_order[i]==s.d.p[s.cur.p].type)choice=i;
    a=popup(add?"Add Element":"Change Type",items,6,menu_x(2),choice);
    if(a>=0)use_type(element_order[a],add);
}
static void find_next(int again){
    int i,offset=s.cur.o,pass;if(!again&&(!prompt("Find (case-sensitive)",search,sizeof(search),0)||!search[0]))return;
    if(!search[0])return;
    for(pass=0;pass<2;pass++)for(i=pass?0:s.cur.p;i<s.d.n&&(pass==0||i<=s.cur.p);i++){const char *p=element_find(s.d.p[i].type,s.d.p[i].text+(pass==0&&i==s.cur.p?offset:0),search);if(p){s.anchor.p=i;s.anchor.o=(int)(p-s.d.p[i].text);s.cur=s.anchor;s.cur.o+=(int)strlen(search);s.selecting=1;strcpy(status,"Found. Ctrl+G: Next");return;}}
    strcpy(status,"Text not found.");
}
static void replace_all(void){
    char with[128]="";int i,count=0;State tmp;
    if(!prompt("Replace All: find text",search,sizeof(search),0)||!search[0]||!prompt("Replace with",with,sizeof(with),0))return;
    state_clone(&tmp,&s);for(i=0;i<tmp.d.n;i++){char *src=tmp.d.p[i].text;const char *p=src,*q;size_t oldlen=strlen(search),newlen=strlen(with),size=strlen(src),matches=0;while((q=element_find(tmp.d.p[i].type,p,search))){matches++;p=q+oldlen;}if(!matches)continue;
        if(newlen>oldlen&&size+matches*(newlen-oldlen)>MAX_TEXT){state_free(&tmp);notice("Result exceeds the paragraph limit. Document unchanged.");return;}
        {char *dst=mem(size+matches*newlen+1),*w=dst;p=src;while((q=element_find(tmp.d.p[i].type,p,search))){memcpy(w,p,q-p);w+=q-p;memcpy(w,with,newlen);w+=newlen;p=q+oldlen;}strcpy(w,p);free(tmp.d.p[i].text);tmp.d.p[i].text=dst;}count+=(int)matches;
    }
    if(count){change();state_free(&s);s=tmp;s.cur.o=0;s.selecting=0;rebuild();}else state_free(&tmp);snprintf(status,sizeof(status),"Replaced %d matches.",count);
}
static void scenes(void){
    int *indices=mem(s.d.n*sizeof(int)),n=0,i,choice=0,start=0;Key k;
    for(i=0;i<s.d.n;i++)if(s.d.p[i].type==SCENE){indices[n]=i;if(i<=s.cur.p)choice=n;n++;}
    if(!n){free(indices);notice("No scenes yet. Use F1 to add a Slugline.");return;}
    for(;;){if(choice<start)start=choice;if(choice>=start+12)start=choice-11;draw();{char title[80];snprintf(title,sizeof(title),"Scene List - %d scenes%s",n,s.d.locked?" (numbers locked)":"");box(3,3,74,19,title);}for(i=0;i<12&&start+i<n;i++){char b[24],number[16];int c=start+i,attr=c==choice?0x07:0x70;fill(5,5+i,70,1,attr);scene_label(&s.d,indices[c],number);snprintf(b,sizeof(b),"%10s ",number);text(5,5+i,b,attr);{char name[60];int j;strncpy(name,s.d.p[indices[c]].text,58);name[58]=0;for(j=0;name[j];j++)name[j]=(char)element_char(SCENE,(unsigned char)name[j]);text(17,5+i,name,attr);}}
        ut(6,18,"Arrows Select  Enter Go  F2 Move Up  F3 Move Down  F4 Duplicate",0x70);ut(6,20,"Esc: return to writing",0x70);screen_flush(5,4);k=key_read(keyboard_layout,1);if(k.code==K_ESC)break;if(k.code==K_UP&&choice)choice--;if(k.code==K_DOWN&&choice+1<n)choice++;if(k.code==K_ENTER){s.cur.p=indices[choice];s.cur.o=0;s.selecting=0;break;}
        if(k.code==K_F2||k.code==K_F3||k.code==K_F4){change();s.cur.p=indices[choice];s.cur.o=0;if(k.code==K_F4){if(!scene_duplicate(&s))strcpy(status,"Could not duplicate this scene.");}else scene_move(&s,k.code==K_F2?-1:1);rebuild();free(indices);return;}
    }free(indices);
}
static void cover(void){char title[128],author[128];strcpy(title,s.d.title);strcpy(author,s.d.author);if(!prompt("Title Page: title (leave blank for no title page)",title,sizeof(title),0)||!prompt("Title Page: author",author,sizeof(author),0))return;change();strcpy(s.d.title,title);strcpy(s.d.author,author);}
static void output(int kind){
    char path[512],folder[512],base[512],err[256];const char *font=getenv("MMFONT");
    path_split(filename,folder,base);if(!path_join(path,folder,kind==0?"OUTPUT.PDF":kind==1?"OUTPUT.TXT":"OUTPUT.FTN")){notice("Folder path is too long.");return;}
    if(!browse_file(path,BROWSE_PDF+kind))return;
    if(!confirm_replace(path))return;
    if(kind==0)export_pdf(&s.d,path,font&&*font?font:"FONT.TTF",err);else export_text(&s.d,path,kind==2,err);notice(err);strcpy(status,"F9: Menu | F10: Save");
}
static void help(void){
    draw();box(2,3,76,19,"Movie Master 0.4.4 - Quick Start");
    ut(5,5,"F1 Slug  F2 Action  F3 Character  F4 Parenthetical",0x70);
    ut(5,6,"F5 Dialog  F6 Transition  F7 Insert/Overwrite  F8 Select",0x70);
    ut(5,8,"F9 / Esc: menus. F10 / Ctrl+S: save. Alt+X: exit.",0x70);
    ut(5,9,"Alt+H: help. Ctrl+O: open. Ctrl+I: import Fountain.",0x70);
    ut(5,11,"F1-F6 add an element at the cursor; empty elements change type.",0x70);
    ut(5,12,"Ctrl+Enter: Add Element. Element > Change Current Type: convert.",0x70);
    ut(5,14,"Tab: next type / complete. Shift+Tab: previous type.",0x70);
    ut(5,15,"Ctrl+Z/Y Undo/Redo. Shift+Arrows Select. Ctrl+C/X/V Copy/Cut/Paste.",0x70);
    ut(5,17,"Ctrl+F Find. Ctrl+G Find Next. Ctrl+H Replace All.",0x70);
    ut(5,18,"Scenes: numbers / lock / list. Sc: scene count; L: numbers locked.",0x70);
    ut(5,20,"Enter / Esc / F9: return to writing",0x70);screen_flush(-1,-1);{Key k;do{k=key_read(keyboard_layout,1);}while(k.code!=K_ENTER&&k.code!=K_ESC&&k.code!=K_F9);}
}
static int menu_shortcut(int ch){static const char keys[]="felimcpthx";const char *p=ch?strchr(keys,lower857(ch)):NULL;return p?(int)(p-keys):-1;}
static void main_menu(int initial){
    static int selected=0;int open=initial>=0,a,b=-1;Key k;
    const char *file[]={"New","Open Document...","Import Fountain...","Save","Save As...","Exit"};
    const char *edit[]={"Undo","Redo","Cut","Copy","Paste","Select All"};
    const char *element[]={"Slugline","Action","Character Name","Parenthetical","Dialog","Transition","Change Current Type..."};
    const char *find[]={"Find...","Find Next","Replace All..."};
    const char *format[]={"Element Type...","Title Page..."};
    const char *scene[]={"Scene List...",s.d.numbering?"Hide Scene Numbers":"Show Scene Numbers",s.d.locked?"Unlock and Renumber...":"Lock Scene Numbers"};
    const char *export[]={"PDF...","Plain Text...","Fountain..."};
    const char *help_items[]={"Quick Start..."};
    const char *exit_items[]={"Exit Movie Master"};
    const char **groups[]={file,edit,element,find,format,scene,export,keyboard_names,help_items,exit_items};
    const int counts[]={6,6,7,3,2,3,3,KEY_COUNT,1,1};
    if(initial>=0)selected=initial;
    for(;;){
        active_menu=selected;
        if(open){b=popup("",groups[selected],counts[selected],menu_x(selected),0);
            if(b>=0)break;
            if(b==-1)open=0;else selected=(selected+(b==-2?9:1))%10;
        }else{
            draw();k=key_read(keyboard_layout,1);
            if(k.code==K_ESC){active_menu=-1;return;}
            if(k.code==K_LEFT)selected=(selected+9)%10;
            if(k.code==K_RIGHT)selected=(selected+1)%10;
            if(k.code==K_DOWN||k.code==K_ENTER||k.code==K_F9)open=1;
            if(k.code==K_CHAR){int at=menu_shortcut(k.ch);if(k.ch>='1'&&k.ch<='9')at=k.ch-'1';if(k.ch=='0')at=9;if(at>=0){selected=at;open=1;}}
        }
    }
    a=selected;active_menu=-1;
    if(a==0){if(b==0)new_file();if(b==1)open_file();if(b==2)import_file();if(b==3)save(0);if(b==4)save(1);if(b==5&&unsaved())running=0;}
    if(a==1){if(b==0)undo(&history,&s,0);if(b==1)undo(&history,&s,1);if(b==2){if(copy()){change();delete_selection(&s);}}if(b==3)copy();if(b==4)paste_clipboard();if(b==5){s.anchor.p=0;s.anchor.o=0;s.cur.p=s.d.n-1;s.cur.o=(int)strlen(s.d.p[s.cur.p].text);s.selecting=1;}rebuild();}
    if(a==2){if(b<6)use_type(element_order[b],1);else edit_type(0);}
    if(a==3){if(b<2)find_next(b);else replace_all();}
    if(a==4){if(b==0)edit_type(0);else cover();}
    if(a==5){
        if(b==0)scenes();
        if(b==1){change();s.d.numbering=!s.d.numbering;}
        if(b==2){
            const char *confirm[]={"Keep locked numbers","Unlock and renumber 1, 2, 3..."};
            if(!s.d.locked||menu("Unlocking changes scene numbers",confirm,2)==1){change();scene_lock(&s.d,!s.d.locked);strcpy(status,s.d.locked?"Scene numbers locked. Insertions use A, B, C suffixes.":"Scene numbers unlocked and renumbered.");}
        }
    }
    if(a==6)output(b);
    if(a==7){FILE *f;keyboard_layout=b;keyboard_reset();f=file_open("MOVIEMST.CFG","w");if(f){fprintf(f,"KEYBOARD %d\n",keyboard_layout);fclose(f);}snprintf(status,sizeof(status),"Keyboard: %s",keyboard_names[keyboard_layout]);}
    if(a==8)help();
    if(a==9&&unsaved())running=0;
}
static void move_cursor(Key k){
    Pos before=s.cur;int row=current_row(),target=row,col=s.cur.o-layout.r[row].start+layout.r[row].indent,len;
    if(k.code==K_LEFT){if(s.cur.o)s.cur.o--;else if(s.cur.p){s.cur.p--;s.cur.o=(int)strlen(s.d.p[s.cur.p].text);}}
    if(k.code==K_RIGHT){if(s.cur.o<(int)strlen(s.d.p[s.cur.p].text))s.cur.o++;else if(s.cur.p+1<s.d.n){s.cur.p++;s.cur.o=0;}}
    if(k.code==K_HOME){if(k.ctrl){s.cur.p=0;s.cur.o=0;}else s.cur.o=layout.r[row].start;}
    if(k.code==K_END){if(k.ctrl){s.cur.p=s.d.n-1;s.cur.o=(int)strlen(s.d.p[s.cur.p].text);}else s.cur.o=layout.r[row].end;}
    if(k.code==K_UP)target--;
    if(k.code==K_DOWN)target++;
    if(k.code==K_PGUP)target-=20;
    if(k.code==K_PGDN)target+=20;
    if(target!=row){int dir=target>row?1:-1;if(target<0)target=0;if(target>=layout.n)target=layout.n-1;while(target>=0&&target<layout.n&&layout.r[target].p<0)target+=dir;if(target>=0&&target<layout.n){Row *r=layout.r+target;s.cur.p=r->p;s.cur.o=r->start+col-r->indent;if(s.cur.o<r->start)s.cur.o=r->start;if(s.cur.o>r->end)s.cur.o=r->end;}}
    len=(int)strlen(s.d.p[s.cur.p].text);if(s.cur.o>len)s.cur.o=len;
    if(k.shift||marking){if(!s.selecting)s.anchor=before;s.selecting=1;}else s.selecting=0;
}
static void handle(Key k){
    int ch=lower857(k.ch);time_t now=time(NULL);
    if(k.code==K_CHAR&&k.ch<0){strcpy(status,"This character is not available in the DOS Latin text set.");return;}
    if(k.code!=K_CHAR||k.ctrl||k.alt)typing=0;
    if(k.code>=K_LEFT&&k.code<=K_PGDN){move_cursor(k);return;}
    if(k.code>=K_F1&&k.code<=K_F6){use_type(element_order[k.code-K_F1],1);return;}
    if(k.code==K_F7){overwrite=!overwrite;strcpy(status,overwrite?"Overwrite mode":"Insert mode");return;}
    if(k.code==K_F8){marking=!marking;s.selecting=marking;if(marking)s.anchor=s.cur;return;}
    if(k.ctrl&&(ch=='z'||ch=='y')){marking=0;undo(&history,&s,ch=='y');rebuild();return;}
    if(k.code==K_F9){main_menu(-1);return;}
    if(k.code==K_F10){save(0);return;}
    if(k.alt){int at=menu_shortcut(ch);if(at==8)help();else if(at==9){if(unsaved())running=0;}else if(at>=0)main_menu(at);return;}
    if(k.ctrl&&k.code==K_ENTER){edit_type(1);return;}
    if(k.ctrl){switch(ch){case 's':case 'w':save(k.shift);break;case 'q':if(unsaved())running=0;break;case 'o':open_file();break;case 'i':import_file();break;case 'n':new_file();break;case 'f':find_next(0);break;case 'g':find_next(1);break;case 'h':replace_all();break;case 'a':marking=0;s.anchor.p=0;s.anchor.o=0;s.cur.p=s.d.n-1;s.cur.o=(int)strlen(s.d.p[s.cur.p].text);s.selecting=1;break;case 'c':copy();break;case 'x':if(copy()){change();if(!delete_selection(&s))strcpy(status,"Paragraph limit exceeded.");}break;case 'v':paste_clipboard();break;}rebuild();return;}
    if(k.code==K_ESC){if(s.selecting||marking){s.selecting=marking=0;}else main_menu(-1);return;}
    if(k.code==K_TAB){const char *suggest=suggestion();change();if(suggest&&!k.shift){char *copy=dupstr(suggest);insert_text(&s,copy);free(copy);}else if(!set_type(&s,(s.d.p[s.cur.p].type+(k.shift?TYPES-1:1))%TYPES))strcpy(status,"Scene number limit. Unlock to renumber.");}
    else if(k.code==K_ENTER){marking=0;change();if(!split_para(&s))strcpy(status,"Cannot create a paragraph. Check the document limits.");}
    else if(k.code==K_BACK||k.code==K_DEL){marking=0;change();backspace(&s,k.code==K_DEL);}
    else if(k.code==K_CHAR&&k.ch>=32&&k.ch<=255){char one[2];if(!typing||now-last_edit>2||s.selecting)checkpoint(&history,&s);typing=1;last_edit=now;one[0]=(char)k.ch;one[1]=0;marking=0;if(overwrite&&!s.selecting&&s.cur.o<(int)strlen(s.d.p[s.cur.p].text)){s.d.p[s.cur.p].text[s.cur.o++]=one[0];}else if(!insert_text(&s,one))strcpy(status,"Paragraph limit: 8190 characters. Press Enter to split.");}
    rebuild();
}
static int ui_selftest(void){
    Key key={0};int i;Pos a,b;
    doc_init(&s.d);rebuild();
    for(i=0;i<6;i++){key.code=K_F1+i;handle(key);assert(s.d.n==1&&s.d.p[0].type==element_order[i]);}
    state_free(&s);history_clear(&history);doc_init(&s.d);assert(insert_text(&s,"abcd"));s.cur.o=2;reset_history();s.cur.o=2;
    key.code=K_F5;handle(key);assert(s.d.n==2&&!strcmp(s.d.p[0].text,"ab")&&s.d.p[1].type==DIALOGUE&&!strcmp(s.d.p[1].text,"cd"));
    key.code=K_CHAR;key.ctrl=1;key.ch='z';handle(key);assert(s.d.n==1&&!strcmp(s.d.p[0].text,"abcd"));
    reset_history();s.cur.o=1;key=(Key){K_F7,0,0,0,0};handle(key);assert(overwrite);
    key=(Key){K_CHAR,'X',0,0,0};handle(key);assert(!strcmp(s.d.p[0].text,"aXcd")&&s.cur.o==2);
    key=(Key){K_CHAR,'z',0,1,0};handle(key);assert(!strcmp(s.d.p[0].text,"abcd"));
    key=(Key){K_F7,0,0,0,0};handle(key);assert(!overwrite);s.cur.o=1;
    key.code=K_F8;handle(key);key.code=K_RIGHT;handle(key);handle(key);assert(marking&&selection(&s,&a,&b)&&a.o==1&&b.o==3);
    key=(Key){K_CHAR,'X',0,0,0};handle(key);assert(!strcmp(s.d.p[0].text,"aXd")&&!marking);
    assert(menu_shortcut('l')==2&&menu_shortcut('h')==8&&menu_shortcut(0)==-1);
    reset_history();free(s.d.p[0].text);s.d.p[0].text=dupstr("McClane walks.");s.cur.o=0;
    key=(Key){K_TAB,0,0,0,0};for(i=0;i<12;i++)handle(key);
    assert(s.d.p[0].type==ACTION&&!strcmp(s.d.p[0].text,"McClane walks."));
    set_type(&s,CHARACTER);key=(Key){K_END,0,0,0,0};handle(key);key=(Key){K_CHAR,'a',0,0,0};handle(key);
    assert(!strcmp(s.d.p[0].text,"McClane walks.a"));
    state_free(&s);history_clear(&history);doc_init(&s.d);assert(insert_text(&s,"McClane"));set_type(&s,CHARACTER);assert(doc_insert(&s.d,1,CHARACTER,"mc"));s.cur.p=1;s.cur.o=2;rebuild();
    /* Complete an uppercase-looking cue without losing its stored spelling. */
    assert(suggestion()&&!strcmp(suggestion(),"Clane"));key=(Key){K_TAB,0,0,0,0};handle(key);assert(!strcmp(s.d.p[1].text,"mcClane"));
    state_free(&s);history_clear(&history);layout_free(&layout);puts("All Movie Master element, overwrite, selection and case-preservation tests passed.");return 0;
}
int main(int argc,char **argv){
    FILE *cfg;char err[256];time_t now;if(argc>1&&!strcmp(argv[1],"/TEST")){int result=selftest();return result?result:ui_selftest();}
#if !defined(_WIN32) && !defined(__DJGPP__)
    fputs("This is a test build. Use the Windows or DOS version.\n",stderr);return 1;
#endif
    doc_init(&s.d);saved_hash=doc_hash(&s.d);cfg=file_open("MOVIEMST.CFG","r");if(!cfg)cfg=file_open("WRITER.CFG","r");if(cfg){int v;if(fscanf(cfg,"KEYBOARD %d",&v)==1){if(v>=0&&v<KEY_COUNT)keyboard_layout=v;}else{rewind(cfg);if(fscanf(cfg,"%d",&v)==1)keyboard_layout=v?KEY_TR_F:KEY_SYSTEM;}fclose(cfg);}rebuild();screen_init();
    cfg=file_open("KURTAR.SEN","rb");if(cfg){const char *m[]={"Open recovery file","Start new (archive previous recovery)"};int choice;fclose(cfg);choice=menu("Recovery from a previous session",m,2);if(choice==0){if(load_doc(&s.d,"KURTAR.SEN",err)){reset_history();saved_hash=doc_hash(&s.d)^1;strcpy(status,"Recovered. Press F10 to save with a new name.");}else{notice(err);running=0;}}else{int i,archived=0;for(i=1;i<1000;i++){char path[16];snprintf(path,sizeof(path),"ESKI%03d.SEN",i);cfg=file_open(path,"rb");if(cfg){fclose(cfg);continue;}if(!file_rename("KURTAR.SEN",path)){archived=1;snprintf(status,sizeof(status),"Previous recovery archived as %s.",path);}break;}if(!archived){notice("Cannot archive recovery. Exiting to preserve the file.");running=0;}}}
    last_recovery=time(NULL);while(running){Key k;draw();k=key_read(keyboard_layout,0);if(k.code)handle(k);else idle();now=time(NULL);if(dirty()&&now-last_recovery>=60){if(save_doc(&s.d,"KURTAR.SEN",err))strcpy(status,"Recovery saved. F10: save your document");else snprintf(status,sizeof(status),"Recovery: %.220s",err);last_recovery=now;}}
    screen_done();state_free(&s);history_clear(&history);doc_free(&clipboard);layout_free(&layout);puts("Movie Master 0.4.4 - Happy writing.");return 0;
}
