#include "writer_format.h"
#include "writer_storage.h"
extern "C" {
#include "font_render.h"
void *font_base_addr;
void *reader_font_base_addr;
}
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <filesystem>
#include <unistd.h>
#include "writer_app.h"
#include "editor_render.inc"

static unsigned key(writer::Button b){return 1u << unsigned(b);}
static void editor_positions(){
  using writer::Button;
  std::string root="/tmp/writer-pixels-"+std::to_string(getpid());
  std::filesystem::create_directory(root);
  writer::Storage storage(root.c_str());
  writer::Application app(storage,glyph_width);app.boot();
  auto tap=[&](unsigned keys){app.frame(keys);app.frame(0);};
  tap(key(Button::A));tap(key(Button::A));
  assert(app.scene()==writer::Scene::EDITOR);
  app.text().set_text("a\na\na\na\na\na\na\na\na\na");
  app.layout().reflow(app.text(),220,glyph_width);app.text().move_home();
  const Button dirs[]={Button::UP,Button::RIGHT,Button::DOWN,Button::LEFT};
  const char* lower[]={"abc","def","hij","klm","nop","qrs","tuw","xyz"};
  const char* upper[]={"ABC","DEF","HIJ","KLM","NOP","QRS","TUW","XYZ"};
  alignas(2) uint8_t actual[240*160],expected[240*160];
  auto compare=[&](bool bar,const char* group,const char* mode){
    std::memset(actual,0,sizeof(actual));std::memset(expected,0,sizeof(expected));
    render_editor(actual,app,storage);
    if(bar){
      const char* label=app.message()[0]?app.message():storage.current_name();
      draw_text_idx8_bus16_range(label,expected+144*240+8,0,128,240,1);
      draw_text_idx8_bus16_range(group,expected+144*240+144,0,32,240,1);
      draw_text_idx8_bus16_range(mode,expected+144*240+184,0,48,240,1);
    }
    for(int row=0;row<(bar?7:9);++row)
      draw_text_idx8_bus16_range("a",expected+row*18*240+8,0,220,240,1);
    // Caret must reach y=0, not merely the font's first nonblank scanline.
    for(int y=0;y<16;++y)expected[y*240+8]=1;
    assert(std::memcmp(actual,expected,sizeof(actual))==0 && "editor pixel positions differ");
    if(bar)for(int y=138;y<144;++y)for(int x=0;x<240;++x)assert(!actual[y*240+x]);
  };
  for(int mode=0;mode<3;++mode){
    for(int layer=0;layer<2;++layer)for(int d=0;d<4;++d){
      app.frame(key(dirs[d])|(layer?key(Button::L):0));
      compare(true,(mode?upper:lower)[layer*4+d],mode==1?"Shift":mode==2?"Caps":"");
      app.frame(0);compare(true,"",mode==1?"Shift":mode==2?"Caps":"");
    }
    tap(key(Button::R));
  }
  // Saving feedback occupies only the file slot; the two indicators survive.
  tap(key(Button::R));tap(key(Button::START)|key(Button::A));app.frame(key(Button::UP));
  assert(app.message()[0]);compare(true,"ABC","Shift");app.frame(0);
  tap(key(Button::START)|key(Button::SELECT));compare(false,"","");
  tap(key(Button::START)|key(Button::SELECT));compare(true,"","Shift");
  // Production framebuffer: a fitting word goes to the next display row,
  // retaining the separator bytes and leaving editor typography unchanged.
  tap(key(Button::START)|key(Button::SELECT));
  std::string prefix;
  while(font_width((prefix+"a bbbbbbbbbb").c_str())<=220)prefix+='a';
  prefix += "a ";
  std::string document=prefix+"bbbbbbbbbb";
  app.text().set_text(document.c_str());app.layout().reflow(app.text(),220,glyph_width);
  assert(app.layout().rows()==2 && app.layout().row_start(1)==prefix.size());
  std::memset(actual,0,sizeof(actual));std::memset(expected,0,sizeof(expected));
  render_editor(actual,app,storage);
  draw_text_idx8_bus16_range(prefix.c_str(),expected+8,0,220,240,1);
  draw_text_idx8_bus16_range("bbbbbbbbbb",expected+18*240+8,0,220,240,1);
  for(int y=18;y<34;++y)expected[y*240+8+font_width("bbbbbbbbbb")]=1;
  assert(std::memcmp(actual,expected,sizeof(actual))==0);
  assert(std::string(app.text().data())==document);
  std::filesystem::remove_all(root);
  std::cout<<"PASS: actual editor pixels: file/group/case, text/caret top, gutter, 7/9 rows and toggles\n";
}

