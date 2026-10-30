#include "writer_layout.h"
#include <cstdlib>
#include <cstring>
namespace writer {
namespace {
std::size_t utf8_length(unsigned c) { return c < 128 ? 1 : c < 224 ? 2 : c < 240 ? 3 : 4; }
bool blank(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
} // namespace
std::size_t Layout::character(const char *s, std::size_t p, char out[5]) {
  std::size_t n = utf8_length(static_cast<unsigned char>(s[p]));
  std::memcpy(out, s + p, n);
  out[n] = 0;
  return p + n;
}
std::size_t Layout::character(const TextModel &text, std::size_t p, char out[5]) {
  std::size_t n = utf8_length(static_cast<unsigned char>(text.at(p)));
  if (p + n > text.bytes())
    n = text.bytes() - p;
  n = text.copy(p, out, n);
  out[n] = 0;
  return p + (n ? n : 1);
}
int Layout::width(const char *s) const {
  if (s[0] == '\r' || !std::strcmp(s, "\xef\xbb\xbf"))
    return 0;
  if (s[0] == '\t')
    return 24;
  return _measure(s);
}
// A line start before anchor with at least MARGIN_ROWS rows in between, when
// it is within SEARCH_BYTES. Rows are counted low: one per line break plus one
// per 56 bytes of a line (no row holds more than 220 / 4 px glyphs). A line
// longer than the search falls back to a word start on a fixed 4 KiB grid, so
// the window start (and so the rows) stays put while typing in that line.
std::size_t Layout::window_start(TextModel &text, std::size_t anchor) const {
  std::size_t p = anchor, lowest = anchor > SEARCH_BYTES ? anchor - SEARCH_BYTES : 0,
              found = std::size_t(-1), line_bytes = 0;
  int rows = 0;
  while (p > lowest) {
    if (text.at(p - 1) == '\n') {
      found = p;
      rows += 1 + int(line_bytes / 56);
      line_bytes = 0;
      if (rows > MARGIN_ROWS)
        return p;
    } else
      ++line_bytes;
    --p;
  }
  if (!lowest)
    return 0;
  if (found != std::size_t(-1))
    return found;
  std::size_t start = lowest & ~std::size_t(4095);
  for (std::size_t q = start; q < start + 256 && q < anchor; ++q)
    if (text.at(q) == ' ')
      return q + 1;
  while (start && (static_cast<unsigned char>(text.at(start)) & 0xc0) == 0x80)
    --start;
  return start;
}
// Same whole-word wrapping as before, from begin until MARGIN_ROWS rows after
// the row holding `last` (or the document end, or MAX_ROWS rows).
void Layout::build(TextModel &text, std::size_t begin, std::size_t last) {
  _rows[0] = uint32_t(begin);
  _count = 1;
  _end = text.bytes();
  int last_row = 0;
  auto push = [&](std::size_t start, bool suppress) {
    if (start > last && _count - last_row > MARGIN_ROWS) {
      _end = start;
      return false;
    }
    if (_count == MAX_ROWS) {
      _end = start;
      return false;
    }
    if (start <= last)
      last_row = _count;
    _rows[_count++] = uint32_t(start) | (suppress ? SUPPRESS : 0);
    return true;
  };
  int x = 0;
  bool line_content = false, suppress = false;
  std::size_t word_end = 0;
  const std::size_t size = text.bytes();
  for (std::size_t p = begin; p < size;) {
    char c = text.at(p);
    // Look ahead once per word, including oversized words. Whitespace remains
    // in the row index and document; only the display boundary moves.
    if (p >= word_end && !blank(c)) {
      int word_width = 0;
      word_end = p;
      while (word_end < size && !blank(text.at(word_end))) {
        char glyph[5];
        std::size_t next = character(text, word_end, glyph);
        // Past the row width the word splits anyway; only find its end.
        if (word_width <= _width)
          word_width += width(glyph);
        word_end = next;
      }
      if (x && word_width <= _width && x + word_width > _width) {
        if (!push(p, false))
          return;
        x = 0;
      }
    }
    char ch[5];
    std::size_t next = character(text, p, ch);
    if (ch[0] == '\n') {
      if (!push(next, false))
        return;
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
        bool hide = separator && line_content;
        if (!push(p, hide))
          return;
        x = 0;
        if (hide) {
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
void Layout::reflow(TextModel &text, int w, Width measure) {
  reflow(text, w, measure, text.caret_byte());
}
void Layout::reflow(TextModel &text, int w, Width measure, std::size_t top) {
  _width = w;
  _desired = -1;
  _measure = measure;
  std::size_t caret = text.caret_byte();
  if (top > text.bytes())
    top = text.bytes();
  std::size_t first = top < caret ? top : caret, last = top < caret ? caret : top;
  build(text, window_start(text, first), last);
  _revision = text.revision();
}
bool Layout::covers(TextModel &text, std::size_t top) const {
  if (_revision != text.revision() || !_measure)
    return false;
  std::size_t caret = text.caret_byte();
  std::size_t first = top < caret ? top : caret, last = top < caret ? caret : top;
  if (first < row_start(0) || last > _end || (last == _end && _end < text.bytes()))
    return false;
  const int page = FULL_VIEW_ROWS;
  return (row_start(0) == 0 || row_of(first) >= page) &&
         (_end == text.bytes() || _count - 1 - row_of(last) >= page);
}
int Layout::row_of(std::size_t byte) const {
  int low = 0, high = _count;
  while (low + 1 < high) {
    int mid = (low + high) / 2;
    if (row_start(mid) <= byte)
      low = mid;
    else
      high = mid;
  }
  return low;
}
std::size_t Layout::row_content_start(TextModel &text, int row) const {
  std::size_t p = row_start(row);
  if (_rows[row] & SUPPRESS) {
    while (p < text.bytes()) {
      char ch[5];
      std::size_t next = character(text, p, ch);
      if (ch[0] != ' ' && ch[0] != '\t' && ch[0] != '\r' &&
          std::strcmp(ch, "\xef\xbb\xbf"))
        break;
      p = next;
    }
  }
  return p;
}
VisualPosition Layout::position(TextModel &text, std::size_t byte) const {
  int row = row_of(byte);
  int x = 0;
  for (std::size_t p = row_content_start(text, row); p < byte;) {
    char ch[5];
    p = character(text, p, ch);
    if (ch[0] != '\n')
      x += width(ch);
  }
  return {row, x};
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
              end = row_end(row);
  int x = 0, distance = std::abs(_desired);
  for (;;) {
    int d = std::abs(x - _desired);
    if (d < distance) {
      best = p;
      distance = d;
    }
    if (p >= end || text.at(p) == '\n')
      break;
    char ch[5];
    std::size_t next = character(text, p, ch);
    x += width(ch);
    p = next;
    if (p == end && (row + 1 < _count || end < text.bytes()))
      break;
  }
  text.set_caret(best);
  return true;
}
} // namespace writer
