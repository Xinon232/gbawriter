#include "writer_core.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
using namespace writer;

static void hold_caps(InputState& s) {
  for(int i=0;i<=48;++i)s.update(1u<<unsigned(Button::R),[](void*,InputEvent){},nullptr);
  s.update(0,[](void*,InputEvent){},nullptr);
}
static void dates() {
  Date d{};
  assert(parse_diary_name("29022028.TXT", d));
  assert(d.day == 29 && d.month == 2 && d.year == 2028);
  assert(!parse_diary_name("29022027.txt", d));
  assert(!parse_diary_name("1.txt", d));
  const char *names[] = {"12121990.txt", "21102001.txt", "bad.txt",
                         "31122000.txt"};
  assert(latest_diary_date(names, 4, d));
  assert(d.day == 21 && d.month == 10 && d.year == 2001);
  d = next_day(d);
  char name[13]{};
  format_diary_name(d, name);
  assert(!std::strcmp(name, "22102001.txt"));
  assert(can_create_new("01012001.txt", names, 4));
}
static void text() {
  TextModel t;
  assert(t.set_text("aé"));
  t.move_left();
  assert(t.backspace());
  assert(!std::strcmp(t.data(), "é"));
  t.move_right();
  assert(t.insert("ß"));
  assert(!std::strcmp(t.data(), "éß"));
}
static void input() {
  InputState s;
  InputEvent e{};
  e = s.press(Button::UP, true);
  e = s.press(Button::B, true);
  assert(e.kind == EventKind::INSERT && !std::strcmp(e.text, "a"));
  s.release(Button::UP);
  s.press(Button::RIGHT, true);
  s.press(Button::R, true);
  e = s.press(Button::R, true);
  assert(!std::strcmp(e.text, "g"));
  s.release(Button::RIGHT);
  s.release(Button::B);s.release(Button::R);
  s.press(Button::R, true);s.release(Button::R);
  assert(s.shift_armed());
  s.press(Button::L, true);
  s.press(Button::UP, true);
  e = s.press(Button::A, true);
  assert(!std::strcmp(e.text, "E"));
  s.release(Button::UP);s.release(Button::L);s.release(Button::A);
  hold_caps(s);
  assert(s.caps());
  s.press(Button::R, true);s.release(Button::R);
  assert(!s.caps() && !s.shift_armed());
  s.press(Button::SELECT, true);
  e = s.press(Button::L, true);
  s.press(Button::UP, true);
  e = s.press(Button::A, true);
  assert(!std::strcmp(e.text, "é"));
  e = s.press(Button::A, true);
  assert(!std::strcmp(e.text, "è"));
  s.release(Button::SELECT);
  s.press(Button::START, true);
  e = s.release(Button::START);
  assert(e.kind == EventKind::INSERT && !std::strcmp(e.text, "\n"));
}
static void select_cycles() {
  InputState s;
  s.press(Button::SELECT, true);
  const char *forward[] = {"1", "2", "3", "4", "5", "6",
                           "7", "8", "9", "0", "1"};
  for (auto v : forward) {
    auto e = s.press(Button::UP, true);
    assert(e.kind == EventKind::REPLACE && !std::strcmp(e.text, v));
    s.release(Button::UP);
  }
  s.release(Button::SELECT);
  s.press(Button::SELECT, true);
  for (char c : std::string("09876543210")) {
    auto e = s.press(Button::DOWN, true);
    assert(e.text[0] == c && !e.text[1]);
    s.release(Button::DOWN);
  }
  s.release(Button::SELECT);
  s.press(Button::SELECT, true);
  for (char c : std::string(",'\":!?.")) {
    auto e = s.press(Button::R, true);
    assert(e.text[0] == c);
    s.release(Button::R);
  }
  s.release(Button::SELECT);
  s.press(Button::SELECT, true);
  for (char c : std::string("?!:\"',.")) {
    auto e = s.press(Button::L, true);
    assert(e.text[0] == c);
    s.release(Button::L);
  }
  s.release(Button::SELECT);
  s.press(Button::SELECT, true);
  for (char c : std::string("()/;@#%&_+=-.")) {
    auto e = s.press(Button::RIGHT, true);
    assert(e.text[0] == c);
    s.release(Button::RIGHT);
  }
}
static void international() {
  struct Family {
    Button dir, key;
    bool layer;
    char base;
  };
  Family fs[] = {{Button::UP, Button::B, false, 'a'},
                 {Button::UP, Button::R, false, 'c'},
                 {Button::UP, Button::A, true, 'e'},
                 {Button::RIGHT, Button::A, false, 'i'},
                 {Button::DOWN, Button::B, false, 'n'},
                 {Button::DOWN, Button::A, false, 'o'},
                 {Button::DOWN, Button::R, true, 's'},
                 {Button::LEFT, Button::A, false, 'u'},
                 {Button::LEFT, Button::A, true, 'y'},
                 {Button::LEFT, Button::R, true, 'z'}};
  for (auto f : fs) {
    InputState s;
    s.press(Button::SELECT, true);
    if (f.layer)
      s.press(Button::L, true);
    s.press(f.dir, true);
    for (int i = 0; i < 20; ++i) {
      auto e = s.press(f.key, true);
      assert(e.kind == EventKind::REPLACE);
      assert(!std::strcmp(e.text, alternate_letter(f.base, i)));
      s.release(f.key);
    }
    s.release(f.dir);
    s.press(f.dir, true);
    auto e = s.press(f.key, true);
    assert(!std::strcmp(e.text, alternate_letter(f.base, 0)));
  }
  InputState s;
  s.press(Button::R, true);
  s.release(Button::R);
  s.press(Button::SELECT, true);
  s.press(Button::L, true);s.press(Button::UP, true);
  assert(!std::strcmp(s.press(Button::A, true).text, "É"));
  s.release(Button::A);
  assert(!std::strcmp(s.press(Button::A, true).text, "È"));
  s.release(Button::A);
  assert(s.shift_armed());
  s.release(Button::UP);s.release(Button::L);
  s.release(Button::SELECT);
  assert(!s.shift_armed());
  hold_caps(s);
  s.press(Button::L, true);
  s.press(Button::DOWN, true);
  assert(!std::strcmp(s.press(Button::R, true).text, "S"));
}
static void session_boundaries() {
  InputState s;
  s.press(Button::RIGHT, true);
  s.press(Button::R, true);
  s.release(Button::R);
  s.press(Button::A, true);
  s.release(Button::A);
  assert(s.press(Button::R, true).kind == EventKind::INSERT); // j i j, not j g
  InputState v;
  v.press(Button::LEFT, true);
  v.press(Button::R, true);
  v.release(Button::R);
  v.press(Button::L, true);
  assert(!std::strcmp(v.press(Button::R, true).text, "z"));
  InputState caps;
  hold_caps(caps);
  caps.press(Button::RIGHT, true);
  caps.press(Button::R, true);
  caps.release(Button::R);
  assert(!std::strcmp(caps.press(Button::R, true).text, "G"));
  InputState sel;
  sel.press(Button::SELECT, true);
  sel.press(Button::START, true);
  assert(sel.press(Button::A, true).kind == EventKind::NONE);
  assert(sel.release(Button::START).kind == EventKind::NONE);
}
static void bounded_text() {
  TextModel t;
  std::string full(TEXT_CAPACITY, 'x');
  assert(t.set_text(full.c_str()));
  assert(!t.replace_before_caret("é"));
  assert(!std::strcmp(t.data(), full.c_str()));
  assert(!t.dirty());
  assert(!t.set_text("\xc0\xaf"));
  assert(!t.insert("\xed\xa0\x80"));
  assert(!t.insert("\xf4\x90\x80\x80"));
  assert(!t.set_text("\xe2\x82"));
  assert(!t.set_text("\x80"));
  assert(t.set_text("é😀z"));
  t.move_home();
  t.move_right();
  assert(t.caret_byte() == 2);
  t.move_right();
  assert(t.caret_byte() == 6);
  t.backspace();
  assert(!std::strcmp(t.data(), "éz"));
  assert(!valid_date({1, 1, 10000}));
}
int main() {
  dates();
  text();
  input();
  select_cycles();
  international();
  session_boundaries();
  bounded_text();
  std::puts("PASS: writer core");
}
