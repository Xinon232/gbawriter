#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../references/superfw/src/utf_util.c"
#include "../references/superfw/src/fonts/font_render.c"
void *font_base_addr;
void *reader_font_base_addr;
static void *load(const char *path) {
    FILE *f=fopen(path,"rb"); assert(f);
    assert(!fseek(f,0,SEEK_END)); long n=ftell(f); assert(n>0);
    rewind(f); void *p=calloc((size_t)n+1,1); assert(p);
    assert(fread(p,1,(size_t)n,f)==(size_t)n); assert(!fclose(f)); return p;
}
int main(int argc, char** argv) {
    assert(argc==4);
    font_base_addr=load(argv[1]);
    reader_font_base_addr=load(argv[2]);
    const char *required="áäàâãåæçčćéèëêíïìîñńóöôòõøœßšśüúùûýÿžźżÁÄÀÂÃÅÆÇČĆÉÈËÊÍÏÌÎÑŃÓÖÔÒÕØŒŠŚÜÚÙÛÝŸŽŹŻ";
    char *help=load(argv[3]);
    for(char *s=help;*s;s+=utf8_chlen(s)) {
        if(*s=='\n')continue;
        t_char_render_info info;
        if(!lookup_chptr(utf8_decode(s),&info)){fprintf(stderr,"Missing help glyph U+%04x\n",utf8_decode(s));abort();}
    }
    for(char *s=strtok(help,"\n");s;s=strtok(NULL,"\n"))assert(font_width(s)<=224);
    free(help);
    unsigned tested=0;
    unsigned char sheet[240*128]; memset(sheet,255,sizeof(sheet));
    for(const char *s=required;*s;s+=utf8_chlen(s),++tested) {
        uint32_t cp=utf8_decode(s); t_char_render_info info;
        assert(lookup_chptr(cp,&info)); assert(info.char_width>0 && info.char_width<=16);
        char one[5]={0}; memcpy(one,s,utf8_chlen(s));
        assert(font_width(one)==info.char_width+info.spacing_cols);
        for(unsigned odd=0;odd<2;++odd) {
            unsigned char memory[32*18]; memset(memory,0,sizeof(memory));
            memset(memory,0xa5,32); memset(memory+32*17,0xa5,32);
            unsigned char *pixels=memory+32+odd;
            draw_text_idx8_bus16_range(one,pixels,0,info.char_width+info.spacing_cols,32,1);
            unsigned count=0;
            for(unsigned y=0;y<18;++y) for(unsigned x=0;x<32;++x) {
                unsigned char v=memory[y*32+x];
                if(y==0||y==17) assert(v==0xa5);
                else if(x<odd||x>=odd+info.char_width) assert(v==0);
                else {assert(v<=1);count+=v;}
            }
            assert(count>0);
        }
        unsigned x=(tested%15)*16, y=(tested/15)*20;
        assert(y+16<=128);
        draw_text_idx8_bus16_range(one,sheet+y*240+x,0,16,240,0);
    }
    FILE *out=fopen("glyph-runtime.pgm","wb");assert(out);
    assert(fprintf(out,"P5\n240 128\n255\n")>0);
    assert(fwrite(sheet,1,sizeof(sheet),out)==sizeof(sheet)); assert(!fclose(out));
    printf("PASS: %u required lowercase/uppercase glyphs resolved and rendered nonblank at even/odd offsets with bounds checks\n",tested);
    free(font_base_addr);free(reader_font_base_addr);
}
