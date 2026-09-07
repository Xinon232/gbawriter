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
  bool line_content = false, suppress = false;
  std::size_t word_end = 0;
  const char *s = text.data();
  for (std::size_t p = 0; p < text.bytes();) {
    // Look ahead once per word, including oversized words. Whitespace remains
    // in the row index and document; only the display boundary moves.
    if (p >= word_end && s[p] != ' ' && s[p] != '\t' &&
        s[p] != '\r' && s[p] != '\n') {
      int word_width = 0;
      word_end = p;
      while (word_end < text.bytes() && s[word_end] != ' ' &&
             s[word_end] != '\t' && s[word_end] != '\r' && s[word_end] != '\n') {
        char glyph[5];
        word_end = character(s, word_end, glyph);
        word_width += width(glyph);
      }
      if (x && word_width <= _width && x + word_width > _width) {
        _rows[_count++] = p;
        x = 0;
      }
    }
    char ch[5];
    std::size_t next = character(s, p, ch);
    if (ch[0] == '\n') {
      _rows[_count++] = next;
      x = 0;
      line_content = suppress = false;
    } else {
      int advance = width(ch);
      bool separator = ch[0] == ' ' || ch[0] == '\t';
      if (suppress && separator) {
        p = next;
        continue;
      }
      if (x && x + advance > _width) {
        _rows[_count++] = p;
        x = 0;
        if (separator && line_content) {
          _rows[_count - 1] |= SUPPRESS;
          suppress = true;
          advance = 0;
        }
      }
      if (!separator && advance) {
        line_content = true;
        suppress = false;
      }
      x += advance;
    }
    p = next;
  }
}
std::size_t Layout::row_content_start(TextModel &text, int row) const {
  std::size_t p = row_start(row);
  if (_rows[row] & SUPPRESS) {
    const char *s = text.data();
    while (p < text.bytes()) {
      char ch[5];
      std::size_t next = character(s, p, ch);
      if (ch[0] != ' ' && ch[0] != '\t' && ch[0] != '\r' &&
          std::strcmp(ch, "\xef\xbb\xbf"))
        break;
      p = next;
    }
  }
  return p;
}
VisualPosition Layout::position(TextModel &text, std::size_t byte) const {
  int low = 0, high = _count;
  while (low + 1 < high) {
    int mid = (low + high) / 2;
    if (row_start(mid) <= byte)
      low = mid;
    else
      high = mid;
  }
  int x = 0;
  const char *s = text.data();
  for (std::size_t p = row_content_start(text, low); p < byte;) {
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
  std::size_t best = row_start(row), p = row_content_start(text, row),
              end = row + 1 < _count ? row_start(row + 1) : text.bytes();
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
