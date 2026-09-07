#include "writer_layout.h"
#include <cstdlib>
#include <cstring>
namespace writer {
std::size_t Layout::character(const char *s, std::size_t p, char out[5]) {
  unsigned c = static_cast<unsigned char>(s[p]);
  std::size_t n = c < 128 ? 1 : c < 224 ? 2 : c < 240 ? 3 : 4;
  std::memcpy(out, s + p, n);
  out[n] = 0;
  return p + n;
}
int Layout::width(const char *s) const {
  if (s[0] == '\r' || !std::strcmp(s, "\xef\xbb\xbf"))
    return 0;
  if (s[0] == '\t')
    return 24;
  return _measure(s);
}
void Layout::reflow(TextModel &text, int w, Width measure) {
  _width = w;
  _desired = -1;
  _measure = measure;
  _rows[0] = 0;
  _count = 1;
  int x = 0;
  const char *s = text.data();
  for (std::size_t p = 0; p < text.bytes();) {
    char ch[5];
    std::size_t next = character(s, p, ch);
    if (ch[0] == '\n') {
      _rows[_count++] = next;
      x = 0;
    } else {
      int advance = width(ch);
      if (x && x + advance > _width) {
        _rows[_count++] = p;
        x = 0;
      }
      x += advance;
    }
    p = next;
  }
}
VisualPosition Layout::position(TextModel &text, std::size_t byte) const {
  int low = 0, high = _count;
  while (low + 1 < high) {
    int mid = (low + high) / 2;
    if (_rows[mid] <= byte)
      low = mid;
    else
      high = mid;
  }
  int x = 0;
  const char *s = text.data();
  for (std::size_t p = _rows[low]; p < byte;) {
    char ch[5];
    p = character(s, p, ch);
    if (ch[0] != '\n')
      x += width(ch);
  }
  return {low, x};
}
bool Layout::move(TextModel &text, int delta) {
  VisualPosition pos = position(text, text.caret_byte());
  if (_desired < 0)
    _desired = pos.x;
  int row = pos.row + delta;
  if (row < 0)
    row = 0;
  if (row >= _count)
    row = _count - 1;
  if (row == pos.row)
    return false;
  std::size_t best = _rows[row], p = best,
              end = row + 1 < _count ? _rows[row + 1] : text.bytes();
  int x = 0, distance = std::abs(_desired);
  const char *s = text.data();
  for (;;) {
    int d = std::abs(x - _desired);
    if (d < distance) {
      best = p;
      distance = d;
    }
    if (p >= end || s[p] == '\n')
      break;
    char ch[5];
    std::size_t next = character(s, p, ch);
    x += width(ch);
    p = next;
    if (p == end && row + 1 < _count)
      break;
  }
  text.set_caret(best);
  return true;
}
} // namespace writer
