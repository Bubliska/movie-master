#ifndef SCREEN_H
#define SCREEN_H
enum { K_NONE=0,K_CHAR=256,K_LEFT,K_RIGHT,K_UP,K_DOWN,K_HOME,K_END,K_PGUP,K_PGDN,K_DEL,K_BACK,K_ENTER,K_TAB,K_ESC,K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10 };
typedef struct { int code,ch,shift,ctrl,alt; } Key;
void screen_init(void);
void screen_done(void);
void cell(int x,int y,int ch,int attr);
void screen_clear(int attr);
void screen_flush(int x,int y);
Key key_read(int f_layout,int wait);
void idle(void);
void system_copy(const Doc *clip);
int system_paste(Doc *clip);
#endif
