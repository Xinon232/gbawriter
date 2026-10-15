/* MIT, Halim Jarrar 2026. TEST ONLY: register-level EZ-Flash Omega model for
 * the exact-ROM mGBA runners. Same globals and model_install() as
 * scsd_model.h, so a runner switches with -DEZFO_MODEL.
 *
 * Modeled from the EZ-Flash Omega / Omega DE kernel sources (Ezcard_OP.c,
 * Apache-2.0): unlock sequence D200/1500 at 9FE0000, 8000000, 8020000,
 * 8040000; ROM page register 9880000 (0x200 = PSRAM game, 0x8002 = kernel);
 * SD control 9400000 (0 off, 1 enable, 3 read state); command 9600000 (LBA
 * low), 9620000 (LBA high), 9640000 (count, +0x8000 = write); lock 9FC0000;
 * data/response window 9E00000 (busy = 0xEEE1). The SD card is initialised
 * by the cart, so the GBA sends no SD commands of its own.
 *
 * Stricter than hardware on purpose (exit codes 60..79):
 *  - while the kernel page is mapped the game ROM is gone: any instruction
 *    fetched from 08000000-0DFFFFFF, any data read outside the kernel header,
 *    the NOR ID window and the SD window, and any switch of the page while the
 *    CPU executes from ROM stop the run;
 *  - the SD window takes 16-bit accesses only, buffer reads need control 1,
 *    buffer writes control 3, polls control 3 (the kernel's own order).
 * Options (env): SD_IMAGE, SD_ALLOW_INDEX_WRITES (open r+b), EZFO_NOR_ID
 * (default 0x223D), EZFO_DELAY_POLLS (busy polls per command, default 64),
 * EZFO_FAIL_READ / EZFO_FAIL_WRITE (1-based command that never completes)
 * with EZFO_FAIL_COUNT (consecutive failing commands, default 1),
 * SD_READONLY (no write ever completes: the driver times out),
 * EZFO_IRQ_HOOK (simulate the kernel's in-game patch: literals 03007FFC in
 * the ROM become 03007FF4 and the BIOS IRQ vector points to a stub at the end
 * of the ROM that jumps through 03007FF4).
 * EZFO_TRACE=1 prints every SD command and, at exit, the CPU's PC.
 * Not hardware proof: timings are synthetic. */
