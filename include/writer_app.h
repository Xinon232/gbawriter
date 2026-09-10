#pragma once
#include "writer_layout.h"
#include "writer_storage.h"
namespace writer {
enum class Scene { MENU, DATE, LOAD, EDITOR, HELP, ERROR, CREDITS };
class Application {
public:
  Application(Storage &storage, Layout::Width measure)
      : _storage(storage), _measure(measure) {}
  void boot();
  void frame(uint16_t held);
  Scene scene() const { return _scene; }
  int menu_selection() const { return _menu; }
  Date date() const { return _date; }
  int date_field() const { return _field; }
  int selected_file() const { return _file; }
  int help_page() const { return _help; }
  int credits_page() const { return _credits; }
  TextModel &text() { return _text; }
  Layout &layout() { return _layout; }
  const char *message() const { return _message; }
  bool sd_ready() const { return _ready; }
  bool shift() const { return _input.shift_armed(); }
  bool caps() const { return _input.caps(); }
  bool status_visible() const { return _status_visible; }
  const char* active_group() const { return _input.active_group(); }
  bool save_feedback(uint16_t held) const;
  int viewport() const { return _viewport; }
  int view_rows() const { return _status_visible ? VIEW_ROWS : FULL_VIEW_ROWS; }
  bool caret_visible() const { return _clock.visible(); }
  bool take_redraw() {
    bool r = _redraw;
    _redraw = false;
    return r;
  }
  static constexpr int HELP_PAGES = 20;
  static constexpr int CREDITS_PAGES = 2;

private:
  Storage &_storage;
  Layout::Width _measure;
  TextModel _text;
  Layout _layout;
  InputState _input;
  CaretClock _clock;
  Scene _scene = Scene::MENU, _return = Scene::MENU;
  int _menu = 0, _field = 0, _file = 0, _help = 0, _credits = 0, _viewport = 0,
      _message_frames = 0;
  Date _date{10, 7, 2026};
  uint16_t _previous = 0;
  bool _redraw = true, _ready = false, _wait_release = false,
       _provisional = false, _provisional_dirty = false, _status_visible = true;
  std::size_t _provisional_end = 0;
  const char *_message = "";
  static void event(void *context, InputEvent e) {
    static_cast<Application *>(context)->consume(e);
  }
  void consume(InputEvent e);
  void change(Scene scene);
  void error(StoreResult result);
  void editor();
  void ensure_visible();
  void adjust_date(int delta);
};
} // namespace writer
