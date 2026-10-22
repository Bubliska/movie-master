#include "writer.h"
#include "screen.h"
#include <string.h>
#include <stdlib.h>
static unsigned char pixels[80*25*2];
void cell(int x,int y,int ch,int attr){if(x>=0&&x<80&&y>=0&&y<25){pixels[(y*80+x)*2]=(unsigned char)ch;pixels[(y*80+x)*2+1]=(unsigned char)attr;}}
void screen_clear(int attr){int x,y;for(y=0;y<25;y++)for(x=0;x<80;x++)cell(x,y,' ',attr);}
#if defined(_WIN32) || defined(__DJGPP__)
static const unsigned char *cursor_frame(int x,int y){
    static unsigned char frame[sizeof(pixels)];
    memcpy(frame,pixels,sizeof(frame));
    if(x>=0&&x<80&&y>=0&&y<25){
        int at=(y*80+x)*2+1;
        /* A steady inverse cell replaces the blinking hardware cursor. Keep
           bit 7 clear: VGA interprets it as a text blink attribute. */
        frame[at]=(frame[at]&0x70)?0x07:0x70;
    }
    return frame;
}
static int special(int scan,int ascii){
    (void)ascii;
    if(scan==28)return K_ENTER;
    if(scan==14)return K_BACK;
    if(scan==15)return K_TAB;
    if(scan==1)return K_ESC;
    if(scan>=59&&scan<=68)return K_F1+scan-59;
    switch(scan){case 75:return K_LEFT;case 77:return K_RIGHT;case 72:return K_UP;case 80:return K_DOWN;case 71:return K_HOME;case 79:return K_END;case 73:return K_PGUP;case 81:return K_PGDN;case 83:return K_DEL;}
    return K_CHAR;
}
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE hin,hout;static DWORD inmode,outmode;static CONSOLE_CURSOR_INFO oldcursor;static CONSOLE_SCREEN_BUFFER_INFO oldinfo;
static DWORD clipboard_sequence;
void screen_init(void){COORD size={80,25};SMALL_RECT rect={0,0,79,24};CONSOLE_CURSOR_INFO ci={20,FALSE};hin=GetStdHandle(STD_INPUT_HANDLE);hout=GetStdHandle(STD_OUTPUT_HANDLE);GetConsoleMode(hin,&inmode);GetConsoleMode(hout,&outmode);GetConsoleCursorInfo(hout,&oldcursor);GetConsoleScreenBufferInfo(hout,&oldinfo);SetConsoleMode(hin,ENABLE_EXTENDED_FLAGS|ENABLE_WINDOW_INPUT);SetConsoleWindowInfo(hout,TRUE,&rect);SetConsoleScreenBufferSize(hout,size);SetConsoleCursorInfo(hout,&ci);SetConsoleTitleW(L"WRITER - Screenplay");}
void screen_done(void){SetConsoleMode(hin,inmode);SetConsoleMode(hout,outmode);SetConsoleTextAttribute(hout,oldinfo.wAttributes);SetConsoleCursorInfo(hout,&oldcursor);SetConsoleCursorPosition(hout,(COORD){0,24});}
void screen_flush(int x,int y){CHAR_INFO out[2000];SMALL_RECT rect={0,0,79,24};const unsigned char *frame=cursor_frame(x,y);int i;for(i=0;i<2000;i++){out[i].Char.UnicodeChar=unicode857[frame[i*2]];out[i].Attributes=frame[i*2+1];}WriteConsoleOutputW(hout,out,(COORD){80,25},(COORD){0,0},&rect);}
Key key_read(int f_layout,int wait){Key k={0};INPUT_RECORD ev;DWORD count;for(;;){KEY_EVENT_RECORD *e;int scan,ascii,cap;if(!wait){GetNumberOfConsoleInputEvents(hin,&count);if(!count)return k;}if(!ReadConsoleInputW(hin,&ev,1,&count))return k;if(ev.EventType!=KEY_EVENT||!ev.Event.KeyEvent.bKeyDown)continue;e=&ev.Event.KeyEvent;scan=e->wVirtualScanCode;ascii=e->uChar.UnicodeChar;k.shift=(e->dwControlKeyState&SHIFT_PRESSED)!=0;k.ctrl=(e->dwControlKeyState&(LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED))!=0;k.alt=(e->dwControlKeyState&(LEFT_ALT_PRESSED|RIGHT_ALT_PRESSED))!=0;cap=(e->dwControlKeyState&CAPSLOCK_ON)!=0;k.code=special(scan,ascii);if(k.code!=K_CHAR)return k;
        if(f_layout)k.ch=f_key(scan,k.shift,cap);else{k.ch=from_unicode(ascii);if(k.ctrl&&ascii>=1&&ascii<=26)k.ch=ascii+'a'-1;}
        if(k.ch>0)return k;
    }}
