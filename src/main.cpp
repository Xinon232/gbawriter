// GBA Writer: GBAReader v0.5.0 SuperFW / Butano / Supercard foundation.
#include "bn_bg_palette_item.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_palette_bitmap_bg_painter.h"
#include "bn_palette_bitmap_bg_ptr.h"
#include "bn_sprite_items_ui_variable_8x16_font.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"
#include "common_variable_8x16_sprite_font.h"
extern "C" {
#include "font_render.h"
}
#include "writer_app.h"
#include <cstdio>
#include "writer_format.h"
#include <cstring>
namespace {
constexpr bn::color colors[16]={bn::color(31,31,31),bn::color(0,0,0),bn::color(12,12,12),bn::color(20,20,20)};
constexpr bn::bg_palette_item palette(bn::span<const bn::color>(colors),bn::bpp_mode::BPP_8);
int glyph_width(const char* text){return int(font_width(text));}
// .sbss is the devkitARM/Butano linker-script EWRAM BSS section, NOT IWRAM.
__attribute__((section(".sbss"))) writer::Storage storage;
__attribute__((section(".sbss"))) writer::Application app(storage,glyph_width);
using Sprites=bn::vector<bn::sprite_ptr,128>;
void line(uint8_t* px,int x,int y,const char* s,int max=224){draw_text_idx8_bus16_range(s,px+y*240+x,0,max,240,1);}
void pixel(uint8_t* px,int x,int y){
 // GBA VRAM does not support byte stores. Preserve the adjacent indexed pixel.
 volatile uint16_t* p=reinterpret_cast<volatile uint16_t*>(px+y*240+(x&~1));
 *p=(x&1)?uint16_t((*p&0x00ff)|0x0100):uint16_t((*p&0xff00)|1);
}
void title(bn::sprite_text_generator& ui,Sprites& sprites,const char* text){ui.set_center_alignment();ui.generate(0,-68,text,sprites);}
void ui_line(bn::sprite_text_generator& ui,Sprites& sprites,int x,int y,const char* text){ui.set_left_alignment();ui.generate(x-120,y-72,text,sprites);}
const char* const help[writer::Application::HELP_PAGES][6]={
 {"ABOUT / FILES","Create and edit TXT files.","Save directly to SD.","Put TXT files in SD root folder:","/gbawriter","Supercard SD required"},
 {"MENUS / FILES","UP/DOWN: select  A: open","NEW FILE: choose date, A: create","LOAD FILE: choose TXT, A: open","B: back  Error message: A: OK","Menu SELECT: help START: credits"},
 {"REPEAT / SAVE SAFETY","Hold A/B alone: repeat edit","Hold START navigation: repeat","Adding a key cancels edit repeat","No autosave or discard shortcut","Do not power off while saving"},
 {"NORMAL LETTERS","D-pad holds a letter group","B = first  A = second  R = third","UP: ABC    RIGHT: DEF","DOWN: HIJ  LEFT: KLM","Release group for next session"},
 {"HOLD L: SECOND LAYER","L is held, never a toggle","B = first  A = second  R = third","L+UP: NOP   L+RIGHT: QRS","L+DOWN: TUW L+LEFT: XYZ","R in a group is a letter"},
 {"SPECIAL LETTERS / BASIC EDIT","Keep DOWN held: R,R = g","Keep L+DOWN held: R,R = v","Release between R presses: jj/ww","A alone: space  B: backspace","START release alone: newline"},
 {"SHIFT / CAPS","Normal: press R alone","Short R release: Shift","R alone 48 frames: Caps","About 0.8 seconds; while held","Shift/Caps: R release clears"},
 {"R HOLD / CHORDS","Other key cancels this hold","Release R; fresh solo hold","Clearing hold cannot rearm","Shift: next accepted letter","Digits/signs do not use Shift"},
 {"START: NAVIGATE","Hold START + LEFT/RIGHT","Move one UTF-8 character","START+UP/DOWN: visual rows","START+L/R: previous/next page","Navigation never types letters"},
 {"START: SAVE","START+A: save this file","START+B: save, then main menu","Save failure keeps your text","Release START after command:","No accidental newline"},
 {"SELECT: ONE LIVE CHARACTER","SELECT alone: inserts . now","Hold SELECT to replace it","Release SELECT to commit one","UP: 1 2 3 4 5 6 7 8 9 0","DOWN: 0 9 8 7 6 5 4 3 2 1"},
 {"SELECT: COMMON PUNCTUATION","SELECT+R forward:",". , ' \" : ! ? .","SELECT+L reverse:",". ? ! : \" ' , .","A letter chord takes priority"},
 {"SELECT: ADDITIONAL SIGNS","SELECT+RIGHT forward:",". ( ) / ; @ # % & _ + = - .","SELECT+LEFT: exact reverse","Every step replaces one glyph","No extra characters appended"},
 {"INTERNATIONAL LETTERS 1","A: á ä à â ã å æ","C: ç č ć  E: é è ë ê","I: í ï ì î","N: ñ ń","SELECT + normal letter chord"},
 {"INTERNATIONAL LETTERS 2","O: ó ö ô ò õ ø œ","S: ß š ś  U: ü ú ù û","Y: ý ÿ  Z: ž ź ż","L-layer chords work here too","Shift / CAPS applies to accents"},
 {"ACCENTS: EITHER ORDER","Hold SELECT, then type a chord","Or type a letter; keep held:","Exact direction, L if used,","and its producing B/A/R button","Then press SELECT: same letter"},
 {"HELD LETTER: SELECT SECOND","First accent; keeps its case","No extra period or letter","No time limit; no extra keys","Release/change chord: ineligible","No variants? Letter unchanged"},
 {"INTERNATIONAL CYCLING","Keep SELECT + group held","Release/repress final B/A/R","E chord: é è ë ê, then é","Release SELECT to keep result","ß stays ß, even with CAPS"},
 {"ACCENT EXAMPLE / STATUS","RIGHT+A held, then SELECT: é","START+SELECT: bar / full screen","Cancels new provisional only","Converted existing letter stays","Release both to end status chord"},
 {"STATUS BAR / DATE","Bottom: file, group, Shift/Caps","Date UP/DOWN: choose field","Date LEFT/RIGHT: change value","Date A: create  B: back","No autosave or discard shortcut"}
};
void render(bn::palette_bitmap_bg_painter& painter,bn::sprite_text_generator& ui,Sprites& sprites){
 sprites.clear();painter.fill(0);auto* px=reinterpret_cast<uint8_t*>(painter.page().data());char buffer[writer::FILE_NAME_SIZE + 2]; // Complete filename + dirty prefix + NUL; clip pixels, not UTF-8 bytes.
 using writer::Scene;
 switch(app.scene()){
 case Scene::MENU:
  title(ui,sprites,"gbawriter V1.1");ui_line(ui,sprites,70,28,"files: /gbawriter");ui.set_center_alignment();ui.generate(0,-22,app.menu_selection()==0?"> NEW FILE":"  NEW FILE",sprites);ui.generate(0,2,app.menu_selection()==1?"> LOAD FILE":"  LOAD FILE",sprites);
  ui_line(ui,sprites,16,140,"Select: Controls");ui_line(ui,sprites,128,140,"Start: Credits");break;
 case Scene::DATE:{
  title(ui,sprites,"NEW FILE");auto d=app.date();
  ui_line(ui,sprites,38,34,app.date_field()==0?"> DAY":"  DAY");writer::format(buffer,sizeof(buffer),"%c DAY     ",app.date_field()==0?'>':' ');int day_x=38+glyph_width(buffer);writer::format(buffer,sizeof(buffer),"%02d",d.day);line(px,day_x,34,buffer);
  ui_line(ui,sprites,38,54,app.date_field()==1?"> MONTH":"  MONTH");writer::format(buffer,sizeof(buffer),"%c MONTH   ",app.date_field()==1?'>':' ');int month_x=38+glyph_width(buffer);writer::format(buffer,sizeof(buffer),"%02d",d.month);line(px,month_x,54,buffer);
  ui_line(ui,sprites,38,74,app.date_field()==2?"> YEAR":"  YEAR");writer::format(buffer,sizeof(buffer),"%c YEAR    ",app.date_field()==2?'>':' ');int year_x=38+glyph_width(buffer);writer::format(buffer,sizeof(buffer),"%04d",d.year);line(px,year_x,74,buffer);
  ui_line(ui,sprites,8,108,"UP/DOWN: FIELD  LEFT/RIGHT: +/-");ui_line(ui,sprites,28,136,"A: CREATE   B: BACK");break;}
 case Scene::LOAD:{
  title(ui,sprites,"LOAD FILE");if(!storage.count())ui_line(ui,sprites,48,64,"NO TXT FILES");
  int first=(app.selected_file()/6)*6;
  for(int i=first;i<storage.count()&&i<first+6;++i){int y=24+(i-first)*18;ui_line(ui,sprites,6,y,i==app.selected_file()?">":" ");line(px,18,y,storage.name(i),216);}
  ui_line(ui,sprites,8,138,"UP/DOWN  A: OPEN  B: BACK");break;}
 case Scene::HELP:
  writer::format(buffer,sizeof(buffer),"CONTROLS %d/%d",app.help_page()+1,writer::Application::HELP_PAGES);title(ui,sprites,buffer);
  ui_line(ui,sprites,8,24,help[app.help_page()][0]);
  for(int row=1;row<6;++row){line(px,8,24+row*18,help[app.help_page()][row]);}ui_line(ui,sprites,8,138,"Left/Right: Page  B: Back");break;
 case Scene::CREDITS:
  title(ui,sprites,"CREDITS");
  line(px,8,38,"Made by Halim Jarrar");line(px,8,60,"(C) 2026");
  line(px,8,82,"halim-jarrar.de");line(px,8,104,"monday@halim-jarrar.de");
  ui_line(ui,sprites,8,138,"B: Back");break;
 case Scene::ERROR:
  title(ui,sprites,"PLEASE NOTE");ui_line(ui,sprites,8,42,app.message());
  if(!std::strcmp(app.message(),"FILE ALREADY EXISTS")){char name[13];writer::format_diary_name(app.date(),name);line(px,48,64,name);ui_line(ui,sprites,20,88,"CHOOSE ANOTHER DATE");}
  else if(!std::strcmp(app.message(),"RECOVERY: CHECK SD ON PC")){ui_line(ui,sprites,8,66,"Preserved recovery copies.");ui_line(ui,sprites,8,86,"Back up SD before repair.");}
  ui_line(ui,sprites,70,132,"A: OK");break;
 case Scene::EDITOR:{
  if(app.status_visible()){
   if(app.message()[0])line(px,8,writer::STATUS_Y,app.message(),128);else{
    writer::format(buffer,sizeof(buffer),"%s%s",app.text().dirty()?"* ":"",storage.current_name());line(px,8,writer::STATUS_Y,buffer,128);
   }
   line(px,144,writer::STATUS_Y,app.active_group(),32);
   if(app.caps())line(px,184,writer::STATUS_Y,"Caps",48);else if(app.shift())line(px,184,writer::STATUS_Y,"Shift",48);
  }
  auto& text=app.text();auto& layout=app.layout();const char* s=text.data();
  int last=app.viewport()+app.view_rows();if(last>layout.rows())last=layout.rows();
  for(int row=app.viewport();row<last;++row){int x=8,y=writer::TEXT_Y+(row-app.viewport())*writer::TEXT_PITCH;std::size_t end=row+1<layout.rows()?layout.row_start(row+1):text.bytes();
   for(std::size_t p=layout.row_content_start(text,row);p<end;){char ch[5];p=writer::Layout::character(s,p,ch);if(ch[0]=='\n')break;int w=layout.width(ch);if(w&&ch[0]!='\t')line(px,x,y,ch,228-x);x+=w;}
  }
  auto caret=layout.position(text,text.caret_byte());if(app.caret_visible()&&caret.row>=app.viewport()&&caret.row<last){int x=8+caret.x,y=writer::TEXT_Y+(caret.row-app.viewport())*writer::TEXT_PITCH;for(int j=0;j<16;++j)pixel(px,x,y+j);}
  break;}
 default: break;
 }
 painter.flip_page_later();
}
uint16_t keys(){
 using writer::Button;uint16_t result=0;
 const bool held[]={bn::keypad::up_held(),bn::keypad::down_held(),bn::keypad::left_held(),bn::keypad::right_held(),bn::keypad::a_held(),bn::keypad::b_held(),bn::keypad::l_held(),bn::keypad::r_held(),bn::keypad::start_held(),bn::keypad::select_held()};
 for(unsigned i=0;i<10;++i){if(held[i])result|=1u<<i;}return result;
}
}
int main(){
 bn::core::init();auto bg=bn::palette_bitmap_bg_ptr::create(palette);bn::palette_bitmap_bg_painter painter(bg);
 bn::sprite_font ui_font(bn::sprite_items::ui_variable_8x16_font,common::variable_8x16_sprite_font_utf8_characters_map.reference(),common::variable_8x16_sprite_font_character_widths);
 bn::sprite_text_generator ui(ui_font);ui.set_palette_item(bn::sprite_items::ui_variable_8x16_font.palette_item());Sprites sprites;
 app.boot();
 while(true){
  uint16_t snapshot=keys();
  // Show feedback BEFORE potentially slow hardware operations. Never probe SD at boot.
  bool checking=app.scene()==writer::Scene::MENU&&bn::keypad::a_pressed();
  bool saving=app.save_feedback(snapshot);
  if(checking||saving){sprites.clear();painter.fill(0);if(checking)ui_line(ui,sprites,32,64,"CHECKING SD...");else line(reinterpret_cast<uint8_t*>(painter.page().data()),32,64,"SAVING - DO NOT POWER OFF");painter.flip_page_later();bn::core::update();}
  app.frame(snapshot);if(app.take_redraw())render(painter,ui,sprites);bn::core::update();
 }
}
