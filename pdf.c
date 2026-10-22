#include "moviemst.h"
#include "fontdata.h"
#include <stdlib.h>
#include <string.h>

static void pdf_string(FILE *f,const char *text,int len){int i;fputc('<',f);for(i=0;i<len;i++)fprintf(f,"00%02X",(unsigned char)text[i]);fputc('>',f);}
static void pdf_text(FILE *f,int x,int y,const char *text,int len){fprintf(f,"BT /F1 12 Tf 1 0 0 1 %d.%d %d Tm ",x/10,x%10,y);pdf_string(f,text,len);fputs(" Tj ET\n",f);}
static void centered(FILE *f,int y,const char *text){int pos=0,len=(int)strlen(text);while(pos<len){int end=pos+60,next;if(end>len)end=len;next=end;if(end<len){int k=end;while(k>pos&&text[k]!=' ')k--;if(k>pos){end=k;next=k+1;}}pdf_text(f,3060-(end-pos)*36,y,text+pos,end-pos);pos=next;y-=16;}}
int export_pdf(const Doc *d,const char *path,const char *font,char *error){
    FILE *f,*ttf;Layout l;long *offset,fontlen,xref;int title=d->title[0]!=0,pages,objects,i,p,ok=1;unsigned char buffer[4096];
    ttf=file_open(font,"rb");if(!ttf){strcpy(error,"FONT.TTF is missing. Keep it beside the program.");return 0;}
    if(fseek(ttf,0,SEEK_END)||(fontlen=ftell(ttf))<=0||fseek(ttf,0,SEEK_SET)){fclose(ttf);strcpy(error,"Cannot read the font file.");return 0;}
    f=file_open(path,"wb");if(!f){fclose(ttf);strcpy(error,"Cannot create the PDF file.");return 0;}
    layout_build(d,&l);pages=l.pages+title;objects=8+pages*2;offset=mem((objects+1)*sizeof(long));
    fputs("%PDF-1.4\n%\342\343\317\323\n",f);
#define OBJECT(n) do { offset[(n)]=ftell(f);fprintf(f,"%d 0 obj\n",(n)); } while(0)
    OBJECT(1);fputs("<< /Type /Catalog /Pages 2 0 R >>\nendobj\n",f);
    OBJECT(2);fprintf(f,"<< /Type /Pages /Count %d /Kids [",pages);for(i=0;i<pages;i++)fprintf(f,"%d 0 R ",9+i*2);fputs("] >>\nendobj\n",f);
    OBJECT(3);fputs("<< /Type /Font /Subtype /Type0 /BaseFont /CourierPrime-Regular /Encoding /Identity-H /DescendantFonts [4 0 R] /ToUnicode 8 0 R >>\nendobj\n",f);
    OBJECT(4);fputs("<< /Type /Font /Subtype /CIDFontType2 /BaseFont /CourierPrime-Regular /CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> /FontDescriptor 5 0 R /DW 600 /CIDToGIDMap 6 0 R >>\nendobj\n",f);
    OBJECT(5);fputs("<< /Type /FontDescriptor /FontName /CourierPrime-Regular /Flags 33 /FontBBox [" FONT_BBOX "] /ItalicAngle 0 /Ascent 892 /Descent -257 /CapHeight 600 /StemV 80 /FontFile2 7 0 R >>\nendobj\n",f);
    OBJECT(6);fputs("<< /Length 512 >>\nstream\n",f);for(i=0;i<256;i++){fputc(font_gid[i]>>8,f);fputc(font_gid[i]&255,f);}fputs("\nendstream\nendobj\n",f);
    OBJECT(7);fprintf(f,"<< /Length %ld /Length1 %ld >>\nstream\n",fontlen,fontlen);{size_t n;while((n=fread(buffer,1,sizeof(buffer),ttf))>0)if(fwrite(buffer,1,n,f)!=n)ok=0;if(ferror(ttf))ok=0;}fclose(ttf);fputs("\nendstream\nendobj\n",f);
    /* Stream lengths use indirect objects only for page content; the CMap is staged in a portable temp stream. */
    {FILE *map=tmpfile();long len;if(!map){ok=0;}else{
        fputs("/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n/CMapName /MovieMaster857 def\n/CMapType 2 def\n1 begincodespacerange\n<0000> <00FF>\nendcodespacerange\n",map);
        for(p=0;p<256;p+=64){fprintf(map,"64 beginbfchar\n");for(i=p;i<p+64;i++)fprintf(map,"<%04X> <%04X>\n",i,unicode857[i]);fputs("endbfchar\n",map);}fputs("endcmap\nCMapName currentdict /CMap defineresource pop\nend\nend\n",map);
        len=ftell(map);rewind(map);OBJECT(8);fprintf(f,"<< /Length %ld >>\nstream\n",len);{size_t n;while((n=fread(buffer,1,sizeof(buffer),map))>0)fwrite(buffer,1,n,f);}fclose(map);fputs("endstream\nendobj\n",f);
    }}
    for(p=0;p<pages&&ok;p++){
        FILE *content=tmpfile();long len;if(!content){ok=0;break;}
        if(title&&p==0){centered(content,450,d->title);centered(content,370,d->author);}
        else{int page=p+1-title;char number[16];for(i=0;i<l.n;i++){Row *r=l.r+i;if(r->page==page&&r->p>=0){char part[60];int k;for(k=r->start;k<r->end;k++)part[k-r->start]=(char)element_char(d->p[r->p].type,(unsigned char)d->p[r->p].text[k]);pdf_text(content,1080+r->indent*72,720-r->line*12,part,r->end-r->start);if(d->numbering&&d->p[r->p].type==SCENE&&r->start==0){char label[16];scene_label(d,r->p,label);pdf_text(content,1008-(int)strlen(label)*72,720-r->line*12,label,(int)strlen(label));}}}snprintf(number,sizeof(number),"%d.",page);pdf_text(content,5300,756,number,(int)strlen(number));}
        len=ftell(content);if(ferror(content)||fflush(content)||fseek(content,0,SEEK_SET)){fclose(content);ok=0;break;}
        OBJECT(9+p*2);fprintf(f,"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents %d 0 R >>\nendobj\n",10+p*2);
        OBJECT(10+p*2);fprintf(f,"<< /Length %ld >>\nstream\n",len);{size_t n;while((n=fread(buffer,1,sizeof(buffer),content))>0)fwrite(buffer,1,n,f);}fclose(content);fputs("\nendstream\nendobj\n",f);
    }
    xref=ftell(f);fprintf(f,"xref\n0 %d\n0000000000 65535 f \n",objects+1);for(i=1;i<=objects;i++)fprintf(f,"%010ld 00000 n \n",offset[i]);fprintf(f,"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",objects+1,xref);
    free(offset);layout_free(&l);if(ferror(f)||fflush(f))ok=0;if(fclose(f))ok=0;
    if(!ok){file_remove(path);strcpy(error,"PDF export failed.");return 0;}strcpy(error,"PDF exported with embedded Courier Prime.");return 1;
#undef OBJECT
}
