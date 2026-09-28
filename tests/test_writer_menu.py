#!/usr/bin/env python3
"""Exercise the production menu render switch with recording font backends.
The real bitmap editor is separately pixel-tested; exact-ROM QA covers Butano.
"""
from pathlib import Path
import subprocess
import tempfile
import re
root = Path(__file__).resolve().parents[1]
main = (root / 'src/main.cpp').read_text()
helpers = main.split('void title(', 1)[1].split('void render(', 1)[0]
body = main.split('using writer::Scene;', 1)[1].split('painter.flip_page_later();', 1)[0]
# The editor has its own production framebuffer test, not this recording backend.
body = body.split('case Scene::EDITOR:{', 1)[0] + 'default: break;\n}\n'
source = '\nconst int cursor_widths[] = {' + ','.join(re.findall(r'^\s*(\d+),', (root/'include/common_variable_8x16_sprite_font.h').read_text().split('character_widths[] = {')[1].split('};')[0], re.M)) + '};\n' + r'''
#include "writer_app.h"
#include "writer_format.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <unistd.h>
#include <fstream>
#include <iterator>
extern "C" {
#include "font_render.h"
void *font_base_addr;
void *reader_font_base_addr;
}
int glyph_width(const char* s){return int(font_width(s));}
struct Draw { std::string text; bool ui; int x,y; std::string font; };
std::vector<Draw> draws;
using Sprites=int;
namespace bn { struct sprite_text_generator {
 // gbamp3 5x7 interface font: 6 px per character; the Butano cursor font keeps its widths.
 const char* font; bool butano=false; int alignment=0;
 void set_center_alignment(){alignment=0;}
 void set_left_alignment(){alignment=1;}
 void generate(int x,int y,const char* s,Sprites&){int width=0;for(auto p=s;*p;++p){assert(*p>=32&&*p<=126);width+=butano?cursor_widths[*p-32]:6;}int left=x+120-(alignment?0:width/2);assert(left>=0&&left+width<=240&&y+72>=0&&y+88<=160);draws.push_back({s,true,left,y+72,font});}
}; }
void line(uint8_t*,int x,int y,const char* s,int max=224){
 assert(x>=0&&y>=0&&y+16<=160&&x+glyph_width(s)<=240&&glyph_width(s)<=max);
 alignas(2) uint8_t pixels[240*160]={};draw_text_idx8_bus16_range(s,pixels+y*240+x,0,max,240,1);
 bool ink=false;for(int py=0;py<160;++py)for(int px=0;px<240;++px)if(pixels[py*240+px]){ink=true;assert(py>=y&&py<y+16&&px>=x&&px<x+glyph_width(s));}
 assert(ink);draws.push_back({s,false,x,y,"superfw"});
}
'''
source += 'void title(' + helpers
source += '\nvoid render(writer::Application& app, writer::Storage& storage){ uint8_t* px=nullptr; bn::sprite_text_generator ui{"ui"},hint{"hint"},cursor{"cursor",true}; Sprites sprites=0; char buffer[writer::FILE_NAME_SIZE+2]; using writer::Scene;\n' + body + '\n}\n'
source += r'''
void check(const char* text,bool ui){bool found=false;for(auto& d:draws)if(d.text==text){found=true;assert(d.ui==ui);}assert(found && "missing required text");}
// The blue Butano font draws only the ">" cursor; key hints are grey.
void check_font(const char* text,const char* font){bool found=false;for(auto& d:draws)if(d.text==text){found=true;assert(d.font==font);}assert(found);
 for(auto& d:draws)if(d.ui)assert((d.font=="cursor")==(d.text==">"));}
Draw cursor(){int n=0;Draw c{};for(auto& d:draws)if(d.font=="cursor"){c=d;++n;}assert(n==1);return c;}
void only_ui(){for(auto& d:draws)assert(d.ui);}
int main(int argc,char** argv){
 assert(argc==3);
 std::ifstream f(argv[1],std::ios::binary),r(argv[2],std::ios::binary);
 std::vector<char> fonts((std::istreambuf_iterator<char>(f)),{}),symbols((std::istreambuf_iterator<char>(r)),{});
 assert(!fonts.empty()&&!symbols.empty());font_base_addr=fonts.data();reader_font_base_addr=symbols.data();
 using namespace writer;
 std::string root="/tmp/writer-menu-"+std::to_string(getpid());
 std::filesystem::create_directory(root); Storage storage(root.c_str());
 Application app(storage,[](const char*){return 6;}); app.boot();
 auto tap=[&](Button b){app.frame(1u<<unsigned(b));app.frame(0);};
 auto draw=[&](){draws.clear();render(app,storage);};
 draw(); check("gbawriter",true);check("files: /gbawriter",true);check("NEW FILE",true);check("LOAD FILE",true);check("Select: Controls",true);check("Start: Credits",true);only_ui();
 for(auto& d:draws)assert(d.text.find("V3.0")==std::string::npos); // Home never shows a version.
 check_font("NEW FILE","ui");check_font("Select: Controls","hint");check_font(">","cursor");
 Draw item{};for(auto& d:draws)if(d.text=="NEW FILE")item=d;assert(cursor().y==item.y&&cursor().x<item.x);
 Draw select{},start{};for(auto& d:draws){if(d.text=="Select: Controls")select=d;if(d.text=="Start: Credits")start=d;}
 assert(select.y==start.y && select.y>=136 && select.x<start.x);
 tap(Button::START);draw();
 for(auto s:{"Made by Halim Jarrar","(C) 2026","halim-jarrar.de","monday@halim-jarrar.de"})check(s,false);
 const char* personal[]={"Made by Halim Jarrar","(C) 2026","halim-jarrar.de","monday@halim-jarrar.de"};
 int personal_index=0;for(auto& d:draws)if(!d.ui){assert(personal_index<4);assert(d.text==personal[personal_index++]);}
 assert(personal_index==4);
 for(auto& d:draws)assert(d.text.find("V3.0")==std::string::npos); // Author-only first page.
 tap(Button::RIGHT);draw();
 check("gbawriter V3.0",true);
 for(auto s:{"SuperFW software font renderer","UNSCII fonts: viznut.fi/unscii","UNSCII source: GPL license","Unifont-derived Hangul blocks","Font notices kept in source","UI font: gbamp3 5x7 font"})check(s,false);
 check("Left/Right: Page  B: Back",true);
 tap(Button::LEFT);draw();for(auto s:personal)check(s,false);
 tap(Button::LEFT);draw();check("SuperFW software font renderer",false);
 tap(Button::B);assert(app.scene()==Scene::MENU);
 tap(Button::START);draw();for(auto s:personal)check(s,false); // reopening resets first page
 tap(Button::B);tap(Button::SELECT);
 assert(std::string(help[0][1])=="Create and edit TXT files.");
 assert(std::string(help[0][2])=="Save directly to SD.");
 assert(std::string(help[0][3])=="Put TXT files in SD root folder:");
 assert(std::string(help[0][4])=="/gbawriter");
 for(int page=0;page<Application::HELP_PAGES;++page){draw();check(help[page][0],true);for(int row=1;row<6;++row)check(help[page][row],false);check("Left/Right: Page  B: Back",true);tap(Button::RIGHT);}
 bool repeat=false,open=false;for(auto& page:help)for(auto s:page){repeat|=std::string(s)=="Hold A/B alone: repeat edit";open|=std::string(s)=="UP/DOWN: select  A: open";}assert(repeat&&open);
 tap(Button::B);tap(Button::DOWN);tap(Button::A);draw();check("NO TXT FILES",true);only_ui();
 tap(Button::B);tap(Button::UP);tap(Button::A);draw();
 check("DAY",true);check("MONTH",true);check("YEAR",true);check_font(">","cursor");
 check("10",false);check("07",false);check("2026",false);
 check_font("UP/DOWN: FIELD  LEFT/RIGHT: +/-","hint");check_font("A: CREATE   B: BACK","hint");
 for(int field=0;field<3;++field){
  draw();const char* values[]={"10","07","2026"};
  assert(cursor().y==34+field*20);
  for(int row=0;row<3;++row){bool found=false;
   for(auto& d:draws){if(d.text==values[row]){assert(!d.ui&&d.x==DATE_VALUE_X&&d.y==34+row*20);found=true;}}
   assert(found);
  }tap(Button::DOWN);
 }
 // Create a real host file, return to menu, inspect filename versus UI marker.
 tap(Button::A);app.frame((1u<<unsigned(Button::START))|(1u<<unsigned(Button::B)));app.frame(0);
 tap(Button::DOWN);tap(Button::A);draw();check("10072026.txt",false);check_font(">","cursor");check_font("UP/DOWN  A: OPEN  B: BACK","hint");
 {Draw name{};for(auto& d:draws)if(d.text=="10072026.txt")name=d;assert(cursor().y==name.y&&cursor().x<name.x);}
 tap(Button::B);tap(Button::UP);tap(Button::A);tap(Button::LEFT);tap(Button::A);draw();
 check("FILE ALREADY EXISTS",true);check("10072026.txt",false);check("CHOOSE ANOTHER DATE",true);check("A: OK",true);
 std::filesystem::remove_all(root);
}
'''
with tempfile.TemporaryDirectory(prefix='writer-menu-') as directory:
    out = Path(directory)
    (out / 'menu.cpp').write_text(source)
    includes = ['-I'+str(root/p) for p in ('include','references/superfw/src','references/superfw/src/fonts')]
    flags = ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie']
    subprocess.run(['gcc','-std=c11',*flags,*includes,'-Wno-discarded-qualifiers','-c',str(root/'src/superfw_font.c'),'-o',str(out/'font.o')],check=True)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-no-pie',*includes,str(out/'menu.cpp'),*[str(root/'src'/('writer_'+s+'.cpp')) for s in ('core','layout','app','storage','format')],str(out/'font.o'),'-o',str(out/'menu')],check=True)
    subprocess.run([str(out/'menu'),str(root/'references/superfw/res/fonts.pack'),str(root/'references/superfw/res/reader-symbols.pack')],check=True)
# Menu SD probing uses interface typography; actual save warning stays bitmap.
assert 'if(checking)ui_line(ui,sprites,32,64,"CHECKING SD...")' in main
assert 'else line(reinterpret_cast<uint8_t*>(painter.page().data()),32,64,"SAVING - DO NOT POWER OFF")' in main
print('PASS: production menu render font routing, exact credits/footer, date/file exceptions, controls content')
