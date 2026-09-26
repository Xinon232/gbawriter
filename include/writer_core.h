#pragma once
#include <cstddef>
#include <cstdint>
#if !defined(__arm__)
#include <string>
#endif

namespace writer {
// Unsaved text held in RAM: bytes typed since the file was opened or last saved.
// The file itself stays on SD and has no size limit (32-bit offsets).
constexpr std::size_t TEXT_CAPACITY = 64 * 1024;
// Pieces describe the document as runs of file bytes and typed bytes.
constexpr int MAX_PIECES = 1024;
bool valid_utf8(const char *s, std::size_t n);
// Incremental valid_utf8 for files read in chunks (also rejects NUL).
class Utf8Stream {
public:
  bool feed(const char *s, std::size_t n);
  bool finish() const { return _ok && !_more; }

private:
  unsigned _more = 0, _value = 0, _minimum = 0;
  bool _ok = true;
};
struct Date {
  int day;
  int month;
  int year;
};
bool valid_date(Date date);
bool parse_diary_name(const char *name, Date &date);
void format_diary_name(Date date, char output[13]);
Date next_day(Date date);
bool latest_diary_date(const char *const *names, int count, Date &latest);
bool can_create_new(const char *name, const char *const *names, int count);

// Read-only bytes of the opened file (kept on SD while editing).
class TextSource {
public:
  virtual std::size_t size() const = 0;
  virtual bool read(std::size_t offset, char *out, std::size_t n) const = 0;

protected:
  ~TextSource() = default;
};

// Piece table: the document is a sequence of runs taken from the source file
// or from the RAM buffer of typed text. Only edits use RAM.
class TextModel {
public:
  TextModel();
  // RAM-only document (new file, tests); it counts against TEXT_CAPACITY.
  bool set_text(const char *utf8);
  // The whole source, unmodified and clean. The source must outlive its use.
  void open(const TextSource *source);
  bool insert(const char *utf8);
  bool replace_before_caret(const char *utf8);
  bool backspace();
  void move_left();
  void move_right();
  void move_home();
  void move_end();
  void set_caret(std::size_t position);
  // Byte at p (0 past the end or after a failed SD read, see read_failed()).
  char at(std::size_t p) const;
  // Copies up to n bytes from p; returns the count (short on end or read error).
  std::size_t copy(std::size_t p, char *out, std::size_t n) const;
  std::size_t bytes() const;
  std::size_t caret_byte() const;
  bool dirty() const;
  void mark_saved();
  void mark_dirty() { _dirty = true; }
  // Changes on every edit; layouts compare it to know when to rebuild.
  unsigned revision() const { return _revision; }
  // First byte the last edit changed.
  std::size_t last_edit_start() const { return _edit_start; }
  std::size_t unsaved_bytes() const { return _add_used; }
  int pieces() const { return _count; }
  bool read_failed() const { return _failed; }
#if !defined(__arm__)
  // Host tests (including the FatFS image test): the whole document as one string.
  std::string str() const {
    std::string s(_size, '\0');
    s.resize(copy(0, &s[0], _size));
    return s;
  }
#endif
  void clear_read_failed() { _failed = false; }

private:
  struct Piece {
    uint32_t start, length;
    bool added;
  };
  const TextSource *_source = nullptr;
  char _add[TEXT_CAPACITY];
  Piece _pieces[MAX_PIECES];
  int _count = 0;
  std::size_t _add_used = 0, _size = 0, _caret = 0, _edit_start = 0;
  bool _dirty = false;
  unsigned _revision = 0;
  mutable int _hint = 0;
  mutable std::size_t _hint_start = 0;
  mutable bool _failed = false;
  int find(std::size_t p, std::size_t &start) const;
  bool insert_at(std::size_t p, const char *s, std::size_t n);
  bool erase(std::size_t p, std::size_t n);
  void edited(std::size_t start);
  std::size_t previous_utf8(std::size_t p) const;
  std::size_t next_utf8(std::size_t p) const;
};

enum class Button : uint8_t {
  UP,
  DOWN,
  LEFT,
  RIGHT,
  A,
  B,
  L,
  R,
  START,
  SELECT
};
enum class EventKind : uint8_t {
  NONE,
  INSERT,
  REPLACE,
  BACKSPACE,
  MOVE_LEFT,
  MOVE_RIGHT,
  MOVE_UP,
  MOVE_DOWN,
  PAGE_PREV,
  PAGE_NEXT,
  SAVE,
  SAVE_MENU,
  TOGGLE_STATUS
};
struct InputEvent {
  EventKind kind;
  const char *text;
};
class InputState {
public:
  InputState();
  static constexpr int NAV_REPEAT_DELAY=24, NAV_REPEAT_INTERVAL=5;
  static constexpr int CAPS_HOLD_DELAY=2*NAV_REPEAT_DELAY;
  using Consumer=void(*)(void*,InputEvent);
  // Events and snapshots share held-session edges. A fresh press is time zero;
  // each subsequent update advances one elapsed frame (events do not tick).
  void update(uint16_t held,Consumer consume,void* context);
  InputEvent press(Button button, bool just_pressed);
  InputEvent release(Button button);
  bool shift_armed() const { return _shift; }
  bool caps() const { return _caps; }
  // Host provisional ownership: an existing-letter session owns no insertion
  // to cancel on Start+Select, and must bypass the provisional replacement gate.
  bool select_active() const {return _select && !_select_existing;}
  const char* active_group() const;
  bool toggle_latched() const { return _toggle_latched; }
  void reset_transient(){bool shift=_shift,caps=_caps;*this=InputState();_shift=shift;_caps=caps;}
  void reject_edit(){_letter_keys=0;_group_r_count=0;_rejected=true;}

private:
  uint16_t _letter_keys=0;
  char _letter_base=0;
  bool _letter_upper=false, _select_existing=false;
  bool _toggle_latched=false;
  uint16_t _held;
  uint16_t _previous=0;
  bool _r_pending=false,_group_upper=false,_rejected=false;
  int _r_frames=0;
  uint16_t _repeat_keys=0;
  int _repeat_frames=0;
  bool _shift, _caps, _start_used, _select;
  int _group_r_count;
  char _select_value[5];
  char _alternate_base;
  int _alternate_index = 0;
  bool _select_alpha = false;
  char _output[5] = {};
  bool held(Button b) const;
  void set(Button b, bool on);
  InputEvent letter_event(char base, bool select);
  const char *case_text(const char *lower, char base);
};
const char *alternate_letter(char base, int index);
} // namespace writer
