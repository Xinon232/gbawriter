#include "writer_app.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
using namespace writer;
static unsigned key(Button b) { return 1u << unsigned(b); }
static int width(const char *) { return 6; }
int main() {
  std::string root = "/tmp/writer-app-" + std::to_string(getpid());
  std::filesystem::remove_all(root);
  std::filesystem::create_directory(root);
  Storage storage(root.c_str());
  Application a(storage, width);
  a.boot();
  assert(storage.operations()==0); // menu and help must not block on SD detection
  assert(a.scene() == Scene::MENU && a.menu_selection() == 0);
  auto tap = [&](Button b) {
    a.frame(key(b));
    a.frame(0);
  };
  tap(Button::START);
  assert(a.scene() != Scene::MENU && a.scene() != Scene::HELP);
  assert(storage.operations() == 0);
  tap(Button::A);
  assert(a.scene() != Scene::MENU); // only B returns
  tap(Button::B);
  assert(a.scene() == Scene::MENU && storage.operations() == 0);
  tap(Button::A);
  assert(a.scene() == Scene::DATE);
  assert(a.date().day == 10 && a.date().month == 7 && a.date().year == 2026);
  tap(Button::DOWN);
  tap(Button::RIGHT);
  assert(a.date().month == 8);
  tap(Button::A);
  assert(a.scene() == Scene::EDITOR);
  assert(std::filesystem::exists(root + "/gbawriter/10082026.txt"));
  a.frame(key(Button::UP) | key(Button::B));
  a.frame(0);
  assert(!strcmp(a.text().data(), "a"));
  a.frame(key(Button::START) | key(Button::B));
  a.frame(0);
  assert(a.scene() == Scene::MENU);
  tap(Button::A);
  assert(a.date().day == 11);
  tap(Button::B);
  tap(Button::DOWN);
  tap(Button::A);
  assert(a.scene() == Scene::LOAD);
  tap(Button::A);
  assert(a.scene() == Scene::EDITOR && std::string(a.text().data()) == "a");
  tap(Button::A);
  storage.fault_at(1);
  a.frame(key(Button::START) | key(Button::B));
  a.frame(0);
  assert(a.scene() == Scene::ERROR);
  tap(Button::A);
  assert(a.scene() == Scene::EDITOR);
  assert(a.text().dirty() && std::string(a.text().data()) == "a ");
  storage.fault_at(-1);
  std::string full(TEXT_CAPACITY, 'x');
  a.text().set_text(full.c_str());
  a.frame(key(Button::SELECT));
  a.frame(key(Button::SELECT) | key(Button::UP));
  a.frame(0);
  assert(std::string(a.text().data()) == full);
  // Holding navigation repeats, never newline/text; activity keeps caret solid.
  a.text().set_text(std::string(200, 'a').c_str());
  a.layout().reflow(a.text(),220,width);
  auto before=a.text().caret_byte();
  for(int i=0;i<120;++i)a.frame(key(Button::START)|key(Button::LEFT));
  assert(a.text().caret_byte()<before-1 && a.caret_visible());
  a.frame(0);
  assert(a.text().bytes()==200);
  // Isolated R commits on release, which must redraw the indicator.
  a.frame(key(Button::R));a.take_redraw();a.frame(0);
  assert(a.shift() && a.take_redraw());
  tap(Button::R);assert(!a.shift()&&!a.caps());
  a.frame(key(Button::R));
  for(int i=1;i<48;++i)a.frame(key(Button::R));
  a.take_redraw();a.frame(key(Button::R));
  assert(a.caps()&&!a.shift()&&a.take_redraw());
  a.frame(0);
  storage.fault_at(1);a.frame(key(Button::START)|key(Button::B));a.frame(0);
  tap(Button::A);assert(a.scene()==Scene::EDITOR && a.caps());
  storage.fault_at(-1);
  tap(Button::R);assert(!a.caps());tap(Button::R);assert(a.shift());
  a.text().set_text(full.c_str());a.layout().reflow(a.text(),220,width);
  a.frame(key(Button::UP)|key(Button::B));a.frame(0);
  assert(a.shift() && a.text().bytes()==TEXT_CAPACITY);
  a.text().set_text(std::string(TEXT_CAPACITY-1,'x').c_str());a.layout().reflow(a.text(),220,width);
  a.frame(key(Button::SELECT));a.frame(key(Button::SELECT)|key(Button::RIGHT));
  a.frame(key(Button::SELECT)|key(Button::RIGHT)|key(Button::A));a.frame(0);
  assert(a.shift() && a.text().data()[TEXT_CAPACITY-1]=='(');
  // Held spaces stop safely at capacity without consuming Shift.
  a.frame(0);a.text().set_text(std::string(TEXT_CAPACITY-2,'x').c_str());
  a.layout().reflow(a.text(),220,width);
  for(int i=0;i<100;++i)a.frame(key(Button::A));
  assert(a.text().bytes()==TEXT_CAPACITY && a.shift() && a.caret_visible());
  assert(a.text().data()[TEXT_CAPACITY-2]==' ' && a.text().data()[TEXT_CAPACITY-1]==' ');
  a.frame(0);
  // UTF-8 backspace repeats at complete codepoint boundaries, including empty.
  a.text().set_text("aé€😀");a.layout().reflow(a.text(),220,width);
  a.frame(key(Button::B));assert(std::string(a.text().data())=="aé€");
  for(int i=1;i<24;++i)a.frame(key(Button::B));
  assert(std::string(a.text().data())=="aé€");
  a.frame(key(Button::B));assert(std::string(a.text().data())=="aé");
  for(int i=0;i<5;++i)a.frame(key(Button::B));
  assert(std::string(a.text().data())=="a");
  for(int i=0;i<100;++i)a.frame(key(Button::B));
  assert(!a.text().bytes() && !a.text().caret_byte() && a.shift());
  a.frame(0);a.text().set_text("aé€");a.text().set_caret(3);
  a.layout().reflow(a.text(),220,width);
  for(int i=0;i<100;++i)a.frame(key(Button::B));
  assert(std::string(a.text().data())=="€" && !a.text().caret_byte());
  a.frame(0);a.frame(key(Button::A));assert(std::string(a.text().data())==" €");
  a.frame(0);
  std::filesystem::remove_all(root);
  std::cout
      << "PASS: writer app storage/UI tracer workflow and failure safety\n";
}
