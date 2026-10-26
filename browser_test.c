/* Exercise the actual modal file operations with a bounded input queue. */
#define main movie_master_program_main
#include "main.c"
#undef main
#include <sys/stat.h>
#ifdef __DJGPP__
#include <fcntl.h>
#endif

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
static void letters(const char *utf){char cp[512];int i;assert(utf8_decode(utf,cp,sizeof(cp)));for(i=0;cp[i];i++){assert(queued<4096);queue[queued++]=(Key){K_CHAR,(unsigned char)cp[i],0,0,0};}}
static void value(const char *utf){queue[queued++]=(Key){K_CHAR,'a',0,1,0};letters(utf);key(K_ENTER);}
static void done_at(int line){if(consumed!=queued)fprintf(stderr,"Input at line %d: consumed %d of %d keys.\n",line,consumed,queued);assert(consumed==queued);consumed=queued=0;}
#define done() done_at(__LINE__)
static void choose(const char *folder,const char *name){key(K_F3);value(folder);value(name);}
static void check_doc(const char *path,unsigned long hash){Doc d={0};char error[256];assert(load_doc(&d,path,error));assert(doc_hash(&d)==hash);doc_free(&d);}
static void editor_menus(void){
    unsigned long hash;
    doc_init(&s.d);assert(insert_text(&s,"Ada waits. Ada leaves."));
    assert(doc_insert(&s.d,1,SCENE,"INT. ADA ROOM - DAY"));assert(doc_insert(&s.d,2,ACTION,"A door opens."));
    assert(doc_insert(&s.d,3,SCENE,"EXT. GARDEN - NIGHT"));rebuild();saved_hash=doc_hash(&s.d);s.cur=(Pos){0,0};
    value("Ada");find_next(0);done();assert(s.selecting&&s.anchor.p==0&&s.anchor.o==0&&s.cur.o==3);
    find_next(1);assert(s.anchor.o==11&&s.cur.o==14);find_next(1);assert(s.cur.p==1&&s.cur.o==8);
    find_next(1);assert(s.cur.p==0&&s.cur.o==3);
    value("Ada");value("Mira");replace_all();done();assert(!strcmp(s.d.p[0].text,"Mira waits. Mira leaves."));
    assert(!strcmp(s.d.p[1].text,"INT. Mira ROOM - DAY"));assert(strstr(status,"3 matches"));
    handle((Key){K_CHAR,'z',0,1,0});assert(!strcmp(s.d.p[0].text,"Ada waits. Ada leaves."));
    handle((Key){K_CHAR,'y',0,1,0});assert(!strcmp(s.d.p[0].text,"Mira waits. Mira leaves."));
    value("Test Film");value("Test Writer");cover();done();assert(!strcmp(s.d.title,"Test Film")&&!strcmp(s.d.author,"Test Writer"));
    value("Cancelled");key(K_ESC);cover();done();assert(!strcmp(s.d.title,"Test Film"));
    hash=doc_hash(&s.d);handle((Key){K_CHAR,'a',0,1,0});handle((Key){K_CHAR,'c',0,1,0});assert(clipboard.n==4);
    handle((Key){K_CHAR,'x',0,1,0});assert(s.d.n==1&&!s.d.p[0].text[0]);
    handle((Key){K_CHAR,'v',0,1,0});assert(doc_hash(&s.d)==hash);
    s.cur=(Pos){1,0};key(K_DOWN);key(K_ENTER);scenes();done();assert(s.cur.p==3);
    key(K_F4);scenes();done();assert(scene_count(&s.d)==3);handle((Key){K_CHAR,'z',0,1,0});assert(doc_hash(&s.d)==hash);
    s.cur=(Pos){1,0};key(K_F3);scenes();done();assert(!strcmp(s.d.p[2].text,"INT. Mira ROOM - DAY"));
    key(K_F2);scenes();done();assert(doc_hash(&s.d)==hash);
    key(K_ENTER);help();done();
    key(K_DOWN);key(K_DOWN);key(K_ENTER);assert(!unsaved());done();assert(doc_hash(&s.d)==hash);
    key(K_DOWN);key(K_ENTER);assert(unsaved());done();assert(dirty());
    state_free(&s);history_clear(&history);layout_free(&layout);doc_free(&clipboard);
}
static void inline_names(const char *folder,unsigned long hash){
    char path[512],browse[512],target[512];int kind;
    /* The initial name is selected: ordinary typing replaces it, without F5. */
    key(K_F3);value(folder);letters("DRAFZ");key(K_BACK);letters("T");key(K_HOME);key(K_DEL);letters("D");key(K_END);letters("2");key(K_ENTER);
    assert(save(1));done();assert(path_join(path,folder,"DRAFT2.SEN"));assert(!strcmp(filename,path));check_doc(path,hash);
    key(K_F3);value(folder);value("BAD/NAME");key(K_ENTER);value("FIXED");assert(save(1));done();
    assert(path_join(path,folder,"FIXED.SEN")&&!strcmp(filename,path));check_doc(path,hash);
    key(K_F3);value(folder);letters("F5FILE");key(K_F5);assert(save(1));done();
    assert(path_join(path,folder,"F5FILE.SEN")&&!strcmp(filename,path));check_doc(path,hash);
    assert(path_join(browse,folder,"BROWSE"));assert(mkdir(browse,0700)==0);
    assert(path_join(target,browse,"TARGET"));assert(mkdir(target,0700)==0);
    key(K_F3);value(browse);key(K_TAB);key(K_DOWN);key(K_ENTER);letters("SUBDIR");key(K_ENTER);assert(save(1));done();
    assert(path_join(path,target,"SUBDIR.SEN"));check_doc(path,hash);
#ifdef __DJGPP__
    if(_use_lfn(folder))
#endif
    {choose(folder,"My Feature Film");assert(save(1));done();assert(path_join(path,folder,"My Feature Film.SEN")&&!strcmp(filename,path));check_doc(path,hash);}
    /* Export has the same editable name and leaves the document name alone. */
    for(kind=0;kind<3;kind++){char chosen[512];strcpy(chosen,filename);key(K_F3);value(folder);letters("INLINE");key(K_ENTER);key(K_ENTER);output(kind);done();assert(!strcmp(filename,chosen));
        assert(path_join(path,folder,kind==0?"INLINE.PDF":kind==1?"INLINE.TXT":"INLINE.FTN"));assert(path_type(path)==PATH_FILE);
    }
}
static void new_and_failed_import(const char *folder){
    char path[512],previous[512],error[256];unsigned long hash=doc_hash(&s.d);FILE *f;
    strcpy(previous,filename);assert(path_join(path,folder,"BROKEN.SEN"));f=file_open(path,"wb");assert(f);fputs("MOVIEMASTER 1\npartial",f);fclose(f);
    key(K_F3);value(path);key(K_ENTER);open_file();done();assert(doc_hash(&s.d)==hash&&!strcmp(filename,previous));
    assert(path_join(path,folder,"BROKEN.FTN"));f=file_open(path,"wb");assert(f);fputs("Unsupported \xf0\x9f\x98\x80",f);fclose(f);
    key(K_F3);value(path);key(K_ENTER);import_file();done();assert(doc_hash(&s.d)==hash&&!strcmp(filename,previous));
    assert(insert_text(&s,"Unsaved"));rebuild();hash=doc_hash(&s.d);
    key(K_DOWN);key(K_DOWN);key(K_ENTER);new_file();done();assert(doc_hash(&s.d)==hash&&!strcmp(filename,previous));
    key(K_ENTER);new_file();done();assert(!filename[0]&&s.d.n==1&&!s.d.p[0].text[0]&&!dirty());check_doc(previous,hash);
    assert(load_doc(&s.d,previous,error));rebuild();saved_hash=doc_hash(&s.d)^1;
    key(K_DOWN);key(K_ENTER);new_file();done();assert(s.d.n==1&&!s.d.p[0].text[0]&&!dirty());
}
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
    editor_menus();numbering_menus();
    doc_init(&s.d);rebuild();
    for(kind=0;kind<KEY_COUNT;kind++){int at;for(at=0;at<kind;at++)key(K_DOWN);key(K_ENTER);main_menu(7);done();assert(keyboard_layout==kind);}
    keyboard_layout=KEY_TR_F;keyboard_reset();state_free(&s);layout_free(&layout);
    assert(getcwd(project,sizeof(project)));assert(path_join(other,project,"chosen"));assert(mkdir(other,0700)==0);
    doc_init(&s.d);assert(utf8_decode("İpek ışığı açar. McClane bekler.",utf,sizeof(utf)));assert(insert_text(&s,utf));rebuild();hash=doc_hash(&s.d);
    /* Cancel from either field: no filename or document change. */
    key(K_ESC);assert(!save(0));done();assert(!filename[0]&&doc_hash(&s.d)==hash);
    key(K_F6);key(K_TAB);key(K_ESC);assert(!save(0));done();assert(!filename[0]);
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
#ifdef __DJGPP__
    assert(path_join(path,other,"IMPORT.FTN"));assert(export_text(&s.d,path,1,error));key(K_F3);value(path);import_file();done();assert(!filename[0]&&doc_hash(&s.d)==hash&&dirty());
#endif
    /* Project shortcut and a typed destination path work without filename uppercasing. */
#ifndef __DJGPP__
    assert(path_join(path,other,"Çağrı.fountain"));assert(export_text(&s.d,path,1,error));key(K_F3);value(path);import_file();done();assert(!filename[0]&&doc_hash(&s.d)==hash&&dirty());
    choose(project,"Türkçe Dosya");assert(save(0));done();assert(strstr(filename,"Türkçe Dosya.SEN"));check_doc(filename,hash);
#endif
    inline_names(other,hash);
    new_and_failed_import(other);
    state_free(&s);history_clear(&history);layout_free(&layout);doc_free(&clipboard);
    puts("PASS: filename typing/editing, Enter/F5 Save, folder navigation, Save As, all exports, cancel, overwrite refusal, Open/Import, Find/Replace, title page, clipboard, scene move/duplicate/numbering, undo/redo, keyboard menus and help.");return 0;
}
