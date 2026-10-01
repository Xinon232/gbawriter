// A file far larger than the typed-text buffer, driven through the real app:
// open from Load File, page through the whole document with START+L/R, type
// at both ends and save. The file streams from disk; only edits use RAM.
#include "writer_app.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
using namespace writer;
static unsigned key(Button b) { return 1u << unsigned(b); }
static int width(const char *s) { return s[0] == 'i' ? 3 : (static_cast<unsigned char>(s[0]) >= 0x80 ? 12 : 7); }
static std::string read_all(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  std::stringstream s;
  s << f.rdbuf();
  return s.str();
}
// What the editor draws: visible rows are contiguous document bytes, the
// caret is on one of them, and no row is wider than the text area.
static void check_view(Application &a, const std::string &doc) {
  Layout &l = a.layout();
  TextModel &t = a.text();
  assert(l.rows() >= 1 && l.rows() <= Layout::MAX_ROWS);
  int top = a.viewport(), last = top + a.view_rows();
  if (last > l.rows())
    last = l.rows();
  for (int row = top; row < last; ++row) {
    std::size_t start = l.row_start(row), end = l.row_end(row);
    assert(start <= end && end <= doc.size());
    if (row + 1 < last)
      assert(end == l.row_start(row + 1));
    int x = 0;
    for (std::size_t p = l.row_content_start(t, row); p < end;) {
      char ch[5];
      p = Layout::character(t, p, ch);
      if (ch[0] != '\n')
        x += l.width(ch);
    }
    assert(x <= 220 + 24);
  }
  auto caret = l.position(t, t.caret_byte());
  assert(caret.row >= top && caret.row < top + a.view_rows());
  assert(l.row_start(caret.row) <= t.caret_byte() && t.caret_byte() <= l.row_end(caret.row));
  assert(!t.read_failed());
}
int main() {
  std::string root = "/tmp/writer-large-" + std::to_string(getpid());
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root + "/gbawriter");
  std::string doc;
  for (int i = 0; doc.size() < 300000; ++i) {
    doc += "Entry " + std::to_string(i) + u8": écrit à la main, 日本語 😀 and a fairly long line of words";
    doc += i % 5 ? " " : "\n";
    if (i % 23 == 0)
      doc += "\n\n";
  }
  { std::ofstream(root + "/gbawriter/big.txt", std::ios::binary) << doc; }
  Storage storage(root.c_str());
  Application a(storage, width);
  a.boot();
  auto tap = [&](Button b) { a.frame(key(b)); a.frame(0); };
  auto chord = [&](Button hold, Button press) {
    a.frame(key(hold));
    a.frame(key(hold) | key(press));
    a.frame(key(hold));
    a.frame(0);
  };
  // V4.0 Home: New File, then the file.
  tap(Button::DOWN);
  assert(a.scene() == Scene::HOME && storage.count() == 1);
  tap(Button::A);
  assert(a.scene() == Scene::EDITOR);
  TextModel &t = a.text();
  assert(t.bytes() == doc.size() && t.unsaved_bytes() == 0 && !t.dirty());
  assert(t.caret_byte() == doc.size());
  check_view(a, doc);
  // Page to the start, checking the view at every page.
  // Paging stops on the first row at the remembered column; Left finishes.
  int pages = 0;
  for (std::size_t before = ~std::size_t(0); t.caret_byte() != before && pages < 20000; ++pages) {
    before = t.caret_byte();
    chord(Button::START, Button::L);
    check_view(a, doc);
  }
  assert(pages > 1000 && t.caret_byte() < 200 && a.layout().row_start(0) == 0 && a.viewport() == 0);
  while (t.caret_byte()) {
    chord(Button::START, Button::LEFT);
    check_view(a, doc);
  }
  // Down again row by row for a while, then page to the end.
  for (int i = 0; i < 200; ++i) {
    std::size_t before = t.caret_byte();
    chord(Button::START, Button::DOWN);
    check_view(a, doc);
    assert(t.caret_byte() > before);
  }
  int back = 0;
  for (std::size_t before = ~std::size_t(0); t.caret_byte() != before && back < 20000; ++back) {
    before = t.caret_byte();
    chord(Button::START, Button::R);
    check_view(a, doc);
  }
  assert(back > 1000 && doc.size() - t.caret_byte() < 200);
  while (t.caret_byte() < doc.size()) {
    chord(Button::START, Button::RIGHT);
    check_view(a, doc);
  }
  // Type at the end (UP + B = "a", A alone = space) and save with START+A.
  chord(Button::UP, Button::B);
  tap(Button::A);
  assert(t.dirty() && t.bytes() == doc.size() + 2 && t.unsaved_bytes() == 2);
  std::string expected = doc + "a ";
  assert(t.str() == expected);
  chord(Button::START, Button::A);
  assert(a.scene() == Scene::EDITOR && !t.dirty() && t.unsaved_bytes() == 0);
  assert(read_all(root + "/gbawriter/big.txt") == expected);
  assert(t.str() == expected && t.caret_byte() == expected.size());
  check_view(a, expected);
  // Page back to the start and type there; the typed buffer only holds the edit.
  for (std::size_t before = ~std::size_t(0); t.caret_byte() != before;) {
    before = t.caret_byte();
    chord(Button::START, Button::L);
  }
  while (t.caret_byte())
    chord(Button::START, Button::LEFT);
  chord(Button::UP, Button::B);
  expected = "a" + expected;
  assert(t.str() == expected && t.unsaved_bytes() == 1);
  check_view(a, expected);
  chord(Button::START, Button::A);
  assert(!t.dirty() && read_all(root + "/gbawriter/big.txt") == expected);
  for (const char *staging : {".gwt", ".gwb", ".gwi"})
    assert(!std::filesystem::exists(root + "/gbawriter/big.txt" + staging));
  std::filesystem::remove_all(root);
  std::cout << "PASS: " << doc.size() << "-byte file paged end to end (" << pages
            << " pages), edited at both ends and saved through the app\n";
}
