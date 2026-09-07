#include "writer_layout.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using namespace writer;
static int calls = 0;
static int width(const char *s) { ++calls; return s[0] == 'i' ? 2 : 6; }
static void wrapping_edges() {
  TextModel text;
  Layout layout;
  auto check = [&](const char* source, int pixels, std::vector<std::size_t> rows) {
    assert(text.set_text(source));
    text.set_caret(0);
    bool dirty = text.dirty();
    calls = 0;
    layout.reflow(text, pixels, width);
    assert(calls <= int(text.bytes() * 2)); // linear, including oversized words
    assert(text.caret_byte() == 0 && text.dirty() == dirty);
    assert(std::string(text.data()) == source);
    assert(layout.rows() == int(rows.size()));
    for (int row = 0; row < layout.rows(); ++row) {
      assert(layout.row_start(row) == rows[row]);
      auto pos = layout.position(text, rows[row]);
      assert(pos.row == row && pos.x == 0);
      std::size_t end = row + 1 < layout.rows() ? rows[row + 1] : text.bytes();
      int x = 0;
      for (std::size_t p = rows[row]; p < end;) {
        auto caret = layout.position(text, p);
        assert(caret.row == row && caret.x == x);
        char ch[5]; p = Layout::character(text.data(), p, ch);
        if (ch[0] != '\n') x += layout.width(ch);
      }
      assert(x <= pixels);
    }
    // Every row start remains reachable through production vertical navigation.
    text.move_home(); layout.reset_column();
    for (int row=1; row<layout.rows(); ++row) {
      assert(layout.move(text, 1)); assert(text.caret_byte()==rows[row]);
    }
    for (int row=layout.rows()-2; row>=0; --row) {
      assert(layout.move(text, -1)); assert(text.caret_byte()==rows[row]);
    }
    assert(!layout.move(text, -1));
    assert(std::string(text.data()) == source);
  };
  check("aa bbb", 24, {0,3});
  check("aa bb", 30, {0}); // exact fit
  check("aii bbb", 24, {0,4}); // proportional-width word push
  check("aa bbbbb", 24, {0,4}); // only an oversized word splits
  check("aaaa b", 24, {0,4}); // whitespace is retained on the next row
  check("aa   bb", 24, {0,4});
  check("aa\tbb", 24, {0,2,3});
  check("aa bbb\r\n\n", 24, {0,3,8,9});
  check("\xef\xbb\xbf" "aa bbb", 24, {0,6});
  check("éé €😀", 24, {0,5}); // 2/3/4-byte code points, no split UTF-8
  check("é€😀é€", 12, {0,5,11});
  check("", 24, {0});
  check("\n", 24, {0,1});
  std::string max_word(TEXT_CAPACITY, 'a');
  text.set_text(max_word.c_str()); calls=0; layout.reflow(text,220,width);
  assert(calls == int(TEXT_CAPACITY*2));
  assert(std::string(text.data()) == max_word);
  std::string newlines(TEXT_CAPACITY, '\n');
  text.set_text(newlines.c_str()); layout.reflow(text,220,width);
  assert(layout.rows()==int(TEXT_CAPACITY+1));
  for (std::size_t p=0;p<=TEXT_CAPACITY;++p) {
    assert(layout.row_start(int(p))==p);
    assert(layout.position(text,p).row==int(p));
  }
  text.set_text("aa bbb\naa bbb");layout.reflow(text,24,width);text.set_caret(1);
  assert(layout.move(text,1) && text.caret_byte()==4);
  assert(layout.move(text,1) && text.caret_byte()==8);
  assert(layout.move(text,-2) && text.caret_byte()==1);
  text.set_caret(3);text.insert("i");layout.reflow(text,24,width);
  assert(layout.row_start(1)==3 && layout.position(text,text.caret_byte()).x==2);
  text.backspace();layout.reflow(text,24,width);
  assert(std::string(text.data())=="aa bbb\naa bbb" && layout.row_start(1)==3);
}
int main() {
  wrapping_edges();
  TextModel t;
  t.set_text("aébc\nx\n\n");
  Layout l;
  l.reflow(t, 12, width);
  assert(l.rows() == 5);
  assert(l.position(t, 0).row == 0);
  assert(l.position(t, 3).row == 1);
  assert(l.position(t, 3).x == 0);
  assert(l.position(t, 5).row == 1);
  assert(l.position(t, 5).x == 12);
  assert(l.position(t, t.bytes()).row == 4);
  t.move_home();
  t.move_right();
  assert(l.move(t, 1) == true);
  assert(t.caret_byte() == 4);
  assert(l.move(t, 1));
  assert(t.caret_byte() == 7);
  assert(l.move(t, -1));
  assert(t.caret_byte() == 4);
  assert(!strcmp(t.data(), "aébc\nx\n\n"));
  t.set_text("aii\nx\naii");
  l.reflow(t, 20, width);
  t.move_home();
  t.move_right();
  t.move_right();
  assert(l.move(t, 1));
  assert(l.move(t, 1));
  assert(t.caret_byte() == 8); // desired x=8 retained
  // A word fitting the viewport moves intact; wrapping never edits bytes.
  t.set_text("aa bbb");
  l.reflow(t, 24, width);
  assert(l.rows() == 2 && l.row_start(1) == 3);
  assert(l.position(t, 3).row == 1 && l.position(t, 3).x == 0);
  assert(!strcmp(t.data(), "aa bbb"));
  CaretClock c;
  for (int i = 0; i < 60; ++i) {
    assert(c.visible());
    c.tick(false);
  }
  assert(c.visible());
  for (int i = 0; i < 36; ++i)
    c.tick(false);
  assert(!c.visible());
  c.tick(true);
  assert(c.visible());
  std::cout << "PASS: writer visual layout and caret clock\n";
}
