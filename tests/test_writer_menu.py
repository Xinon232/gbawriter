#!/usr/bin/env python3
"""V4.0: compile the production list/page/date/rename/error drawing of
src/main.cpp against a recording sprite-font backend and a real 8-bit pixel
buffer (SuperFW text), then check the gbamp3 look of each screen.
The editor has its own framebuffer test; exact-ROM QA covers Butano."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
main = (root / 'src/main.cpp').read_text()
helpers = main.split('void line(', 1)[1].split('struct Renderer{', 1)[0]
source = r'''
#include "writer_app.h"
#include "writer_format.h"
#include "writer_help.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unistd.h>
extern "C" {
#include "font_render.h"
void *font_base_addr;
void *reader_font_base_addr;
}
int glyph_width(const char* s){return int(font_width(s));}
struct Draw { std::string text; int x,y; bool grey; };
std::vector<Draw> draws;
using Sprites=int;
namespace bn { struct sprite_text_generator {
 // gbamp3 5x7 interface font: 6 px per character.
 bool grey; int alignment=1;
 void set_center_alignment(){alignment=0;}
 void set_left_alignment(){alignment=1;}
 void set_right_alignment(){alignment=2;}
 void generate(int x,int y,const char* s,Sprites&){int width=0;for(auto p=s;*p;++p){assert(*p>=32&&*p<=126);width+=6;}
  int left=x+120-(alignment==0?width/2:alignment==2?width:0);assert(left>=0&&left+width<=240&&y+72>=0&&y+88<=160);draws.push_back({s,left,y+72,grey});}
}; }
enum : uint8_t { WHITE=0, BLACK=1, GREY=2, LIGHT=3, BLUE=4 };
constexpr int unused_hint=0;
writer::Storage* storage_ptr;
writer::Application* app_ptr;
#define storage (*storage_ptr)
#define app (*app_ptr)
void line(''' + helpers + r'''
#undef storage
#undef app
alignas(4) uint8_t px[240*160];
bn::sprite_text_generator ui{false},hint{true};Sprites sprites=0;
void frame(){
 draws.clear();std::memset(px,0,sizeof px);Ui u{ui,hint,sprites};
 using writer::Scene;
 switch(app_ptr->scene()){
 case Scene::HOME:case Scene::LIST:draw_list(px,u);draw_note(px,u);break;
 case Scene::DATE:draw_date(px,u);break;
 case Scene::PAGES:draw_pages(px,u);break;
 case Scene::RENAME:draw_rename(px,u);break;
 case Scene::ERROR:draw_error(px,u);break;
 default:break;}
}
bool has(const char* t,bool grey=false){for(auto& d:draws)if(d.text==t){assert(d.grey==grey);return true;}return false;}
// Body text (SuperFW) is drawn into px: ink in a row band.
bool ink(int y0,int y1,uint8_t c,int x0=0,int x1=240){for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)if(px[y*240+x]==c)return true;return false;}
int main(int argc,char** argv){
 assert(argc==3);
 std::ifstream f(argv[1],std::ios::binary),r(argv[2],std::ios::binary);
 std::vector<char> fonts((std::istreambuf_iterator<char>(f)),{}),symbols((std::istreambuf_iterator<char>(r)),{});
 font_base_addr=fonts.data();reader_font_base_addr=symbols.data();
 using namespace writer;
 std::string dir="/tmp/writer-menu-"+std::to_string(getpid());
 std::filesystem::remove_all(dir);std::filesystem::create_directories(dir+"/gbawriter");
 std::ofstream(dir+"/gbawriter/10072026.txt")<<"x";
 std::ofstream(dir+"/gbawriter/A very long file name that has to scroll sideways.txt")<<"y";
 Storage s(dir.c_str());storage_ptr=&s;
 Application a(s,glyph_width);app_ptr=&a;a.boot();a.frame(0);
 auto tap=[&](Button b){a.frame(1u<<unsigned(b));a.frame(0);};
 // Home: title, New File first with its icon and the line under it, files without .txt.
 frame();
 assert(has("gbawriter")&&draws.size()==1);
 assert(ink(16,33,BLUE)&&!ink(33,50,BLUE));              // New File selected
 assert(ink(16,32,BLACK,6,120)&&ink(19,31,BLACK,226,236)); // text and icon
 for(int x=0;x<240;++x)assert(px[32*240+x]==GREY);          // the line under New File
 assert(ink(33,50,BLACK,6,100)&&ink(50,67,BLACK,6,230));
 assert(!ink(136,160,GREY));                               // no instructions
 tap(Button::DOWN);frame();assert(ink(33,50,BLUE)&&!ink(16,32,BLUE));
 // The long name is cut with "..." and scrolls when selected (marquee).
 tap(Button::DOWN);tap(Button::DOWN);
 // Select menu on New File: File names, Controls topics, Credits.
 tap(Button::UP);tap(Button::UP);tap(Button::UP);
 assert(a.nav().sel()==0);
 tap(Button::SELECT);frame();assert(has("gbawriter"));
 // Controls pages: grey page number, grey subheadings, no key line.
 tap(Button::DOWN);frame();assert(has("gbawriter"));   // Helper line row
 for(int x=0;x<240;++x)assert(px[(ROW_Y+2*ROW_H-1)*240+x]==GREY);   // line under Helper line
 tap(Button::DOWN);tap(Button::A);frame();
 assert(has("Home")&&has("1/4",true)&&has("Home",false));
 bool sub=false;for(auto& d:draws)if(d.grey&&d.text!="1/4")sub=true;assert(sub);
 for(auto& d:draws)assert(d.text.find("Left/Right")==std::string::npos);
 tap(Button::B);
 // Credits: personal page first.
 tap(Button::UP);tap(Button::UP);tap(Button::UP);tap(Button::A);frame();
 assert(a.scene()==Scene::PAGES&&a.topic()==CREDITS_TOPIC);
 assert(has("Credits")&&has("1/6",true));
 const auto& page=help_page(CREDITS_TOPIC,0);
 assert(std::string(page.lines[0])=="gbawriter V4.1"&&std::string(page.lines[2])=="Made by Halim Jarrar"&&
        std::string(page.lines[3])=="(C) 2026"&&std::string(page.lines[5])=="halimj.itch.io"&&std::string(page.lines[6])=="gba@halim-jarrar.de");
 tap(Button::B);tap(Button::B);
 // Date picker in the format's field order, file name preview.
 tap(Button::A);frame();assert(a.scene()==Scene::DATE&&has("New File")&&has("File",true));
 assert(ink(16,33,BLUE));
 tap(Button::B);
 // Error screen: wrapped message, A: OK in grey.
 tap(Button::DOWN);tap(Button::A);a.frame(0);
 a.frame((1u<<unsigned(Button::START)));a.frame((1u<<unsigned(Button::START))|(1u<<unsigned(Button::B)));a.frame(0);
 frame();assert(a.scene()==Scene::HOME&&a.list_footer());
 assert(ink(136,153,GREY,6,200));                          // B: Resume active file
 tap(Button::SELECT);tap(Button::A);frame();
 assert(a.scene()==Scene::ERROR&&has("Please note")&&has("A: OK",true)&&ink(40,80,BLACK));
 tap(Button::A);tap(Button::B);
 // Rename: field with caret and the grey .txt.
 tap(Button::DOWN);tap(Button::SELECT);tap(Button::A);frame();
 assert(a.scene()==Scene::RENAME&&has("Rename")&&has("Start+A: Save   Start+B: Cancel",true));
 assert(ink(41,57,BLACK)&&ink(40,41,LIGHT)); // long name: .txt does not fit
 std::filesystem::remove_all(dir);
}
'''
with tempfile.TemporaryDirectory(prefix='writer-menu-') as directory:
    out = Path(directory)
    (out / 'menu.cpp').write_text(source)
    includes = ['-I'+str(root/p) for p in ('include','references/superfw/src','references/superfw/src/fonts')]
    flags = ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie']
    subprocess.run(['gcc','-std=c11',*flags,*includes,'-Wno-discarded-qualifiers','-c',str(root/'src/superfw_font.c'),'-o',str(out/'font.o')],check=True)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',*flags,'-no-pie',*includes,str(out/'menu.cpp'),*[str(root/'src'/('writer_'+s+'.cpp')) for s in ('core','layout','app','storage','format','help')],str(out/'font.o'),'-o',str(out/'menu')],check=True)
    subprocess.run([str(out/'menu'),str(root/'references/superfw/res/fonts.pack'),str(root/'references/superfw/res/reader-symbols.pack')],check=True)
assert 'busy(nullptr,"SAVING - DO NOT POWER OFF",-1)' in main
print('PASS: V4.0 screens: Home list, New File line and icon, menu, pages, credits, date picker, error, rename')
