/* gbawriter exact-ROM runner on gbamp3's modeled Supercard SD (scsd_model.h,
 * MIT, Halim Jarrar). TEST ONLY: drives the keypad from a script and saves
 * screenshots; the ROM runs its own SD driver and FatFS on the image.
 *
 * usage: writer_runner ROM SCRIPT OUTDIR      (env SD_IMAGE=card.img)
 * script lines:  wait N | hold KEYS N | tap KEYS | shot NAME
 * KEYS: A B SELECT START RIGHT LEFT UP DOWN R L joined by '+'. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/timing.h>
#include <mgba/internal/gba/gba.h>
#include <mgba-util/vfs.h>
#include "scsd_model.h"
static struct mCore *core;
static color_t *pixels;
static unsigned frame_no;
static unsigned keys_of(const char *s) {
  static const char *const names[10] = {"A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN", "R", "L"};
  unsigned k = 0;
  char buf[128];
  snprintf(buf, sizeof buf, "%s", s);
  for (char *t = strtok(buf, "+"); t; t = strtok(NULL, "+"))
    for (int i = 0; i < 10; i++)
      if (!strcmp(t, names[i]))
        k |= 1u << i;
  return k;
}
static void run(unsigned keys, unsigned n) {
  for (unsigned i = 0; i < n; i++) {
    core->setKeys(core, keys);
    core->runFrame(core);
    frame_no++;
  }
}
static void shot(const char *dir, const char *name) {
  char path[1024];
  snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
  FILE *z = fopen(path, "wb");
  fprintf(z, "P6\n240 160\n255\n");
  for (int i = 0; i < 240 * 160; i++) {
    unsigned char q[3] = {pixels[i] & 255, (pixels[i] >> 8) & 255, (pixels[i] >> 16) & 255};
    fwrite(q, 1, 3, z);
  }
  fclose(z);
}
int main(int argc, char **argv) {
  if (argc != 4)
    return 2;
  core = mCoreFind(argv[1]);
  if (!core || !core->init(core))
    return 3;
  mCoreInitConfig(core, NULL);
  mCoreConfigSetIntValue(&core->config, "idleOptimization", 0);
  mCoreConfigSetIntValue(&core->config, "logLevel", 0);
  mCoreLoadForeignConfig(core, &core->config);
  pixels = calloc(240 * 160, sizeof(*pixels));
  core->setVideoBuffer(core, pixels, 240);
  struct VFile *v = VFileOpen(argv[1], O_RDONLY);
  if (!v || !core->loadROM(core, v))
    return 4;
  core->reset(core);
  setenv("SD_ALLOW_INDEX_WRITES", "1", 0);
  model_install(core);
  FILE *script = fopen(argv[2], "r");
  if (!script)
    return 5;
  char line[256];
  while (fgets(line, sizeof line, script)) {
    char cmd[32] = {0}, arg[128] = {0};
    unsigned n = 0;
    int got = sscanf(line, "%31s %127s %u", cmd, arg, &n);
    if (got < 1 || cmd[0] == '#')
      continue;
    if (!strcmp(cmd, "wait"))
      run(0, (unsigned)atoi(arg));
    else if (!strcmp(cmd, "hold"))
      run(keys_of(arg), n);
    else if (!strcmp(cmd, "tap")) {
      run(keys_of(arg), 4);
      run(0, 6);
    } else if (!strcmp(cmd, "shot"))
      shot(argv[3], arg);
  }
  printf("frames=%u SD reads=%u writes=%u\n", frame_no, sd_reads, sd_writes);
  core->deinit(core);
  if (image_file)
    fclose(image_file);
  return 0;
}
