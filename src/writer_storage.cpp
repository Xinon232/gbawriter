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
// A diary date in any NameFormat; the chosen format decides names that two
// formats can read (01022026: 1 February or 2 January).
bool diary_date(const char *n, NameFormat preferred, Date &d) {
  if (parse_diary_name(n, preferred, d))
    return true;
  for (int f = 0; f < NAME_FORMATS; ++f)
    if (NameFormat(f) != preferred && parse_diary_name(n, NameFormat(f), d))
      return true;
  return false;
}
int fold(unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
int compare_names(const char *a, const char *b) {
  for (int i = 0;; ++i) {
    int x = fold(a[i]), y = fold(b[i]);
    if (x != y)
      return x - y;
    if (!x)
      return std::strcmp(a, b);
  }
}
bool same_name(const char *a, const char *b) {
  for (;; ++a, ++b) {
    if (fold(*a) != fold(*b))
      return false;
    if (!*a)
      return true;
  }
}
// Starting order: diary dates newest first, then other names A to Z.
int order(const char *a, const char *b, NameFormat preferred) {
  Date da{}, db{};
  bool ax = diary_date(a, preferred, da), by = diary_date(b, preferred, db);
  if (ax != by)
    return ax ? -1 : 1;
  if (ax) {
    int d = (db.year - da.year) * 372 + (db.month - da.month) * 31 + db.day - da.day;
    if (d)
      return d;
  }
  return compare_names(a, b);
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
  case StoreResult::OPEN_FILE:
    return "This file is open. Open another file first.";
  case StoreResult::NAME_USED:
    return "Name already used";
  default:
    return "SD I/O ERROR - TEXT KEPT";
  }
}
Storage::Storage(const char *root) {
  writer::format(_card, sizeof(_card), "%s", root);
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
  char p[800];
  path(name, p);
  return stat_path(p, size, directory);
}
int Storage::stat_path(const char *p, std::size_t &size, bool &directory) {
  if (!gate())
    return -1;
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
  char p[800];
  path(name, p);
  return open_path(p, writing);
}
bool Storage::open_path(const char *p, bool writing) {
  if (!gate())
    return false;
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
bool Storage::make_dir(const char *a) {
  if (!gate())
    return false;
  char p[800];
  path(a, p);
#ifdef __DEVKITARM__
  FRESULT r = f_mkdir(p);
  if (r != FR_OK && r != FR_EXIST)
    return false;
  f_chmod(p, AM_HID | AM_SYS, AM_HID | AM_SYS);
  return true;
#else
  return !::mkdir(p, 0700) || errno == EEXIST;
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
bool Storage::dir_open() { return dir_open_path(_root); }
bool Storage::dir_open_path(const char *p) {
  if (!gate())
    return false;
#ifdef __DEVKITARM__
  return f_opendir(&_dir, p) == FR_OK;
#else
  writer::format(_dir_path, sizeof(_dir_path), "%s", p);
  _dir = opendir(p);
  return _dir;
#endif
}
bool Storage::dir_next(char *name, bool &directory) {
  bool hidden = false;
  return dir_next_entry(name, directory, hidden);
}
bool Storage::dir_next_entry(char *name, bool &directory, bool &hidden) {
  if (!gate())
    return false;
#ifdef __DEVKITARM__
  FILINFO e;
  if (f_readdir(&_dir, &e) != FR_OK)
    return false;
  std::strcpy(name, e.fname);
  directory = e.fattrib & AM_DIR;
  hidden = (e.fattrib & (AM_HID | AM_SYS)) || e.fname[0] == '.';
#else
  for (;;) {
    errno = 0;
    dirent *e = readdir(_dir);
    if (!e) {
      name[0] = 0;
      return !errno;
    }
    if (!std::strcmp(e->d_name, ".") || !std::strcmp(e->d_name, ".."))
      continue;
    writer::format(name, FILE_NAME_SIZE, "%s", e->d_name);
    break;
  }
  char p[1100];
  writer::format(p, sizeof(p), "%s/%s", _dir_path, name);
  struct ::stat info;
  if (::stat(p, &info))
    return false;
  directory = S_ISDIR(info.st_mode);
  hidden = name[0] == '.';
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
  _mounted = true;
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
// ---- List memory (Home and the import browser) ----
namespace {
constexpr uint16_t NO_RANK = 0xffff;
uint32_t name_hash(const char *n) { return fnv(FNV_START, n, std::strlen(n)); }
constexpr char STATE_DIR[] = "GBAWRITER.SYS";
constexpr const char *STATE_SLOTS[2] = {"GBAWRITER.SYS/STATE0.DAT", "GBAWRITER.SYS/STATE1.DAT"};
constexpr char STATE_MAGIC[8] = {'G', 'B', 'W', 'S', 'T', '0', '0', '1'};
constexpr std::size_t STATE_HEADER = 24;
} // namespace
void Storage::list_clear() {
  _count = 0;
  _pool_used = 0;
  std::memset(_hash, 0, sizeof(_hash));
}
bool Storage::list_add(const char *n, bool folder) {
  const std::size_t len = std::strlen(n) + 1;
  if (_count >= MAX_FILES || _pool_used + len > NAME_POOL_BYTES)
    return false;
  std::memcpy(_pool + _pool_used, n, len);
  _offsets[_count] = uint16_t(_pool_used);
  _folder[_count] = folder;
  _rank[_count] = NO_RANK;
  _order[_count] = uint16_t(_count);
  for (uint32_t h = name_hash(n) & 2047;; h = (h + 1) & 2047)
    if (!_hash[h]) {
      _hash[h] = uint16_t(_count + 1);
      break;
    }
  _pool_used += len;
  ++_count;
  return true;
}
// Index in _offsets (not the list row) of an exact name, or -1.
int Storage::list_find(const char *n) const {
  for (uint32_t h = name_hash(n) & 2047; _hash[h]; h = (h + 1) & 2047)
    if (!std::strcmp(_pool + _offsets[_hash[h] - 1], n))
      return _hash[h] - 1;
  return -1;
}
StoreResult Storage::scan() {
  auto r = recover_all();
  if (r != StoreResult::OK)
    return r;
  _browsing = false;
  list_clear();
  _total = 0;
  Date latest{};
  bool found = false;
  if (!dir_open())
    return StoreResult::IO_ERROR;
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
    if (directory || !txt(n))
      continue;
    ++_total;
    Date d{};
    if (parse_diary_name(n, _settings.format, d) && (!found || d.year > latest.year ||
                                                     (d.year == latest.year &&
                                                      (d.month > latest.month ||
                                                       (d.month == latest.month && d.day > latest.day))))) {
      latest = d;
      found = true;
    }
    list_add(n, false);
  }
  if (!dir_close())
    ok = false;
  if (!ok)
    return StoreResult::IO_ERROR;
  _mounted = true; // /gbawriter could be read
  // Settings and the saved order (missing or damaged: defaults).
  read_state();
  _proposed = found ? next_day(latest) : _settings.has_date ? next_day(_settings.date) : Date{10, 7, 2026};
  if (!valid_date(_proposed))
    _proposed = found ? latest : _settings.date;
  // Unknown files first (starting order), then the saved order.
  bool changed = false;
  int known = 0;
  for (int i = 0; i < _count; ++i)
    if (_rank[i] == NO_RANK)
      changed = true;
    else
      ++known;
  if (known != _saved_count)
    changed = true;
  const NameFormat f = _settings.format;
  auto before = [&](uint16_t a, uint16_t b) {
    const bool ua = _rank[a] == NO_RANK, ub = _rank[b] == NO_RANK;
    if (ua != ub)
      return ua;
    if (!ua)
      return _rank[a] < _rank[b];
    return order(_pool + _offsets[a], _pool + _offsets[b], f) < 0;
  };
  for (int gap = _count / 2; gap; gap /= 2)
    for (int i = gap; i < _count; ++i)
      for (int j = i; j >= gap && before(_order[j], _order[j - gap]); j -= gap) {
        uint16_t t = _order[j];
        _order[j] = _order[j - gap];
        _order[j - gap] = t;
      }
  // Keep the positions of new files once an order is saved.
  if (_have_order && changed)
    save_state();
  return StoreResult::OK;
}
void Storage::move(int i, int j) {
  if (_browsing || i < 0 || j < 0 || i >= _count || j >= _count)
    return;
  uint16_t t = _order[i];
  _order[i] = _order[j];
  _order[j] = t;
}
// ---- GBAWRITER.SYS ----
// "GBWST001", u32 generation, u8 format, reorder, delete, has_date, u8 day,
// u8 month, u16 year, u32 count, u32 reserved, count NUL-terminated names,
// u32 FNV-1a of everything before. Little endian.
bool Storage::read_slot(int slot, uint32_t &generation, bool apply) {
  std::size_t size = 0;
  bool directory = false;
  if (stat(STATE_SLOTS[slot], size, directory) != 1 || directory || size < STATE_HEADER + 4)
    return false;
  if (!open(STATE_SLOTS[slot], false))
    return false;
  unsigned char header[STATE_HEADER];
  uint32_t hash = FNV_START, stored = 0;
  std::size_t done = 0, got = 0;
  bool ok = read(header, STATE_HEADER, got) && got == STATE_HEADER &&
            !std::memcmp(header, STATE_MAGIC, 8);
  if (ok) {
    hash = fnv(hash, reinterpret_cast<const char *>(header), STATE_HEADER);
    done = STATE_HEADER;
  }
  char name[FILE_NAME_SIZE];
  std::size_t name_len = 0;
  uint32_t names = 0;
  uint16_t rank = 0;
  // Names, then the checksum (the last four bytes).
  while (ok && done < size - 4) {
    std::size_t want = size - 4 - done < sizeof(_chunk) ? size - 4 - done : sizeof(_chunk);
    if (!read(_chunk, want, got) || got != want) {
      ok = false;
      break;
    }
    hash = fnv(hash, _chunk, want);
    for (std::size_t k = 0; k < want && ok; ++k) {
      if (name_len >= FILE_NAME_SIZE) {
        ok = false;
        break;
      }
      name[name_len++] = _chunk[k];
      if (!_chunk[k]) {
        ++names;
        if (apply) {
          int at = list_find(name);
          if (at >= 0 && _rank[at] == NO_RANK)
            _rank[at] = rank++;
        }
        name_len = 0;
      }
    }
    done += want;
  }
  unsigned char tail[4];
  ok = ok && read(tail, 4, got) && got == 4 && !name_len;
  if (!close())
    ok = false;
  stored = tail[0] | uint32_t(tail[1]) << 8 | uint32_t(tail[2]) << 16 | uint32_t(tail[3]) << 24;
  const uint32_t count = header[16] | uint32_t(header[17]) << 8 | uint32_t(header[18]) << 16 |
                         uint32_t(header[19]) << 24;
  ok = ok && stored == hash && names == count;
  if (!ok)
    return false;
  generation = header[8] | uint32_t(header[9]) << 8 | uint32_t(header[10]) << 16 |
               uint32_t(header[11]) << 24;
  if (apply) {
    Settings s;
    s.format = header[12] < NAME_FORMATS ? NameFormat(header[12]) : NameFormat::DDMMYYYY;
    s.reorder = header[13];
    s.delete_files = header[14];
    s.has_date = header[15] & 1;
    s.date = {header[20], header[21], header[22] | header[23] << 8};
    if (!valid_date(s.date)) {
      s.has_date = false;
      s.date = {10, 7, 2026};
    }
    _settings = s;
    _saved_count = int(rank);
  }
  return true;
}
StoreResult Storage::read_state() {
  for (int i = 0; i < _count; ++i)
    _rank[i] = NO_RANK;
  _saved_count = 0;
  uint32_t g[2] = {0, 0};
  bool valid[2] = {read_slot(0, g[0], false), read_slot(1, g[1], false)};
  if (!valid[0] && !valid[1]) {
    _have_order = false;
    _generation = 0;
    _newest = 1;
    // Settings changed in this session stay until saved again.
    return StoreResult::OK;
  }
  _newest = valid[1] && (!valid[0] || g[1] > g[0]) ? 1 : 0;
  _generation = g[_newest];
  _have_order = read_slot(_newest, g[_newest], true);
  return StoreResult::OK;
}
StoreResult Storage::save_state() {
  if (_browsing || !_mounted)
    return StoreResult::IO_ERROR;
  if (!make_dir(STATE_DIR))
    return StoreResult::IO_ERROR;
  const int slot = _newest ^ 1;
  std::size_t size = 0;
  bool directory = false;
  int exists = stat(STATE_SLOTS[slot], size, directory);
  if (exists < 0 || (exists && !remove(STATE_SLOTS[slot])))
    return StoreResult::IO_ERROR;
  if (!open(STATE_SLOTS[slot], true))
    return StoreResult::IO_ERROR;
  unsigned char header[STATE_HEADER] = {};
  std::memcpy(header, STATE_MAGIC, 8);
  const uint32_t generation = _generation + 1, count = uint32_t(_count);
  for (int k = 0; k < 4; ++k) {
    header[8 + k] = uint8_t(generation >> (8 * k));
    header[16 + k] = uint8_t(count >> (8 * k));
  }
  header[12] = uint8_t(_settings.format);
  header[13] = _settings.reorder;
  header[14] = _settings.delete_files;
  header[15] = _settings.has_date;
  header[20] = uint8_t(_settings.date.day);
  header[21] = uint8_t(_settings.date.month);
  header[22] = uint8_t(_settings.date.year);
  header[23] = uint8_t(_settings.date.year >> 8);
  uint32_t hash = fnv(FNV_START, reinterpret_cast<const char *>(header), STATE_HEADER);
  bool ok = write(header, STATE_HEADER);
  // Names through the chunk buffer, 512 bytes per write.
  std::size_t fill = 0;
  for (int i = 0; i < _count && ok; ++i) {
    const char *n = name(i);
    const std::size_t len = std::strlen(n) + 1;
    for (std::size_t k = 0; k < len && ok; ++k) {
      _chunk[fill++] = n[k];
      if (fill == sizeof(_chunk)) {
        hash = fnv(hash, _chunk, fill);
        ok = write(_chunk, fill);
        fill = 0;
      }
    }
  }
  if (ok && fill) {
    hash = fnv(hash, _chunk, fill);
    ok = write(_chunk, fill);
  }
  unsigned char tail[4] = {uint8_t(hash), uint8_t(hash >> 8), uint8_t(hash >> 16), uint8_t(hash >> 24)};
  if (ok)
    ok = write(tail, 4) && sync();
  if (!close())
    ok = false;
  if (!ok) {
    remove(STATE_SLOTS[slot]);
    return StoreResult::IO_ERROR;
  }
  _generation = generation;
  _newest = slot;
  _have_order = true;
  _saved_count = _count;
  return StoreResult::OK;
}
StoreResult Storage::set_settings(const Settings &s) {
  const Settings old = _settings;
  _settings = s;
  auto r = save_state();
  if (r != StoreResult::OK)
    _settings = old;
  return r;
}
StoreResult Storage::delete_configuration() {
  bool ok = true;
  for (const char *slot : STATE_SLOTS) {
    std::size_t size = 0;
    bool directory = false;
    int r = stat(slot, size, directory);
    if (r < 0 || (r && !remove(slot)))
      ok = false;
  }
  if (!ok)
    return StoreResult::IO_ERROR;
  _settings = Settings();
  _have_order = false;
  _generation = 0;
  _newest = 1;
  _saved_count = 0;
  return StoreResult::OK;
}
// ---- Rename, delete ----
bool Storage::is_open(const char *n) const { return _current[0] && !std::strcmp(_current, n); }
bool Storage::valid_base(const char *b) {
  if (!b || !*b)
    return false;
  const std::size_t len = std::strlen(b);
  if (len + 4 >= FILE_NAME_SIZE - 5 || b[0] == ' ' || b[len - 1] == ' ' || b[len - 1] == '.')
    return false;
  for (const char *p = b; *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c < 32 || c == 127 || std::strchr("/\\:*?\"<>|", c))
      return false;
  }
  return true;
}
// Recovery copies of name left by an interrupted save.
bool Storage::recovery_files(const char *n, int &found) {
  found = 0;
  for (const char *ext : {".gwt", ".gwb", ".gwi"}) {
    char p[FILE_NAME_SIZE + 8];
    writer::format(p, sizeof(p), "%s%s", n, ext);
    std::size_t size = 0;
    bool directory = false;
    int r = stat(p, size, directory);
    if (r < 0)
      return false;
    found += r;
  }
  return true;
}
StoreResult Storage::rename_file(int i, const char *base) {
  if (_browsing || i < 0 || i >= _count)
    return StoreResult::IO_ERROR;
  char old[FILE_NAME_SIZE], target[FILE_NAME_SIZE];
  std::strcpy(old, name(i));
  if (!valid_base(base))
    return StoreResult::INVALID_NAME;
  writer::format(target, sizeof(target), "%s.txt", base);
  if (!safe_name(target))
    return StoreResult::INVALID_NAME;
  if (is_open(old))
    return StoreResult::OPEN_FILE;
  if (!std::strcmp(old, target))
    return StoreResult::OK;
  int recovery = 0;
  if (!recovery_files(old, recovery))
    return StoreResult::IO_ERROR;
  if (recovery)
    return StoreResult::RECOVERY_NEEDED;
  // Any other entry with this name, also in other upper/lower case.
  if (!dir_open())
    return StoreResult::IO_ERROR;
  bool used = false, ok = true;
  for (;;) {
    char n[FILE_NAME_SIZE];
    bool directory = false;
    if (!dir_next(n, directory)) {
      ok = false;
      break;
    }
    if (!n[0])
      break;
    if (std::strcmp(n, old) && same_name(n, target))
      used = true;
  }
  if (!dir_close())
    ok = false;
  if (!ok)
    return StoreResult::IO_ERROR;
  if (used)
    return StoreResult::NAME_USED;
  if (same_name(old, target)) {
    // Only upper/lower case changes: through a temporary .txt name, so an
    // interruption leaves a visible file.
    const char temp[] = "gbawriter rename.txt";
    std::size_t size = 0;
    bool directory = false;
    int r = stat(temp, size, directory);
    if (r)
      return r < 0 ? StoreResult::IO_ERROR : StoreResult::NAME_USED;
    if (!rename(old, temp))
      return StoreResult::IO_ERROR;
    if (!rename(temp, target)) {
      rename(temp, old);
      return StoreResult::IO_ERROR;
    }
  } else if (!rename(old, target))
    return StoreResult::IO_ERROR;
  // The renamed file keeps its row.
  const std::size_t len = std::strlen(target) + 1;
  if (_pool_used + len <= NAME_POOL_BYTES) {
    std::memcpy(_pool + _pool_used, target, len);
    _offsets[_order[i]] = uint16_t(_pool_used);
    _pool_used += len;
    save_state();
    return StoreResult::OK;
  }
  return scan();
}
StoreResult Storage::delete_file(int i) {
  if (_browsing || i < 0 || i >= _count)
    return StoreResult::IO_ERROR;
  char old[FILE_NAME_SIZE];
  std::strcpy(old, name(i));
  if (is_open(old))
    return StoreResult::OPEN_FILE;
  int recovery = 0;
  if (!recovery_files(old, recovery))
    return StoreResult::IO_ERROR;
  if (recovery)
    return StoreResult::RECOVERY_NEEDED;
  if (!remove(old))
    return StoreResult::IO_ERROR;
  for (int k = i; k + 1 < _count; ++k)
    _order[k] = _order[k + 1];
  --_count;
  --_total;
  if (_have_order)
    save_state();
  return StoreResult::OK;
}
// ---- Import ----
StoreResult Storage::browse(const char *where) {
  _browsing = true;
  list_clear();
  char p[800];
  writer::format(p, sizeof(p), "%s%s", _card, where);
  const bool root = !where[1];
  if (!dir_open_path(p))
    return StoreResult::IO_ERROR;
  bool ok = true;
  for (;;) {
    char n[FILE_NAME_SIZE];
    bool directory = false, hidden = false;
    if (!dir_next_entry(n, directory, hidden)) {
      ok = false;
      break;
    }
    if (!n[0])
      break;
    if (hidden || (root && directory && same_name(n, "gbawriter")))
      continue;
    if (directory || txt(n))
      list_add(n, directory);
  }
  if (!dir_close())
    ok = false;
  // Folders first, then TXT files, each A to Z.
  auto before = [&](uint16_t a, uint16_t b) {
    if (_folder[a] != _folder[b])
      return _folder[a] > _folder[b];
    return compare_names(_pool + _offsets[a], _pool + _offsets[b]) < 0;
  };
  for (int gap = _count / 2; gap; gap /= 2)
    for (int i = gap; i < _count; ++i)
      for (int j = i; j >= gap && before(_order[j], _order[j - gap]); j -= gap) {
        uint16_t t = _order[j];
        _order[j] = _order[j - gap];
        _order[j - gap] = t;
      }
  return ok ? StoreResult::OK : StoreResult::IO_ERROR;
}
StoreResult Storage::import_name(const char *n, char (&target)[FILE_NAME_SIZE]) {
  char base[FILE_NAME_SIZE];
  std::size_t len = std::strlen(n);
  if (len < 5 || len >= FILE_NAME_SIZE)
    return StoreResult::INVALID_NAME;
  std::memcpy(base, n, len - 4);
  base[len - 4] = 0;
  // Which of name, "base (2).txt" .. "base (99).txt" are taken.
  bool taken[100] = {};
  if (!dir_open())
    return StoreResult::IO_ERROR;
  bool ok = true;
  const std::size_t blen = std::strlen(base);
  for (;;) {
    char e[FILE_NAME_SIZE];
    bool directory = false;
    if (!dir_next(e, directory)) {
      ok = false;
      break;
    }
    if (!e[0])
      break;
    if (same_name(e, n)) {
      taken[1] = true;
      continue;
    }
    const std::size_t elen = std::strlen(e);
    if (elen < blen + 8 || e[blen] != ' ' || e[blen + 1] != '(' || !txt(e))
      continue;
    char head[FILE_NAME_SIZE];
    std::memcpy(head, e, blen);
    head[blen] = 0;
    if (!same_name(head, base))
      continue;
    int number = 0;
    std::size_t k = blen + 2;
    while (k < elen && e[k] >= '0' && e[k] <= '9' && number < 1000)
      number = number * 10 + (e[k++] - '0');
    if (k == elen - 5 && e[k] == ')' && number >= 2 && number <= 99)
      taken[number] = true;
  }
  if (!dir_close())
    ok = false;
  if (!ok)
    return StoreResult::IO_ERROR;
  if (!taken[1]) {
    if (!safe_name(n))
      return StoreResult::INVALID_NAME;
    std::strcpy(target, n);
    return StoreResult::OK;
  }
  for (int k = 2; k <= 99; ++k)
    if (!taken[k]) {
      if (writer::format(target, sizeof(target), "%s (%d).txt", base, k) >= int(sizeof(target)) ||
          !safe_name(target))
        return StoreResult::INVALID_NAME;
      return StoreResult::NAME_USED;
    }
  return StoreResult::EXISTS;
}
StoreResult Storage::import_file(const char *source, const char *target, Progress progress,
                                 void *context) {
  if (!safe_name(target))
    return StoreResult::INVALID_NAME;
  char from[800];
  writer::format(from, sizeof(from), "%s%s", _card, source);
  std::size_t total = 0;
  bool directory = false;
  if (stat_path(from, total, directory) != 1 || directory)
    return StoreResult::IO_ERROR;
  if (total > MAX_FILE_BYTES)
    return StoreResult::TOO_LARGE;
  if (!gate())
    return StoreResult::IO_ERROR;
#ifdef __DEVKITARM__
  if (f_open(&_in, from, FA_READ) != FR_OK)
    return StoreResult::IO_ERROR;
#else
  _in = std::fopen(from, "rb");
  if (!_in)
    return StoreResult::IO_ERROR;
#endif
  if (!open(target, true)) {
#ifdef __DEVKITARM__
    f_close(&_in);
#else
    std::fclose(_in);
    _in = nullptr;
#endif
    return StoreResult::EXISTS;
  }
  bool ok = true;
  std::size_t done = 0;
  int shown = -1;
  while (ok && done < total) {
    std::size_t want = total - done < sizeof(_copy) ? total - done : sizeof(_copy), got = 0;
#ifdef __DEVKITARM__
    UINT n = 0;
    ok = gate() && f_read(&_in, _copy, want, &n) == FR_OK && n == want;
    got = n;
#else
    got = std::fread(_copy, 1, want, _in);
    ok = gate() && got == want;
#endif
    if (ok)
      ok = write(_copy, got);
    done += got;
    const int percent = total ? int((uint64_t(done) * 100) / total) : 100;
    if (progress && percent != shown) {
      shown = percent;
      progress(context, percent);
    }
  }
  if (ok)
    ok = sync();
  if (!close())
    ok = false;
#ifdef __DEVKITARM__
  f_close(&_in);
#else
  std::fclose(_in);
  _in = nullptr;
#endif
  if (ok) {
    std::size_t size = 0;
    ok = stat(target, size, directory) == 1 && size == total;
  }
  if (!ok) {
    remove(target);
    return StoreResult::IO_ERROR;
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