#ifndef WRITER_HEADER_CAPACITY
#error Compile with the actual main.cpp header buffer capacity
#endif

static void *load(const char *path) {
  FILE *file = std::fopen(path, "rb");
  assert(file);
  assert(std::fseek(file, 0, SEEK_END) == 0);
  long size = std::ftell(file);
  assert(size > 0);
  std::rewind(file);
  void *data = std::malloc(static_cast<std::size_t>(size));
  assert(data);
  assert(std::fread(data, 1, static_cast<std::size_t>(size), file) ==
         static_cast<std::size_t>(size));
  assert(std::fclose(file) == 0);
  return data;
}

static unsigned checked = 0;
static void check(const std::string &name, bool dirty) {
  assert(name.size() < writer::FILE_NAME_SIZE - 5);
  char buffer[WRITER_HEADER_CAPACITY];
  writer::format(buffer, sizeof(buffer), "%s%s", dirty ? "* " : "",
                 name.c_str());
  alignas(2) unsigned char pixels[240 * 16] = {};
  // Same header origin, clipping width and production renderer as main.cpp.
  // ASan instruments the formatter, this buffer, and the renderer itself.
  draw_text_idx8_bus16_range(buffer, pixels + 8, 0, 128, 240, 1);
  const std::string expected = (dirty ? "* " : "") + name;
  assert(std::strcmp(buffer, expected.c_str()) == 0);
  ++checked;
}

int main(int argc, char **argv) {
  assert(argc == 3);
  font_base_addr = load(argv[1]);
  reader_font_base_addr = load(argv[2]);
  editor_positions();
  for(const char* label:{"abc","def","hij","klm","nop","qrs","tuw","xyz",
                         "ABC","DEF","HIJ","KLM","NOP","QRS","TUW","XYZ",
                         "Shift","Caps","UP/DOWN: FIELD  LEFT/RIGHT: +/-"}){
    unsigned limit=std::strlen(label)==3?32:std::strlen(label)<=5?48:224;
    unsigned width=font_width(label);assert(width<=limit);
    std::cout<<"WIDTH: "<<label<<" = "<<width<<" / "<<limit<<" pixels\n";
    alignas(2) unsigned char pixels[240*160]={};
    unsigned x=limit==32?144:limit==48?184:8;
    unsigned y=limit==224?108:144;
    draw_text_idx8_bus16_range(label,pixels+y*240+x,0,limit,240,1);
    bool ink=false;
    for(unsigned py=0;py<160;++py)for(unsigned px=0;px<240;++px){
      if(pixels[py*240+px]){ink=true;assert(py>=y&&py<y+16&&px>=x&&px<x+width);}
    }
    assert(ink);
  }
  std::string regression;
  for (int i = 0; i < 20; ++i)
    regression += "\xf0\x9f\x98\x80";
  check(regression + ".txt", false);
  check(regression + ".txt", true);
  for (const char *glyph : {"a", "é", "€", "\xf0\x9f\x98\x80"}) {
    std::string name;
    while (name.size() + std::strlen(glyph) + 4 < writer::FILE_NAME_SIZE - 5) {
      name += glyph;
      check(name + ".txt", false);
      check(name + ".txt", true);
    }
  }
  std::free(font_base_addr);
  std::free(reader_font_base_addr);
  std::cout << "PASS: " << checked
            << " complete clean/dirty filename headers through real renderer (ASan/UBSan)\n";
}
