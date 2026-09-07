#include "writer_layout.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace writer;
static int width(const char *s) { return s[0] == 'i' ? 2 : 6; }
int main() {
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
