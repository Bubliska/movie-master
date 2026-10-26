#include "moviemst.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#ifdef __DJGPP__
#include <fcntl.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <wchar.h>
static WCHAR *wide_path(const char *s){
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0);
    WCHAR *w;
    if(!n){errno=EINVAL;return NULL;}
    w=mem(n*sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n);
    return w;
}
static char *utf8_path(const WCHAR *w){
    int n=WideCharToMultiByte(CP_UTF8,0,w,-1,NULL,0,NULL,NULL);
    char *s=mem(n);WideCharToMultiByte(CP_UTF8,0,w,-1,s,n,NULL,NULL);return s;
}
FILE *file_open(const char *path,const char *mode){
    WCHAR *w=wide_path(path),m[16];FILE *f;size_t i;
    if(!w)return NULL;
    for(i=0;mode[i]&&i<15;i++)m[i]=(unsigned char)mode[i];m[i]=0;
    f=_wfopen(w,m);free(w);return f;
}
int file_remove(const char *path){WCHAR *w=wide_path(path);int n;if(!w)return -1;n=_wremove(w);free(w);return n;}
int file_rename(const char *from,const char *to){WCHAR *a=wide_path(from),*b=wide_path(to);int n;if(!a||!b){free(a);free(b);return -1;}n=_wrename(a,b);free(a);free(b);return n;}
int path_type(const char *path){
    WCHAR *w=wide_path(path);DWORD flags,err;
    if(!w)return -1;
    flags=GetFileAttributesW(w);err=GetLastError();free(w);
    if(flags==INVALID_FILE_ATTRIBUTES){errno=(err==ERROR_FILE_NOT_FOUND||err==ERROR_PATH_NOT_FOUND)?ENOENT:EACCES;return -1;}
    return flags&FILE_ATTRIBUTE_DIRECTORY?PATH_DIR:PATH_FILE;
}
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
FILE *file_open(const char *path,const char *mode){return fopen(path,mode);}
int file_remove(const char *path){return remove(path);}
int file_rename(const char *from,const char *to){return rename(from,to);}
int path_type(const char *path){struct stat st;if(stat(path,&st))return -1;return S_ISDIR(st.st_mode)?PATH_DIR:S_ISREG(st.st_mode)?PATH_FILE:PATH_OTHER;}
#endif

