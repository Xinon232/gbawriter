// GBA Writer: GBAReader v0.5.0 SuperFW / Butano / Supercard foundation.
#include "bn_bg_palette_item.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_palette_bitmap_bg_painter.h"
#include "bn_palette_bitmap_bg_ptr.h"
#include "bn_sprite_items_ui_small_font.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"
#include "common_variable_8x16_sprite_font.h"
#include "ui_small_font.h"
extern "C" {
#include "font_render.h"
}
#include "writer_app.h"
#include "writer_help.h"
#include <cstdio>
#include "writer_format.h"
#include <cstring>
namespace {
// gbamp3 colours: white, black, grey, light grey, light-blue selected row.
constexpr bn::color colors[16]={bn::color(31,31,31),bn::color(0,0,0),bn::color(12,12,12),bn::color(20,20,20),bn::color(21,26,31)};
enum : uint8_t { WHITE=0, BLACK=1, GREY=2, LIGHT=3, BLUE=4 };
constexpr bn::bg_palette_item palette(bn::span<const bn::color>(colors),bn::bpp_mode::BPP_8);
// Layout measures every character it lays out; ASCII widths are cached (width + 1).
int glyph_width(const char* text){
 const unsigned c=static_cast<unsigned char>(text[0]);
 if(c>=128||!c||text[1])return int(font_width(text));
 static uint8_t ascii[128];if(!ascii[c])ascii[c]=uint8_t(font_width(text)+1);return ascii[c]-1;
}
// gbamp3 grey (0x4210) for key hints; other interface text is black.
constexpr bn::color hint_colors[16]={bn::color(31,0,31),bn::color(16,16,16),bn::color(31,31,31)};
constexpr bn::sprite_palette_item hint_palette(bn::span<const bn::color>(hint_colors),bn::bpp_mode::BPP_4);
// .sbss is the devkitARM/Butano linker-script EWRAM BSS section, NOT IWRAM.
__attribute__((section(".sbss"))) writer::Storage storage;
__attribute__((section(".sbss"))) writer::Application app(storage,glyph_width);
using Sprites=bn::vector<bn::sprite_ptr,128>;
void line(uint8_t* px,int x,int y,const char* s,int max=224){draw_text_idx8_bus16_range(s,px+y*240+x,0,max,240,1);}
// skip: pixels scrolled out on the left (marquee).
void text(uint8_t* px,int x,int y,const char* s,unsigned skip,int width,uint8_t color){draw_text_idx8_bus16_range(s,px+y*240+x,skip,width,240,color);}
void put(uint8_t* row,int x,uint8_t c){
 // GBA VRAM does not support byte stores. Preserve the adjacent indexed pixel.
 volatile uint16_t* p=reinterpret_cast<volatile uint16_t*>(row+(x&~1));
 *p=(x&1)?uint16_t((*p&0x00ff)|(c<<8)):uint16_t((*p&0xff00)|c);
}
void pixel(uint8_t* px,int x,int y){put(px+y*240,x,1);}
void fill(uint8_t* px,int x,int y,int w,int h,uint8_t c){
 if(x<0){w+=x;x=0;}if(x+w>240)w=240-x;
 for(int j=0;j<h&&y+j<160;++j){uint8_t* row=px+(y+j)*240;int i=x;
  if((i&1)&&i<x+w){put(row,i,c);++i;}
  auto* p=reinterpret_cast<volatile uint16_t*>(row+i);uint16_t both=uint16_t(c|(c<<8));
  for(;i+1<x+w;i+=2)*p++=both;
  if(i<x+w)put(row,i,c);}
}
// Interface text: gbamp3 5x7 font in 16 px cells (glyphs on cell rows 5-11).
struct Ui {
 bn::sprite_text_generator& ui;bn::sprite_text_generator& hint;Sprites& sprites;
 void at(int x,int y,const char* t,bool grey=false){auto& g=grey?hint:ui;g.set_left_alignment();g.generate(x-120,y-72,t,sprites);}
 void center(int y,const char* t,bool grey=false){auto& g=grey?hint:ui;g.set_center_alignment();g.generate(0,y-72,t,sprites);g.set_left_alignment();}
 void right(int x,int y,const char* t,bool grey=true){auto& g=grey?hint:ui;g.set_right_alignment();g.generate(x-120,y-72,t,sprites);g.set_left_alignment();}
};
constexpr int ROW_Y=writer::list::ROW_Y,ROW_H=writer::list::ROW_H,TEXT_X=6,TEXT_RIGHT=234;
// gbamp3 marquee: wait, scroll one pixel every second frame to the end, hold, restart.
struct Marquee {
 unsigned off=0,wait=60,hold=0,frame=0;
 void reset(){off=0;wait=60;hold=0;}
 bool step(unsigned width,unsigned avail){
  ++frame;
  if(width<=avail)return false;
  if(wait){--wait;return false;}
  if(width-avail<=off){if(++hold<60)return false;reset();return true;}
  if(frame&1)return false;
  ++off;return true;
 }
} marquee;
// gbamp3 icons at x 226, in black on the row's colour.
void icon(uint8_t* px,int y,writer::RowIcon i,uint8_t bg){
 using writer::RowIcon;
 if(i==RowIcon::PLAY){for(int j=0;j<9;++j)fill(px,226,y+4+j,j<5?j+1:9-j,1,BLACK);}
 else if(i==RowIcon::FOLDER){fill(px,226,y+4,4,2,BLACK);fill(px,226,y+6,10,7,BLACK);fill(px,227,y+7,8,5,bg);}
 else if(i==RowIcon::NEW_FILE){
  // A page with a folded corner and a plus.
  fill(px,226,y+3,7,1,BLACK);fill(px,226,y+3,1,12,BLACK);fill(px,226,y+14,10,1,BLACK);fill(px,235,y+6,1,9,BLACK);
  fill(px,232,y+3,1,4,BLACK);fill(px,233,y+5,2,1,BLACK);fill(px,233,y+4,1,1,BLACK);fill(px,234,y+5,1,1,BLACK);fill(px,232,y+6,4,1,BLACK);
  fill(px,228,y+10,5,1,BLACK);fill(px,230,y+8,1,5,BLACK);}
}
void row_text(uint8_t* px,int y,const char* t,bool selected,unsigned skip,int right,uint8_t color){
 unsigned avail=unsigned(right-TEXT_X),w=font_width(t);
 if(w<=avail){text(px,TEXT_X,y+1,t,0,int(avail),color);return;}
 if(selected){text(px,TEXT_X,y+1,t,skip,int(avail),color);return;}
 char shown[300];unsigned n=font_width_cap(t,avail-font_width("..."));if(n>sizeof(shown)-4)n=sizeof(shown)-4;
 std::memcpy(shown,t,n);std::memcpy(shown+n,"...",4);text(px,TEXT_X,y+1,shown,0,int(avail),color);
}
void draw_list(uint8_t* px,Ui& u){
 char title[64];u.center(0,app.list_title(title));
 const auto& nav=app.nav();
 for(int r=0;r<nav.rows();++r){
  uint32_t i=nav.top()+uint32_t(r);if(i>=nav.count())break;
  int y=ROW_Y+r*ROW_H;char t[writer::FILE_NAME_SIZE+32];bool enabled=app.list_row(int(i),t);
  bool sel=enabled&&i==nav.sel();if(sel)fill(px,0,y,240,ROW_H,BLUE);
  auto ic=app.list_icon(int(i));icon(px,y,ic,sel?BLUE:WHITE);
  int right=ic!=writer::RowIcon::NONE?222:TEXT_RIGHT;
  if(enabled)row_text(px,y,t,sel,marquee.off,right,BLACK);
  else text(px,(240-int(font_width(t)))/2,y+1,t,0,228,GREY);
  if(app.list_underline(int(i)))fill(px,0,y+ROW_H-1,240,1,GREY);
 }
 if(const char* f=app.list_footer())text(px,TEXT_X,ROW_Y+(writer::list::ROWS-1)*ROW_H+1,f,0,TEXT_RIGHT-TEXT_X,GREY);
}
void draw_note(uint8_t* px,Ui& u){
 const char* n=app.note();if(!n[0])return;
 fill(px,0,140,240,20,WHITE);fill(px,0,140,240,1,LIGHT);u.center(142,n);
}
// Up to four lines of body text, broken at spaces.
int wrap(uint8_t* px,int y,const char* s){
 char buf[200];std::size_t n=std::strlen(s);if(n>=sizeof(buf))n=sizeof(buf)-1;std::memcpy(buf,s,n);buf[n]=0;
 char* p=buf;int lines=0;
 while(*p&&lines<4){
  unsigned fit=font_width_cap(p,224);std::size_t len=std::strlen(p);
  if(fit<len){unsigned k=fit;while(k&&p[k]!=' ')--k;if(k)fit=k;}
  char keep=p[fit];p[fit]=0;line(px,8,y,p);p[fit]=keep;while(*p&&fit--)++p;while(*p==' ')++p;y+=18;++lines;}
 return y;
}
void draw_pages(uint8_t* px,Ui& u){
 const auto& page=writer::help_page(app.topic(),app.page());
 u.center(0,page.title);
 char number[8];writer::format(number,sizeof(number),"%d/%d",app.page()+1,writer::help_topic_pages(app.topic()));
 u.right(234,0,number);
 for(int i=0;i<writer::HELP_LINES;++i){
  const char* t=page.lines[i];int y=ROW_Y+i*ROW_H;
  if(t[0]=='#')u.at(8,y+1,t+1,true);else if(t[0])line(px,8,y+1,t);
 }
}
void draw_date(uint8_t* px,Ui& u){
 u.center(0,"New File");auto d=app.date();char value[16];
 static const char* const labels[3]={"Day","Month","Year"};
 for(int r=0;r<3;++r){
  int part=app.date_part(r),y=ROW_Y+r*ROW_H;bool sel=r==app.date_field();
  if(sel)fill(px,0,y,240,ROW_H,BLUE);
  line(px,TEXT_X,y+1,labels[part]);
  if(part==2)writer::format(value,sizeof(value),"%04d",d.year);else writer::format(value,sizeof(value),"%02d",part?d.month:d.day);
  line(px,120,y+1,value);
 }
 char name[writer::DIARY_NAME_SIZE];writer::format_diary_name(d,storage.settings().format,name);
 int y=ROW_Y+4*ROW_H;u.at(TEXT_X,y+1,"File",true);line(px,64,y+1,name);
}
void draw_error(uint8_t* px,Ui& u){
 u.center(0,"Please note");
 int y=wrap(px,ROW_Y+8,app.message());
 if(!std::strcmp(app.message(),"FILE ALREADY EXISTS")){char name[writer::DIARY_NAME_SIZE];writer::format_diary_name(app.date(),storage.settings().format,name);line(px,8,y,name);u.at(8,y+22,"Choose another date.",true);}
 else if(!std::strcmp(app.message(),"RECOVERY: CHECK SD ON PC")){u.at(8,y+4,"Preserved recovery copies.",true);u.at(8,y+22,"Back up SD before repair.",true);}
 u.center(142,"A: OK",true);
}
void draw_rename(uint8_t* px,Ui& u){
 u.center(0,"Rename");
 // As in gbamp3: the letter group and Shift/Caps at the top right.
 const char* g=app.name_group();const char* m=app.name_caps()?"Caps":app.name_shift()?"Shift":"";
 int x=234;if(g&&g[0]){u.right(x,0,g);x-=int(std::strlen(g))*6+6;}if(m[0])u.right(x,0,m);
 int y=40;fill(px,0,y,240,ROW_H,LIGHT);fill(px,6,y+1,228,ROW_H-1,WHITE);
 const char* t=app.name_text();char before[writer::FILE_NAME_SIZE];std::size_t c=app.name_caret();
 std::memcpy(before,t,c);before[c]=0;unsigned cx=font_width(before);unsigned skip=cx>220?cx-220:0;
 text(px,8,y+1,t,skip,224,BLACK);
 // ".txt" is added to the name: grey after the text when it fits.
 int end=8+int(font_width(t))-int(skip)+2;if(end+int(font_width(".txt"))<232)text(px,end,y+1,".txt",0,232-end,GREY);
 int caret_x=8+int(cx)-int(skip);for(int j=0;j<15;++j)put(px+(y+1+j)*240,caret_x<233?caret_x:233,1);
 if(app.note()[0])u.center(84,app.note());
 u.center(120,"Start+A: Save   Start+B: Cancel",true);
}
void draw_busy(uint8_t* px,Ui& u,const char* t,int percent){
 u.center(0,"gbawriter");
 if(percent>=0){char b[48];writer::format(b,sizeof(b),"%s %d%%",t,percent);text(px,(240-int(font_width(b)))/2,64,b,0,228,BLACK);}
 else text(px,(240-int(font_width(t)))/2,64,t,0,228,BLACK);
}
struct Renderer{bn::palette_bitmap_bg_painter& painter;bn::sprite_text_generator& ui;bn::sprite_text_generator& hint;Sprites& sprites;};
Renderer* renderer=nullptr;
void busy(void*,const char* t,int percent){
 if(!renderer)return;
 auto& r=*renderer;r.sprites.clear();r.painter.fill(0);
 Ui u{r.ui,r.hint,r.sprites};draw_busy(reinterpret_cast<uint8_t*>(r.painter.page().data()),u,t,percent);
 r.painter.flip_page_later();bn::core::update();
}
void render(bn::palette_bitmap_bg_painter& painter,bn::sprite_text_generator& ui,bn::sprite_text_generator& hint,Sprites& sprites){
 sprites.clear();painter.fill(0);auto* px=reinterpret_cast<uint8_t*>(painter.page().data());char buffer[writer::FILE_NAME_SIZE + 2]; // Complete filename + dirty prefix + NUL; clip pixels, not UTF-8 bytes.
 Ui u{ui,hint,sprites};
 using writer::Scene;
 switch(app.scene()){
 case Scene::HOME:case Scene::LIST:draw_list(px,u);draw_note(px,u);break;
 case Scene::DATE:draw_date(px,u);break;
 case Scene::PAGES:draw_pages(px,u);break;
 case Scene::RENAME:draw_rename(px,u);break;
 case Scene::ERROR:draw_error(px,u);break;
 case Scene::EDITOR:{
  if(app.status_visible()){
   if(app.message()[0])line(px,8,writer::STATUS_Y,app.message(),128);else{
    writer::format(buffer,sizeof(buffer),"%s%s",app.text().dirty()?"* ":"",storage.current_name());line(px,8,writer::STATUS_Y,buffer,128);
   }
   line(px,144,writer::STATUS_Y,app.active_group(),32);
   if(app.caps())line(px,184,writer::STATUS_Y,"Caps",48);else if(app.shift())line(px,184,writer::STATUS_Y,"Shift",48);
  }
  auto& text=app.text();auto& layout=app.layout();
  int last=app.viewport()+app.view_rows();if(last>layout.rows())last=layout.rows();
  for(int row=app.viewport();row<last;++row){int x=8,y=writer::TEXT_Y+(row-app.viewport())*writer::TEXT_PITCH;std::size_t end=layout.row_end(row);
   for(std::size_t p=layout.row_content_start(text,row);p<end;){char ch[5];p=writer::Layout::character(text,p,ch);if(ch[0]=='\n')break;int w=layout.width(ch);if(w&&ch[0]!='\t')line(px,x,y,ch,228-x);x+=w;}
  }
  auto caret=layout.position(text,text.caret_byte());if(app.caret_visible()&&caret.row>=app.viewport()&&caret.row<last){int x=8+caret.x,y=writer::TEXT_Y+(caret.row-app.viewport())*writer::TEXT_PITCH;for(int j=0;j<16;++j)pixel(px,x,y+j);}
  break;}
 default: break;
 }
 painter.flip_page_later();
}
// The marquee follows the selected row of a list.
bool marquee_step(){
 using writer::Scene;
 static int last_scene=-1;static uint32_t last_sel=~0u,last_top=~0u;static int last_kind=-1;
 if(app.scene()!=Scene::HOME&&app.scene()!=Scene::LIST){last_scene=-1;return false;}
 const auto& nav=app.nav();
 if(int(app.scene())!=last_scene||nav.sel()!=last_sel||nav.top()!=last_top||int(app.list_kind())!=last_kind){
  last_scene=int(app.scene());last_sel=nav.sel();last_top=nav.top();last_kind=int(app.list_kind());marquee.reset();return false;}
 if(nav.sel()>=nav.count())return false;
 char t[writer::FILE_NAME_SIZE+32];if(!app.list_row(int(nav.sel()),t))return false;
 int right=app.list_icon(int(nav.sel()))!=writer::RowIcon::NONE?222:TEXT_RIGHT;
 return marquee.step(font_width(t),unsigned(right-TEXT_X));
}
uint16_t keys(){
 using writer::Button;uint16_t result=0;
 const bool held[]={bn::keypad::up_held(),bn::keypad::down_held(),bn::keypad::left_held(),bn::keypad::right_held(),bn::keypad::a_held(),bn::keypad::b_held(),bn::keypad::l_held(),bn::keypad::r_held(),bn::keypad::start_held(),bn::keypad::select_held()};
 for(unsigned i=0;i<10;++i){if(held[i])result|=1u<<i;}return result;
}
}
int main(){
 bn::core::init();auto bg=bn::palette_bitmap_bg_ptr::create(palette);bn::palette_bitmap_bg_painter painter(bg);
 // Interface text uses gbamp3's 5x7 font.
 bn::sprite_font ui_font(bn::sprite_items::ui_small_font,bn::utf8_characters_map_ref(),writer::ui_small_font_character_widths);
 bn::sprite_text_generator ui(ui_font);ui.set_palette_item(bn::sprite_items::ui_small_font.palette_item());
 bn::sprite_text_generator hint(ui_font);hint.set_palette_item(hint_palette);Sprites sprites;
 Renderer r{painter,ui,hint,sprites};renderer=&r;app.set_busy(busy,nullptr);
 app.boot();
 while(true){
  uint16_t snapshot=keys();
  // Show feedback BEFORE a potentially slow save.
  if(app.save_feedback(snapshot))busy(nullptr,"SAVING - DO NOT POWER OFF",-1);
  app.frame(snapshot);bool step=marquee_step();if(app.take_redraw()||step)render(painter,ui,hint,sprites);bn::core::update();
 }
}
