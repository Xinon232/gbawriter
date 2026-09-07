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
class Layout {
public:
  using Width = int (*)(const char *);
  void reflow(TextModel &text, int width, Width measure);
  int rows() const { return _count; }
  std::size_t row_start(int row) const { return _rows[row]; }
  VisualPosition position(TextModel &text, std::size_t byte) const;
  bool move(TextModel &text, int rows);
  void reset_column() { _desired = -1; }
  static std::size_t character(const char *s, std::size_t p, char out[5]);
  int width(const char *glyph) const;

private:
  uint16_t _rows[TEXT_CAPACITY + 1] = {};
  int _count = 1, _width = 220, _desired = -1;
  Width _measure = nullptr;
};
} // namespace writer
