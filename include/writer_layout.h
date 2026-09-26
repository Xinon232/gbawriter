#pragma once
#include "writer_core.h"
namespace writer {
constexpr int CARET_IDLE_FRAMES = 60, CARET_BLINK_FRAMES = 36;
constexpr int TEXT_Y = 0, TEXT_HEIGHT = 16, TEXT_PITCH = 18;
constexpr int STATUS_Y = 144, STATUS_GAP = 6;
constexpr int VIEW_ROWS = (STATUS_Y-STATUS_GAP-TEXT_HEIGHT)/TEXT_PITCH+1;
constexpr int FULL_VIEW_ROWS = (160-TEXT_HEIGHT)/TEXT_PITCH+1;
struct VisualPosition {
  int row, x;
};
class CaretClock {
public:
  void tick(bool activity) {
    if (activity)
      _idle = 0;
    else if (_idle < CARET_IDLE_FRAMES + CARET_BLINK_FRAMES * 2)
      ++_idle;
    else
      _idle = CARET_IDLE_FRAMES + 1;
  }
  bool visible() const {
    return _idle < CARET_IDLE_FRAMES ||
           ((_idle - CARET_IDLE_FRAMES) / CARET_BLINK_FRAMES) % 2 == 0;
  }

private:
  int _idle = 0;
};
// Rows for a window of the document only: from a line start a little before
// the top row / caret to a few pages after, rebuilt as the view moves.
class Layout {
public:
  using Width = int (*)(const char *);
  // Rows around the caret (and `top`, the first byte of the top visible row).
  void reflow(TextModel &text, int width, Width measure);
  void reflow(TextModel &text, int width, Width measure, std::size_t top);
  // Rows are current for this text and cover top and the caret with a page to spare.
  bool covers(TextModel &text, std::size_t top) const;
  int rows() const { return _count; }
  std::size_t row_start(int row) const { return _rows[row] & ~SUPPRESS; }
  // Start of the next row; the last row ends where the window ends.
  std::size_t row_end(int row) const { return row + 1 < _count ? row_start(row + 1) : _end; }
  // Row holding byte (0 for bytes before the window).
  int row_of(std::size_t byte) const;
  // First displayed byte; suppressed separators remain in the byte row index.
  std::size_t row_content_start(TextModel &text, int row) const;
  VisualPosition position(TextModel &text, std::size_t byte) const;
  bool move(TextModel &text, int rows);
  void reset_column() { _desired = -1; }
  static std::size_t character(const char *s, std::size_t p, char out[5]);
  static std::size_t character(const TextModel &text, std::size_t p, char out[5]);
  int width(const char *glyph) const;
  // Rows kept before the top row and after the caret row (two full pages).
  static constexpr int MARGIN_ROWS = 2 * FULL_VIEW_ROWS;
  static constexpr int MAX_ROWS = 2048;
  // How far back a window start is searched for a line start. Lines up to this
  // long wrap exactly as a whole-document layout would; the search stops early
  // once enough rows are certain, so ordinary text only scans a few hundred bytes.
  static constexpr std::size_t SEARCH_BYTES = TEXT_CAPACITY + 8 * 1024;

private:
  static constexpr uint32_t SUPPRESS = 0x80000000u;
  uint32_t _rows[MAX_ROWS] = {};
  int _count = 1, _width = 220, _desired = -1;
  std::size_t _end = 0;
  unsigned _revision = ~0u;
  Width _measure = nullptr;
  std::size_t window_start(TextModel &text, std::size_t anchor) const;
  void build(TextModel &text, std::size_t begin, std::size_t last);
};
} // namespace writer
