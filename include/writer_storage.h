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
  // Opens name as the document source; the file stays open while editing.
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
  // Two sources: a newly loaded or saved file is checked before the old one closes.
  FileSource _sources[2];
  int _active = -1;
  // Current directory name of the active source (it is renamed while saving).
  char _source_name[FILE_NAME_SIZE] = {};
  char _chunk[512];
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
  StoreResult read_file(const char *name, char *data, std::size_t capacity, std::size_t &size);
  // Streams name: its size and FNV-1a hash.
  StoreResult hash_file(const char *name, std::size_t &size, uint32_t &hash);
  bool open_source(const char *name, int slot);
  void close_source();
  bool is_source(const char *name) const;
  void source_moved(const char *name);
};
} // namespace writer
