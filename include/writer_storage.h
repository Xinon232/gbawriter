#pragma once
#include "writer_core.h"
#ifdef __DEVKITARM__
#include "ff.h"
#else
#include <cstdio>
#include <dirent.h>
#endif
namespace writer {
enum class StoreResult {
  OK,
  IO_ERROR,
  EXISTS,
  INVALID_NAME,
  TOO_LARGE,
  INVALID_UTF8,
  RECOVERY_NEEDED,
  OPEN_FILE, // rename / delete of the file open in the editor
  NAME_USED  // rename / import: the name is taken (also upper/lower case only)
};
const char *store_message(StoreResult result);
constexpr int FILE_PAGE_SIZE = 32;
constexpr int FILE_NAME_SIZE = 256;
// The open TXT file, read through a small window cache (as gbareader streams
// books) so the document never has to fit in RAM.
class FileSource final : public TextSource {
public:
  static constexpr std::size_t CACHE_BYTES = 2048;
  FileSource() = default;
  FileSource(const FileSource &) = delete;
  FileSource &operator=(const FileSource &) = delete;
  ~FileSource() { close(); }
  bool open(const char *path);
  void close();
  // The file was renamed while open (saving): reopen under this path if needed.
  void moved(const char *path);
  bool is_open() const { return _open; }
  std::size_t size() const override { return _size; }
  bool read(std::size_t offset, char *out, std::size_t n) const override;

private:
#ifdef __DEVKITARM__
  mutable FIL _file{};
#else
  mutable FILE *_file = nullptr;
#endif
  bool _open = false;
  std::size_t _size = 0;
  char _path[800] = {};
  bool refill(std::size_t start, std::size_t want) const;
  mutable char _cache[CACHE_BYTES];
  mutable std::size_t _cache_start = 0, _cache_size = 0;
};
// V4.0 settings kept in /gbawriter/GBAWRITER.SYS (two checked slots).
struct Settings {
  NameFormat format = NameFormat::DDMMYYYY;
  bool reorder = true;     // Secret Settings: A + Up/Down on Home
  bool delete_files = false; // Secret Settings: Delete in the Select menu
  bool has_date = false;   // last date chosen for New File
  Date date{10, 7, 2026};
};
// Home lists every TXT up to this count; all names stay in RAM (reordering).
constexpr int MAX_FILES = 1000;
constexpr std::size_t NAME_POOL_BYTES = 40 * 1024;
constexpr int BROWSE_PATH_SIZE = 512;
class Storage {
public:
  explicit Storage(const char *root = "");
  StoreResult init();
  bool mounted() const { return _mounted; }
  // Reads /gbawriter: every TXT in the saved order, files the order does not
  // know yet first (dates newest first, then A to Z). Saves the order when it
  // changed (new or missing files).
  StoreResult scan();
  int count() const { return _count; }
  // All TXT files found (also those over MAX_FILES or the name memory).
  int total() const { return _total; }
  const char *name(int i) const {
    return i >= 0 && i < _count ? _pool + _offsets[_order[i]] : "";
  }
  // Exchange list rows i and j in RAM; save_order() writes it.
  void move(int i, int j);
  StoreResult save_order() { return save_state(); }
  Date proposed_date() const { return _proposed; }
  const Settings &settings() const { return _settings; }
  StoreResult set_settings(const Settings &s);
  // Removes GBAWRITER.SYS; settings and order go back to their defaults.
  StoreResult delete_configuration();
  StoreResult create(const char *name, TextModel &text);
  // Opens name as the document source; the file stays open while editing.
  StoreResult load(const char *name, TextModel &text);
  StoreResult save(TextModel &text);
  StoreResult recover(const char *name);
  const char *current_name() const { return _current; }
  // The document file stays open (also on Home): it may not be renamed or deleted.
  bool is_open(const char *name) const;
  // Rename list row i to base + ".txt" (it keeps its place), delete row i.
  StoreResult rename_file(int i, const char *base);
  StoreResult delete_file(int i);
  // Import browser (whole card). path: "/" or "/folder/sub".
  StoreResult browse(const char *path);
  int browse_count() const { return _count; }
  const char *browse_name(int i) const { return name(i); }
  bool browse_is_folder(int i) const { return i >= 0 && i < _count && _folder[_order[i]]; }
  // A free name in /gbawriter for name: name itself, else "base (2).txt" ..
  // "base (99).txt". OK, EXISTS (no free number) or INVALID_NAME (too long).
  StoreResult import_name(const char *name, char (&target)[FILE_NAME_SIZE]);
  using Progress = void (*)(void *context, int percent);
  // Copies source (a card path) to /gbawriter/target; never replaces a file
  // and leaves no partial copy behind.
  StoreResult import_file(const char *source, const char *target, Progress progress,
                          void *context);
  // Text field for names: valid for rename / import (no .txt).
  static bool valid_base(const char *base);
#ifndef __DEVKITARM__
  void fault_at(int n) {
    _fail = n;
    _operations = 0;
  }
  int operations() const { return _operations; }
#endif
private:
  char _card[512], _root[512], _current[FILE_NAME_SIZE];
  // Names of the list (Home or the import browser): NUL-terminated in _pool.
  char _pool[NAME_POOL_BYTES];
  uint16_t _offsets[MAX_FILES], _order[MAX_FILES];
  uint8_t _folder[MAX_FILES];
  uint16_t _rank[MAX_FILES];   // saved position, NO_RANK when not saved
  uint16_t _hash[2048];        // name -> index + 1 (open addressing)
  std::size_t _pool_used = 0;
  // Two sources: a newly loaded or saved file is checked before the old one closes.
  FileSource _sources[2];
  int _active = -1;
  // Current directory name of the active source (it is renamed while saving).
  char _source_name[FILE_NAME_SIZE] = {};
  char _chunk[512];
  char _copy[4096];
#ifdef __DEVKITARM__
  FIL _in{};
#else
  FILE *_in = nullptr;
#endif
  StoreResult recover_all();
  int _count = 0, _total = 0;
  Date _proposed{10, 7, 2026};
  Settings _settings;
  bool _mounted = false, _state_read = false, _have_order = false;
  uint32_t _generation = 0;
  int _newest = 1, _saved_count = 0;
  // Saved order (file names) as read from GBAWRITER.SYS, until merged.
  bool _browsing = false;
#ifdef __DEVKITARM__
  FIL _file{};
  DIR _dir{};
  FATFS _fatfs{};
#else
  FILE *_file = nullptr;
  DIR *_dir = nullptr;
  char _dir_path[800] = {};
  int _fail = -1, _operations = 0;
#endif
  bool gate();
  bool path(const char *name, char *out) const;
  int stat(const char *name, std::size_t &size, bool &directory);
  int stat_path(const char *full, std::size_t &size, bool &directory);
  bool open(const char *name, bool write);
  bool open_path(const char *full, bool write);
  bool close();
  bool sync();
  bool read(void *data, std::size_t want, std::size_t &got);
  bool write(const void *data, std::size_t size);
  bool rename(const char *from, const char *to);
  bool remove(const char *name);
  bool make_dir(const char *name);
  bool dir_open();
  bool dir_open_path(const char *full);
  bool dir_next(char *name, bool &directory);
  bool dir_next_entry(char *name, bool &directory, bool &hidden);
  bool dir_close();
  bool write_file(const char *name, const char *data, std::size_t size);
  StoreResult read_file(const char *name, char *data, std::size_t capacity, std::size_t &size);
  // Streams name: its size and FNV-1a hash.
  StoreResult hash_file(const char *name, std::size_t &size, uint32_t &hash);
  bool open_source(const char *name, int slot);
  void close_source();
  bool is_source(const char *name) const;
  void source_moved(const char *name);
  // List memory.
  void list_clear();
  bool list_add(const char *name, bool folder);
  int list_find(const char *name) const;
  // GBAWRITER.SYS
  StoreResult read_state();
  StoreResult save_state();
  bool read_slot(int slot, uint32_t &generation, bool apply);
  bool recovery_files(const char *name, int &found);
};
} // namespace writer
