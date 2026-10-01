#pragma once
#include "writer_help.h"
#include "writer_layout.h"
#include "writer_list.h"
#include "writer_storage.h"
namespace writer {
// V4.0 screens. HOME is the file list (New File first); LIST covers the
// Select menu, Secret Settings, the import browser and the questions.
enum class Scene { HOME, DATE, EDITOR, LIST, PAGES, RENAME, ERROR };
enum class ListKind { MENU, SECRET, IMPORT, IMPORT_ASK, NUMBERED_ASK, UNSAVED };
// Row icons (right end, gbamp3): new file, play mark, folder.
enum class RowIcon { NONE, NEW_FILE, PLAY, FOLDER };
class Application {
public:
  Application(Storage &storage, Layout::Width measure)
      : _storage(storage), _measure(measure) {}
  void boot();
  void frame(uint16_t held);
  Scene scene() const { return _scene; }
  ListKind list_kind() const { return _kind; }
  Date date() const { return _date; }
  // Selected date picker row (0 top) and the date part it shows (0 day, 1 month, 2 year).
  int date_field() const { return _field; }
  int date_part(int row) const;
  TextModel &text() { return _text; }
  Layout &layout() { return _layout; }
  const char *message() const { return _message; }
  bool sd_ready() const { return _ready; }
  bool shift() const { return _input.shift_armed(); }
  bool caps() const { return _input.caps(); }
  bool status_visible() const { return _status_visible; }
  const char* active_group() const { return _input.active_group(); }
  bool save_feedback(uint16_t held) const;
  // Top visible row, as an index into layout() rows.
  int viewport() const { return _layout.row_of(_top); }
  int view_rows() const { return _status_visible ? VIEW_ROWS : FULL_VIEW_ROWS; }
  bool caret_visible() const { return _clock.visible(); }
  bool take_redraw() {
    bool r = _redraw;
    _redraw = false;
    return r;
  }
  // The list shown by HOME and LIST: title, rows, cursor.
  const char *list_title(char (&out)[64]) const;
  int list_rows() const;
  // Row text (names without .txt); false: a grey, never selected row.
  bool list_row(int row, char (&out)[FILE_NAME_SIZE + 32]) const;
  RowIcon list_icon(int row) const;
  bool list_underline(int row) const { return _scene == Scene::HOME && row == 0; }
  const list::Nav &nav() const { return _scene == Scene::HOME ? _home : _list; }
  // Grey bottom line ("B: Resume active file") or nullptr.
  const char *list_footer() const;
  // Short note at the bottom of a list or Rename (e.g. "Imported").
  const char *note() const { return _note_frames ? _note : ""; }
  bool active() const { return _active; }
  // Controls / Credits pages.
  int topic() const { return _topic; }
  int page() const { return _page; }
  // Rename field.
  const char *name_text() const { return _name; }
  std::size_t name_caret() const { return _name_caret; }
  bool name_shift() const { return _name_input.shift_armed(); }
  bool name_caps() const { return _name_input.caps(); }
  const char *name_group() const { return _name_input.active_group(); }
  // Long work (SD check, saving, copying): main.cpp shows it at once.
  using Busy = void (*)(void *context, const char *text, int percent);
  void import_percent(int percent);
  void set_busy(Busy show, void *context) {
    _busy = show;
    _busy_context = context;
  }

private:
  Storage &_storage;
  Layout::Width _measure;
  TextModel _text;
  Layout _layout;
  InputState _input, _name_input;
  CaretClock _clock;
  Scene _scene = Scene::HOME, _return = Scene::HOME;
  ListKind _kind = ListKind::MENU;
  list::Nav _home, _list;
  list::Input _keys;
  int _field = 0, _message_frames = 0, _note_frames = 0, _topic = 0, _page = 0;
  // List rows: the Select menu remembers which file it acts on (Home row).
  int _menu_file = -1, _confirm = -1;
  Date _date{10, 7, 2026};
  uint16_t _previous = 0;
  bool _redraw = true, _ready = false, _wait_release = false,
       _provisional = false, _provisional_dirty = false, _status_visible = true;
  // A file is open (Start+B keeps it); its row has the play mark.
  bool _active = false, _need_scan = true, _a_moved = false, _order_dirty = false;
  uint16_t _previous_home = 0, _previous_rename = 0;
  int _import_row = 0;
  // After the Save / Discard question: what to open (file name, or New File).
  char _pending[FILE_NAME_SIZE] = {};
  bool _pending_new = false;
  std::size_t _provisional_end = 0;
  // First byte of the top visible row (layout rows cover only a window).
  std::size_t _top = 0;
  const char *_message = "", *_note = "";
  // Import browser.
  char _browse[BROWSE_PATH_SIZE] = "/", _import_source[BROWSE_PATH_SIZE] = {},
                                      _import_target[FILE_NAME_SIZE] = {};
  // Rename field.
  char _name[FILE_NAME_SIZE] = {};
  std::size_t _name_caret = 0, _name_provisional_end = 0;
  bool _name_provisional = false;
  Busy _busy = nullptr;
  void *_busy_context = nullptr;
  static void event(void *context, InputEvent e) {
    static_cast<Application *>(context)->consume(e);
  }
  static void name_event(void *context, InputEvent e) {
    static_cast<Application *>(context)->consume_name(e);
  }
  void consume(InputEvent e);
  void consume_name(InputEvent e);
  void scene_frame(uint16_t held, uint16_t pressed);
  void released();
  void change(Scene scene);
  void error(StoreResult result);
  void editor();
  void resume();
  void ensure_visible();
  void cover();
  void check_read();
  void adjust_date(int delta);
  void busy(const char *text, int percent = -1);
  void flash(const char *text);
  // Screens.
  bool ensure_sd();
  void rescan(const char *select = nullptr);
  void home_frame(uint16_t held, uint16_t pressed);
  void home_activate();
  void open_file(const char *name);
  void new_file();
  void open_list(ListKind kind, int select = 0);
  void list_frame(uint16_t held, uint16_t pressed);
  void list_activate(int row);
  void list_back();
  int menu_row(int row) const; // menu row -> action
  int list_rows_for(ListKind kind) const;
  void browse(const char *path, const char *select = nullptr);
  void import_now();
  void pages_frame(uint16_t pressed);
  void rename_frame(uint16_t held);
  void rename_commit();
  void leave_editor();
  int file_row() const { return int(_home.sel()) - 1; } // -1: New File
};
} // namespace writer
