#pragma once
#include <cstddef>
#include <cstdint>

namespace writer {
constexpr std::size_t TEXT_CAPACITY = 24 * 1024;
bool valid_utf8(const char *s, std::size_t n);
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

class TextModel {
public:
  TextModel();
  bool set_text(const char *utf8);
  bool insert(const char *utf8);
  bool replace_before_caret(const char *utf8);
  bool backspace();
  void move_left();
  void move_right();
  void move_home();
  void move_end();
  void set_caret(std::size_t position);
  const char *data();
  std::size_t bytes() const;
  std::size_t caret_byte() const;
  bool dirty() const;
  void mark_saved();

private:
  char _storage[TEXT_CAPACITY + 1];
  std::size_t _gap_begin, _gap_end, _size;
  bool _dirty;
  void move_gap(std::size_t position);
  static std::size_t previous_utf8(const char *s, std::size_t p);
  static std::size_t next_utf8(const char *s, std::size_t n, std::size_t p);
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
  using Consumer=void(*)(void*,InputEvent);
  void update(uint16_t held,Consumer consume,void* context);
  InputEvent press(Button button, bool just_pressed);
  InputEvent release(Button button);
  bool shift_armed() const { return _shift; }
  bool caps() const { return _caps; }
  bool select_active() const {return _select;}
  const char* active_group() const;
  bool toggle_latched() const { return _toggle_latched; }
  void reset_transient(){bool shift=_shift,caps=_caps;*this=InputState();_shift=shift;_caps=caps;}
  void reject_edit(){_group_r_count=0;_rejected=true;}

private:
  bool _toggle_latched=false;
  uint16_t _held;
  uint16_t _previous=0;
  bool _r_pending=false,_group_upper=false,_rejected=false;
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
