#include "writer_format.h"
#include "writer_storage.h"
extern "C" {
#include "font_render.h"
void *font_base_addr;
void *reader_font_base_addr;
}
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#ifndef WRITER_HEADER_CAPACITY
#error Compile with the actual main.cpp header buffer capacity
#endif

static void *load(const char *path) {
  FILE *file = std::fopen(path, "rb");
  assert(file);
  assert(std::fseek(file, 0, SEEK_END) == 0);
  long size = std::ftell(file);
  assert(size > 0);
  std::rewind(file);
  void *data = std::malloc(static_cast<std::size_t>(size));
  assert(data);
  assert(std::fread(data, 1, static_cast<std::size_t>(size), file) ==
         static_cast<std::size_t>(size));
  assert(std::fclose(file) == 0);
  return data;
}

static unsigned checked = 0;
static void check(const std::string &name, bool dirty) {
  assert(name.size() < writer::FILE_NAME_SIZE - 5);
  char buffer[WRITER_HEADER_CAPACITY];
  writer::format(buffer, sizeof(buffer), "%s%s", dirty ? "* " : "",
                 name.c_str());
  alignas(2) unsigned char pixels[240 * 16] = {};
  // Same header origin, clipping width and production renderer as main.cpp.
  // ASan instruments the formatter, this buffer, and the renderer itself.
  draw_text_idx8_bus16_range(buffer, pixels + 8, 0, 164, 240, 1);
  const std::string expected = (dirty ? "* " : "") + name;
  assert(std::strcmp(buffer, expected.c_str()) == 0);
  ++checked;
}

int main(int argc, char **argv) {
  assert(argc == 3);
  font_base_addr = load(argv[1]);
  reader_font_base_addr = load(argv[2]);
  std::string regression;
  for (int i = 0; i < 20; ++i)
    regression += "\xf0\x9f\x98\x80";
  check(regression + ".txt", false);
  check(regression + ".txt", true);
  for (const char *glyph : {"a", "é", "€", "\xf0\x9f\x98\x80"}) {
    std::string name;
    while (name.size() + std::strlen(glyph) + 4 < writer::FILE_NAME_SIZE - 5) {
      name += glyph;
      check(name + ".txt", false);
      check(name + ".txt", true);
    }
  }
  std::free(font_base_addr);
  std::free(reader_font_base_addr);
  std::cout << "PASS: " << checked
            << " complete clean/dirty filename headers through real renderer (ASan/UBSan)\n";
}
