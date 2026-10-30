#pragma once
#include <cstdint>

// gbamp3-style list navigation, from gbavocab V4.3 (include/entries_nav.h).
// Modelled on gbamp3 v1.8 (player/src/input.c and ui.c long lists): one row
// per press, then a row every 8 frames after 24 frames of holding; Left/Right
// move a page (the cursor keeps its screen row), repeating every 30 frames;
// a fresh press past either end wraps; L + Up/Down steps by first letter.
namespace writer {
namespace list {

constexpr int ROWS = 8;            // visible rows under the 16 px header
constexpr int ROW_Y = 16, ROW_H = 17;
constexpr unsigned LIST_DELAY = 24, ROW_REPEAT = 8, PAGE_REPEAT = 30, LETTER_REPEAT = 30;

enum Key : uint16_t { UP = 1, DOWN = 2, LEFT = 4, RIGHT = 8, A = 16, B = 32, L = 64, R = 128, START = 256, SELECT = 512 };

// Steps requested by one frame of held keys (gbamp3 input_frame, elapsed 1).
struct Steps {
    uint16_t pressed = 0, held = 0;
    uint8_t rows[2] = {}, pages[2] = {}, letters[2] = {};   // [0] up/left, [1] down/right
    bool fast = false;                                      // a hold is repeating
};

class Input {
public:
    void reset(uint16_t held) { previous_ = held; for (auto& f : frames_) f = 0; }
    Steps frame(uint16_t held) {
        Steps s;
        s.held = held;
        s.pressed = held & ~previous_;
        const uint16_t keys[4] = {UP, DOWN, LEFT, RIGHT};
        unsigned before[4];
        for (int i = 0; i < 4; ++i) {
            before[i] = frames_[i];
            frames_[i] = (held & keys[i]) ? (frames_[i] < 60000 ? frames_[i] + 1 : frames_[i]) : 0;
        }
        bool l = held & L;
        for (int j = 0; j < 2; ++j) {
            unsigned p = (s.pressed & keys[j]) ? 1 : 0, h = frames_[j], h0 = p ? 0 : before[j];
            unsigned rows = p, letters = p;
            if (held & keys[j]) {
                rows += ticks(h, ROW_REPEAT) - ticks(h0, ROW_REPEAT);
                letters += ticks(h, LETTER_REPEAT) - ticks(h0, LETTER_REPEAT);
            }
            if (l) s.letters[j] = uint8_t(letters); else s.rows[j] = uint8_t(rows);
            unsigned q = (s.pressed & keys[2 + j]) ? 1 : 0, g = frames_[2 + j], g0 = q ? 0 : before[2 + j];
            unsigned pages = q;
            if (held & keys[2 + j]) pages += ticks(g, PAGE_REPEAT) - ticks(g0, PAGE_REPEAT);
            s.pages[j] = l ? 0 : uint8_t(pages);
            if (frames_[j] > LIST_DELAY || frames_[2 + j] > LIST_DELAY) s.fast = true;
        }
        previous_ = held;
        return s;
    }
private:
    static unsigned ticks(unsigned h, unsigned period) { return h <= LIST_DELAY ? 0 : (h - LIST_DELAY) / period; }
    uint16_t previous_ = 0;
    unsigned frames_[4] = {};
};

// Cursor and first visible row over count rows. Rows for which skip() is
// true (box dividers) are shown but never selected.
class Nav {
public:
    using Skip = bool (*)(void* context, uint32_t row);
    Nav(uint32_t count = 0, Skip skip = nullptr, void* context = nullptr) { reset(count, skip, context); }
    void reset(uint32_t count, Skip skip, void* context) {
        count_ = count; skip_ = skip; context_ = context; sel_ = top_ = 0;
        if (count_) sel_ = forward(0);
    }
    // Visible rows (7 when a bottom line is shown).
    void set_rows(int rows) { rows_ = rows; visible(); }
    int rows() const { return rows_; }
    uint32_t count() const { return count_; }
    uint32_t sel() const { return sel_; }
    uint32_t top() const { return top_; }
    bool empty() const { return !count_ || skipped(sel_); }
    // Moves; true when the cursor or the page changed.
    bool step(int direction, bool fresh) {
        if (empty()) return false;
        uint32_t old = sel_, oldtop = top_;
        if (direction > 0) {
            uint32_t n = next(sel_);
            if (n == sel_ && fresh) n = forward(0);   // wrap only on a fresh press
            sel_ = n;
        } else {
            uint32_t p = previous(sel_);
            if (p == sel_ && fresh) p = backward(count_ - 1);
            sel_ = p;
        }
        visible();
        return sel_ != old || top_ != oldtop;
    }
    // A page: the list moves by the visible rows and the cursor keeps its screen row.
    bool page(int direction) {
        if (empty() || count_ <= uint32_t(rows_)) {
            // Short lists: first / last row.
            uint32_t old = sel_;
            sel_ = direction > 0 ? backward(count_ - 1) : forward(0);
            visible();
            return sel_ != old;
        }
        uint32_t max = count_ - rows_, row = sel_ - top_, old = top_;
        uint32_t t = direction > 0 ? (top_ + rows_ < max ? top_ + rows_ : max) : (top_ > uint32_t(rows_) ? top_ - rows_ : 0);
        if (t == old) return false;
        top_ = t;
        sel_ = top_ + row;
        if (skipped(sel_)) { uint32_t n = next(sel_); sel_ = n != sel_ ? n : previous(sel_); }
        visible();
        return true;
    }
    // Select row t and show it on the top row as far as the list allows.
    void select_on_top(uint32_t t) {
        if (empty() || t >= count_) return;
        sel_ = skipped(t) ? forward(t) : t;
        uint32_t max = count_ > uint32_t(rows_) ? count_ - rows_ : 0;
        top_ = sel_ < max ? sel_ : max;
        if (top_ == sel_ && top_ && skipped(top_ - 1)) --top_;   // keep its box divider above
    }
    void select(uint32_t t) {
        if (empty()) return;
        if (t >= count_) t = count_ - 1;
        sel_ = skipped(t) ? (forward(t) != t ? forward(t) : backward(t)) : t;
        visible();
    }
    void resize(uint32_t count) {
        uint32_t keep = sel_;
        count_ = count;
        if (!count_) { sel_ = top_ = 0; return; }
        if (keep >= count_) keep = count_ - 1;
        if (top_ >= count_) top_ = 0;
        sel_ = keep;
        if (skipped(sel_)) sel_ = next(sel_) != sel_ ? next(sel_) : previous(sel_);
        visible();
    }
private:
    bool skipped(uint32_t i) const { return skip_ && i < count_ && skip_(context_, i); }
    uint32_t forward(uint32_t i) const { while (i < count_ && skipped(i)) ++i; return i < count_ ? i : (count_ ? count_ - 1 : 0); }
    uint32_t backward(uint32_t i) const { for (;;) { if (!skipped(i)) return i; if (!i) return forward(0); --i; } }
    uint32_t next(uint32_t i) const { for (uint32_t j = i + 1; j < count_; ++j) if (!skipped(j)) return j; return i; }
    uint32_t previous(uint32_t i) const { for (uint32_t j = i; j-- > 0;) if (!skipped(j)) return j; return i; }
    void visible() {
        if (sel_ < top_) top_ = sel_;
        if (sel_ >= top_ + rows_) top_ = sel_ - rows_ + 1;
        // Show the divider just above the first entry of a box.
        if (top_ == sel_ && top_ && skipped(top_ - 1)) --top_;
        if (top_ == 1 && skipped(0)) top_ = 0;
    }
    uint32_t count_ = 0, sel_ = 0, top_ = 0;
    int rows_ = ROWS;
    Skip skip_ = nullptr;
    void* context_ = nullptr;
};

} // namespace list
} // namespace writer