#include <mgba/internal/arm/arm.h>
#include <stdio.h>
#include <string.h>
static struct ARMMemory original_memory;
static struct mCore *model_core;
static unsigned model_stats, io_irq_overlaps, io_audio_reads;
static uint64_t io_start, io_total, io_max;
static unsigned io_sample_start;
static FILE *image_file;
static uint64_t image_size;
static unsigned char *sdimage;
static unsigned sd_reads, sd_writes, read_cmds, multi_cmds, stop_cmds, high_reads, max_lba, sdhc;
static unsigned configured_delay, cmd8_seen; /* scsd_model.h names some runners print */
/* Omega state */
static unsigned ez_page = 0x200, ez_step, ez_unlocked, ez_ctl, ez_lba, ez_busy, ez_delay = 64;
static unsigned ez_nor_id = 0x223D, ez_nor_mode, ez_nor_step, ez_nor_left_in_id;
static unsigned ez_cmds_read, ez_cmds_write, ez_fail_read, ez_fail_write, ez_fail_count = 1, ez_stuck;
static unsigned ez_kernel_entries, ez_irq_in_kernel, ez_locked_rom_writes, ez_buffer_written;
static unsigned ez_irq_stub;
static uint16_t ez_buf[1024];
static const char ez_kernel_title[16] = "EZ-OMEGA KERNEL";
#define EZ_KERNEL (ez_page != 0x200)
static void ez_fail(int code, const char *what, uint32_t a) {
  fprintf(stderr, "EZFO MODEL: %s at %08X (pc %08X, page %04X)\n", what, a,
          ((struct ARMCore *)model_core->cpu)->gprs[15], ez_page);
  exit(code);
}
static int ez_rom(uint32_t a) { return a >= 0x08000000 && a < 0x0E000000; }
static int ez_window(uint32_t a) { return a >= 0x09E00000 && a < 0x09E00800; }
static void ez_image(uint64_t lba, unsigned n, int write) {
  uint64_t off = lba * 512;
  if (!image_file || off > image_size || image_size - off < (uint64_t)n * 512) ez_fail(70, "LBA beyond image", (uint32_t)lba);
  if (fseeko(image_file, (off_t)off, SEEK_SET)) exit(71);
  if (write) {
    if (fwrite(ez_buf, 512, n, image_file) != n || fflush(image_file)) exit(72);
  } else if (fread(ez_buf, 512, n, image_file) != n) exit(73);
  if (lba + n - 1 > max_lba) max_lba = (unsigned)(lba + n - 1);
  if (lba >= 0x400000) high_reads++;
}
static unsigned ez_trace;
static void ez_command(unsigned cnt) {
  unsigned n = cnt & 0x7fff, write = (cnt & 0x8000) != 0;
  if (ez_trace) fprintf(stderr, "EZFO %s lba=%u n=%u t=%llu pc=%08X\n", write ? "write" : "read", ez_lba, n,
                        (unsigned long long)mTimingGlobalTime(model_core->timing), ((struct ARMCore *)model_core->cpu)->gprs[15]);
  if (!n || n > 4) ez_fail(64, "SD command count not 1..4", cnt);
  ez_busy = ez_delay;
  if (write) {
    ez_cmds_write++;
    if (!ez_buffer_written) ez_fail(65, "write command without buffer data", cnt);
    ez_buffer_written = 0;
    if (getenv("SD_READONLY") || (ez_fail_write && ez_cmds_write >= ez_fail_write && ez_cmds_write < ez_fail_write + ez_fail_count)) { ez_stuck = 1; return; }
    if (!getenv("SD_ALLOW_INDEX_WRITES")) ez_fail(66, "write to read-only image", ez_lba);
    ez_image(ez_lba, n, 1);
    sd_writes += n;
  } else {
    ez_cmds_read++; read_cmds++;
    if (n > 1) multi_cmds++;
    if (ez_fail_read && ez_cmds_read >= ez_fail_read && ez_cmds_read < ez_fail_read + ez_fail_count) { ez_stuck = 1; memset(ez_buf, 0xEE, sizeof ez_buf); return; }
    ez_image(ez_lba, n, 0);
    sd_reads += n;
  }
  ez_stuck = 0;
  if (model_core->busRead16(model_core, 0x04000208)) io_audio_reads++;
}
static void ez_set_page(unsigned v) {
  struct ARMCore *cpu = model_core->cpu;
  if (v != 0x200 && v != 0x8002) ez_fail(62, "unsupported ROM page", v);
  if (ez_rom(cpu->gprs[15])) ez_fail(61, "ROM page switched while executing from ROM", v);
  if (v == ez_page) return;
  if (v == 0x8002) {
    ez_kernel_entries++;
    io_start = mTimingGlobalTime(model_core->timing);
    io_sample_start = model_stats ? model_core->busRead32(model_core, model_stats + 32) : 0;
  } else {
    uint64_t e = mTimingGlobalTime(model_core->timing) - io_start;
    io_total += e;
    if (e > io_max) io_max = e;
    if (model_stats && model_core->busRead32(model_core, model_stats + 32) > io_sample_start) io_irq_overlaps++;
    if (ez_nor_mode) ez_nor_left_in_id++;
    ez_nor_mode = 0;
  }
  ez_page = v;
}
/* Returns 1 when the store was consumed by the cart. */
static int ez_store16(uint32_t a, uint16_t v) {
  static const uint32_t seq_a[4] = {0x09FE0000, 0x08000000, 0x08020000, 0x08040000};
  static const uint16_t seq_v[4] = {0xD200, 0x1500, 0xD200, 0x1500};
  if (!ez_rom(a)) return 0;
  if (a == seq_a[ez_step] && v == seq_v[ez_step]) {
    if (++ez_step == 4) { ez_step = 0; ez_unlocked = 1; }
    return 1;
  }
  ez_step = (a == seq_a[0] && v == seq_v[0]) ? 1 : 0;
  if (ez_unlocked) {
    switch (a) {
    case 0x09FC0000: if (v == 0x1500) ez_unlocked = 0; return 1;
    case 0x09880000: ez_set_page(v); return 1;
    case 0x09400000: if (v > 3) ez_fail(63, "bad SD control", v); ez_ctl = v; return 1;
    case 0x09600000: ez_lba = (ez_lba & 0xFFFF0000u) | v; return 1;
    case 0x09620000: ez_lba = (ez_lba & 0xFFFFu) | ((unsigned)v << 16); return 1;
    case 0x09640000: if (!EZ_KERNEL) ez_fail(67, "SD command outside kernel mode", v); ez_command(v); return 1;
    case 0x09860000: case 0x09C00000: case 0x09420000: case 0x096E0000: case 0x09700000: return 1;
    }
    ez_fail(68, "unknown register write while unlocked", a);
  }
  if (EZ_KERNEL) {
    if (ez_window(a)) {
      if (ez_ctl != 3) ez_fail(69, "SD buffer write without read state", a);
      ez_buf[(a - 0x09E00000) >> 1] = v; ez_buffer_written = 1; return 1;
    }
    if (a >= 0x09000000 && a < 0x09001000) { /* S98 NOR command interface */
      unsigned o = a - 0x09000000;
      if (v == 0xF0) { ez_nor_mode = 0; ez_nor_step = 0; }
      else if (o == 0x555 * 2 && v == 0xAA && ez_nor_step == 0) ez_nor_step = 1;
      else if (o == 0x2AA * 2 && v == 0x55 && ez_nor_step == 1) ez_nor_step = 2;
      else if (o == 0x555 * 2 && v == 0x90 && ez_nor_step == 2) { ez_nor_step = 0; ez_nor_mode = 1; }
      else ez_fail(74, "unexpected NOR command", a);
      return 1;
    }
    ez_fail(75, "ROM write in kernel mode", a);
  }
  ez_locked_rom_writes++; /* PSRAM mode: the game ROM ignores writes */
  return 1;
}
static int ez_load16(uint32_t a, uint32_t *out) {
  if (!ez_rom(a) || !EZ_KERNEL) return 0;
  if (ez_window(a)) {
    if (ez_ctl == 3 && a == 0x09E00000) {
      if (ez_stuck || ez_busy) { if (ez_busy) ez_busy--; *out = 0xEEE1; return 1; }
      *out = 0; return 1;
    }
    if (ez_ctl != 1) ez_fail(76, "SD buffer read without enable", a);
    if (ez_busy && !ez_stuck) ez_fail(77, "SD buffer read before the response", a);
    *out = ez_buf[(a - 0x09E00000) >> 1]; return 1;
  }
  if (a >= 0x080000A0 && a < 0x080000C0) { unsigned o = a - 0x080000A0; *out = (uint8_t)ez_kernel_title[o & 15] | ((uint8_t)ez_kernel_title[(o + 1) & 15] << 8); return 1; }
  if (a >= 0x09000000 && a < 0x09001000) { *out = ez_nor_mode && a == 0x09000000 + 0xE * 2 ? ez_nor_id : 0xFFFF; return 1; }
  ez_fail(78, "game ROM data read in kernel mode", a);
  return 1;
}
static void ez_store16_hook(struct ARMCore *c, uint32_t a, int16_t v, int *cy) {
  if (ez_store16(a, (uint16_t)v)) { if (cy) *cy += 5; return; }
  original_memory.store16(c, a, v, cy);
}
static void ez_store32_hook(struct ARMCore *c, uint32_t a, int32_t v, int *cy) {
  if (ez_rom(a)) {
    if (EZ_KERNEL || ez_unlocked) ez_fail(79, "32-bit cart access", a);
    ez_locked_rom_writes++; return;
  }
  original_memory.store32(c, a, v, cy);
  if (ez_irq_stub && ((a & ~0x00FF8000u) == 0x03007FF4)) original_memory.store32(c, 0x03007FFC, (int32_t)ez_irq_stub, 0);
}
static void ez_store8_hook(struct ARMCore *c, uint32_t a, int8_t v, int *cy) {
  if (ez_rom(a)) { if (EZ_KERNEL || ez_unlocked) ez_fail(79, "8-bit cart access", a); ez_locked_rom_writes++; return; }
  original_memory.store8(c, a, v, cy);
}
static uint32_t ez_load16_hook(struct ARMCore *c, uint32_t a, int *cy) {
  uint32_t v;
  if (ez_load16(a, &v)) { if (cy) *cy += 5; return v; }
  return original_memory.load16(c, a, cy);
}
static uint32_t ez_load32_hook(struct ARMCore *c, uint32_t a, int *cy) {
  if (EZ_KERNEL && ez_rom(a)) { if (ez_window(a)) ez_fail(79, "32-bit cart access", a); uint32_t lo, hi; ez_load16(a, &lo); ez_load16(a + 2, &hi); return lo | hi << 16; }
  return original_memory.load32(c, a, cy);
}
static uint32_t ez_load8_hook(struct ARMCore *c, uint32_t a, int *cy) {
  if (EZ_KERNEL && ez_rom(a)) { if (ez_window(a)) ez_fail(79, "8-bit cart access", a); uint32_t v; ez_load16(a & ~1u, &v); return (a & 1) ? v >> 8 : v & 255; }
  return original_memory.load8(c, a, cy);
}
static uint32_t ez_loadm_hook(struct ARMCore *c, uint32_t a, int mask, enum LSMDirection d, int *cy) {
  if (EZ_KERNEL && ez_rom(a)) ez_fail(78, "load-multiple from cart in kernel mode", a);
  return original_memory.loadMultiple(c, a, mask, d, cy);
}
static uint32_t ez_storem_hook(struct ARMCore *c, uint32_t a, int mask, enum LSMDirection d, int *cy) {
  if (ez_rom(a) && (EZ_KERNEL || ez_unlocked)) ez_fail(79, "store-multiple to cart", a);
  return original_memory.storeMultiple(c, a, mask, d, cy);
}
static void ez_active_hook(struct ARMCore *c, uint32_t a) {
  if (EZ_KERNEL) {
    if (ez_rom(a)) ez_fail(60, "instruction fetch from game ROM in kernel mode", a);
    if (a == 0x18) ez_irq_in_kernel++;
  }
  original_memory.setActiveRegion(c, a);
}
static void ez_patch_irq_hook(struct mCore *c) {
  /* What the kernel does to a ROM loaded with the in-game menu patches on. */
  struct GBA *gba = c->board;
  uint32_t size = (uint32_t)gba->pristineRomSize, patched = 0;
  for (uint32_t o = 0; o + 4 <= size; o += 4) {
    uint32_t w = gba->memory.rom[o / 4];
    if (w == 0x03007FFC || w == 0x03FFFFFC) { c->rawWrite32(c, 0x08000000 + o, -1, 0x03007FF4); patched++; }
  }
  uint32_t stub = 0x08000000 + ((size + 15) & ~15u);
  c->rawWrite32(c, stub + 0, -1, (int32_t)0xE59F1004);  /* ldr r1,[pc,#4] */
  c->rawWrite32(c, stub + 4, -1, (int32_t)0xE5911000);  /* ldr r1,[r1]    */
  c->rawWrite32(c, stub + 8, -1, (int32_t)0xE12FFF11);  /* bx r1          */
  c->rawWrite32(c, stub + 12, -1, 0x03007FF4);
  ez_irq_stub = stub;
  /* The patch copied the ROM: refresh the CPU's cached fetch region. */
  struct ARMCore *cpu = c->cpu;
  cpu->memory.setActiveRegion(cpu, cpu->gprs[15]);
  fprintf(stderr, "EZFO MODEL: IRQ hook patch, %u literals, stub %08X\n", patched, stub);
}
static void ez_atexit(void) {
  struct ARMCore *cpu = model_core->cpu;
  fprintf(stderr, "EZFO exit: pc=%08X lr=%08X sp=%08X cpsr=%08X page=%04X\n", cpu->gprs[15], cpu->gprs[14], cpu->gprs[13], cpu->cpsr.packed, ez_page);
}
static void model_install(struct mCore *c) {
  model_core = c;
  const char *p = getenv("SD_IMAGE");
  if (p && *p) {
    image_file = fopen(p, getenv("SD_ALLOW_INDEX_WRITES") ? "r+b" : "rb");
    if (!image_file) exit(37);
    fseeko(image_file, 0, SEEK_END); image_size = ftello(image_file); rewind(image_file);
    sdimage = malloc(1);
  }
  if ((p = getenv("EZFO_NOR_ID"))) ez_nor_id = strtoul(p, 0, 0);
  if ((p = getenv("EZFO_DELAY_POLLS"))) ez_delay = strtoul(p, 0, 0);
  configured_delay = ez_delay;
  if ((p = getenv("EZFO_FAIL_READ"))) ez_fail_read = strtoul(p, 0, 0);
  if ((p = getenv("EZFO_FAIL_WRITE"))) ez_fail_write = strtoul(p, 0, 0);
  if ((p = getenv("EZFO_FAIL_COUNT"))) ez_fail_count = strtoul(p, 0, 0);
  if (getenv("EZFO_IRQ_HOOK")) ez_patch_irq_hook(c);
  if (getenv("EZFO_TRACE")) { ez_trace = 1; atexit(ez_atexit); }
  struct ARMCore *cpu = c->cpu;
  original_memory = cpu->memory;
  cpu->memory.store16 = ez_store16_hook; cpu->memory.store32 = ez_store32_hook; cpu->memory.store8 = ez_store8_hook;
  cpu->memory.load16 = ez_load16_hook; cpu->memory.load32 = ez_load32_hook; cpu->memory.load8 = ez_load8_hook;
  cpu->memory.loadMultiple = ez_loadm_hook; cpu->memory.storeMultiple = ez_storem_hook;
  cpu->memory.setActiveRegion = ez_active_hook;
}
/* One line a test can parse; call at the end of a run. */
__attribute__((unused)) static void ez_report(FILE *f) {
  fprintf(f, "EZFO page=%04X kernel_entries=%u read_cmds=%u write_cmds=%u sectors_read=%u sectors_written=%u "
             "irq_in_kernel=%u irq_overlaps=%u max_window_cycles=%llu total_window_cycles=%llu nor_left_in_id=%u "
             "locked_rom_writes=%u\n",
          ez_page, ez_kernel_entries, ez_cmds_read, ez_cmds_write, sd_reads, sd_writes, ez_irq_in_kernel,
          io_irq_overlaps, (unsigned long long)io_max, (unsigned long long)io_total, ez_nor_left_in_id,
          ez_locked_rom_writes);
}
