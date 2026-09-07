#include "writer_format.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
 char out[80];
 assert(writer::format(out,sizeof out,"%c DAY %02d",'>',7)==8);
 assert(!strcmp(out,"> DAY 07"));
 writer::format(out,sizeof out,"%s/%s","/gbawriter","é.txt");
 assert(!strcmp(out,"/gbawriter/é.txt"));
 writer::format(out,sizeof out,"GWW1 %u %08lx\n",24576u,0x123abul);
 assert(!strcmp(out,"GWW1 24576 000123ab\n"));
 unsigned size=0; uint32_t hash=0;
 assert(writer::parse_manifest(out,strlen(out),size,hash));
 assert(size==24576 && hash==0x123ab);
 assert(writer::parse_manifest("GWW1 0 00000000\n",16,size,hash) && size==0 && hash==0);
 for(const char* bad:{"GWW1 1 00000000\nextra","GWW1 -1 00000000\n","GWW1 4294967296 00000000\n","GWW1 1 100000000\n","GWW1 1 0000000z\n","GWW1 1 00000000"})
   assert(!writer::parse_manifest(bad,strlen(bad),size,hash));
 assert(writer::format(out,4,"%s","hello")==5 && !strcmp(out,"hel"));
 assert(writer::format(nullptr,0,"%04d",2026)==4);
 writer::format(out,sizeof out,"%d",-2147483647-1);
 assert(!strcmp(out,"-2147483648"));
 std::cout<<"PASS: bounded writer formatting and strict save manifest\n";
}