int path_join(char *out,const char *folder,const char *name){
    size_t n=strlen(folder),m=strlen(name);int slash=n&&folder[n-1]!='/'&&folder[n-1]!='\\';
    if(n+m+slash>=512)return 0;
    memmove(out,folder,n);if(slash)out[n++]='/';memmove(out+n,name,m+1);return 1;
}
void path_split(const char *path,char *folder,char *name){
    const char *base=path+strlen(path);size_t n;
    while(base>path&&base[-1]!='/'&&base[-1]!='\\')base--;
    strcpy(name,base);n=(size_t)(base-path);
    while(n>1&&path[n-1]=='/'&&!(n==3&&path[1]==':'))n--;
    while(n>1&&path[n-1]=='\\'&&!(n==3&&path[1]==':'))n--;
    memcpy(folder,path,n);folder[n]=0;
}
void path_parent(char *path){
    size_t n=strlen(path),root=(n>=2&&path[1]==':')?3:1;
    while(n>root&&(path[n-1]=='/'||path[n-1]=='\\'))path[--n]=0;
    while(n>root&&path[n-1]!='/'&&path[n-1]!='\\')path[--n]=0;
    if(n>root)path[--n]=0;
}
void path_display(const char *path,char *out,size_t cap){
    if(!cap)return;
#ifdef __DJGPP__
    strncpy(out,path,cap-1);out[cap-1]=0;
#else
    /* Filenames remain UTF-8 for I/O. Display only is limited to DOS glyphs. */
    const unsigned char *p=(const unsigned char*)path;size_t n=0;
    while(*p&&n+1<cap){unsigned int c=*p++;int k=0,v;
        if(c>=0xc2&&c<=0xdf){c&=31;k=1;}else if(c>=0xe0&&c<=0xef){c&=15;k=2;}else if(c>=0xf0&&c<=0xf4){c&=7;k=3;}else if(c>=128)c='?';
        while(k--){if((*p&192)!=128){c='?';break;}c=(c<<6)|(*p++&63);}
        v=from_unicode(c);out[n++]=(char)(v>=32?v:'?');
    }out[n]=0;
#endif
}
void desktop_path(char *out){
#ifdef _WIN32
    WCHAR path[MAX_PATH];
    if(SUCCEEDED(SHGetFolderPathW(NULL,CSIDL_DESKTOPDIRECTORY,NULL,SHGFP_TYPE_CURRENT,path))){char *u=utf8_path(path);if(strlen(u)<512){strcpy(out,u);free(u);return;}free(u);}
    strcpy(out,"C:/");
#elif defined(__DJGPP__)
    const char *custom=getenv("WRDESK");
    if(custom&&strlen(custom)<512&&path_type(custom)==PATH_DIR){strcpy(out,custom);return;}
    if(path_type("D:/")==PATH_DIR){strcpy(out,"D:/");return;}
    if(!getcwd(out,512))strcpy(out,"C:/");
#else
    const char *base=getenv("HOME");
    if(base&&path_join(out,base,"Desktop")&&path_type(out)==PATH_DIR)return;
    if(!getcwd(out,512))strcpy(out,"/");
#endif
}
static int ascii_casecmp(const char *a,const char *b){
    while(*a&&*b){int c=tolower((unsigned char)*a)-tolower((unsigned char)*b);if(c)return c;a++;b++;}
    return (unsigned char)*a-(unsigned char)*b;
}
int destination_path(char *out,const char *folder,const char *name,const char *extension,char *error){
    char filename[512];const char *ext;size_t n=strlen(name),i;
    if(!n||n>=sizeof(filename)-5||name[n-1]==' '||name[n-1]=='.'){strcpy(error,"Enter a file name, for example SCRIPT.SEN.");return 0;}
    for(i=0;i<n;i++)if((unsigned char)name[i]<32||strchr("/\\:*?<>|\"",name[i])){strcpy(error,"Enter only a file name. Browse folders or use F3 for a full path.");return 0;}
    strcpy(filename,name);ext=strrchr(filename,'.');
    if(ext&&ascii_casecmp(ext,extension)){snprintf(error,256,"Use the %s file extension.",extension);return 0;}
    if(!ext){strcat(filename,extension);ext=strrchr(filename,'.');}
    if(ext==filename){strcpy(error,"Enter a name before the file extension.");return 0;}
    if(!ascii_casecmp(filename,"KURTAR.SEN")){strcpy(error,"KURTAR.SEN is reserved for automatic recovery.");return 0;}
    {size_t base=strcspn(filename,".");char device[16];if(base<sizeof(device)){memcpy(device,filename,base);device[base]=0;for(i=0;i<base;i++)device[i]=(char)toupper((unsigned char)device[i]);
        if(!ascii_casecmp(device,"CON")||!ascii_casecmp(device,"PRN")||!ascii_casecmp(device,"AUX")||!ascii_casecmp(device,"NUL")||!ascii_casecmp(device,"CONIN$")||!ascii_casecmp(device,"CONOUT$")||
           (base==4&&(!strncmp(device,"COM",3)||!strncmp(device,"LPT",3))&&device[3]>='1'&&device[3]<='9')){strcpy(error,"That name is reserved by DOS/Windows. Choose another name.");return 0;}
    }}
#ifdef __DJGPP__
    if(!_use_lfn(folder)){
        if(ext-filename>8||strlen(ext)>4){strcpy(error,"This drive supports only DOS 8.3 names, for example SCRIPT.SEN.");return 0;}
        for(i=0;filename[i];i++){if(strchr(" +,;=[]",filename[i])||(filename[i]=='.'&&filename+i!=ext)){strcpy(error,"Use a short DOS file name without spaces or extra dots.");return 0;}filename[i]=(char)toupper((unsigned char)filename[i]);}
    }
#endif
    if(path_type(folder)!=PATH_DIR){strcpy(error,"The destination folder does not exist or cannot be read.");return 0;}
    if(!path_join(out,folder,filename)){strcpy(error,"File path is too long.");return 0;}
    return 1;
}
int file_matches(const char *name,int filter){
    const char *ext=strrchr(name,'.');
    if(filter==2)return 1;
    if(!ext)return 0;
    if(filter>=3&&filter<=6){const char *extensions[]={".sen",".pdf",".txt",".ftn"};return !ascii_casecmp(ext,extensions[filter-3]);}
    if(filter==0)return !ascii_casecmp(ext,".sen")||!ascii_casecmp(ext,".bak");
    return !ascii_casecmp(ext,".fountain")||!ascii_casecmp(ext,".ftn")||!ascii_casecmp(ext,".fou")||!ascii_casecmp(ext,".txt");
}
void files_free(FileEntry *entries,int count){int i;for(i=0;i<count;i++)free(entries[i].name);free(entries);}
static int add_entry(FileEntry **entries,int *count,int *capacity,const char *name,int directory,int filter){
    if(!strcmp(name,".")||!strcmp(name,"..")||(!directory&&!file_matches(name,filter)))return 1;
    if(*count==8192)return 0;
    if(*count==*capacity){int cap=*capacity?*capacity*2:32;FileEntry *next=mem(cap*sizeof(FileEntry));if(*count)memcpy(next,*entries,*count*sizeof(FileEntry));free(*entries);*entries=next;*capacity=cap;}
    (*entries)[*count].name=dupstr(name);(*entries)[(*count)++].directory=directory;return 1;
}
static int entry_compare(const void *left,const void *right){const FileEntry *a=left,*b=right;if(a->directory!=b->directory)return b->directory-a->directory;return ascii_casecmp(a->name,b->name);}
int files_list(const char *path,FileEntry **entries,int *count,int filter,char *error){
    FileEntry *list=NULL;int n=0,capacity=0,ok=1;
#ifdef _WIN32
    char pattern[512];WCHAR *w;WIN32_FIND_DATAW item;HANDLE handle;
    if(!path_join(pattern,path,"*")){strcpy(error,"Folder path is too long.");return 0;}
    w=wide_path(pattern);if(!w){strcpy(error,"Invalid folder path.");return 0;}
    handle=FindFirstFileW(w,&item);free(w);
    if(handle==INVALID_HANDLE_VALUE){if(GetLastError()!=ERROR_FILE_NOT_FOUND){strcpy(error,"Cannot read this folder.");return 0;}}
    else{do{char *name=utf8_path(item.cFileName);ok=add_entry(&list,&n,&capacity,name,(item.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0,filter);free(name);if(!ok)break;}while(FindNextFileW(handle,&item));if(ok&&GetLastError()!=ERROR_NO_MORE_FILES)ok=0;FindClose(handle);}
#else
    DIR *dir=opendir(path);struct dirent *entry;
    if(!dir){strcpy(error,"Cannot read this folder.");return 0;}
    while((entry=readdir(dir))!=NULL){char full[512];int type;if(!path_join(full,path,entry->d_name))continue;type=path_type(full);if(type<0)continue;if(!add_entry(&list,&n,&capacity,entry->d_name,type==PATH_DIR,filter)){ok=0;break;}}
    closedir(dir);
#endif
    if(!ok){files_free(list,n);strcpy(error,"Folder is unreadable or has too many entries (limit 8192).");return 0;}
    if(n>1)qsort(list,n,sizeof(FileEntry),entry_compare);
    *entries=list;*count=n;return 1;
}
int drive_roots(char roots[26][4]){
    int n=0,i;
#ifdef _WIN32
    DWORD drives=GetLogicalDrives();for(i=0;i<26;i++)if(drives&(1UL<<i)){roots[n][0]=(char)('A'+i);strcpy(roots[n++]+1,":/");}
#elif defined(__DJGPP__)
    for(i=2;i<26;i++){char path[4]={(char)('A'+i),':','/',0};if(path_type(path)==PATH_DIR)strcpy(roots[n++],path);}
#else
    (void)i;strcpy(roots[n++],"/");
#endif
    return n;
}
