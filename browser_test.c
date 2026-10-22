/* Exercise the actual modal file operations with a bounded input queue. */
#define main writer_program_main
#include "main.c"
#undef main
#include <sys/stat.h>

static Key queue[4096];
static int queued, consumed;
void screen_init(void){}
void screen_done(void){}
void cell(int x,int y,int ch,int attr){(void)x;(void)y;(void)ch;(void)attr;}
void screen_clear(int attr){(void)attr;}
void screen_flush(int x,int y){(void)x;(void)y;}
void idle(void){}
void system_copy(const Doc *d){(void)d;}
int system_paste(Doc *d){(void)d;return 0;}
Key key_read(int f,int wait){(void)f;(void)wait;assert(consumed<queued);return queue[consumed++];}
static void key(int code){assert(queued<4096);queue[queued++]=(Key){code,0,0,0,0};}
static void value(const char *utf){char cp[512];int i;assert(utf8_decode(utf,cp,sizeof(cp)));queue[queued++]=(Key){K_CHAR,'a',0,1,0};for(i=0;cp[i];i++){assert(queued<4096);queue[queued++]=(Key){K_CHAR,(unsigned char)cp[i],0,0,0};}key(K_ENTER);}
static void done(void){assert(consumed==queued);consumed=queued=0;}
static void choose(const char *folder,const char *name){key(K_F3);value(folder);key(K_F5);value(name);}
static void check_doc(const char *path,unsigned long hash){Doc d={0};char error[256];assert(load_doc(&d,path,error));assert(doc_hash(&d)==hash);doc_free(&d);}
static void numbering_menus(void){
    char label[16];
    doc_init(&s.d);assert(set_type(&s,SCENE));assert(insert_text(&s,"INT. FIRST - DAY"));
    assert(doc_insert(&s.d,1,SCENE,"EXT. SECOND - NIGHT"));rebuild();saved_hash=doc_hash(&s.d);
    key(K_DOWN);key(K_DOWN);key(K_ENTER);main_menu(5);done();assert(s.d.locked&&s.d.numbering&&dirty());
    use_type(SCENE,1);scene_label(&s.d,1,label);assert(!strcmp(label,"1A"));scene_label(&s.d,2,label);assert(!strcmp(label,"2"));
    key(K_DOWN);key(K_ENTER);main_menu(5);done();assert(!s.d.numbering&&s.d.locked);
    /* Cancelling the unlock confirmation must preserve all production labels. */
    key(K_DOWN);key(K_DOWN);key(K_ENTER);key(K_ESC);main_menu(5);done();assert(s.d.locked);
    key(K_DOWN);key(K_DOWN);key(K_ENTER);key(K_DOWN);key(K_ENTER);main_menu(5);done();assert(!s.d.locked);
    scene_label(&s.d,2,label);assert(!strcmp(label,"3"));
    assert(undo(&history,&s,0)&&s.d.locked);scene_label(&s.d,1,label);assert(!strcmp(label,"1A"));
    state_free(&s);history_clear(&history);layout_free(&layout);
}

int main(void){
    char project[512],other[512],path[512],error[256],oldfile[512],utf[128];unsigned long hash;FILE *f;int kind;
    numbering_menus();assert(getcwd(project,sizeof(project)));assert(path_join(other,project,"chosen"));assert(mkdir(other,0700)==0);
    doc_init(&s.d);assert(utf8_decode("İpek ışığı açar. McClane bekler.",utf,sizeof(utf)));assert(insert_text(&s,utf));rebuild();hash=doc_hash(&s.d);
    /* Cancel both the browser and filename prompt: no filename or document change. */
    key(K_ESC);assert(!save(0));done();assert(!filename[0]&&doc_hash(&s.d)==hash);
    key(K_F6);key(K_F5);key(K_ESC);key(K_ESC);assert(!save(0));done();assert(!filename[0]);
    /* First Save into a selected directory, then normal Save without any modal input. */
    choose(other,"DRAFT");assert(save(0));done();assert(path_join(path,other,"DRAFT.SEN"));assert(!strcmp(filename,path));check_doc(path,hash);
    assert(save(0));done();check_doc(path,hash);strcpy(oldfile,filename);
    /* Existing destination: decline overwrite and preserve both name and target. */
    assert(path_join(path,other,"KEEP.SEN"));f=file_open(path,"wb");assert(f);fputs("keep me",f);fclose(f);
    key(K_F3);value(path);key(K_DOWN);key(K_ENTER);assert(!save(1));done();assert(!strcmp(filename,oldfile));
    f=file_open(path,"rb");assert(f);assert(fgets(utf,sizeof(utf),f)&&!strcmp(utf,"keep me"));fclose(f);
    choose(project,"SECOND");assert(save(1));done();assert(path_join(path,project,"SECOND.SEN"));assert(!strcmp(filename,path));check_doc(path,hash);
    /* Every export selects a destination; cancellation writes no default output. */
    for(kind=0;kind<3;kind++){
        const char *ext=kind==0?".PDF":kind==1?".TXT":".FTN";char name[32];
        key(K_ESC);output(kind);done();snprintf(name,sizeof(name),"OUTPUT%s",ext);assert(path_type(name)!=PATH_FILE);
        choose(other,"EXPORT");key(K_ENTER);output(kind);done();snprintf(name,sizeof(name),"EXPORT%s",ext);assert(path_join(path,other,name));assert(path_type(path)==PATH_FILE);
    }
    /* Open and import follow a selected path, including a Turkish filename. */
    key(K_F3);value(oldfile);open_file();done();assert(!strcmp(filename,oldfile)&&doc_hash(&s.d)==hash);
    assert(path_join(path,other,"Çağrı.fountain"));assert(export_text(&s.d,path,1,error));key(K_F3);value(path);import_file();done();assert(!filename[0]&&doc_hash(&s.d)==hash&&dirty());
    /* Project shortcut and a typed destination path work without filename uppercasing. */
    choose(project,"Türkçe Dosya");assert(save(0));done();assert(strstr(filename,"Türkçe Dosya.SEN"));check_doc(filename,hash);
    state_free(&s);history_clear(&history);layout_free(&layout);doc_free(&clipboard);
    puts("PASS: scene numbering menus, lock/unlock/cancel, undo, first Save, Save As, direct Save, cancel, overwrite refusal, chosen PDF/TXT/Fountain export, Open/Import and Turkish paths.");return 0;
}
