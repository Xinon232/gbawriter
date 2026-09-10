#include "writer_core.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace writer;
int main() {
  const unsigned groups[]={1,65,8,72,2,66,4,68};
  const char* letters[]={"abc","def","hij","klm","nop","qrs","tuw","xyz"};
  const unsigned buttons[]={32,16,128};
  for(unsigned g=0;g<8;++g) for(unsigned b=0;b<3;++b) for(int mode=0;mode<3;++mode) {
    InputState s;
    auto sink=[](void*,InputEvent){};
    if(mode){s.update(128,sink,nullptr);if(mode==2)for(int i=0;i<48;++i)s.update(128,sink,nullptr);s.update(0,sink,nullptr);}
    s.update(groups[g],sink,nullptr);
    char expected_group[4];std::strcpy(expected_group,letters[g]);
    if(mode)for(int i=0;i<3;++i)expected_group[i]-=32;
    assert(!std::strcmp(s.active_group(),expected_group));
    struct Result{char text[8]={};EventKind kind=EventKind::NONE;} result;
    s.update(groups[g]|buttons[b],[](void* p,InputEvent e){auto& r=*static_cast<Result*>(p);r.kind=e.kind;std::strcpy(r.text,e.text);},&result);
    char expected[]={char(letters[g][b]-(mode?32:0)),0};
    if(result.kind!=EventKind::INSERT || std::strcmp(result.text,expected)) {
      std::fprintf(stderr,"group=%u button=%u mode=%d expected=%s actual=%s\n",groups[g],buttons[b],mode,expected,result.text);return 1;
    }
  }
  puts("PASS V1.2 exact 8-group mapping, all selectors and case modes");
}