void idle(void){Sleep(15);}
void system_copy(const Doc *clip){
    size_t count=1;int i;HGLOBAL block;WCHAR *p;
    for(i=0;i<clip->n;i++)count+=strlen(clip->p[i].text)+2;
    block=GlobalAlloc(GMEM_MOVEABLE,count*sizeof(WCHAR));if(!block)return;p=GlobalLock(block);if(!p){GlobalFree(block);return;}
    for(i=0;i<clip->n;i++){const unsigned char *t=(const unsigned char*)clip->p[i].text;if(i){*p++='\r';*p++='\n';}while(*t)*p++=unicode857[*t++];}*p=0;GlobalUnlock(block);
    if(!OpenClipboard(GetConsoleWindow())){GlobalFree(block);return;}
    if(!EmptyClipboard()||!SetClipboardData(CF_UNICODETEXT,block))GlobalFree(block);else clipboard_sequence=GetClipboardSequenceNumber();CloseClipboard();
}
int system_paste(Doc *clip){
    HGLOBAL block;const WCHAR *p;size_t count,i;Doc tmp={0};char line[MAX_TEXT+1];int len=0,ok=1;
    if(clipboard_sequence&&GetClipboardSequenceNumber()==clipboard_sequence)return 0;
    if(!IsClipboardFormatAvailable(CF_UNICODETEXT)||!OpenClipboard(GetConsoleWindow()))return 0;
    block=GetClipboardData(CF_UNICODETEXT);p=block?GlobalLock(block):NULL;count=block?GlobalSize(block)/sizeof(WCHAR):0;
    if(!p||count>8UL*1024*1024){if(p)GlobalUnlock(block);CloseClipboard();return -1;}
    for(i=0;i<count&&p[i];i++){int c;if(p[i]=='\r'&&i+1<count&&p[i+1]=='\n')continue;if(p[i]=='\n'||p[i]=='\r'){line[len]=0;if(!doc_insert(&tmp,tmp.n,ACTION,line)){ok=0;break;}len=0;continue;}c=from_unicode(p[i]);if(c<32||len>=MAX_TEXT){ok=0;break;}line[len++]=(char)c;}
    if(i==count)ok=0;line[len]=0;if(ok&&!doc_insert(&tmp,tmp.n,ACTION,line))ok=0;
    GlobalUnlock(block);CloseClipboard();if(!ok){doc_free(&tmp);return -1;}doc_free(clip);*clip=tmp;return 1;
}
#elif defined(__DJGPP__)
#include <conio.h>
#include <bios.h>
#include <dpmi.h>
#include <sys/movedata.h>
#include <go32.h>
#include <dos.h>
#include <unistd.h>
static unsigned char oldfont[4096];static int fontseg=-1,fontsel;static int oldmode;
static void loadfont(const unsigned char *font){__dpmi_regs r;dosmemput(font,4096,(unsigned long)fontseg*16);memset(&r,0,sizeof(r));r.x.ax=0x1100;r.h.bh=16;r.x.cx=256;r.x.es=fontseg;__dpmi_int(0x10,&r);}
static void turkish_font(void){__dpmi_regs r;unsigned char font[4096],*dotless=font+141*16;int first,end,codes[]={166,167,141,152,158,159},base[]={'G','g','i','I','S','s'},i;memset(&r,0,sizeof(r));r.x.ax=0x1130;r.h.bh=6;__dpmi_int(0x10,&r);dosmemget((unsigned long)r.x.es*16+r.x.bp,4096,oldfont);memcpy(font,oldfont,4096);
    for(i=0;i<6;i++)memcpy(font+codes[i]*16,font+base[i]*16,16);
    font[166*16+0]=0x44;font[166*16+1]=0x38;font[167*16+2]=0x44;font[167*16+3]=0x38;
    /* VGA fonts place i's dot on different rows. Remove its first ink run,
       stopping at the blank row above the stem, instead of fixed row numbers. */
    for(first=0;first<16&&!dotless[first];first++){}
    for(end=first;end<16&&dotless[end];end++){}
    if(end<16)memset(dotless+first,0,end-first);
    font[152*16+0]=0x18;font[152*16+1]=0x18;
    font[158*16+13]=0x18;font[158*16+14]=0x08;font[158*16+15]=0x30;font[159*16+13]=0x18;font[159*16+14]=0x08;font[159*16+15]=0x30;
    fontseg=__dpmi_allocate_dos_memory(256,&fontsel);if(fontseg>=0)loadfont(font);
}
void screen_init(void){struct text_info info;gettextinfo(&info);oldmode=info.currmode;textmode(C80);turkish_font();_setcursortype(_NOCURSOR);}
void screen_done(void){if(fontseg>=0){loadfont(oldfont);__dpmi_free_dos_memory(fontsel);}textmode(oldmode);clrscr();}
void screen_flush(int x,int y){puttext(1,1,80,25,(void*)cursor_frame(x,y));}
Key key_read(int f_layout,int wait){
    Key k={0};int raw,scan,ascii,mods,caps;
    if(!wait&&!bioskey(1))return k;
    raw=bioskey(0);mods=bioskey(2);scan=(raw>>8)&255;ascii=raw&255;caps=(mods&64)!=0;
    k.shift=(mods&3)!=0;k.ctrl=(mods&4)!=0;k.alt=(mods&8)!=0;k.code=special(scan,ascii);
    /* BIOS buffers ASCII with the scan code, but shift status is live. Recover
       released modifiers from that ASCII so rapid input keeps Ctrl and case. */
    if(k.code==K_CHAR){
        if(ascii>=1&&ascii<=26)k.ctrl=1;
        if(!k.ctrl){
            if(ascii>='a'&&ascii<='z')k.shift=caps;
            else if(ascii>='A'&&ascii<='Z')k.shift=!caps;
            else if(upper857(ascii)!=ascii)k.shift=caps;
            else if(lower857(ascii)!=ascii)k.shift=!caps;
            else{
                const char *plain="1234567890-=",*shifted="!@#$%^&*()_+";
                if(scan>=2&&scan<=13){if(ascii==plain[scan-2])k.shift=0;else if(ascii==shifted[scan-2])k.shift=1;}
                else if(scan==39&&(ascii==';'||ascii==':'))k.shift=ascii==':';
                else if(scan==40&&(ascii=='\''||ascii=='\"'))k.shift=ascii=='\"';
                else if(scan==51&&(ascii==','||ascii=='<'))k.shift=ascii=='<';
                else if(scan==52&&(ascii=='.'||ascii=='>'))k.shift=ascii=='>';
                else if(scan==53&&(ascii=='/'||ascii=='?'))k.shift=ascii=='?';
            }
            if(f_layout){
                int plain=f_key(scan,0,caps),shifted=f_key(scan,1,caps);
                if(plain!=shifted){if(ascii==plain)k.shift=0;else if(ascii==shifted)k.shift=1;}
            }
        }
        if(f_layout)k.ch=f_key(scan,k.shift,caps);
        else{k.ch=ascii;if(k.ctrl&&ascii>=1&&ascii<=26)k.ch=ascii+'a'-1;}
    }
    if(k.code==K_TAB)k.shift=ascii==0;
    if(k.code==K_ENTER&&ascii==10)k.ctrl=1;
    return k;
}
void idle(void){usleep(15000);}
void system_copy(const Doc *clip){(void)clip;}
int system_paste(Doc *clip){(void)clip;return 0;}
#else
/* The host build is for core tests; production UIs target DOS and Windows. */
void screen_init(void){}
void screen_done(void){}
void screen_flush(int x,int y){(void)x;(void)y;}
Key key_read(int f_layout,int wait){Key k={K_ESC,0,0,0,0};(void)f_layout;(void)wait;return k;}
void idle(void){}
void system_copy(const Doc *clip){(void)clip;}
int system_paste(Doc *clip){(void)clip;return 0;}
#endif
