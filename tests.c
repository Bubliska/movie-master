#include "moviemst.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef __DJGPP__
#include <fcntl.h>
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
static void put(Doc *d,int type,const char *utf){char text[MAX_TEXT+1];assert(utf8_decode(utf,text,sizeof(text)));assert(doc_insert(d,d->n,type,text));}
static int same_filename(const char *a,const char *b){while(*a&&*b)if(tolower((unsigned char)*a++)!=tolower((unsigned char)*b++))return 0;return !*a&&!*b;}
static void label_is(const Doc *d,int para,const char *expected){char label[16];scene_label(d,para,label);assert(!strcmp(label,expected));}
static void numbering_tests(void){
    State s={0};History h={0};Doc loaded={0},clip={0};char error[256],text[256];unsigned long before;int i,warnings,base,suffix;FILE *f;
    for(i=0;i<12;i++)assert(doc_insert(&s.d,s.d.n,SCENE,"INT. ROOM - DAY"));
    assert(scene_count(&s.d)==12);s.d.numbering=1;label_is(&s.d,10,"11");
    checkpoint(&h,&s);scene_lock(&s.d,1);assert(s.d.locked);label_is(&s.d,11,"12");
    assert(undo(&h,&s,0)&&!s.d.locked);assert(undo(&h,&s,1)&&s.d.locked);
    checkpoint(&h,&s);assert(doc_insert(&s.d,11,SCENE,"EXT. DESERT - NIGHT"));
    label_is(&s.d,11,"11A");label_is(&s.d,12,"12");
    assert(undo(&h,&s,0)&&scene_count(&s.d)==12);assert(undo(&h,&s,1));label_is(&s.d,11,"11A");
    assert(doc_insert(&s.d,12,SCENE,"EXT. DUNES - NIGHT"));label_is(&s.d,12,"11B");label_is(&s.d,13,"12");
    /* Deleted production labels remain reserved, including after save/reopen. */
    doc_remove(&s.d,11);assert(doc_insert(&s.d,12,SCENE,"EXT. RIDGE - NIGHT"));label_is(&s.d,12,"11C");
    s.cur.p=12;assert(scene_move(&s,-1));label_is(&s.d,11,"11C");assert(scene_move(&s,1));label_is(&s.d,12,"11C");
    assert(scene_duplicate(&s));label_is(&s.d,13,"11D");label_is(&s.d,14,"12");
    assert(doc_insert(&s.d,0,SCENE,"INT. PROLOGUE - DAY"));label_is(&s.d,0,"0A");
    assert(doc_insert(&s.d,s.d.n,SCENE,"EXT. END - DAY"));label_is(&s.d,s.d.n-1,"13");
    doc_remove(&s.d,s.d.n-1);assert(doc_insert(&s.d,s.d.n,SCENE,"EXT. NEW END - DAY"));label_is(&s.d,s.d.n-1,"14");
    s.cur.p=0;assert(set_type(&s,ACTION));assert(set_type(&s,SCENE));label_is(&s.d,0,"0B");
    s.anchor=(Pos){0,0};s.cur=(Pos){1,0};s.selecting=1;assert(copy_selection(&s,&clip));
    s.cur=(Pos){0,0};s.selecting=0;assert(paste(&s,&clip));label_is(&s.d,1,"0C");
    file_remove("NUMBER.SEN");file_remove("NUMBER.BAK");assert(save_doc(&s.d,"NUMBER.SEN",error));
    assert(load_doc(&loaded,"NUMBER.SEN",error)&&doc_hash(&loaded)==doc_hash(&s.d));
    assert(doc_insert(&loaded,1,SCENE,"EXT. INSERT - DAY"));label_is(&loaded,1,"0D");
    assert(export_text(&s.d,"NUMBER.FTN",1,error));assert(import_fountain(&loaded,"NUMBER.FTN",error,&warnings));
    assert(loaded.locked&&loaded.numbering&&scene_count(&loaded)==scene_count(&s.d));label_is(&loaded,1,"0C");
    assert(export_text(&s.d,"NUMBER.TXT",0,error));assert(export_pdf(&s.d,"NUMBER.PDF","FONT.TTF",error));
    f=file_open("NUMBER.TXT","rb");assert(f&&fgets(text,sizeof(text),f)&&strstr(text,"0B  INT. PROLOGUE"));fclose(f);
    before=doc_hash(&loaded);
    for(i=0;i<4;i++){
        f=file_open("BADNUM.SEN","wb");assert(f);
        fputs("WRITER 2\nT\t\nY\t\nN\t1\t1\t2\nS\t11\t0\nS\t11\t1\n",f);
        fputs(i==0?"0\t1\tfirst\n0\t1\tduplicate\n":i==1?"0\t3\tbad ref\n":i==2?"1\t1\taction with number\n":"0\t9999999999999999999999999\tbad\n",f);fclose(f);
        assert(!load_doc(&loaded,"BADNUM.SEN",error)&&doc_hash(&loaded)==before);
    }
    f=file_open("DUPNUM.FTN","wb");assert(f);fputs(".INT. ONE #11A#\n\n.INT. TWO #11A#\n",f);fclose(f);
    assert(!import_fountain(&loaded,"DUPNUM.FTN",error,&warnings)&&doc_hash(&loaded)==before);
    scene_lock(&s.d,0);assert(!s.d.locked&&!s.d.nn);label_is(&s.d,0,"1");label_is(&s.d,1,"2");
    scene_lock(&s.d,1);for(i=0;i<27;i++)assert(doc_insert(&s.d,1,SCENE,"INT. INSERT - DAY"));label_is(&s.d,1,"1AA");
    assert(scene_parse("11AZ",&base,&suffix)&&base==11&&suffix==52);assert(!scene_parse("11A2",&base,&suffix));assert(!scene_parse("9999999999999999999999",&base,&suffix));
    {State limit={0};Doc incoming={0};unsigned long hash;
        assert(doc_insert(&limit.d,0,SCENE,"INT. LIMIT - DAY"));limit.d.p[0].scene_id=scene_register(&limit.d,1000000,0);limit.d.locked=limit.d.numbering=1;
        limit.cur.o=(int)strlen(limit.d.p[0].text);hash=doc_hash(&limit.d);
        assert(!scene_duplicate(&limit)&&doc_hash(&limit.d)==hash);
        assert(doc_insert(&incoming,0,ACTION,"More text"));assert(doc_insert(&incoming,1,SCENE,"EXT. INSERT - DAY"));
        assert(!paste(&limit,&incoming)&&doc_hash(&limit.d)==hash);
        state_free(&limit);doc_free(&incoming);
    }
    state_free(&s);history_clear(&h);doc_free(&loaded);doc_free(&clip);
}
static void destination_tests(void){
    char path[512],folder[512],name[512],error[256];Doc d={0},loaded={0};
    path_split("C:/PROJECT/FILM.SEN",folder,name);assert(!strcmp(folder,"C:/PROJECT")&&!strcmp(name,"FILM.SEN"));
    path_split("C:\\FILM.SEN",folder,name);assert(!strcmp(folder,"C:\\")&&!strcmp(name,"FILM.SEN"));
    path_split("/FILM.SEN",folder,name);assert(!strcmp(folder,"/")&&!strcmp(name,"FILM.SEN"));
    path_split("FILM.SEN",folder,name);assert(!folder[0]&&!strcmp(name,"FILM.SEN"));
    assert(destination_path(path,".","CHOOSE",".SEN",error)&&!strcmp(path,"./CHOOSE.SEN"));
    file_remove(path);file_remove("CHOOSE.BAK");doc_init(&d);assert(doc_insert(&d,1,ACTION,"Chosen destination."));assert(save_doc(&d,path,error));assert(load_doc(&loaded,path,error)&&doc_hash(&loaded)==doc_hash(&d));doc_free(&d);doc_free(&loaded);
    assert(!destination_path(path,".","Kurtar.sen",".SEN",error));
    assert(!destination_path(path,".","con.foo.sen",".SEN",error));assert(!destination_path(path,".","com1.sen",".SEN",error));
    assert(!destination_path(path,".","SUB/FILM.SEN",".SEN",error));assert(!destination_path(path,".","FILM.PDF",".SEN",error));
    assert(!destination_path(path,".","",".SEN",error));assert(!destination_path(path,".","BAD.",".SEN",error));
    assert(!destination_path(path,"./NO-SUCH-DIR-ABC","FILM",".SEN",error));
    assert(file_matches("FILM.SEN",3)&&!file_matches("FILM.BAK",3)&&file_matches("OUT.pdf",4)&&file_matches("OUT.TXT",5)&&file_matches("OUT.FTN",6));
#ifdef __DJGPP__
    if(_use_lfn(".")){assert(destination_path(path,".","My Feature Film",".SEN",error)&&!strcmp(path,"./My Feature Film.SEN"));}
    else{assert(!destination_path(path,".","TOO-LONG-NAME",".SEN",error));assert(!destination_path(path,".","BAD NAME",".SEN",error));}
#else
    assert(destination_path(path,".","Türkçe Dosya",".SEN",error)&&!strcmp(path,"./Türkçe Dosya.SEN"));
#endif
}
static void case_tests(void){
    State s={0};Doc loaded={0};char original[256],query[32],error[256],line[256];FILE *f;int i,warnings;
    doc_init(&s.d);assert(utf8_decode("İpek, ışığı açar. McClane ve NASA bekler.",original,sizeof(original)));assert(insert_text(&s,original));
    assert(utf8_decode("IŞIĞI",query,sizeof(query)));assert(element_find(CHARACTER,original,query));assert(!element_find(ACTION,original,query));assert(!element_find(DIALOGUE,original,"MCCLANE"));assert(element_find(SCENE,original,"MCCLANE"));
    for(i=0;i<TYPES*2;i++){set_type(&s,i%TYPES);assert(!strcmp(s.d.p[0].text,original));}
    set_type(&s,ACTION);assert(!strcmp(s.d.p[0].text,original));
    assert(doc_insert(&s.d,1,CHARACTER,original));assert(doc_insert(&s.d,2,DIALOGUE,original));
    file_remove("CASE.SEN");file_remove("CASE.BAK");assert(save_doc(&s.d,"CASE.SEN",error));assert(load_doc(&loaded,"CASE.SEN",error));assert(doc_hash(&s.d)==doc_hash(&loaded));
    s.cur.p=0;set_type(&s,SCENE);
    assert(export_text(&s.d,"CASE.FTN",1,error));assert(import_fountain(&loaded,"CASE.FTN",error,&warnings));assert(doc_hash(&loaded)==doc_hash(&s.d));
    s.cur.p=1;set_type(&s,ACTION);
    assert(export_text(&s.d,"CASE.TXT",0,error));assert(export_pdf(&s.d,"CASE.PDF","FONT.TTF",error));
    f=file_open("CASE.TXT","rb");assert(f);assert(fgets(line,sizeof(line),f));assert(!strcmp(line,"İPEK, IŞIĞI AÇAR. MCCLANE VE NASA BEKLER.\n"));fclose(f);
    assert(!strcmp(s.d.p[0].text,original));state_free(&s);doc_free(&loaded);
}
static void fountain_tests(void){
    Doc d={0};char error[256],utf[256];int warnings,i,n;FileEntry *entries=NULL;FILE *f;
    const char *fixture="\xef\xbb\xbfTitle:\r\n    **SON IŞIK**\r\nAuthor: Çağrı Şen\r\n\r\n"
        "INT. ROOM - NIGHT #12#\r\n\r\nA **bold** word and *soft* voice. She says “hello”…\r\n"
        "The phone shows \\*69.\r\n\r\n@Ada\r\n(whispering)\r\nMerhaba.\r\nNasılsın?\r\n\r\n"
        ">CUT TO:\r\n\r\n.DIŞ. SOKAK - GECE\r\n\r\n!LOUD ACTION\r\n\r\n"
        "İPEK ^\r\nİyiyim.\r\n\r\n[[private note]]\r\n/* INT. HIDDEN - DAY\r\nHidden action. */\r\n";
    f=file_open("IMPORT.FTN","wb");assert(f);assert(fwrite(fixture,1,strlen(fixture),f)==strlen(fixture));fclose(f);
    assert(import_fountain(&d,"IMPORT.FTN",error,&warnings));assert(warnings>0);assert(d.n==10);
    assert(utf8_decode("SON IŞIK",utf,sizeof(utf))&&!strcmp(d.title,utf));
    assert(utf8_decode("Çağrı Şen",utf,sizeof(utf))&&!strcmp(d.author,utf));
    assert(d.p[0].type==SCENE&&!strcmp(d.p[0].text,"INT. ROOM - NIGHT"));
    assert(d.p[1].type==ACTION&&!strcmp(d.p[1].text,"A bold word and soft voice. She says \"hello\"... The phone shows *69."));
    assert(d.p[2].type==CHARACTER&&!strcmp(d.p[2].text,"Ada"));assert(d.p[3].type==PAREN);assert(d.p[4].type==DIALOGUE);
    assert(utf8_decode("Merhaba. Nasılsın?",utf,sizeof(utf))&&!strcmp(d.p[4].text,utf));
    assert(d.p[5].type==TRANSITION&&d.p[6].type==SCENE&&d.p[7].type==ACTION&&d.p[8].type==CHARACTER);
    assert(d.p[9].type==DIALOGUE);
    doc_free(&d);
    f=file_open("ESCAPE.FTN","wb");assert(f);fputs("\\.LITERAL\nAction continues.\n\nCUT TO:  \n\nint kitchen - day\n\n@McCLANE\nHello.\n",f);fclose(f);
    assert(import_fountain(&d,"ESCAPE.FTN",error,&warnings));assert(d.n==5);
    assert(d.p[0].type==ACTION&&!strcmp(d.p[0].text,".LITERAL Action continues."));assert(d.p[1].type==ACTION);
    assert(d.p[2].type==SCENE&&!strcmp(d.p[2].text,"int kitchen - day"));assert(d.p[3].type==CHARACTER&&!strcmp(d.p[3].text,"McCLANE"));doc_free(&d);
    for(i=0;i<4;i++){
        unsigned long before;doc_init(&d);assert(doc_insert(&d,1,ACTION,"Keep this document"));before=doc_hash(&d);
        f=file_open("BAD.FTN","wb");assert(f);
        if(i==0)fwrite("\xc0\xaf",1,2,f);
        if(i==1)fputs("INT. ROOM\n\nEmoji \xf0\x9f\x98\x80",f);
        if(i==2)fputs("INT. ROOM\n/* unfinished comment",f);
        if(i==3){int j;for(j=0;j<MAX_TEXT+1;j++)fputc('A',f);}
        fclose(f);assert(!import_fountain(&d,"BAD.FTN",error,&warnings));assert(doc_hash(&d)==before);doc_free(&d);
    }
    assert(file_matches("Film.FOUNTAIN",1)&&file_matches("FILM.FOU",1)&&file_matches("film.ftn",1)&&!file_matches("film.pdf",1));
    assert(path_join(utf,"D:/folder","film.fountain")&&!strcmp(utf,"D:/folder/film.fountain"));path_parent(utf);assert(!strcmp(utf,"D:/folder"));path_parent(utf);assert(!strcmp(utf,"D:/"));path_parent(utf);assert(!strcmp(utf,"D:/"));
    assert(files_list(".",&entries,&n,1,error));for(i=0;i<n;i++)if(same_filename(entries[i].name,"IMPORT.FTN"))break;assert(i<n);files_free(entries,n);
#ifndef __DJGPP__
    f=file_open("Türkçe Çağrı.fountain","wb");assert(f);fputs("INT. ROOM - NIGHT\n\nA working Unicode file name.\n",f);fclose(f);
    assert(import_fountain(&d,"Türkçe Çağrı.fountain",error,&warnings));doc_free(&d);assert(file_remove("Türkçe Çağrı.fountain")==0);
#endif
}
static int mapped(int layout,int scan,int shift,int caps,int altgr,int compose){keyboard_layout=layout;return keyboard_key(layout,scan,shift,caps,altgr,compose);}
static void keyboard_tests(void){
    assert(keyboard_control_scan(119)==71&&keyboard_control_scan(117)==79);
    assert(keyboard_control_scan(115)==75&&keyboard_control_scan(116)==77);
    assert(keyboard_control_scan(132)==73&&keyboard_control_scan(118)==81);
    assert(keyboard_control_scan(141)==72&&keyboard_control_scan(145)==80&&keyboard_control_scan(147)==83);
    assert(!keyboard_control_scan(71)&&!keyboard_control_scan(30));
    keyboard_reset();
    assert(mapped(KEY_US,16,0,0,0,1)=='q'&&mapped(KEY_US,23,1,0,0,1)=='I');
    assert(mapped(KEY_US,23,0,1,0,1)=='I'&&mapped(KEY_US,23,1,1,0,1)=='i');
    assert(mapped(KEY_UK,4,1,0,0,1)==from_unicode(0xa3)&&mapped(KEY_UK,40,1,0,0,1)=='@');
    assert(mapped(KEY_DVORAK,16,0,0,0,1)=='\''&&mapped(KEY_DVORAK,17,0,0,0,1)==',');
    assert(mapped(KEY_TR_Q,23,0,0,0,1)==141&&mapped(KEY_TR_Q,40,1,0,0,1)==152);
    assert(mapped(KEY_TR_F,16,0,0,0,1)=='f'&&mapped(KEY_TR_F,31,1,0,0,1)==152);
    assert(mapped(KEY_ES,39,0,0,0,1)==from_unicode(0xf1));
    assert(mapped(KEY_ES,40,0,0,0,1)==0);
    /* Modifier probing must not discard a pending accent. */
    assert(mapped(KEY_ES,30,0,0,0,0)=='a');
    assert(mapped(KEY_ES,30,0,0,0,1)==from_unicode(0xe1));
    assert(mapped(KEY_ES,3,0,0,1,1)=='@');
    assert(mapped(KEY_FR,16,0,0,0,1)=='a'&&mapped(KEY_FR,30,0,0,0,1)=='q');
    assert(mapped(KEY_FR,3,0,0,0,1)==from_unicode(0xe9));
    assert(mapped(KEY_FR,26,0,0,0,1)==0&&mapped(KEY_FR,18,0,0,0,1)==from_unicode(0xea));
    assert(mapped(KEY_DE,21,0,0,0,1)=='z'&&mapped(KEY_DE,44,0,0,0,1)=='y');
    assert(mapped(KEY_DE,39,0,1,0,1)==from_unicode(0xd6));
    assert(mapped(KEY_IT,26,0,0,0,1)==from_unicode(0xe8));
    assert(mapped(KEY_PT,43,0,0,0,1)==0&&mapped(KEY_PT,30,0,0,0,1)==from_unicode(0xe3));
    assert(mapped(KEY_BR,26,0,0,0,1)==0&&mapped(KEY_BR,30,0,0,0,1)==from_unicode(0xe1));
    assert(mapped(KEY_FR,26,0,0,0,1)==0&&mapped(KEY_FR,48,0,0,0,1)=='^');
    assert(keyboard_pending()=='b'&&!keyboard_pending());
    assert(mapped(KEY_ES,40,0,0,0,1)==0&&mapped(KEY_ES,57,0,0,0,1)==from_unicode(0xb4));
    keyboard_layout=KEY_FR;assert(upper857(from_unicode(0xe9))==from_unicode(0xc9));
    keyboard_layout=KEY_TR_F;keyboard_reset();
}
int selftest(void){
    State s={0};History h={0};Doc clip={0},loaded={0};Layout l;char text[256],err[256],*utf;unsigned long before;int i;
    keyboard_tests();doc_init(&s.d);
    assert(utf8_decode("ÇçĞğıİÖöŞşÜü",text,sizeof(text)));utf=utf8_encode(text);assert(!strcmp(utf,"ÇçĞğıİÖöŞşÜü"));free(utf);
    assert(upper857('i')==152&&upper857(141)=='I'&&lower857('I')==141&&lower857(152)=='i');
    assert(keyboard_key(KEY_TR_F,16,0,0,0,0)=='f'&&keyboard_key(KEY_TR_F,18,0,0,0,0)==167&&keyboard_key(KEY_TR_F,19,0,0,0,0)==141&&keyboard_key(KEY_TR_F,31,1,0,0,0)==152&&keyboard_key(KEY_TR_F,31,1,1,0,0)=='i'&&keyboard_key(KEY_TR_F,50,0,0,0,0)=='s');
    assert(!utf8_decode("\xc0\xaf",text,sizeof(text))&&!utf8_decode("\xed\xa0\x80",text,sizeof(text)));
    checkpoint(&h,&s);assert(insert_text(&s,"ilk"));assert(split_para(&s));set_type(&s,CHARACTER);assert(insert_text(&s,"ADA"));assert(split_para(&s));assert(s.d.p[2].type==DIALOGUE);assert(insert_text(&s,"Merhaba."));
    s.anchor.p=0;s.anchor.o=1;s.cur.p=2;s.cur.o=3;s.selecting=1;assert(copy_selection(&s,&clip));assert(delete_selection(&s));assert(!strcmp(s.d.p[0].text,"ihaba."));assert(paste(&s,&clip));assert(s.d.n==3&&!strcmp(s.d.p[0].text,"ilk")&&!strcmp(s.d.p[2].text,"Merhaba."));
    assert(undo(&h,&s,0));assert(s.d.n==1&&!s.d.p[0].text[0]);assert(undo(&h,&s,1));assert(s.d.n==3);
    /* Turkish characters, literal slashes, metadata, backup, and rejection without data loss. */
    assert(utf8_decode("İLK FİLM",s.d.title,sizeof(s.d.title)));assert(utf8_decode("Çağrı Şen",s.d.author,sizeof(s.d.author)));put(&s.d,SCENE,"İÇ. EV - GÜNDÜZ");put(&s.d,ACTION,"Işık söner. C:\\FILM\\SES");put(&s.d,SCENE,"DIŞ. SOKAK - GECE");put(&s.d,ACTION,"Yağmur yağar.");
    file_remove("TEST.SEN");file_remove("TEST.TMP");file_remove("TEST.OLD");file_remove("TEST.BAK");assert(save_doc(&s.d,"TEST.SEN",err));assert(load_doc(&loaded,"TEST.SEN",err));assert(doc_hash(&s.d)==doc_hash(&loaded));
    {FILE *f=file_open("TEST.SEN","rb");assert(f);assert(fgets(text,sizeof(text),f)&&!strcmp(text,"MOVIEMASTER 1\n"));fclose(f);}
    {Doc old={0};FILE *f=file_open("LEGACY.SEN","wb");assert(f);fputs("WRITER 1\nT\t\nY\t\n1\tA preserved legacy draft.\n",f);fclose(f);assert(load_doc(&old,"LEGACY.SEN",err)&&old.n==1&&!strcmp(old.p[0].text,"A preserved legacy draft."));doc_free(&old);}
    s.cur.p=3;s.cur.o=0;s.selecting=0;assert(scene_duplicate(&s));assert(s.d.n==9);assert(scene_move(&s,1));assert(scene_move(&s,-1));assert(save_doc(&s.d,"TEST.SEN",err));
    before=doc_hash(&loaded);{FILE *f=file_open("BAD.SEN","wb");assert(f);fputs("WRITER 1\nT\tbad\nY\t\n0\tpartial",f);fclose(f);}assert(!load_doc(&loaded,"BAD.SEN",err)&&doc_hash(&loaded)==before);
    {FILE *f=file_open("TEST.TMP","wb");assert(f);fputs("do not overwrite",f);fclose(f);}assert(!save_doc(&s.d,"TEST.SEN",err));file_remove("TEST.TMP");assert(load_doc(&loaded,"TEST.SEN",err)&&doc_hash(&loaded)==doc_hash(&s.d));
    for(i=0;i<200;i++)put(&s.d,ACTION,"Uzun bir sahne içinde doğru sayfalama ve Türkçe yazı sınaması. İnsanlar konuşur, gün doğar.");
    {char longword[1001];memset(longword,'W',1000);longword[1000]=0;assert(doc_insert(&s.d,s.d.n,ACTION,longword));}
    layout_build(&s.d,&l);assert(l.pages>5);for(i=0;i<l.n;i++){assert(l.r[i].line>=0&&l.r[i].line<54);assert(l.r[i].end-l.r[i].start<=60);if(i)assert(l.r[i].page>=l.r[i-1].page);}layout_free(&l);
    assert(export_text(&s.d,"TEST.TXT",0,err));assert(export_text(&s.d,"TEST.FTN",1,err));assert(export_pdf(&s.d,"TEST.PDF","FONT.TTF",err));
    {Doc roundtrip={0};int warnings;assert(import_fountain(&roundtrip,"TEST.FTN",err,&warnings));assert(doc_hash(&roundtrip)==doc_hash(&s.d));doc_free(&roundtrip);}
    fountain_tests();case_tests();destination_tests();numbering_tests();
    {FILE *f=file_open("TEST.OK","w");assert(f);fputs("PASS: encoding, F keyboard, editing, undo/redo, clipboard, scenes, safe save/load, pagination, TXT/Fountain/PDF\n",f);fclose(f);}
    state_free(&s);history_clear(&h);doc_free(&clip);doc_free(&loaded);puts("All Movie Master core tests passed.");return 0;
}
