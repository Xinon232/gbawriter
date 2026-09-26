#include "writer_storage.h"
#include <cstdio>
#include "writer_format.h"
#include <cstring>
#include <initializer_list>
#ifdef __DEVKITARM__
extern "C" {
#include "gbahw.h"
#include "supercard_driver.h"
}
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace writer {
namespace {
bool txt(const char *n) {
  std::size_t s = std::strlen(n);
  return s > 4 && n[s - 4] == '.' && (n[s - 3] == 't' || n[s - 3] == 'T') &&
         (n[s - 2] == 'x' || n[s - 2] == 'X') &&
         (n[s - 1] == 't' || n[s - 1] == 'T');
}
int order(const char *a, const char *b) {
  Date da{}, db{};
  bool ax = parse_diary_name(a, da), by = parse_diary_name(b, db);
  if (ax != by)
    return ax ? -1 : 1;
  if (ax) {
    int d = (db.year - da.year) * 372 + (db.month - da.month) * 31 + db.day - da.day;
    if (d)
      return d;
  }
  for (int i = 0;; ++i) {
    unsigned char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z')
      x += 32;
    if (y >= 'A' && y <= 'Z')
      y += 32;
    if (x != y)
      return int(x) - int(y);
    if (!x)
      return std::strcmp(a, b);
  }
}
bool safe_name(const char *n) {
  return n && *n && std::strlen(n) < FILE_NAME_SIZE - 5 &&
         !std::strchr(n, '/') && !std::strchr(n, '\\') &&
         !std::strchr(n, ':') && txt(n);
}
uint32_t fnv(uint32_t hash, const char *data, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i)
    hash = (hash ^ static_cast<unsigned char>(data[i])) * 16777619u;
  return hash;
}
constexpr uint32_t FNV_START = 2166136261u;
// Piece offsets are 32-bit; FAT32 files stop below 4 GiB anyway.
constexpr std::size_t MAX_FILE_BYTES = 0x7fffffffu;
} // namespace
bool FileSource::open(const char *path) {
  close();
  if (std::strlen(path) >= sizeof(_path))
    return false;
  std::strcpy(_path, path);
#ifdef __DEVKITARM__
  if (f_open(&_file, path, FA_READ) != FR_OK)
    return false;
  _size = f_size(&_file);
#else
  _file = std::fopen(path, "rb");
  if (!_file)
    return false;
  if (std::fseek(_file, 0, SEEK_END) || std::ftell(_file) < 0) {
    std::fclose(_file);
    _file = nullptr;
    return false;
  }
  _size = std::size_t(std::ftell(_file));
#endif
  _open = true;
  _cache_size = 0;
  return true;
}
void FileSource::moved(const char *path) {
  if (std::strlen(path) < sizeof(_path))
    std::strcpy(_path, path);
}
// FatFS keeps a file object failed after one disk error, so a failed read
// reopens the file (same name, same size) and tries once more.
bool FileSource::refill(std::size_t start, std::size_t want) const {
  for (int attempt = 0; attempt < 2; ++attempt) {
#ifdef __DEVKITARM__
    UINT got = 0;
    if (attempt) {
      f_close(&_file);
      if (f_open(&_file, _path, FA_READ) != FR_OK || f_size(&_file) != _size)
        continue;
    }
    if (f_lseek(&_file, start) == FR_OK &&
        f_read(&_file, _cache, want, &got) == FR_OK && got == want) {
#else
    if (attempt) {
      std::fclose(_file);
      _file = std::fopen(_path, "rb");
      if (!_file)
        continue;
    }
    if (!std::fseek(_file, long(start), SEEK_SET) &&
        std::fread(_cache, 1, want, _file) == want) {
#endif
      _cache_start = start;
      _cache_size = want;
      return true;
    }
  }
  return false;
}
void FileSource::close() {
  if (!_open)
    return;
#ifdef __DEVKITARM__
  f_close(&_file);
#else
  if (_file)
    std::fclose(_file);
  _file = nullptr;
#endif
  _open = false;
  _size = _cache_size = 0;
}
bool FileSource::read(std::size_t offset, char *out, std::size_t n) const {
  if (!_open || offset > _size || n > _size - offset)
    return false;
  while (n) {
    if (offset < _cache_start || offset >= _cache_start + _cache_size) {
      // Refill one sector-aligned window around the requested byte.
      std::size_t start = offset & ~(CACHE_BYTES - 1);
      std::size_t want = _size - start < CACHE_BYTES ? _size - start : CACHE_BYTES;
      _cache_size = 0;
      if (!refill(start, want))
        return false;
    }
    std::size_t take = _cache_start + _cache_size - offset;
    if (take > n)
      take = n;
    std::memcpy(out, _cache + (offset - _cache_start), take);
    out += take;
    offset += take;
    n -= take;
  }
  return true;
}
const char *store_message(StoreResult r) {
  switch (r) {
  case StoreResult::OK:
    return "SAVED";
  case StoreResult::EXISTS:
    return "FILE ALREADY EXISTS";
  case StoreResult::INVALID_NAME:
    return "INVALID / LONG FILENAME";
  case StoreResult::TOO_LARGE:
    return "FILE TOO LARGE (2 GIB)";
  case StoreResult::INVALID_UTF8:
    return "INVALID UTF-8 / NUL";
  case StoreResult::RECOVERY_NEEDED:
    return "RECOVERY: CHECK SD ON PC";
  default:
    return "SD I/O ERROR - TEXT KEPT";
  }
}
Storage::Storage(const char *root) {
  writer::format(_root, sizeof(_root), "%s/gbawriter", root);
  _current[0] = 0;
}
bool Storage::gate() {
#ifndef __DEVKITARM__
  ++_operations;
  return _operations != _fail;
#else
  return true;
#endif
}
bool Storage::path(const char *name, char *out) const {
  return writer::format(out, 800, "%s/%s", _root, name) < 800;
}
int Storage::stat(const char *name, std::size_t &size, bool &directory) {
  if (!gate())
    return -1;
  char p[800];
  path(name, p);
#ifdef __DEVKITARM__
  FILINFO info;
  FRESULT r = f_stat(p, &info);
  if (r == FR_NO_FILE || r == FR_NO_PATH)
    return 0;
  if (r != FR_OK)
    return -1;
  size = info.fsize;
  directory = info.fattrib & AM_DIR;
#else
  struct ::stat info;
  if (::stat(p, &info)) {
    return errno == ENOENT ? 0 : -1;
  }
  size = info.st_size;
  directory = S_ISDIR(info.st_mode);
#endif
  return 1;
}
bool Storage::open(const char *name, bool writing) {
  if (!gate())
    return false;
  char p[800];
  path(name, p);
#ifdef __DEVKITARM__
  return f_open(&_file, p, writing ? (FA_WRITE | FA_CREATE_NEW) : FA_READ) ==
         FR_OK;
#else
  if (writing) {
    int fd = ::open(p, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
      return false;
    _file = fdopen(fd, "wb");
    if (!_file)
      ::close(fd);
  } else
    _file = std::fopen(p, "rb");
  return _file;
#endif
}
bool Storage::close() {
  bool ok = gate();
#ifdef __DEVKITARM__
  return f_close(&_file) == FR_OK && ok;
#else
  int r = std::fclose(_file);
  _file = nullptr;
  return !r && ok;
#endif
}
bool Storage::sync() {
  if (!gate())
    return false;
#ifdef __DEVKITARM__
  return f_sync(&_file) == FR_OK;
#else
  return !std::fflush(_file) && !::fsync(fileno(_file));
#endif
}
bool Storage::read(void *data, std::size_t want, std::size_t &got) {
  if (!gate())
    return false;
#ifdef __DEVKITARM__
  UINT n = 0;
  FRESULT r = f_read(&_file, data, want, &n);
  got = n;
  return r == FR_OK;
#else
  got = std::fread(data, 1, want, _file);
  return !std::ferror(_file);
#endif
}
bool Storage::write(const void *data, std::size_t size) {
  if (!gate())
    return false;
#ifdef __DEVKITARM__
  UINT n = 0;
  return f_write(&_file, data, size, &n) == FR_OK && n == size;
#else
  return std::fwrite(data, 1, size, _file) == size;
#endif
}
bool Storage::rename(const char *a, const char *b) {
  if (!gate())
    return false;
  char p[800], q[800];
  path(a, p);
  path(b, q);
#ifdef __DEVKITARM__
  return f_rename(p, q) == FR_OK;
#else
  struct ::stat st;
  if (!::stat(q, &st) || errno != ENOENT)
    return false;
  return !::rename(p, q);
#endif
}
bool Storage::remove(const char *a) {
  if (!gate())
    return false;
  char p[800];
  path(a, p);
#ifdef __DEVKITARM__
  return f_unlink(p) == FR_OK;
#else
  return !::unlink(p);
#endif
}
bool Storage::dir_open() {
  if (!gate())
    return false;
#ifdef __DEVKITARM__
  return f_opendir(&_dir, _root) == FR_OK;
#else
  _dir = opendir(_root);
  return _dir;
#endif
}
bool Storage::dir_next(char *name, bool &directory) {
  if (!gate())
    return false;
#ifdef __DEVKITARM__
  FILINFO e;
  if (f_readdir(&_dir, &e) != FR_OK)
    return false;
  std::strcpy(name, e.fname);
  directory = e.fattrib & AM_DIR;
#else
  errno = 0;
  dirent *e = readdir(_dir);
  if (!e) {
    name[0] = 0;
    return !errno;
  }
  writer::format(name, FILE_NAME_SIZE, "%s", e->d_name);
  std::size_t size;
  int r = stat(name, size, directory);
  if (r != 1)
    return false;
#endif
  return true;
}
bool Storage::dir_close() {
  bool ok = gate();
#ifdef __DEVKITARM__
  return f_closedir(&_dir) == FR_OK && ok;
#else
  int r = closedir(_dir);
  _dir = nullptr;
  return !r && ok;
#endif
}
StoreResult Storage::init() {
#ifdef __DEVKITARM__
  REG_WAITCNT = 0x40c0;
  set_supercard_mode(MAPPED_SDRAM, true, true);
  t_card_info info;
  if (sdcard_init(&info) || f_mount(&_fatfs, "0:", 1) != FR_OK)
    return StoreResult::IO_ERROR;
  FRESULT r = f_mkdir(_root);
  if (r != FR_OK && r != FR_EXIST)
    return StoreResult::IO_ERROR;
#else
  if (::mkdir(_root, 0700) && errno != EEXIST)
    return StoreResult::IO_ERROR;
#endif
  return scan();
}
StoreResult Storage::recover_all() {
  for (;;) {
    if (!dir_open())
      return StoreResult::IO_ERROR;
    char target[FILE_NAME_SIZE] = {};
    bool ok = true;
    for (;;) {
      char n[FILE_NAME_SIZE];
      bool directory = false;
      if (!dir_next(n, directory)) {
        ok = false;
        break;
      }
      if (!n[0])
        break;
      std::size_t len = std::strlen(n);
      if (!directory && len > 4 &&
          (!std::strcmp(n + len - 4, ".gwt") ||
           !std::strcmp(n + len - 4, ".gwb") ||
           !std::strcmp(n + len - 4, ".gwi"))) {
        n[len - 4] = 0;
        if (safe_name(n)) {
          std::strcpy(target, n);
          break;
        }
      }
    }
    if (!dir_close()) {
      ok = false;
    }
    if (!ok)
      return StoreResult::IO_ERROR;
    if (!target[0])
      return StoreResult::OK;
    auto r = recover(target);
    if (r != StoreResult::OK)
      return r;
  }
}
StoreResult Storage::scan() {
  _page = 0;
  auto r = recover_all();
  return r == StoreResult::OK ? scan_page() : r;
}
StoreResult Storage::next_page() {
  ++_page;
  return scan_page();
}
StoreResult Storage::previous_page() {
  if (_page)
    --_page;
  return scan_page();
}
StoreResult Storage::scan_page() {
  char after[FILE_NAME_SIZE] = {};
  for (int page = 0; page <= _page; ++page) {
    _count = 0;
    _total = 0;
    Date latest{};
    bool found = false;
    if (!dir_open())
      return StoreResult::IO_ERROR;
    bool ok = true;
    for (;;) {
      char name[FILE_NAME_SIZE];
      bool directory = false;
      if (!dir_next(name, directory)) {
        ok = false;
        break;
      }
      if (!name[0])
        break;
      if (directory || !txt(name))
        continue;
      ++_total;
      Date d{};
      if (parse_diary_name(name, d) &&
          (!found || d.year > latest.year ||
           (d.year == latest.year &&
            (d.month > latest.month ||
             (d.month == latest.month && d.day > latest.day))))) {
        latest = d;
        found = true;
      }
      if (after[0] && order(name, after) <= 0)
        continue;
      int i = 0;
      while (i < _count && order(_names[i], name) < 0)
        ++i;
      if (i < FILE_PAGE_SIZE) {
        if (_count < FILE_PAGE_SIZE)
          ++_count;
        for (int j = _count - 1; j > i; --j)
          std::strcpy(_names[j], _names[j - 1]);
        std::strcpy(_names[i], name);
      }
    }
    if (!dir_close())
      ok = false;
    _proposed = found ? next_day(latest) : Date{10, 7, 2026};
    if (!valid_date(_proposed))
      _proposed = latest;
    if (!ok)
      return StoreResult::IO_ERROR;
    if (_count)
      std::strcpy(after, _names[_count - 1]);
  }
  return StoreResult::OK;
}
bool Storage::write_file(const char *name, const char *data, std::size_t size) {
  if (!open(name, true))
    return false;
  bool ok = true;
  for (std::size_t p = 0; p < size && ok;) {
    std::size_t n = size - p;
    if (n > 512)
      n = 512;
    ok = write(data + p, n);
    p += n;
  }
  if (ok)
    ok = sync();
  if (!close())
    ok = false;
  return ok;
}
// Small whole-file reads only (the save journal).
StoreResult Storage::read_file(const char *name, char *data, std::size_t capacity,
                               std::size_t &size) {
  bool directory = false;
  int r = stat(name, size, directory);
  if (r != 1 || directory)
    return StoreResult::IO_ERROR;
  if (size >= capacity)
    return StoreResult::TOO_LARGE;
  if (!open(name, false))
    return StoreResult::IO_ERROR;
  std::size_t got = 0;
  char extra;
  bool ok = read(data, size, got) && got == size;
  if (ok)
    ok = read(&extra, 1, got) && got == 0;
  if (!close())
    ok = false;
  if (!ok)
    return StoreResult::IO_ERROR;
  data[size] = 0;
  return StoreResult::OK;
}
StoreResult Storage::hash_file(const char *name, std::size_t &size, uint32_t &hash) {
  bool directory = false;
  std::size_t expected = 0;
  int r = stat(name, expected, directory);
  if (r != 1 || directory)
    return StoreResult::IO_ERROR;
  if (!open(name, false))
    return StoreResult::IO_ERROR;
  bool ok = true;
  size = 0;
  hash = FNV_START;
  for (;;) {
    std::size_t got = 0;
    if (!read(_chunk, sizeof(_chunk), got)) {
      ok = false;
      break;
    }
    if (!got)
      break;
    hash = fnv(hash, _chunk, got);
    size += got;
  }
  if (!close())
    ok = false;
  return ok && size == expected ? StoreResult::OK : StoreResult::IO_ERROR;
}
bool Storage::open_source(const char *name, int slot) {
  char p[800];
  if (!gate() || !path(name, p))
    return false;
  return _sources[slot].open(p);
}
void Storage::close_source() {
  if (_active >= 0)
    _sources[_active].close();
  _active = -1;
  _source_name[0] = 0;
}
void Storage::source_moved(const char *name) {
  char p[800];
  std::strcpy(_source_name, name);
  if (path(name, p))
    _sources[_active].moved(p);
}
// The document reads this file: recovery must never remove it.
bool Storage::is_source(const char *name) const {
  return _active >= 0 && !std::strcmp(_source_name, name);
}
StoreResult Storage::create(const char *name, TextModel &text) {
  if (!safe_name(name))
    return StoreResult::INVALID_NAME;
  auto recovery = recover_all();
  if (recovery != StoreResult::OK)
    return recovery;
  if (!dir_open())
    return StoreResult::IO_ERROR;
  bool exists = false, ok = true;
  for (;;) {
    char n[FILE_NAME_SIZE];
    bool dir = false;
    if (!dir_next(n, dir)) {
      ok = false;
      break;
    }
    if (!n[0])
      break;
    const char *names[] = {n};
    if (!can_create_new(name, names, 1)) {
      exists = true;
      break;
    }
  }
  if (!dir_close()) {
    ok = false;
  }
  if (!ok)
    return StoreResult::IO_ERROR;
  if (exists)
    return StoreResult::EXISTS;
  std::size_t size = 0;
  bool directory = false;
  int r = stat(name, size, directory);
  if (r < 0)
    return StoreResult::IO_ERROR;
  if (r)
    return StoreResult::EXISTS;
  if (!write_file(name, "", 0))
    return StoreResult::IO_ERROR;
  std::size_t n = 0;
  uint32_t hash = 0;
  if (hash_file(name, n, hash) != StoreResult::OK || n)
    return StoreResult::IO_ERROR;
  close_source();
  std::strcpy(_current, name);
  text.set_text("");
  return StoreResult::OK;
}
StoreResult Storage::recover(const char *name) {
  if (!safe_name(name))
    return StoreResult::INVALID_NAME;
  char temp[FILE_NAME_SIZE], backup[FILE_NAME_SIZE], journal[FILE_NAME_SIZE];
  std::strcpy(temp, name);
  std::strcat(temp, ".gwt");
  std::strcpy(backup, name);
  std::strcat(backup, ".gwb");
  std::strcpy(journal, name);
  std::strcat(journal, ".gwi");
  std::size_t n = 0;
  uint32_t hash = 0;
  bool dir = false;
  int original = stat(name, n, dir);
  if (original < 0 || dir)
    return StoreResult::IO_ERROR;
  int b = stat(backup, n, dir);
  if (b < 0 || dir)
    return StoreResult::IO_ERROR;
  int t = stat(temp, n, dir);
  if (t < 0 || dir)
    return StoreResult::IO_ERROR;
  int j = stat(journal, n, dir);
  if (j < 0 || dir)
    return StoreResult::IO_ERROR;
  if (!b && !t && !j)
    return StoreResult::OK;
  // FatFS rename is NOT atomic. A failed directory update can leave two names
  // referring to one cluster chain. Never unlink either in this ambiguous state.
  if(original && t) return StoreResult::RECOVERY_NEEDED;
  // Rename-back is safe when the original name disappeared between the two
  // renames.
  if (b && !original) {
    if (hash_file(backup, n, hash) != StoreResult::OK)
      return StoreResult::RECOVERY_NEEDED;
    if (!rename(backup, name))
      return StoreResult::IO_ERROR;
    if (is_source(backup))
      source_moved(name);
    b = 0;
    original = 1;
  } else if (b) {
    // A replacement may only supersede the backup if the transient manifest
    // validates it.
    char record[64];
    if (!j || read_file(journal, record, sizeof(record), n) != StoreResult::OK)
      return StoreResult::RECOVERY_NEEDED;
    unsigned expected_size = 0;
    uint32_t expected_hash = 0;
    if (!parse_manifest(record, n, expected_size, expected_hash))
      return StoreResult::RECOVERY_NEEDED;
    if (hash_file(name, n, hash) != StoreResult::OK || n != expected_size ||
        hash != expected_hash)
      return StoreResult::RECOVERY_NEEDED;
  }
  if (!original)
    return StoreResult::RECOVERY_NEEDED;
  // Before deleting staging artifacts, confirm a readable canonical copy
  // remains.
  if (hash_file(name, n, hash) != StoreResult::OK)
    return StoreResult::RECOVERY_NEEDED;
  // The open document may still read an old copy: never delete it.
  if ((b && is_source(backup)) || (t && is_source(temp)))
    return StoreResult::RECOVERY_NEEDED;
  if (b && !remove(backup))
    return StoreResult::IO_ERROR;
  if (t && !remove(temp))
    return StoreResult::IO_ERROR;
  if (j && !remove(journal))
    return StoreResult::IO_ERROR;
  return StoreResult::OK;
}
// Streams the document (file bytes plus edits) into the temp copy, verifies
// it by hash, swaps it in with backup + journal as before, then reopens the
// saved file as the document so the typed-text buffer is free again.
StoreResult Storage::save(TextModel &text) {
  if (!safe_name(_current))
    return StoreResult::INVALID_NAME;
  char temp[FILE_NAME_SIZE], backup[FILE_NAME_SIZE], journal[FILE_NAME_SIZE];
  std::strcpy(temp, _current);
  std::strcat(temp, ".gwt");
  std::strcpy(backup, _current);
  std::strcat(backup, ".gwb");
  std::strcpy(journal, _current);
  std::strcat(journal, ".gwi");
  for (const char *p : {temp, backup, journal}) {
    std::size_t size = 0;
    bool dir = false;
    int r = stat(p, size, dir);
    if (r < 0)
      return StoreResult::IO_ERROR;
    if (r)
      return StoreResult::RECOVERY_NEEDED;
  }
  const std::size_t bytes = text.bytes();
  uint32_t crc = FNV_START;
  if (!open(temp, true))
    return StoreResult::IO_ERROR;
  bool ok = true;
  for (std::size_t p = 0; p < bytes && ok;) {
    std::size_t want = bytes - p < sizeof(_chunk) ? bytes - p : sizeof(_chunk);
    ok = text.copy(p, _chunk, want) == want && write(_chunk, want);
    crc = fnv(crc, _chunk, want);
    p += want;
  }
  if (ok)
    ok = sync();
  if (!close())
    ok = false;
  if (text.read_failed()) {
    text.clear_read_failed();
    ok = false;
  }
  if (!ok)
    return StoreResult::IO_ERROR;
  std::size_t n = 0;
  uint32_t hash = 0;
  if (hash_file(temp, n, hash) != StoreResult::OK || n != bytes || hash != crc)
    return StoreResult::IO_ERROR;
  char record[48];
  int len = writer::format(record, sizeof(record), "GWW1 %u %08lx\n",
                          unsigned(n), static_cast<unsigned long>(crc));
  if (!write_file(journal, record, len))
    return StoreResult::IO_ERROR;
  // The open document keeps reading the old copy under its backup name.
  if (!rename(_current, backup))
    return StoreResult::IO_ERROR;
  if (is_source(_current))
    source_moved(backup);
  if (!rename(temp, _current))
    return StoreResult::IO_ERROR;
  if (hash_file(_current, n, hash) != StoreResult::OK || n != bytes || hash != crc)
    return StoreResult::IO_ERROR;
  // The saved file becomes the document; then the old copy can go.
  const int slot = _active == 0 ? 1 : 0;
  if (!open_source(_current, slot) || _sources[slot].size() != bytes)
    return StoreResult::IO_ERROR;
  const std::size_t caret = text.caret_byte();
  close_source();
  _active = slot;
  std::strcpy(_source_name, _current);
  text.open(&_sources[slot]);
  text.set_caret(caret);
  if (!remove(backup) || !remove(journal)) {
    text.mark_dirty();
    return StoreResult::IO_ERROR;
  }
  return StoreResult::OK;
}
StoreResult Storage::load(const char *name, TextModel &text) {
  if (!safe_name(name))
    return StoreResult::INVALID_NAME;
  std::size_t size = 0;
  bool directory = false;
  int r = stat(name, size, directory);
  if (r != 1 || directory)
    return StoreResult::IO_ERROR;
  if (size > MAX_FILE_BYTES)
    return StoreResult::TOO_LARGE;
  // Check the new file through the spare source; the current document stays.
  const int slot = _active == 0 ? 1 : 0;
  FileSource &source = _sources[slot];
  if (!open_source(name, slot))
    return StoreResult::IO_ERROR;
  Utf8Stream utf8;
  bool ok = source.size() == size;
  for (std::size_t p = 0; ok && p < size;) {
    std::size_t want = size - p < sizeof(_chunk) ? size - p : sizeof(_chunk);
    ok = source.read(p, _chunk, want);
    if (ok && !utf8.feed(_chunk, want)) {
      source.close();
      return StoreResult::INVALID_UTF8;
    }
    p += want;
  }
  if (!ok) {
    source.close();
    return StoreResult::IO_ERROR;
  }
  if (!utf8.finish()) {
    source.close();
    return StoreResult::INVALID_UTF8;
  }
  close_source();
  _active = slot;
  std::strcpy(_source_name, name);
  std::strcpy(_current, name);
  text.open(&source);
  return StoreResult::OK;
}
} // namespace writer
