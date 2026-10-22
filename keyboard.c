#include "moviemst.h"
#include <string.h>

/* PC scan-code rows. US/UK/Dvorak and standard Latin national layouts.
   Reference: https://gitlab.freedesktop.org/xkeyboard-config/xkeyboard-config/-/tree/master/symbols
   Spaces in AltGr rows mean no third-level character. */
int keyboard_layout=KEY_TR_F;
const char *keyboard_names[KEY_COUNT]={"System keyboard","English - US","English - UK","English - Dvorak","Turkish - Q","Turkish - F","Spanish","French - AZERTY","German - QWERTZ","Italian","Portuguese","Portuguese - Brazil"};
const char *keyboard_tags[KEY_COUNT]={"SYS","US","UK","DV","TRQ","TRF","ES","FR","DE","IT","PT","BR"};
typedef struct {const char *plain[4],*shift[4],*altgr[4];const char *iso;} Keys;
static const Keys layouts[KEY_COUNT]={
    { {0},{0},{0},0 },
    {{"`1234567890-=","qwertyuiop[]","asdfghjkl;'","\\zxcvbnm,./"},{"~!@#$%^&*()_+","QWERTYUIOP{}","ASDFGHJKL:\"","|ZXCVBNM<>?"},{0},"\\|"},
    {{"`1234567890-=","qwertyuiop[]","asdfghjkl;'","#zxcvbnm,./"},{"¬!\"£$%^&*()_+","QWERTYUIOP{}","ASDFGHJKL:@","~ZXCVBNM<>?"},{0},"\\|"},
    {{"`1234567890[]","',.pyfgcrl/=","aoeuidhtns-","\\;qjkxbmwvz"},{"~!@#$%^&*(){}","\"<>PYFGCRL?+","AOEUIDHTNS_","|:QJKXBMWVZ"},{0},"\\|"},
    {{"\"1234567890*-","qwertyuıopğü","asdfghjklşi",",zxcvbnmöç."},{"é!'^+%&/()=?_","QWERTYUIOPĞÜ","ASDFGHJKLŞİ",";ZXCVBNMÖÇ:"},{"<>£#$  {[]}\\|","@          ~","         ´'","`          "},"<>"},
    {{"+1234567890/-","fgğıodrnhpqw","uieaütkmlyş","xjövcçzsb.,"},{"*!\"^$%&'()=?_","FGĞIODRNHPQW","UİEAÜTKMLYŞ","XJÖVCÇZSB:;"},{"¬¹²#¼  {[]}\\|","@    ¥®  £¨~","      µ  ´#","`«» ¢   ×÷·"},"<>"},
    {{"º1234567890'¡","qwertyuiop`+","asdfghjklñ´","çzxcvbnm,.-"},{"ª!\"·$%&/()=?¿","QWERTYUIOP^*","ASDFGHJKLÑ¨","ÇZXCVBNM;:_"},{"\\|@#~    {}\\ ","          []","          {","}          "},"<>"},
    {{"²&é\"'(-è_çà)=","azertyuiop^$","qsdfghjklmù","*wxcvbn,;:!"},{"²1234567890°+","AZERTYUIOP¨£","QSDFGHJKLM%","µWXCVBN?./§"},{"  ~#{[|`\\^@]}","           ¤","           ","           "},"<>"},
    {{"^1234567890ß´","qwertzuiopü+","asdfghjklöä","#yxcvbnm,.-"},{"°!\"§$%&/()=?`","QWERTZUIOPÜ*","ASDFGHJKLÖÄ","'YXCVBNM;:_"},{"  ²³   {[ ]}\\","@          ~","           ","           "},"<>"},
    {{"\\1234567890'ì","qwertyuiopè+","asdfghjklòà","ùzxcvbnm,.-"},{"|!\"£$%&/()=?^","QWERTYUIOPé*","ASDFGHJKLç°","§ZXCVBNM;:_"},{"             ","          []","         @#","           "},"<>"},
    {{"\\1234567890'«","qwertyuiop+´","asdfghjklçº","~zxcvbnm,.-"},{"|!\"#$%&/()=?»","QWERTYUIOP*`","ASDFGHJKLÇª","^ZXCVBNM;:_"},{"   £§   {[ ]}","          ¨ ","           ","           "},"<>"},
    {{"'1234567890-=","qwertyuiop´[","asdfghjklç~","]zxcvbnm,.;"},{"\"!@#$%¨&*()_+","QWERTYUIOP`{","ASDFGHJKLÇ^","}ZXCVBNM<>:"},{" ¹²³£¢¬     §","           ª","           ","º          "},"\\|"}
};
static int accent,queued;
void keyboard_reset(void){accent=queued=0;}
int keyboard_pending(void){int c=queued;queued=0;return c;}
static unsigned int glyph(const char *s,int at){
    if(!s)return 0;
    while(*s){unsigned int c=(unsigned char)*s++;int n=0;if(c>=0xc0){n=c<0xe0?1:2;c&=n==1?31:15;}while(n--)c=(c<<6)|((unsigned char)*s++&63);if(!at--)return c;}
    return 0;
}
int letter_case(int c,int upper){
    unsigned int u=unicode857[(unsigned char)c],v=u;int result,turkish=keyboard_layout==KEY_TR_F||keyboard_layout==KEY_TR_Q;
    if(upper){if(u=='i'&&turkish)v=0x130;else if(u>='a'&&u<='z')v=u-32;else if((u>=0xe0&&u<=0xfe&&u!=0xf7))v=u-32;else if(u==0x131)v='I';else if(u==0x11f||u==0x15f)v=u-1;}
    else{if(u=='I'&&turkish)v=0x131;else if(u>='A'&&u<='Z')v=u+32;else if((u>=0xc0&&u<=0xde&&u!=0xd7))v=u+32;else if(u==0x130)v='i';else if(u==0x11e||u==0x15e)v=u+1;}
    result=from_unicode(v);return result>=32?result:c;
}
int keyboard_key(int layout,int scan,int shift,int caps,int altgr,int compose){
    const Keys *k;int row,at,c;unsigned int u;
    if(layout<=0||layout>=KEY_COUNT)return 0;
    k=&layouts[layout];
    if(scan==57)u=' ';
    else if(scan==86)u=altgr?'|':glyph(k->iso,shift!=0);
    else{
        if(scan==41){row=0;at=0;}else if(scan>=2&&scan<=13){row=0;at=scan-1;}
        else if(scan>=16&&scan<=27){row=1;at=scan-16;}
        else if(scan>=30&&scan<=40){row=2;at=scan-30;}
        else if(scan>=43&&scan<=53){row=3;at=scan-43;}else return 0;
        u=glyph(altgr?k->altgr[row]:shift?k->shift[row]:k->plain[row],at);
        if(altgr&&u==' ')return 0;
    }
    if(!u)return 0;
    c=from_unicode(u);if(c<32)return -1;
    if(caps&&!altgr){int upper=letter_case(c,1),lower=letter_case(c,0);if(upper!=lower)c=shift?lower:upper;}
    if(!compose)return c;
    if(accent){
        static const char *bases="aAeEiIoOuUyYnNcC";
        static const struct {unsigned int mark;const char *letters;} compositions[]={
            {0xb4,"áÁéÉíÍóÓúÚýÝ  ćĆ"},{'`',"àÀèÈìÌòÒùÙ      "},{'^',"âÂêÊîÎôÔûÛ      "},
            {0xa8,"äÄëËïÏöÖüÜÿ     "},{'~',"ãÃ    õÕ    ñÑ  "}
        };
        int mark=accent;size_t i;const char *base=strchr(bases,c);accent=0;
        if(c==' '||unicode857[c]==(unsigned int)mark)return from_unicode(mark);
        if(base)for(i=0;i<sizeof(compositions)/sizeof(*compositions);i++)if(compositions[i].mark==(unsigned int)mark){unsigned int combined=glyph(compositions[i].letters,(int)(base-bases));int mapped=from_unicode(combined);if(combined!=' '&&mapped>=32)return mapped;}
        queued=c;return from_unicode(mark);
    }
    if(layout>=KEY_ES&&(u=='^'||u=='`'||u=='~'||u==0xb4||u==0xa8)){accent=(int)u;return 0;}
    return c;
}
