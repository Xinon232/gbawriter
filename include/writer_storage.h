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
  RECOVERY_NEEDED
};
const char *store_message(StoreResult result);
constexpr int FILE_PAGE_SIZE = 32;
constexpr int FILE_NAME_SIZE = 256;
class Storage {
public:
  explicit Storage(const char *root = "");
  StoreResult init();
  StoreResult scan();
  StoreResult next_page();
  StoreResult previous_page();
  int count() const { return _count; }
  int total() const { return _total; }
  const char *name(int i) const {
    return i >= 0 && i < _count ? _names[i] : "";
  }
  Date proposed_date() const { return _proposed; }
  StoreResult create(const char *name, TextModel &text);
  StoreResult load(const char *name, TextModel &text);
  StoreResult save(TextModel &text);
  StoreResult recover(const char *name);
  const char *current_name() const { return _current; }
#ifndef __DEVKITARM__
  void fault_at(int n) {
    _fail = n;
    _operations = 0;
  }
  int operations() const { return _operations; }
#endif
private:
  char _root[512], _current[FILE_NAME_SIZE],
      _names[FILE_PAGE_SIZE][FILE_NAME_SIZE];
  char _scratch[TEXT_CAPACITY + 1];
  StoreResult scan_page();
  StoreResult recover_all();
  int _page = 0;
  int _count = 0, _total = 0;
  Date _proposed{10, 7, 2026};
#ifdef __DEVKITARM__
  FIL _file{};
  DIR _dir{};
  FATFS _fatfs{};
#else
  FILE *_file = nullptr;
  DIR *_dir = nullptr;
  int _fail = -1, _operations = 0;
#endif
  bool gate();
  bool path(const char *name, char *out) const;
  int stat(const char *name, std::size_t &size, bool &directory);
  bool open(const char *name, bool write);
  bool close();
  bool sync();
  bool read(void *data, std::size_t want, std::size_t &got);
  bool write(const void *data, std::size_t size);
  bool rename(const char *from, const char *to);
  bool remove(const char *name);
  bool dir_open();
  bool dir_next(char *name, bool &directory);
  bool dir_close();
  bool write_file(const char *name, const char *data, std::size_t size);
  StoreResult read_file(const char *name, char *data, std::size_t &size);
};
} // namespace writer
