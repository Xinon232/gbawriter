// gbawriter V4.0: Home list order, GBAWRITER.SYS, rename, delete, import,
// file name formats, and the Home / menu / import screens.
#include "writer_app.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
using namespace writer;
namespace fs = std::filesystem;
static std::string root;
static void put(const std::string &rel, const std::string &text) { std::ofstream(root + "/" + rel) << text; }
static std::string read(const std::string &rel) {
  std::ifstream f(root + "/" + rel, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
static std::string names_of(const Storage &s) {
  std::string out;
  for (int i = 0; i < s.count(); ++i)
    out += std::string(i ? "," : "") + s.name(i);
  return out;
}
static void fresh() {
  fs::remove_all(root);
  fs::create_directories(root + "/gbawriter");
}
static void formats() {
  Date d{1, 10, 2026};
  char n[DIARY_NAME_SIZE];
  const char *expect[NAME_FORMATS] = {"01102026.txt", "01.10.2026.txt", "10012026.txt", "10.01.2026.txt",
                                      "2026-10-01.txt"};
  for (int f = 0; f < NAME_FORMATS; ++f) {
    format_diary_name(d, NameFormat(f), n);
    assert(!strcmp(n, expect[f]));
    Date back{};
    assert(parse_diary_name(n, NameFormat(f), back) && back.day == 1 && back.month == 10 && back.year == 2026);
    int order[3];
    date_field_order(NameFormat(f), order);
    assert(order[0] == (f == 4 ? 2 : f >= 2 ? 1 : 0));
  }
  Date x{};
  assert(!parse_diary_name("01102026.txt", NameFormat::DD_MM_YYYY, x));
  assert(!parse_diary_name("2026-13-01.txt", NameFormat::YYYY_MM_DD, x));
  assert(parse_diary_name("13012026.TXT", NameFormat::DDMMYYYY, x) && !parse_diary_name("13012026.txt", NameFormat::MMDDYYYY, x));
  std::cout << "PASS: five file name formats, date picker order\n";
}
static void order_and_state() {
  fresh();
  put("gbawriter/09072026.txt", "a");
  put("gbawriter/10072026.txt", "b");
  put("gbawriter/2026-07-11.txt", "c"); // another format still counts as a date
  put("gbawriter/Zebra.txt", "z");
  put("gbawriter/apple.txt", "x");
  {
    Storage s(root.c_str());
    assert(s.init() == StoreResult::OK);
    assert(names_of(s) == "2026-07-11.txt,10072026.txt,09072026.txt,apple.txt,Zebra.txt");
    // Proposed date: the chosen format only.
    assert(s.proposed_date().day == 11 && s.proposed_date().month == 7);
    // No settings file until something is changed.
    assert(!fs::exists(root + "/gbawriter/GBAWRITER.SYS"));
    s.move(4, 3);
    assert(s.save_order() == StoreResult::OK);
    assert(fs::exists(root + "/gbawriter/GBAWRITER.SYS/STATE0.DAT"));
  }
  put("gbawriter/new.txt", "n");
  fs::remove(root + "/gbawriter/09072026.txt");
  {
    Storage s(root.c_str());
    assert(s.init() == StoreResult::OK);
    // New files first; missing files dropped; the saved order kept.
    assert(names_of(s) == "new.txt,2026-07-11.txt,10072026.txt,Zebra.txt,apple.txt");
    // The merged order was saved (slot 1 now).
    assert(fs::exists(root + "/gbawriter/GBAWRITER.SYS/STATE1.DAT"));
    Settings set = s.settings();
    assert(set.format == NameFormat::DDMMYYYY && set.reorder && !set.delete_files);
    set.format = NameFormat::YYYY_MM_DD;
    set.delete_files = true;
    assert(s.set_settings(set) == StoreResult::OK);
    assert(s.scan() == StoreResult::OK);
    assert(s.proposed_date().day == 12 && s.proposed_date().month == 7);
  }
  {
    // A damaged newest slot: the other one is used.
    Storage s(root.c_str());
    std::fstream f(root + "/gbawriter/GBAWRITER.SYS/STATE0.DAT", std::ios::in | std::ios::out | std::ios::binary);
    f.seekp(30);
    f.put('#');
    f.close();
    assert(s.init() == StoreResult::OK);
    assert(s.settings().format == NameFormat::DDMMYYYY); // slot 1 (before the change)
    assert(names_of(s) == "new.txt,2026-07-11.txt,10072026.txt,Zebra.txt,apple.txt");
    assert(s.delete_configuration() == StoreResult::OK);
    assert(!fs::exists(root + "/gbawriter/GBAWRITER.SYS/STATE0.DAT") &&
           !fs::exists(root + "/gbawriter/GBAWRITER.SYS/STATE1.DAT"));
    assert(s.scan() == StoreResult::OK);
    assert(names_of(s) == "2026-07-11.txt,10072026.txt,apple.txt,new.txt,Zebra.txt");
  }
  std::cout << "PASS: Home order: new files first, saved order, two settings slots, delete configuration\n";
}
static void rename_delete() {
  fresh();
  put("gbawriter/a.txt", "A");
  put("gbawriter/b.txt", "B");
  put("gbawriter/c.txt", "C");
  Storage s(root.c_str());
  assert(s.init() == StoreResult::OK);
  assert(names_of(s) == "a.txt,b.txt,c.txt");
  assert(s.rename_file(0, "B") == StoreResult::NAME_USED); // b.txt, other case
  assert(s.rename_file(0, "x/y") == StoreResult::INVALID_NAME);
  assert(s.rename_file(0, "") == StoreResult::INVALID_NAME);
  assert(s.rename_file(0, "trailing.") == StoreResult::INVALID_NAME);
  assert(s.rename_file(0, "Diary (2)") == StoreResult::OK);
  assert(names_of(s) == "Diary (2).txt,b.txt,c.txt" && read("gbawriter/Diary (2).txt") == "A");
  assert(s.rename_file(0, "DIARY (2)") == StoreResult::OK); // case only
  assert(names_of(s) == "DIARY (2).txt,b.txt,c.txt" && read("gbawriter/DIARY (2).txt") == "A");
  TextModel t;
  assert(s.load("b.txt", t) == StoreResult::OK);
  assert(s.rename_file(1, "bee") == StoreResult::OPEN_FILE);
  assert(s.delete_file(1) == StoreResult::OPEN_FILE);
  put("gbawriter/c.txt.gwb", "old");
  assert(s.rename_file(2, "see") == StoreResult::RECOVERY_NEEDED);
  assert(s.delete_file(2) == StoreResult::RECOVERY_NEEDED);
  fs::remove(root + "/gbawriter/c.txt.gwb");
  assert(s.delete_file(2) == StoreResult::OK && !fs::exists(root + "/gbawriter/c.txt"));
  assert(names_of(s) == "DIARY (2).txt,b.txt");
  // The renamed file keeps its place after a restart.
  Storage again(root.c_str());
  assert(again.init() == StoreResult::OK && names_of(again) == "DIARY (2).txt,b.txt");
  std::cout << "PASS: rename (collisions, case only, invalid), delete, open file and recovery refusals\n";
}
static int last_percent = -1;
static void progress(void *, int p) { last_percent = p; }
static void import() {
  fresh();
  fs::create_directories(root + "/Books/Old");
  fs::create_directories(root + "/.hidden");
  put("Books/Story.txt", std::string(10000, 's'));
  put("Books/song.mp3", "x");
  put("Books/Old/Letter.TXT", "l");
  put("readme.txt", "r");
  put("gbawriter/Story.txt", "mine");
  put("gbawriter/story (2).txt", "two");
  Storage s(root.c_str());
  assert(s.init() == StoreResult::OK);
  assert(s.browse("/") == StoreResult::OK);
  assert(s.browse_count() == 2 && !strcmp(s.browse_name(0), "Books") && s.browse_is_folder(0) &&
         !strcmp(s.browse_name(1), "readme.txt"));
  assert(s.browse("/Books") == StoreResult::OK);
  assert(s.browse_count() == 2 && !strcmp(s.browse_name(0), "Old") && !strcmp(s.browse_name(1), "Story.txt"));
  char target[FILE_NAME_SIZE];
  assert(s.import_name("Story.txt", target) == StoreResult::NAME_USED && !strcmp(target, "Story (3).txt"));
  assert(s.import_file("/Books/Story.txt", target, progress, nullptr) == StoreResult::OK);
  assert(read("gbawriter/Story (3).txt") == std::string(10000, 's') && last_percent == 100);
  assert(read("gbawriter/Story.txt") == "mine");
  assert(s.import_name("readme.txt", target) == StoreResult::OK && !strcmp(target, "readme.txt"));
  assert(s.import_file("/readme.txt", "Story.txt", nullptr, nullptr) == StoreResult::EXISTS);
  s.fault_at(5);
  assert(s.import_file("/readme.txt", "readme.txt", nullptr, nullptr) != StoreResult::OK);
  s.fault_at(-1);
  assert(!fs::exists(root + "/gbawriter/readme.txt")); // no partial copy
  assert(s.scan() == StoreResult::OK);
  assert(names_of(s) == "story (2).txt,Story (3).txt,Story.txt");
  std::cout << "PASS: import browser (folders first, hidden and /gbawriter left out), numbered copies, no partial copy\n";
}
static unsigned key(Button b) { return 1u << unsigned(b); }
static int width(const char *) { return 6; }
static void screens() {
  fresh();
  put("gbawriter/a.txt", "A");
  put("gbawriter/b.txt", "B");
  put("gbawriter/c.txt", "C");
  Storage s(root.c_str());
  Application app(s, width);
  app.boot();
  auto frame = [&](unsigned k) { app.frame(k); };
  auto tap = [&](Button b) { frame(key(b)); frame(0); };
  frame(0);
  assert(app.scene() == Scene::HOME && app.list_rows() == 4 && app.nav().sel() == 0);
  char text[FILE_NAME_SIZE + 32];
  assert(app.list_row(0, text) && !strcmp(text, "New File") && app.list_icon(0) == RowIcon::NEW_FILE);
  assert(app.list_row(1, text) && !strcmp(text, "a") && app.list_underline(0) && !app.list_footer());
  // Hold A + Down twice on "a": it moves to the end; A did not open it.
  tap(Button::DOWN);
  frame(key(Button::A));
  frame(key(Button::A) | key(Button::DOWN));
  frame(key(Button::A));
  frame(key(Button::A) | key(Button::DOWN));
  frame(0);
  assert(app.scene() == Scene::HOME && names_of(s) == "b.txt,c.txt,a.txt" && app.nav().sel() == 3);
  // New File cannot move; a file cannot move above it.
  tap(Button::UP);
  tap(Button::UP);
  frame(key(Button::A));
  frame(key(Button::A) | key(Button::UP));
  frame(0);
  assert(names_of(s) == "b.txt,c.txt,a.txt" && app.scene() == Scene::HOME);
  // Reorder off (Secret Settings): A + Up/Down does nothing.
  Settings set = s.settings();
  set.reorder = false;
  assert(s.set_settings(set) == StoreResult::OK);
  frame(key(Button::A));
  frame(key(Button::A) | key(Button::DOWN));
  frame(0);
  assert(names_of(s) == "b.txt,c.txt,a.txt" && app.scene() == Scene::HOME);
  set.reorder = true;
  assert(s.set_settings(set) == StoreResult::OK);
  // Select menu: Rename, (no Delete by default), File names, topics, Credits.
  tap(Button::SELECT);
  assert(app.scene() == Scene::LIST && app.list_kind() == ListKind::MENU);
  char title[64];
  assert(!strcmp(app.list_title(title), "b") && app.list_rows() == 3 + HELP_TOPICS);
  assert(app.list_row(0, text) && !strcmp(text, "Rename"));
  assert(app.list_row(1, text) && !strcmp(text, "File names: 13072026"));
  assert(app.list_row(2, text) && !strcmp(text, "Helper line: On"));
  {
    // File names: the example is always 13 July 2026, in each format.
    const char *shown[NAME_FORMATS] = {"File names: 13072026", "File names: 13.07.2026", "File names: 07132026",
                                       "File names: 07.13.2026", "File names: 2026-07-13"};
    for (int f = 0; f < NAME_FORMATS; ++f) {
      assert(app.list_row(1, text) && !strcmp(text, shown[f]));
      tap(Button::DOWN);
      tap(Button::A);
      tap(Button::UP);
    }
    assert(s.settings().format == NameFormat::DDMMYYYY && app.list_row(1, text) && !strcmp(text, shown[0]));
  }
  // The line under Helper line: settings above, instructions below.
  assert(app.list_underline(2) && !app.list_underline(1) && !app.list_underline(3) && !app.list_underline(0));
  assert(app.list_row(2 + HELP_TOPICS, text) && !strcmp(text, "Credits"));
  // Rename b -> "bx".
  tap(Button::A);
  assert(app.scene() == Scene::RENAME && !strcmp(app.name_text(), "b"));
  frame(key(Button::RIGHT));
  frame(key(Button::RIGHT) | key(Button::B)); // "h"
  frame(0);
  assert(!strcmp(app.name_text(), "bh"));
  frame(key(Button::START));
  frame(0); // Start alone: no new line in a name
  assert(!strcmp(app.name_text(), "bh"));
  frame(key(Button::START));
  frame(key(Button::START) | key(Button::A));
  frame(0);
  assert(app.scene() == Scene::HOME && names_of(s) == "bh.txt,c.txt,a.txt" && app.nav().sel() == 1);
  // Credits, last page: A opens Secret Settings; Delete files on.
  tap(Button::SELECT);
  tap(Button::UP); // wraps to Credits
  tap(Button::A);
  assert(app.scene() == Scene::PAGES && app.topic() == CREDITS_TOPIC && app.page() == 0);
  for (int i = 0; i < 10; ++i)
    tap(Button::RIGHT);
  assert(help_secret_page(app.topic(), app.page()));
  tap(Button::A);
  assert(app.scene() == Scene::LIST && app.list_kind() == ListKind::SECRET);
  tap(Button::DOWN);
  tap(Button::A);
  assert(s.settings().delete_files);
  tap(Button::B);
  tap(Button::B);
  tap(Button::B);
  assert(app.scene() == Scene::HOME);
  // Delete "bh": Sure?, then A.
  tap(Button::SELECT);
  assert(app.list_rows() == 4 + HELP_TOPICS);
  tap(Button::DOWN);
  tap(Button::A);
  assert(app.list_row(1, text) && !strcmp(text, "Sure?") && fs::exists(root + "/gbawriter/bh.txt"));
  tap(Button::A);
  assert(app.scene() == Scene::HOME && !fs::exists(root + "/gbawriter/bh.txt") && names_of(s) == "c.txt,a.txt");
  assert(app.nav().sel() == 1); // the next file takes the deleted row
  // V4.1 Helper line setting: saved, decides how files open; Start+Select
  // still toggles while writing.
  tap(Button::UP); // New File: File names, Helper line, topics
  tap(Button::SELECT);
  tap(Button::DOWN);
  assert(app.list_row(1, text) && !strcmp(text, "Helper line: On"));
  tap(Button::A);
  assert(!s.settings().status_bar && app.list_row(1, text) && !strcmp(text, "Helper line: Off"));
  {
    Storage again(root.c_str());
    assert(again.init() == StoreResult::OK && !again.settings().status_bar && again.settings().delete_files);
  }
  tap(Button::B);
  tap(Button::DOWN);
  tap(Button::A);
  assert(app.scene() == Scene::EDITOR && !app.status_visible());
  frame(key(Button::START) | key(Button::SELECT));
  frame(0);
  assert(app.status_visible());
  frame(key(Button::START));
  frame(key(Button::START) | key(Button::B));
  frame(0);
  tap(Button::SELECT);
  tap(Button::DOWN);
  tap(Button::DOWN);
  tap(Button::DOWN);
  tap(Button::A); // back On
  assert(s.settings().status_bar);
  tap(Button::B);
  tap(Button::A); // the open file again
  assert(app.scene() == Scene::EDITOR && app.status_visible());
  frame(key(Button::START));
  frame(key(Button::START) | key(Button::B));
  frame(0);
  assert(app.scene() == Scene::HOME);
  // Open c, type, Start+B: Home with the play mark; c cannot be renamed.
  tap(Button::A);
  assert(app.scene() == Scene::EDITOR);
  frame(key(Button::UP));
  frame(key(Button::UP) | key(Button::B));
  frame(0);
  frame(key(Button::START));
  frame(key(Button::START) | key(Button::B));
  frame(0);
  assert(app.scene() == Scene::HOME && app.active() && app.list_icon(1) == RowIcon::PLAY && read("gbawriter/c.txt") == "C");
  tap(Button::SELECT);
  tap(Button::A);
  assert(app.scene() == Scene::ERROR && !strcmp(app.message(), "This file is open. Open another file first."));
  tap(Button::A);
  tap(Button::B);
  // Another file: Save / Discard / Cancel.
  tap(Button::DOWN);
  tap(Button::A);
  assert(app.scene() == Scene::LIST && app.list_kind() == ListKind::UNSAVED);
  tap(Button::DOWN);
  tap(Button::DOWN);
  tap(Button::A); // Cancel
  assert(app.scene() == Scene::HOME && app.text().dirty());
  tap(Button::A);
  tap(Button::A); // Save, then a opens
  assert(app.scene() == Scene::EDITOR && !strcmp(s.current_name(), "a.txt") && read("gbawriter/c.txt") == "Ca");
  // Import from Start: Books/x.txt
  frame(key(Button::START));
  frame(key(Button::START) | key(Button::B));
  frame(0);
  fs::create_directories(root + "/Books");
  put("Books/a.txt", "imported");
  tap(Button::START);
  assert(app.scene() == Scene::LIST && app.list_kind() == ListKind::IMPORT && !strcmp(app.list_title(title), "Import to /gbawriter"));
  tap(Button::A); // Books
  assert(!strcmp(app.list_title(title), "/Books"));
  tap(Button::A); // a.txt
  assert(app.list_kind() == ListKind::IMPORT_ASK && app.nav().sel() == 0);
  tap(Button::DOWN);
  tap(Button::A);
  assert(app.list_kind() == ListKind::NUMBERED_ASK && app.list_row(1, text) && !strcmp(text, "Yes, as a (2).txt"));
  tap(Button::DOWN);
  tap(Button::A);
  assert(app.scene() == Scene::HOME && read("gbawriter/a (2).txt") == "imported" && app.nav().sel() == 1 &&
         !strcmp(app.note(), "Imported"));
  std::cout << "PASS: Home reorder, Select menu, rename, Secret Settings, delete, play mark, Save/Discard/Cancel, import\n";
}
int main() {
  root = "/tmp/gbawriter-v4-" + std::to_string(getpid());
  formats();
  order_and_state();
  rename_delete();
  import();
  screens();
  fs::remove_all(root);
}
