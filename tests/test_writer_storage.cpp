#include "writer_storage.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <unistd.h>
#include <vector>
using namespace writer;
static std::string read(const std::string &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
int main() {
  std::string root = "/tmp/gbawriter-store-" + std::to_string(getpid());
  std::filesystem::remove_all(root);
  std::filesystem::create_directory(root);
  Storage s(root.c_str());
  assert(s.init() == StoreResult::OK);
  TextModel t;
  assert(s.create("21102001.txt", t) == StoreResult::OK);
  assert(!t.dirty());
  std::ofstream(root + "/gbawriter/21102001.txt") << "untouched";
  assert(s.create("21102001.txt", t) == StoreResult::EXISTS);
  assert(read(root + "/gbawriter/21102001.txt") == "untouched");
  std::ofstream(root + "/gbawriter/12121990.txt") << "old";
  std::ofstream(root + "/gbawriter/z.txt") << "z";
  std::ofstream(root + "/gbawriter/a.TXT") << "a";
  std::ofstream(root + "/gbawriter/29022027.txt") << "invalid date";
  assert(s.scan() == StoreResult::OK);
  assert(s.count() == 5);
  assert(!strcmp(s.name(0), "21102001.txt"));
  assert(!strcmp(s.name(1), "12121990.txt"));
  Date d = s.proposed_date();
  assert(d.day == 22 && d.month == 10 && d.year == 2001);
  assert(s.load("21102001.txt", t) == StoreResult::OK);
  assert(!strcmp(t.data(), "untouched"));
  assert(t.set_text("é ä ç ñ ø œ ß ž"));
  t.insert("\n");
  assert(s.save(t) == StoreResult::OK);
  assert(!t.dirty());
  assert(read(root + "/gbawriter/21102001.txt") == "é ä ç ñ ø œ ß ž\n");
  for (int i = 0; i < 3; ++i) {
    t.insert("more");
    assert(s.save(t) == StoreResult::OK);
  }
  assert(s.load("21102001.txt", t) == StoreResult::OK);
  assert(!strcmp(t.data(), "é ä ç ñ ø œ ß ž\nmoremoremore"));
  for (auto &e : std::filesystem::directory_iterator(root + "/gbawriter"))
    assert(e.path().extension() == ".txt" || e.path().extension() == ".TXT");
  // Every filesystem operation can fail; successful recovery exposes a whole
  // old/new document.
  const std::string old = "known good é",
                    edited = std::string(1800, 'x') + "øœßž";
  s.fault_at(-1);
  t.set_text(edited.c_str());
  t.insert("!");
  assert(s.save(t) == StoreResult::OK);
  int operations = s.operations();
  for (int fail = 1; fail <= operations; ++fail) {
    std::filesystem::remove_all(root + "/gbawriter");
    std::filesystem::create_directory(root + "/gbawriter");
    std::ofstream(root + "/gbawriter/21102001.txt") << old;
    s.fault_at(-1);
    assert(s.load("21102001.txt", t) == StoreResult::OK);
    t.set_text(edited.c_str());
    t.insert("!");
    s.fault_at(fail);
    auto result = s.save(t);
    assert(result != StoreResult::OK);
    assert(t.dirty());
    assert(std::string(t.data()) == edited + "!");
    s.fault_at(-1);
    auto recovered=s.recover("21102001.txt");
    assert(recovered==StoreResult::OK || recovered==StoreResult::RECOVERY_NEEDED);
    auto actual = read(root + "/gbawriter/21102001.txt");
    assert(actual == old || actual == edited + "!");
    if(recovered==StoreResult::OK) {
      assert(s.save(t) == StoreResult::OK);
      assert(read(root + "/gbawriter/21102001.txt") == edited + "!");
    }
  }
  std::cout << "Verified " << operations << " save fault points\n";
  std::filesystem::rename(root + "/gbawriter/21102001.txt",
                          root + "/gbawriter/21102001.txt.gwb");
  Storage reboot(root.c_str());
  assert(reboot.init() == StoreResult::OK);
  assert(reboot.count() == 1);
  assert(!strcmp(reboot.name(0), "21102001.txt"));
  std::ofstream(root + "/gbawriter/22102001.TXT") << "DO NOT TOUCH";
  assert(reboot.create("22102001.txt", t) == StoreResult::EXISTS);
  assert(read(root + "/gbawriter/22102001.TXT") == "DO NOT TOUCH");
  for (int i = 0; i < 100; ++i) {
    char name[40];
    snprintf(name, sizeof(name), "/gbawriter/n%03d.txt", i);
    std::ofstream(root + name) << i;
  }
  assert(reboot.scan() == StoreResult::OK);
  assert(reboot.total() == 102);
  std::vector<std::string> names;
  do {
    for (int i = 0; i < reboot.count(); ++i)
      names.emplace_back(reboot.name(i));
  } while (reboot.next_page() == StoreResult::OK && reboot.count());
  assert(names.size() == 102);
  std::set<std::string> unique(names.begin(), names.end());
  assert(unique.size() == 102);
  assert(reboot.previous_page() == StoreResult::OK);
  assert(!strcmp(reboot.name(reboot.count() - 1), "n099.txt"));
  std::filesystem::remove_all(root);
  std::cout << "PASS: writer storage\n";
}
