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
} // namespace
const char *store_message(StoreResult r) {
  switch (r) {
  case StoreResult::OK:
    return "SAVED";
  case StoreResult::EXISTS:
    return "FILE ALREADY EXISTS";
  case StoreResult::INVALID_NAME:
    return "INVALID / LONG FILENAME";
  case StoreResult::TOO_LARGE:
    return "FILE EXCEEDS 24 KIB";
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
StoreResult Storage::read_file(const char *name, char *data,
                               std::size_t &size) {
  bool directory = false;
  int r = stat(name, size, directory);
  if (r != 1 || directory)
    return StoreResult::IO_ERROR;
  if (size > TEXT_CAPACITY)
    return StoreResult::TOO_LARGE;
  if (!open(name, false))
    return StoreResult::IO_ERROR;
  bool ok = true;
  for (std::size_t p = 0; p < size && ok;) {
    std::size_t n = size - p, got = 0;
    if (n > 512)
      n = 512;
    ok = read(data + p, n, got) && got == n;
    p += n;
  }
  char extra;
  std::size_t got = 0;
  if (ok)
    ok = read(&extra, 1, got) && got == 0;
  if (!close())
    ok = false;
  if (!ok)
    return StoreResult::IO_ERROR;
  data[size] = 0;
  return StoreResult::OK;
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
  auto result = read_file(name, _scratch, n);
  if (result != StoreResult::OK || n)
    return StoreResult::IO_ERROR;
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
    if (read_file(backup, _scratch, n) != StoreResult::OK)
      return StoreResult::RECOVERY_NEEDED;
    if (!rename(backup, name))
      return StoreResult::IO_ERROR;
    b = 0;
    original = 1;
  } else if (b) {
    // A replacement may only supersede the backup if the transient manifest
    // validates it.
    if (!j || read_file(journal, _scratch, n) != StoreResult::OK)
      return StoreResult::RECOVERY_NEEDED;
    unsigned expected_size = 0;
    uint32_t expected_hash = 0;
    if (!parse_manifest(_scratch, n, expected_size, expected_hash))
      return StoreResult::RECOVERY_NEEDED;
    if (read_file(name, _scratch, n) != StoreResult::OK || n != expected_size)
      return StoreResult::RECOVERY_NEEDED;
    uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < n; ++i)
      hash = (hash ^ static_cast<unsigned char>(_scratch[i])) * 16777619u;
    if (hash != expected_hash)
      return StoreResult::RECOVERY_NEEDED;
  }
  if (!original)
    return StoreResult::RECOVERY_NEEDED;
  // Before deleting staging artifacts, confirm a readable canonical copy
  // remains.
  if (read_file(name, _scratch, n) != StoreResult::OK)
    return StoreResult::RECOVERY_NEEDED;
  if (b && !remove(backup))
    return StoreResult::IO_ERROR;
  if (t && !remove(temp))
    return StoreResult::IO_ERROR;
  if (j && !remove(journal))
    return StoreResult::IO_ERROR;
  return StoreResult::OK;
}
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
  if (!write_file(temp, text.data(), text.bytes()))
    return StoreResult::IO_ERROR;
  std::size_t n = 0;
  auto r = read_file(temp, _scratch, n);
  if (r != StoreResult::OK || n != text.bytes() ||
      std::memcmp(_scratch, text.data(), n))
    return StoreResult::IO_ERROR;
  uint32_t crc = 2166136261u;
  for (std::size_t i = 0; i < n; ++i)
    crc = (crc ^ static_cast<unsigned char>(_scratch[i])) * 16777619u;
  char record[48];
  int len = writer::format(record, sizeof(record), "GWW1 %u %08lx\n",
                          unsigned(n), static_cast<unsigned long>(crc));
  if (!write_file(journal, record, len))
    return StoreResult::IO_ERROR;
  if (!rename(_current, backup))
    return StoreResult::IO_ERROR;
  if (!rename(temp, _current))
    return StoreResult::IO_ERROR;
  r = read_file(_current, _scratch, n);
  if (r != StoreResult::OK || n != text.bytes() ||
      std::memcmp(_scratch, text.data(), n))
    return StoreResult::IO_ERROR;
  if (!remove(backup) || !remove(journal))
    return StoreResult::IO_ERROR;
  text.mark_saved();
  return StoreResult::OK;
}
StoreResult Storage::load(const char *name, TextModel &text) {
  if (!safe_name(name))
    return StoreResult::INVALID_NAME;
  std::size_t size = 0;
  auto r = read_file(name, _scratch, size);
  if (r != StoreResult::OK)
    return r;
  if (!valid_utf8(_scratch, size))
    return StoreResult::INVALID_UTF8;
  text.set_text(_scratch);
  std::strcpy(_current, name);
  return StoreResult::OK;
}
} // namespace writer
