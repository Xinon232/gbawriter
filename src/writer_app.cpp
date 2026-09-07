#include "writer_app.h"
namespace writer {
namespace {
unsigned bit(Button b) { return 1u << unsigned(b); }
} // namespace
void Application::boot() {
  // SD probing is deliberately lazy: boot/menu/help remain usable without a card.
  _ready = false;
  _status_visible = true;
  _message = "";
  _redraw = true;
}
void Application::change(Scene s) {
  _scene = s;
  _redraw = true;
  _wait_release = true;
  _provisional = false;
}
void Application::error(StoreResult r) {
  _return = _scene;
  _message = store_message(r);
  change(Scene::ERROR);
}
void Application::editor() {
  _input = InputState();
  _clock = CaretClock();
  _viewport = 0;
  _message = "";
  _layout.reflow(_text, 220, _measure);
  ensure_visible();
  change(Scene::EDITOR);
}
void Application::ensure_visible() {
  int row = _layout.position(_text, _text.caret_byte()).row;
  if (row < _viewport)
    _viewport = row;
  if (row >= _viewport + view_rows())
    _viewport = row - view_rows() + 1;
}
void Application::adjust_date(int delta) {
  if (_field == 0) {
    _date.day += delta;
    if (_date.day < 1) {
      _date.day = 31;
      while (!valid_date(_date))
        --_date.day;
    } else if (!valid_date(_date))
      _date.day = 1;
  } else {
    if (_field == 1) {
      _date.month += delta;
      if (_date.month < 1)
        _date.month = 12;
      if (_date.month > 12)
        _date.month = 1;
    } else {
      _date.year += delta;
      if (_date.year < 1)
        _date.year = 9999;
      if (_date.year > 9999)
        _date.year = 1;
    }
    while (!valid_date(_date))
      --_date.day;
  }
}
void Application::consume(InputEvent e) {
  if (_scene != Scene::EDITOR)
    return;
  _clock.tick(true);
  using K = EventKind;
  bool edit = false, ok = true;
  switch (e.kind) {
  case K::TOGGLE_STATUS:
    if(_input.select_active() && _provisional && _text.caret_byte()==_provisional_end){
      edit=_text.backspace();
      if(!_provisional_dirty)_text.mark_saved();
    }
    _provisional=false;
    _status_visible=!_status_visible;
    break;
  case K::INSERT:
    if(_input.select_active())_provisional_dirty=_text.dirty();
    ok = _text.insert(e.text);
    edit = ok;
    if (_input.select_active()) {
      _provisional = ok;
      _provisional_end = _text.caret_byte();
    }
    break;
  case K::REPLACE:
    if (_input.select_active() &&
        (!_provisional || _text.caret_byte() != _provisional_end)) {
      _input.reject_edit();
      return;
    }
    ok = _text.replace_before_caret(e.text);
    edit = ok;
    if (_input.select_active() && ok)
      _provisional_end = _text.caret_byte();
    break;
  case K::BACKSPACE:
    edit = _text.backspace();
    break;
  case K::MOVE_LEFT:
    _text.move_left();
    _layout.reset_column();
    break;
  case K::MOVE_RIGHT:
    _text.move_right();
    _layout.reset_column();
    break;
  case K::MOVE_UP:
    _layout.move(_text, -1);
    break;
  case K::MOVE_DOWN:
    _layout.move(_text, 1);
    break;
  case K::PAGE_PREV:
    _layout.move(_text, -view_rows());
    _viewport = _viewport >= view_rows() ? _viewport - view_rows() : 0;
    break;
  case K::PAGE_NEXT:
    _layout.move(_text, view_rows());
    _viewport += view_rows();
    if (_viewport >= _layout.rows())
      _viewport = _layout.rows() - 1;
    break;
  case K::SAVE:
  case K::SAVE_MENU: {
    auto recovery = _storage.recover(_storage.current_name());
    if (recovery != StoreResult::OK) {
      error(recovery);
      break;
    }
    auto r = _storage.save(_text);
    if (r != StoreResult::OK) {
      error(r);
      break;
    }
    if (e.kind == K::SAVE_MENU) {
      _menu = 0;
      change(Scene::MENU);
    } else {
      _message = "SAVED";
      _message_frames = 120;
    }
    break;
  }
  default:
    break;
  }
  if (!ok) {
    _input.reject_edit();
    _message = "BUFFER FULL - TEXT KEPT";
    _message_frames = 180;
  }
  if (edit)
    _layout.reflow(_text, 220, _measure);
  ensure_visible();
  _redraw = true;
}
bool Application::save_feedback(uint16_t held) const {
  if(_wait_release)return false;
  unsigned pressed=held&~_previous;
  if(_scene==Scene::DATE)return pressed&bit(Button::A);
  return _scene==Scene::EDITOR && !_input.toggle_latched() &&
    !(held&bit(Button::SELECT)) &&
    (held&bit(Button::START)) && (pressed&(bit(Button::A)|bit(Button::B)));
}
void Application::frame(uint16_t held) {
  uint16_t pressed = held & ~_previous;
  _previous = held;
  if (_wait_release) {
    if (!held) {
      _wait_release = false;
      _input.reset_transient();
    }
    return;
  }
  auto p = [&](Button b) { return (pressed & bit(b)) != 0; };
  if (_scene == Scene::EDITOR) {
    bool visible = _clock.visible();
    _clock.tick(pressed != 0);
    if (visible != _clock.visible())
      _redraw = true;
    if (_message_frames && !--_message_frames) {
      _message = "";
      _redraw = true;
    }
    if (pressed)
      _redraw = true;
    bool shift = _input.shift_armed(), caps = _input.caps();
    const char* group = _input.active_group();
    _input.update(held, event, this);
    if(group != _input.active_group())_redraw=true;
    if(shift != _input.shift_armed() || caps != _input.caps())_redraw=true;
    return;
  }
  if (!pressed)
    return;
  _redraw = true;
  switch (_scene) {
  case Scene::MENU:
    if (p(Button::UP) || p(Button::DOWN))
      _menu = 1 - _menu;
    if (p(Button::SELECT)) {
      _help = 0;
      change(Scene::HELP);
    } else if (p(Button::START)) {
      change(Scene::CREDITS);
    } else if (p(Button::A)) {
      auto r = _ready ? _storage.scan() : _storage.init();
      _ready = r == StoreResult::OK;
      if (!_ready) {
        error(r);
        break;
      }
      if (_menu) {
        _file = 0;
        change(Scene::LOAD);
      } else {
        _date = _storage.proposed_date();
        _field = 0;
        change(Scene::DATE);
      }
    }
    break;
  case Scene::DATE:
    if (p(Button::UP))
      _field = (_field + 2) % 3;
    if (p(Button::DOWN))
      _field = (_field + 1) % 3;
    if (p(Button::RIGHT))
      adjust_date(1);
    if (p(Button::LEFT))
      adjust_date(-1);
    if (p(Button::B))
      change(Scene::MENU);
    else if (p(Button::A)) {
      char name[13];
      format_diary_name(_date, name);
      auto r = _storage.create(name, _text);
      if (r == StoreResult::OK)
        editor();
      else
        error(r);
    }
    break;
  case Scene::LOAD:
    if (p(Button::B)) {
      change(Scene::MENU);
      break;
    }
    if (p(Button::DOWN) && _storage.count()) {
      if (_file + 1 < _storage.count())
        ++_file;
      else {
        auto r = _storage.next_page();
        if (r != StoreResult::OK) {
          error(r);
          break;
        }
        if (_storage.count())
          _file = 0;
        else {
          r = _storage.previous_page();
          if (r != StoreResult::OK)
            error(r);
          _file = _storage.count() - 1;
        }
      }
    }
    if (p(Button::UP) && _storage.count()) {
      if (_file > 0)
        --_file;
      else {
        char first[FILE_NAME_SIZE];
        const char *n = _storage.name(0);
        for (int i = 0; (first[i] = n[i]); ++i) {
        }
        auto r = _storage.previous_page();
        if (r != StoreResult::OK) {
          error(r);
          break;
        }
        if (_storage.name(0)[0] && __builtin_strcmp(first, _storage.name(0)))
          _file = _storage.count() - 1;
      }
    }
    if (p(Button::A) && _storage.count()) {
      auto r = _storage.load(_storage.name(_file), _text);
      if (r == StoreResult::OK)
        editor();
      else
        error(r);
    }
    break;
  case Scene::HELP:
    if (p(Button::LEFT))
      _help = (_help + HELP_PAGES - 1) % HELP_PAGES;
    if (p(Button::RIGHT))
      _help = (_help + 1) % HELP_PAGES;
    if (p(Button::B))
      change(Scene::MENU);
    break;
  case Scene::CREDITS:
    if (p(Button::B))
      change(Scene::MENU);
    break;
  case Scene::ERROR:
    if (p(Button::A) || p(Button::B)) {
      Scene back = _return;
      _message = "";
      change(back);
    }
    break;
  default:
    break;
  }
}
} // namespace writer
