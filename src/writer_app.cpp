#include "writer_app.h"
#include "writer_format.h"
#include <cstring>
namespace writer {
namespace {
unsigned bit(Button b) { return 1u << unsigned(b); }
constexpr unsigned A_KEY = 1u << unsigned(Button::A), B_KEY = 1u << unsigned(Button::B),
                   UP_KEY = 1u << unsigned(Button::UP), DOWN_KEY = 1u << unsigned(Button::DOWN),
                   LEFT_KEY = 1u << unsigned(Button::LEFT), RIGHT_KEY = 1u << unsigned(Button::RIGHT),
                   START_KEY = 1u << unsigned(Button::START), SELECT_KEY = 1u << unsigned(Button::SELECT);
// Select menu actions.
enum { ACT_RENAME, ACT_DELETE, ACT_FORMAT, ACT_STATUS, ACT_TOPIC };
// Longest name typed in Rename: the file name (with .txt) stays under 251 bytes.
constexpr std::size_t RENAME_MAX = FILE_NAME_SIZE - 10;
void copy_base(const char *name, char *out, std::size_t cap) {
  std::size_t n = std::strlen(name);
  if (n > 4 && name[n - 4] == '.')
    n -= 4;
  if (n >= cap)
    n = cap - 1;
  std::memcpy(out, name, n);
  out[n] = 0;
}
std::size_t utf8_previous(const char *s, std::size_t p) {
  if (!p)
    return 0;
  do
    --p;
  while (p && (static_cast<unsigned char>(s[p]) & 0xc0) == 0x80);
  return p;
}
std::size_t utf8_next(const char *s, std::size_t p) {
  if (!s[p])
    return p;
  ++p;
  while (s[p] && (static_cast<unsigned char>(s[p]) & 0xc0) == 0x80)
    ++p;
  return p;
}
bool home_skip(void *context, uint32_t row) {
  return row == 1 && !static_cast<Storage *>(context)->count();
}
} // namespace
void Application::boot() {
  _ready = false;
  _status_visible = true;
  _message = "";
  _scene = Scene::HOME;
  _need_scan = true;
  _home.reset(2, home_skip, &_storage);
  _redraw = true;
}
void Application::busy(const char *text, int percent) {
  if (_busy)
    _busy(_busy_context, text, percent);
}
void Application::flash(const char *text) {
  _note = text;
  _note_frames = 120;
  _redraw = true;
}
void Application::change(Scene s) {
  _scene = s;
  _note_frames = 0;
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
  // V4.1: Select menu > Helper line decides how a file opens; Start+Select
  // still toggles it while writing.
  _status_visible = _storage.settings().status_bar;
  _input = InputState();
  _clock = CaretClock();
  _message = "";
  _top = _text.caret_byte();
  _layout.reflow(_text, 220, _measure, _top);
  _top = _layout.row_start(_layout.row_of(_top));
  ensure_visible();
  _active = true;
  change(Scene::EDITOR);
}
// Back into the open text: caret, view and Shift/Caps as they were.
void Application::resume() {
  _clock = CaretClock();
  _message = "";
  change(Scene::EDITOR);
  ensure_visible();
}
void Application::leave_editor() {
  _message = "";
  _message_frames = 0;
  change(Scene::HOME);
  rescan(_storage.current_name());
}
// Rows exist only around the view: lay them out again when the text changed
// or the view neared the window edge.
void Application::cover() {
  if (!_layout.covers(_text, _top))
    _layout.reflow(_text, 220, _measure, _top);
}
int Application::date_part(int row) const {
  int order[3];
  date_field_order(_storage.settings().format, order);
  return order[row < 0 || row > 2 ? 0 : row];
}
void Application::ensure_visible() {
  cover();
  int row = _layout.position(_text, _text.caret_byte()).row;
  int top = _layout.row_of(_top);
  if (row < top)
    top = row;
  if (row >= top + view_rows())
    top = row - view_rows() + 1;
  _top = _layout.row_start(top);
  cover();
  check_read();
}
// A failed SD read of the open file cannot show text; report it (text kept).
void Application::check_read() {
  if (_text.read_failed() && _scene == Scene::EDITOR) {
    _text.clear_read_failed();
    error(StoreResult::IO_ERROR);
  }
}
void Application::adjust_date(int delta) {
  const int part = date_part(_field);
  if (part == 0) {
    _date.day += delta;
    if (_date.day < 1) {
      _date.day = 31;
      while (!valid_date(_date))
        --_date.day;
    } else if (!valid_date(_date))
      _date.day = 1;
  } else {
    if (part == 1) {
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
  cover();
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
  case K::PAGE_PREV: {
    int top = _layout.row_of(_top);
    _layout.move(_text, -view_rows());
    _top = _layout.row_start(top >= view_rows() ? top - view_rows() : 0);
    break;
  }
  case K::PAGE_NEXT: {
    int top = _layout.row_of(_top) + view_rows();
    _layout.move(_text, view_rows());
    _top = _layout.row_start(top < _layout.rows() ? top : _layout.rows() - 1);
    break;
  }
  case K::SAVE: {
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
    _message = "SAVED";
    _message_frames = 120;
    break;
  }
  case K::SAVE_MENU:
    // V4.0: Start+B goes Home without saving; the text stays open.
    leave_editor();
    return;
  default:
    break;
  }
  if (!ok) {
    _input.reject_edit();
    // Unsaved typed text is full (or the edit list is): saving frees both.
    _message = "BUFFER FULL - SAVE";
    _message_frames = 180;
  }
  if (edit) {
    if (_text.last_edit_start() < _top)
      _top = _text.last_edit_start();
    _layout.reflow(_text, 220, _measure, _top);
  }
  ensure_visible();
  _redraw = true;
}
bool Application::save_feedback(uint16_t held) const {
  if (_wait_release)
    return false;
  unsigned pressed = held & ~_previous;
  if (_scene == Scene::DATE)
    return pressed & bit(Button::A);
  return _scene == Scene::EDITOR && !_input.toggle_latched() && !(held & bit(Button::SELECT)) &&
         (held & bit(Button::START)) && (pressed & bit(Button::A));
}
// ---- Home ----
bool Application::ensure_sd() {
  if (_ready)
    return true;
  busy("Checking SD...");
  auto r = _storage.init();
  _ready = r == StoreResult::OK;
  if (!_ready) {
    error(r);
    return false;
  }
  rescan();
  return true;
}
void Application::rescan(const char *select) {
  _need_scan = false;
  if (_storage.mounted()) {
    auto r = _storage.scan();
    if (r != StoreResult::OK) {
      _home.reset(2, home_skip, &_storage);
      _home.set_rows(_active ? list::ROWS - 1 : list::ROWS);
      error(r);
      return;
    }
  }
  const int files = _storage.mounted() ? _storage.count() : 0;
  _home.reset(uint32_t(1 + (files ? files : 1)), home_skip, &_storage);
  _home.set_rows(_active ? list::ROWS - 1 : list::ROWS);
  if (select && select[0])
    for (int i = 0; i < files; ++i)
      if (!std::strcmp(_storage.name(i), select)) {
        _home.select(uint32_t(i + 1));
        break;
      }
  _redraw = true;
}
const char *Application::list_footer() const {
  return _scene == Scene::HOME && _active ? "B: Resume active file" : nullptr;
}
void Application::home_frame(uint16_t held, uint16_t pressed) {
  list::Steps s = _keys.frame(held);
  const uint16_t released = _previous_home & ~held;
  _previous_home = held;
  if (held & A_KEY) {
    // Hold A + Up/Down: move the selected file (New File stays first).
    for (int j = 0; j < 2; ++j)
      for (unsigned n = 0; n < s.rows[j]; ++n) {
        _a_moved = true;
        const int f = file_row(), to = j ? f + 1 : f - 1;
        if (!_storage.settings().reorder || f < 0 || to < 0 || to >= _storage.count())
          continue;
        _storage.move(f, to);
        _home.select(uint32_t(to + 1));
        _order_dirty = true;
        _redraw = true;
      }
  } else {
    for (int j = 0; j < 2; ++j)
      for (unsigned n = 0; n < s.rows[j]; ++n) {
        bool fresh = !n && (s.pressed & (j ? list::DOWN : list::UP));
        if (_home.step(j ? 1 : -1, fresh))
          _redraw = true;
      }
    for (int j = 0; j < 2; ++j)
      for (unsigned n = 0; n < s.pages[j]; ++n)
        if (_home.page(j ? 1 : -1))
          _redraw = true;
  }
  if (released & A_KEY) {
    const bool moved = _a_moved;
    _a_moved = false;
    if (_order_dirty) {
      _order_dirty = false;
      auto r = _storage.save_order();
      if (r != StoreResult::OK)
        flash("Order not saved");
    }
    if (!moved)
      home_activate();
    return;
  }
  if (held & A_KEY)
    return;
  if (pressed & B_KEY) {
    if (_active)
      resume();
  } else if (pressed & START_KEY) {
    if (ensure_sd())
      browse("/");
  } else if (pressed & SELECT_KEY) {
    _menu_file = _storage.mounted() && file_row() < _storage.count() ? file_row() : -1;
    open_list(ListKind::MENU);
  }
}
void Application::home_activate() {
  if (!ensure_sd())
    return;
  const int f = file_row();
  if (f < 0) {
    _pending_new = true;
    _pending[0] = 0;
  } else {
    if (f >= _storage.count())
      return;
    if (_active && _storage.is_open(_storage.name(f))) {
      resume();
      return;
    }
    _pending_new = false;
    std::strcpy(_pending, _storage.name(f));
  }
  if (_active && _text.dirty()) {
    open_list(ListKind::UNSAVED);
    return;
  }
  if (_pending_new)
    new_file();
  else
    open_file(_pending);
}
void Application::new_file() {
  _date = _storage.proposed_date();
  _field = 0;
  change(Scene::DATE);
}
void Application::open_file(const char *name) {
  busy("Opening file...");
  auto r = _storage.load(name, _text);
  if (r == StoreResult::OK)
    editor();
  else
    error(r);
}
// ---- Lists: Select menu, Secret Settings, import browser, questions ----
void Application::open_list(ListKind kind, int select) {
  _kind = kind;
  _confirm = -1;
  _list.reset(uint32_t(list_rows_for(kind)), nullptr, nullptr);
  _list.set_rows(list::ROWS);
  _list.select(uint32_t(select));
  change(Scene::LIST);
}
int Application::list_rows_for(ListKind kind) const {
  switch (kind) {
  case ListKind::MENU:
    return (_menu_file >= 0 ? (_storage.settings().delete_files ? 2 : 1) : 0) + 2 + HELP_TOPICS;
  case ListKind::SECRET:
    return 3;
  case ListKind::IMPORT:
    return _storage.browse_count() ? _storage.browse_count() : 1;
  case ListKind::UNSAVED:
    return 3;
  default:
    return 2;
  }
}
int Application::list_rows() const {
  return _scene == Scene::HOME ? int(_home.count()) : list_rows_for(_kind);
}
int Application::menu_row(int row) const {
  if (_menu_file >= 0) {
    if (row == 0)
      return ACT_RENAME;
    if (_storage.settings().delete_files && row == 1)
      return ACT_DELETE;
    row -= _storage.settings().delete_files ? 2 : 1;
  }
  return row == 0 ? ACT_FORMAT : row == 1 ? ACT_STATUS : ACT_TOPIC + row - 2;
}
const char *Application::list_title(char (&out)[64]) const {
  if (_scene == Scene::HOME)
    return "gbawriter";
  switch (_kind) {
  case ListKind::MENU:
    if (_menu_file < 0)
      return "gbawriter";
    {
      char base[FILE_NAME_SIZE];
      copy_base(_storage.name(_menu_file), base, sizeof(base));
      writer::format(out, sizeof(out), "%s", base);
      return out;
    }
  case ListKind::SECRET:
    return "Secret Settings";
  case ListKind::IMPORT: {
    if (!_browse[1])
      return "Import to /gbawriter";
    // A long folder path keeps its end.
    const std::size_t n = std::strlen(_browse);
    if (n < 36)
      return _browse;
    const char *tail = _browse + n - 32;
    while ((static_cast<unsigned char>(*tail) & 0xc0) == 0x80)
      ++tail;
    writer::format(out, sizeof(out), "...%s", tail);
    return out;
  }
  case ListKind::IMPORT_ASK:
    return "Import into /gbawriter?";
  case ListKind::NUMBERED_ASK:
    return "Name already used";
  default:
    return "Unsaved changes";
  }
}
bool Application::list_row(int row, char (&out)[FILE_NAME_SIZE + 32]) const {
  out[0] = 0;
  if (_scene == Scene::HOME) {
    if (row == 0) {
      std::strcpy(out, "New File");
      return true;
    }
    if (!_storage.mounted() || !_storage.count()) {
      std::strcpy(out, !_storage.mounted() ? (_need_scan ? "" : "No SD card") : "No TXT files in /gbawriter");
      return false;
    }
    copy_base(_storage.name(row - 1), out, sizeof(out));
    return true;
  }
  if (_confirm == row) {
    std::strcpy(out, "Sure?");
    return true;
  }
  switch (_kind) {
  case ListKind::MENU:
    switch (menu_row(row)) {
    case ACT_RENAME:
      std::strcpy(out, "Rename");
      break;
    case ACT_DELETE:
      std::strcpy(out, "Delete");
      break;
    case ACT_FORMAT: {
      // V4.2: always the same example date; day 13 cannot be a month, so
      // the order of day and month is clear in every format.
      char name[DIARY_NAME_SIZE];
      format_diary_name(Date{13, 7, 2026}, _storage.settings().format, name);
      name[std::strlen(name) - 4] = 0;
      writer::format(out, sizeof(out), "File names: %s", name);
      break;
    }
    case ACT_STATUS:
      std::strcpy(out, _storage.settings().status_bar ? "Helper line: On" : "Helper line: Off");
      break;
    default:
      std::strcpy(out, help_topic_name(menu_row(row) - ACT_TOPIC));
    }
    return true;
  case ListKind::SECRET:
    if (row == 0)
      std::strcpy(out, _storage.settings().reorder ? "Reorder list (A+DPAD): On" : "Reorder list (A+DPAD): Off");
    else if (row == 1)
      std::strcpy(out, _storage.settings().delete_files ? "Delete files: On" : "Delete files: Off");
    else
      std::strcpy(out, "Delete configuration");
    return true;
  case ListKind::IMPORT:
    if (!_storage.browse_count()) {
      std::strcpy(out, "No folders or TXT files");
      return false;
    }
    writer::format(out, sizeof(out), "%s", _storage.browse_name(row));
    return true;
  case ListKind::IMPORT_ASK:
    std::strcpy(out, row ? "Yes" : "No");
    return true;
  case ListKind::NUMBERED_ASK:
    if (row)
      writer::format(out, sizeof(out), "Yes, as %s", _import_target);
    else
      std::strcpy(out, "No");
    return true;
  default: {
    static const char *const rows[] = {"Save", "Discard", "Cancel"};
    std::strcpy(out, rows[row < 3 ? row : 2]);
    return true;
  }
  }
}
bool Application::list_underline(int row) const {
  if (_scene == Scene::HOME)
    return row == 0;
  return _scene == Scene::LIST && _kind == ListKind::MENU && _confirm != row && menu_row(row) == ACT_STATUS;
}
RowIcon Application::list_icon(int row) const {
  if (_scene == Scene::HOME) {
    if (row == 0)
      return RowIcon::NEW_FILE;
    if (_active && _storage.mounted() && row - 1 < _storage.count() &&
        _storage.is_open(_storage.name(row - 1)))
      return RowIcon::PLAY;
    return RowIcon::NONE;
  }
  if (_scene == Scene::LIST && _kind == ListKind::IMPORT && _storage.browse_is_folder(row))
    return RowIcon::FOLDER;
  return RowIcon::NONE;
}
void Application::list_frame(uint16_t held, uint16_t pressed) {
  list::Steps s = _keys.frame(held);
  const uint32_t before = _list.sel();
  for (int j = 0; j < 2; ++j)
    for (unsigned n = 0; n < s.rows[j]; ++n) {
      bool fresh = !n && (s.pressed & (j ? list::DOWN : list::UP));
      _list.step(j ? 1 : -1, fresh);
    }
  for (int j = 0; j < 2; ++j)
    for (unsigned n = 0; n < s.pages[j]; ++n)
      _list.page(j ? 1 : -1);
  if (_list.sel() != before) {
    _confirm = -1; // moving cancels "Sure?"
    _redraw = true;
  }
  if (pressed & B_KEY) {
    if (_confirm >= 0) {
      _confirm = -1;
      _redraw = true;
    } else
      list_back();
  } else if (pressed & A_KEY) {
    if (!_list.empty())
      list_activate(int(_list.sel()));
  }
}
void Application::list_back() {
  switch (_kind) {
  case ListKind::MENU:
  case ListKind::UNSAVED:
    change(Scene::HOME);
    break;
  case ListKind::SECRET:
    _topic = CREDITS_TOPIC;
    _page = help_topic_pages(_topic) - 1;
    change(Scene::PAGES);
    break;
  case ListKind::IMPORT: {
    if (!_browse[1]) {
      change(Scene::HOME);
      rescan();
      break;
    }
    char leaf[FILE_NAME_SIZE];
    char *slash = _browse;
    for (char *c = _browse; *c; ++c)
      if (*c == '/')
        slash = c;
    writer::format(leaf, sizeof(leaf), "%s", slash + 1);
    char parent[BROWSE_PATH_SIZE];
    std::memcpy(parent, _browse, std::size_t(slash - _browse));
    parent[slash - _browse] = 0;
    if (!parent[0])
      std::strcpy(parent, "/");
    browse(parent, leaf);
    break;
  }
  default:
    open_list(ListKind::IMPORT, _import_row);
  }
}
void Application::browse(const char *path, const char *select) {
  busy("Reading folder...");
  writer::format(_browse, sizeof(_browse), "%s", path);
  auto r = _storage.browse(_browse);
  int row = 0;
  if (select)
    for (int i = 0; i < _storage.browse_count(); ++i)
      if (!std::strcmp(_storage.browse_name(i), select))
        row = i;
  open_list(ListKind::IMPORT, row);
  if (r != StoreResult::OK)
    error(r);
}
namespace {
void import_progress(void *context, int percent) {
  auto *app = static_cast<Application *>(context);
  app->import_percent(percent);
}
} // namespace
void Application::import_percent(int percent) { busy("Importing...", percent); }
void Application::import_now() {
  busy("Importing...", 0);
  auto r = _storage.import_file(_import_source, _import_target, import_progress, this);
  if (r != StoreResult::OK) {
    browse(_browse);
    error(r);
    return;
  }
  // Home with the new file (at the top of the list) selected.
  change(Scene::HOME);
  rescan(_import_target);
  flash("Imported");
}
void Application::list_activate(int row) {
  switch (_kind) {
  case ListKind::MENU: {
    const int act = menu_row(row);
    if (act == ACT_RENAME || act == ACT_DELETE) {
      const char *n = _storage.name(_menu_file);
      if (_storage.is_open(n)) {
        error(StoreResult::OPEN_FILE);
        return;
      }
      if (act == ACT_RENAME) {
        copy_base(n, _name, sizeof(_name));
        _name_caret = std::strlen(_name);
        _name_provisional = false;
        _name_input = InputState();
        _note_frames = 0;
        change(Scene::RENAME);
        return;
      }
      if (_confirm != row) {
        _confirm = row;
        _redraw = true;
        return;
      }
      auto r = _storage.delete_file(_menu_file);
      if (r != StoreResult::OK) {
        _confirm = -1;
        error(r);
        return;
      }
      change(Scene::HOME);
      // The row now at the deleted file's place (copied: rescan rebuilds the names).
      const int f = _menu_file < _storage.count() ? _menu_file : _storage.count() - 1;
      char next[FILE_NAME_SIZE] = {};
      if (f >= 0)
        std::strcpy(next, _storage.name(f));
      rescan(next);
      flash("Deleted");
      return;
    }
    if (act == ACT_FORMAT) {
      Settings s = _storage.settings();
      s.format = NameFormat((int(s.format) + 1) % NAME_FORMATS);
      auto r = _storage.set_settings(s);
      if (r == StoreResult::OK)
        r = _storage.scan(); // the proposed date follows the format
      _redraw = true;
      if (r != StoreResult::OK)
        error(r);
      return;
    }
    if (act == ACT_STATUS) {
      Settings s = _storage.settings();
      s.status_bar = !s.status_bar;
      auto r = _storage.set_settings(s);
      if (r == StoreResult::OK)
        _status_visible = s.status_bar; // also for the open file
      _redraw = true;
      if (r != StoreResult::OK)
        error(r);
      return;
    }
    _topic = act - ACT_TOPIC;
    _page = 0;
    change(Scene::PAGES);
    return;
  }
  case ListKind::SECRET: {
    Settings s = _storage.settings();
    if (row == 2) {
      if (_confirm != row) {
        _confirm = row;
        _redraw = true;
        return;
      }
      _confirm = -1;
      auto r = _storage.delete_configuration();
      if (r == StoreResult::OK)
        r = _storage.scan();
      if (r != StoreResult::OK) {
        error(r);
        return;
      }
      rescan();
      flash("Configuration deleted");
      return;
    }
    if (row == 0)
      s.reorder = !s.reorder;
    else
      s.delete_files = !s.delete_files;
    auto r = _storage.set_settings(s);
    _redraw = true;
    if (r != StoreResult::OK)
      error(r);
    return;
  }
  case ListKind::IMPORT: {
    if (!_storage.browse_count())
      return;
    const char *n = _storage.browse_name(row);
    char next[BROWSE_PATH_SIZE];
    const int len = writer::format(next, sizeof(next), "%s%s%s", _browse, _browse[1] ? "/" : "", n);
    if (len >= int(sizeof(next))) {
      error(StoreResult::INVALID_NAME);
      return;
    }
    if (_storage.browse_is_folder(row)) {
      browse(next);
      return;
    }
    _import_row = row;
    std::strcpy(_import_source, next);
    writer::format(_import_target, sizeof(_import_target), "%s", n);
    open_list(ListKind::IMPORT_ASK, 0);
    return;
  }
  case ListKind::IMPORT_ASK: {
    if (!row) {
      open_list(ListKind::IMPORT, _import_row);
      return;
    }
    char source_name[FILE_NAME_SIZE];
    std::strcpy(source_name, _import_target);
    auto r = _storage.import_name(source_name, _import_target);
    if (r == StoreResult::OK)
      import_now();
    else if (r == StoreResult::NAME_USED)
      open_list(ListKind::NUMBERED_ASK, 0);
    else {
      open_list(ListKind::IMPORT, _import_row);
      if (r == StoreResult::EXISTS) {
        _return = Scene::LIST;
        _message = "No free name (2 to 99)";
        change(Scene::ERROR);
      } else
        error(r == StoreResult::INVALID_NAME ? StoreResult::INVALID_NAME : r);
    }
    return;
  }
  case ListKind::NUMBERED_ASK:
    if (row)
      import_now();
    else
      open_list(ListKind::IMPORT, _import_row);
    return;
  case ListKind::UNSAVED:
    if (row == 2) {
      change(Scene::HOME);
      return;
    }
    if (row == 0) {
      busy("SAVING - DO NOT POWER OFF");
      auto r = _storage.recover(_storage.current_name());
      if (r == StoreResult::OK)
        r = _storage.save(_text);
      if (r != StoreResult::OK) {
        change(Scene::HOME);
        error(r);
        return;
      }
    }
    if (_pending_new)
      new_file();
    else
      open_file(_pending);
    return;
  default:
    return;
  }
}
// ---- Controls / Credits pages ----
void Application::pages_frame(uint16_t pressed) {
  const int pages = help_topic_pages(_topic);
  if (pressed & B_KEY) {
    int row = (_menu_file >= 0 ? (_storage.settings().delete_files ? 2 : 1) : 0) + 2 + _topic;
    open_list(ListKind::MENU, row);
  } else if ((pressed & LEFT_KEY) && _page > 0) {
    --_page;
    _redraw = true;
  } else if ((pressed & RIGHT_KEY) && _page + 1 < pages) {
    ++_page;
    _redraw = true;
  } else if ((pressed & A_KEY) && help_secret_page(_topic, _page))
    open_list(ListKind::SECRET);
}
// ---- Rename ----
void Application::rename_frame(uint16_t held) {
  bool shift = _name_input.shift_armed(), caps = _name_input.caps();
  const char *group = _name_input.active_group();
  if (held & ~_previous_rename)
    _redraw = true;
  _previous_rename = held;
  _name_input.update(held, name_event, this);
  if (group != _name_input.active_group() || shift != _name_input.shift_armed() ||
      caps != _name_input.caps())
    _redraw = true;
}
void Application::consume_name(InputEvent e) {
  if (_scene != Scene::RENAME)
    return;
  using K = EventKind;
  _redraw = true;
  auto erase_before = [&]() {
    const std::size_t p = utf8_previous(_name, _name_caret);
    std::memmove(_name + p, _name + _name_caret, std::strlen(_name + _name_caret) + 1);
    _name_caret = p;
  };
  auto insert = [&](const char *t) {
    const std::size_t n = std::strlen(t), len = std::strlen(_name);
    if (len + n > RENAME_MAX) {
      _name_input.reject_edit();
      flash("Name too long");
      return false;
    }
    std::memmove(_name + _name_caret + n, _name + _name_caret, len - _name_caret + 1);
    std::memcpy(_name + _name_caret, t, n);
    _name_caret += n;
    return true;
  };
  switch (e.kind) {
  case K::INSERT:
    if (!e.text[0] || e.text[0] == '\n' || e.text[0] == '\t' || e.text[0] == '\r')
      return; // Start alone: no new lines in a name
    if (insert(e.text) && _name_input.select_active()) {
      _name_provisional = true;
      _name_provisional_end = _name_caret;
    }
    break;
  case K::REPLACE: {
    if (_name_input.select_active() && (!_name_provisional || _name_caret != _name_provisional_end)) {
      _name_input.reject_edit();
      return;
    }
    if (!_name_caret)
      return;
    char keep[FILE_NAME_SIZE];
    std::strcpy(keep, _name);
    const std::size_t caret = _name_caret;
    erase_before();
    if (!insert(e.text)) {
      std::strcpy(_name, keep);
      _name_caret = caret;
      return;
    }
    if (_name_input.select_active())
      _name_provisional_end = _name_caret;
    break;
  }
  case K::BACKSPACE:
    erase_before();
    break;
  case K::MOVE_LEFT:
    _name_caret = utf8_previous(_name, _name_caret);
    break;
  case K::MOVE_RIGHT:
    _name_caret = utf8_next(_name, _name_caret);
    break;
  case K::TOGGLE_STATUS:
    if (_name_input.select_active() && _name_provisional && _name_caret == _name_provisional_end)
      erase_before();
    _name_provisional = false;
    break;
  case K::SAVE:
    rename_commit();
    break;
  case K::SAVE_MENU:
    open_list(ListKind::MENU, 0);
    break;
  default:
    break;
  }
}
void Application::rename_commit() {
  if (!_name[0]) {
    flash("Name cannot be empty");
    return;
  }
  if (!Storage::valid_base(_name)) {
    flash("Not allowed: / \\ : * ? \" < > |");
    return;
  }
  busy("Renaming...");
  const int f = _menu_file;
  auto r = _storage.rename_file(f, _name);
  if (r == StoreResult::NAME_USED) {
    flash("Name already used");
    return;
  }
  if (r == StoreResult::INVALID_NAME) {
    flash("Name not allowed");
    return;
  }
  if (r != StoreResult::OK) {
    open_list(ListKind::MENU, 0);
    error(r);
    return;
  }
  char renamed[FILE_NAME_SIZE];
  writer::format(renamed, sizeof(renamed), "%s.txt", _name);
  change(Scene::HOME);
  rescan(renamed);
  flash("Renamed");
}
// ---- Frame ----
void Application::frame(uint16_t held) {
  uint16_t pressed = held & ~_previous;
  _previous = held;
  if (_note_frames && !--_note_frames)
    _redraw = true;
  if (_need_scan && _scene == Scene::HOME) {
    // First Home: check the card once (the list needs it).
    if (!_ready) {
      busy("Checking SD...");
      _ready = _storage.init() == StoreResult::OK;
      _need_scan = false;
      if (_ready)
        rescan();
      else {
        _home.reset(2, home_skip, &_storage);
        _redraw = true;
      }
    } else
      rescan(_active ? _storage.current_name() : nullptr);
  }
  if (_wait_release) {
    if (!held)
      released();
    return;
  }
  scene_frame(held, pressed);
  // A screen opened on a release (Home acts when A is released) is ready at once.
  if (_wait_release && !held)
    released();
}
void Application::released() {
  _wait_release = false;
  _input.reset_transient();
  _keys.reset(0);
  _previous_home = 0;
  _previous_rename = 0;
  _a_moved = false;
}
void Application::scene_frame(uint16_t held, uint16_t pressed) {
  auto p = [&](Button b) { return (pressed & bit(b)) != 0; };
  switch (_scene) {
  case Scene::EDITOR: {
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
    const char *group = _input.active_group();
    _input.update(held, event, this);
    if (group != _input.active_group())
      _redraw = true;
    if (shift != _input.shift_armed() || caps != _input.caps())
      _redraw = true;
    return;
  }
  case Scene::HOME:
    home_frame(held, pressed);
    return;
  case Scene::LIST:
    list_frame(held, pressed);
    return;
  case Scene::RENAME:
    rename_frame(held);
    return;
  default:
    break;
  }
  if (!pressed)
    return;
  _redraw = true;
  switch (_scene) {
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
      change(Scene::HOME);
    else if (p(Button::A)) {
      char name[DIARY_NAME_SIZE];
      format_diary_name(_date, _storage.settings().format, name);
      auto r = _storage.create(name, _text);
      if (r == StoreResult::OK) {
        Settings s = _storage.settings();
        s.has_date = true;
        s.date = _date;
        _storage.set_settings(s); // remembered for the next proposal
        _need_scan = true;
        editor();
      } else
        error(r);
    }
    break;
  case Scene::PAGES:
    pages_frame(pressed);
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
