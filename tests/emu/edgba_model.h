/* MIT, Halim Jarrar 2026. TEST ONLY: register-level EverDrive GBA (Mini)
 * model for the exact-ROM mGBA runners. Same globals and model_install() as
 * scsd_model.h, so a runner switches with -DEDGBA_MODEL.
 *
 * Built from krikzz's edio-gba sample (github.com/krikzz/gba-ed-pub, MIT):
 * registers at 0x09FC0000 + reg*2 (CFG 0, STATUS 1, FPGA_VER 5, SD_CMD 8,
 * SD_DAT 9, SD_CFG 0x0A, SD_RAM 0x0B, KEY 0x5A); writes need the key 0xA5,
 * reads need CFG bit 0. The FPGA behaviour below is this model's reading of
 * krikzz's code, not a hardware specification:
 *  - SD_CFG mode bits (1-2) give the SD clocks per access: 1, 2, 4 or 8;
 *  - SD_CMD read returns the 8-bit command latch, then clocks the CMD line
 *    in (one bit per clock); reg 0x0A reads the latch without clocking;
 *    SD_CMD write clocks the low byte out, MSB first;
 *  - SD_DAT read returns the data latch in bits 8-15, then clocks (MODE1:
 *    DAT0 only, otherwise a nibble per clock); SD_DAT write clocks the low
 *    byte out (MODE2) or the low then the high byte (MODE4), high nibble
 *    first; a read with SD_WAIT_F0 waits for the next data block (STATUS
 *    bit 1 when there is none), whose 256 halfwords are then read at
 *    0x09FC0012 + 2*i (as krikzz's DMA does), CRC handled by the FPGA;
 *  - SD_RAM: write 0 rewinds it; reads return the last sector written over
 *    SD_DAT in lane order (128 bytes per DAT line, MSB first);
 *  - STATUS busy is never set (accesses complete at once).
 * Stricter than hardware on purpose (exit codes 80..99): a CFG write that
 * unmaps PSRAM (bit 1 clear) or turns PSRAM writes on (bit 2), a write CRC
 * that does not match the data, an unknown command or a bad command CRC.
 * Options (env): SD_IMAGE (empty: no card, the card never answers),
 * SD_ALLOW_INDEX_WRITES (otherwise writes stay in memory), SDHC, SDV1,
 * SD_READONLY (every block write answers "write error"),
 * EDGBA_FAIL_READ (1-based data block that is never found) with
 * EDGBA_FAIL_COUNT, EDGBA_TRACE=1 (every command).
 * Not hardware proof: timings are synthetic. */
#include <mgba/internal/arm/arm.h>
#include <stdio.h>
#include <stdlib.h>
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
__attribute__((unused)) static unsigned configured_delay, cmd8_seen;

#define ED_BASE 0x09FC0000u
static unsigned ed_key, ed_regs_on, ed_cfg = 2, ed_sd_cfg, ed_tout, ed_trace;
static unsigned ed_cfg_writes, ed_locked_writes, ed_rom_writes, ed_eep_writes, ed_key_writes;
/* CMD line */
static uint8_t ed_cmd_latch = 0xFF;
static uint64_t ed_cmd_in; static unsigned ed_cmd_bits, ed_cmd_active;
static uint8_t ed_cmd_out[24]; static unsigned ed_cmd_out_bits, ed_cmd_out_pos;
/* card */
static unsigned ed_card_state, ed_app_cmd, ed_acmd41_calls, ed_rca = 0x1234, ed_v1, ed_present, ed_bus4;
static uint64_t ed_read_lba; static unsigned ed_reading, ed_stream_left, ed_stream_pos;
static uint16_t ed_block[256];
static unsigned ed_blocks_read, ed_fail_read, ed_fail_count = 1;
/* DAT lines */
static uint8_t ed_dat_latch = 0xFF;
static uint8_t ed_dat_out[64]; static unsigned ed_dat_out_n, ed_dat_out_pos;
static unsigned ed_writing; static uint64_t ed_write_lba;
static unsigned ed_wr_state, ed_wr_n; static uint8_t ed_wr_nib[1024 + 16 + 4];
static uint8_t ed_ram[512]; static unsigned ed_ram_fill, ed_ram_pos;
static unsigned ed_crc_errors_sent;

static void ed_fail(int code, const char *what, uint32_t a) {
  fprintf(stderr, "EDGBA MODEL: %s (%08X, pc %08X)\n", what, a, ((struct ARMCore *)model_core->cpu)->gprs[15]);
  exit(code);
}
static unsigned ed_clocks(void) { static const unsigned c[4] = {1, 2, 4, 8}; return c[(ed_sd_cfg >> 1) & 3]; }

/* ---- card storage (writes stay in memory without SD_ALLOW_INDEX_WRITES) ---- */
static struct ed_sector { uint32_t lba; uint8_t data[512]; } *ed_cow;
static unsigned ed_cow_n, ed_cow_cap;
static uint8_t *ed_cow_find(uint32_t lba, int add) {
  for (unsigned i = 0; i < ed_cow_n; i++) if (ed_cow[i].lba == lba) return ed_cow[i].data;
  if (!add) return 0;
  if (ed_cow_n == ed_cow_cap) { ed_cow_cap = ed_cow_cap ? ed_cow_cap * 2 : 64; ed_cow = realloc(ed_cow, ed_cow_cap * sizeof *ed_cow); if (!ed_cow) exit(98); }
  ed_cow[ed_cow_n].lba = lba;
  return ed_cow[ed_cow_n++].data;
}
static void ed_sector_io(uint64_t lba, uint8_t *buf, int write) {
  uint64_t off = lba * 512;
  if (!image_file || off > image_size || image_size - off < 512) ed_fail(80, "LBA beyond image", (uint32_t)lba);
  if (write && !getenv("SD_ALLOW_INDEX_WRITES")) { memcpy(ed_cow_find((uint32_t)lba, 1), buf, 512); return; }
  if (fseeko(image_file, (off_t)off, SEEK_SET)) exit(81);
  if (write) { if (fwrite(buf, 1, 512, image_file) != 512 || fflush(image_file)) exit(82); }
  else {
    if (fread(buf, 1, 512, image_file) != 512) exit(83);
    uint8_t *c = ed_cow_find((uint32_t)lba, 0);
    if (c) memcpy(buf, c, 512);
  }
  if (lba > max_lba) max_lba = (unsigned)lba;
  if (lba >= 0x400000) high_reads++;
}

/* ---- CMD line ---- */
static unsigned ed_crc7(const uint8_t *p, unsigned n) {
  unsigned crc = 0;
  for (unsigned j = 0; j < n; j++)
    for (int bit = 7; bit >= 0; bit--) { unsigned fb = ((crc >> 6) ^ ((p[j] >> bit) & 1)) & 1; crc = ((crc << 1) & 127) ^ (fb ? 9 : 0); }
  return (crc << 1) | 1;
}
static void ed_respond(const uint8_t *bytes, unsigned n) {
  memset(ed_cmd_out, 0xFF, sizeof ed_cmd_out);
  /* Ncr: two idle bits before the response. */
  ed_cmd_out_bits = 2 + n * 8; ed_cmd_out_pos = 0;
  for (unsigned i = 0; i < n * 8; i++) {
    unsigned b = 2 + i, v = (bytes[i / 8] >> (7 - i % 8)) & 1;
    if (!v) ed_cmd_out[b / 8] &= (uint8_t)~(0x80 >> (b % 8));
  }
}
static void ed_r1(unsigned cmd) {
  uint8_t r[6] = {(uint8_t)cmd, 0, 0, 0x09, (uint8_t)(0x00 | (ed_app_cmd ? 0x20 : 0)), 0};
  /* status: READY_FOR_DATA (bit 8), state tran (4) in bits 12..9 */
  r[3] = (uint8_t)((ed_card_state == 4 ? 0x08 : 0x06) | 0x01);
  r[5] = (uint8_t)ed_crc7(r, 5);
  ed_respond(r, 6);
}
static void ed_r2(const uint8_t *body15) {
  uint8_t r[17]; r[0] = 0x3F; memcpy(r + 1, body15, 15); r[16] = (uint8_t)ed_crc7(body15, 15);
  ed_respond(r, 17);
}
static void ed_dat_busy(unsigned zeros) {
  /* DAT0 low for a while, then high (nibbles 0 / F). */
  ed_dat_out_n = 0; ed_dat_out_pos = 0;
  for (unsigned i = 0; i < zeros && ed_dat_out_n < sizeof ed_dat_out; i++) ed_dat_out[ed_dat_out_n++] = 0x0;
}
static void ed_command(void) {
  uint8_t p[6]; uint64_t v = ed_cmd_in;
  for (int i = 5; i >= 0; i--) { p[i] = (uint8_t)v; v >>= 8; }
  unsigned cmd = p[0] & 63, arg = ((unsigned)p[1] << 24) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 8) | p[4];
  if (ed_crc7(p, 5) != p[5]) ed_fail(84, "bad command CRC", cmd);
  if (ed_trace) fprintf(stderr, "EDGBA %sCMD%u %08X\n", ed_app_cmd ? "A" : "", cmd, arg);
  ed_cmd_out_bits = 0;
  if (!ed_present) { ed_app_cmd = 0; return; }   /* no card: never answers */
  unsigned app = ed_app_cmd; ed_app_cmd = 0;
  if (app && cmd == 41) {
    ed_acmd41_calls++;
    unsigned ready = ed_acmd41_calls >= 3;
    uint8_t r[6] = {0x3F, (uint8_t)((ready ? 0x80 : 0) | (ready && sdhc && !ed_v1 ? 0x40 : 0) | 0x00), 0xFF, 0x80, 0x00, 0xFF};
    if (ready && ed_card_state < 1) ed_card_state = 1;
    ed_respond(r, 6); return;
  }
  if (app && cmd == 6) { if (ed_card_state != 4) ed_fail(85, "ACMD6 outside transfer state", arg); ed_bus4 = (arg & 3) == 2; ed_r1(6); return; }
  switch (cmd) {
  case 0: ed_card_state = 0; ed_acmd41_calls = 0; ed_reading = ed_writing = 0; return;
  case 8: if (ed_v1) return; cmd8_seen++; { uint8_t r[6] = {8, 0, 0, (uint8_t)(arg >> 8), (uint8_t)arg, 0}; r[5] = (uint8_t)ed_crc7(r, 5); ed_respond(r, 6); } return;
  case 55: ed_app_cmd = 1; ed_r1(55); return;
  case 2: { static const uint8_t cid[15] = {0x03, 'S', 'D', 'M', 'O', 'D', 'E', 'L', 0x10, 1, 2, 3, 4, 0x01, 0x6A}; ed_card_state = 2; ed_r2(cid); } return;
  case 3: { uint8_t r[6] = {3, (uint8_t)(ed_rca >> 8), (uint8_t)ed_rca, 0x05, 0x00, 0}; r[5] = (uint8_t)ed_crc7(r, 5); ed_card_state = 3; ed_respond(r, 6); } return;
  case 7: if ((arg >> 16) != ed_rca) { if (ed_card_state == 4) ed_card_state = 3; return; } ed_card_state = 4; ed_r1(7); return;
  case 9: { uint8_t csd[15] = {0x40, 0x0E, 0x00, 0x32, 0x5B, 0x59, 0x00, 0x00, 0x1D, 0x8A, 0x7F, 0x80, 0x0A, 0x40, 0x00}; ed_r2(csd); } return;
  case 18: case 25:
    if (ed_card_state != 4) ed_fail(86, "read/write command outside transfer state", cmd);
    if (!ed_bus4) ed_fail(87, "data command before the 4-bit bus (ACMD6)", cmd);
    if (!sdhc || ed_v1) { if (arg % 512) ed_fail(88, "SDSC address not a multiple of 512", arg); arg /= 512; }
    ed_r1(cmd);
    if (cmd == 18) { ed_reading = 1; ed_read_lba = arg; ed_stream_left = 0; read_cmds++; multi_cmds++; }
    else { ed_writing = 1; ed_write_lba = arg; ed_wr_state = 0; ed_wr_n = 0; }
    return;
  case 12: ed_reading = 0; ed_stream_left = 0; ed_writing = 0; stop_cmds++; ed_r1(12); ed_dat_busy(3); return;
  default: ed_fail(89, "unmodeled command", cmd);
  }
}
static void ed_cmd_clock_out(unsigned bit) {
  /* Host drives CMD: a new command starts with a 0 bit. */
  if (!ed_cmd_active) { if (bit) return; ed_cmd_active = 1; ed_cmd_bits = 0; ed_cmd_in = 0; ed_cmd_out_bits = 0; }
  ed_cmd_in = (ed_cmd_in << 1) | bit;
  if (++ed_cmd_bits == 48) { ed_cmd_active = 0; ed_command(); }
}
static unsigned ed_cmd_clock_in(void) {
  if (ed_cmd_out_pos < ed_cmd_out_bits) { unsigned b = ed_cmd_out_pos++; return (ed_cmd_out[b / 8] >> (7 - b % 8)) & 1; }
  return 1;
}

/* ---- DAT lines ---- */
static unsigned ed_crc16_bits(const uint8_t *bits, unsigned n) {
  unsigned crc = 0;
  for (unsigned i = 0; i < n; i++) { unsigned fb = ((crc >> 15) ^ bits[i]) & 1; crc = ((crc << 1) & 0xFFFF) ^ (fb ? 0x1021 : 0); }
  return crc;
}
static void ed_write_block_done(void) {
  /* ed_wr_nib: 1024 data nibbles + 16 CRC nibbles. Check the CRC per line. */
  uint8_t data[512], bits[1024];
  for (unsigned i = 0; i < 512; i++) data[i] = (uint8_t)((ed_wr_nib[2 * i] << 4) | ed_wr_nib[2 * i + 1]);
  for (unsigned lane = 0; lane < 4; lane++) {
    for (unsigned j = 0; j < 1024; j++) bits[j] = (ed_wr_nib[j] >> lane) & 1;
    unsigned want = ed_crc16_bits(bits, 1024), got = 0;
    for (unsigned j = 0; j < 16; j++) got = (got << 1) | ((ed_wr_nib[1024 + j] >> lane) & 1);
    if (want != got) ed_fail(90, "write CRC does not match the data", (lane << 16) | got);
  }
  unsigned status = 2;   /* 010: accepted */
  if (getenv("SD_READONLY")) status = 6;   /* 110: write error */
  else { ed_sector_io(ed_write_lba, data, 1); ed_write_lba++; sd_writes++; }
  if (status != 2) ed_crc_errors_sent++;
  /* DAT0: two clocks high, start 0, status (3 bits), end 1, busy, then high. */
  ed_dat_out_n = 0; ed_dat_out_pos = 0;
  ed_dat_out[ed_dat_out_n++] = 0xF; ed_dat_out[ed_dat_out_n++] = 0xF;
  ed_dat_out[ed_dat_out_n++] = 0xE;
  for (int b = 2; b >= 0; b--) ed_dat_out[ed_dat_out_n++] = (uint8_t)(0xE | ((status >> b) & 1));
  ed_dat_out[ed_dat_out_n++] = 0xF;
  for (unsigned i = 0; i < 6; i++) ed_dat_out[ed_dat_out_n++] = 0xE;
}
static void ed_dat_clock_out(unsigned nib) {
  if (!ed_writing) return;
  if (ed_wr_state == 0) { if (nib == 0) { ed_wr_state = 1; ed_wr_n = 0; } return; }   /* start nibble */
  ed_wr_nib[ed_wr_n++] = (uint8_t)nib;
  if (ed_wr_n == 1024 + 16) { ed_wr_state = 2; return; }
  if (ed_wr_state == 2) { /* the end bit */ }
}
static void ed_dat_end_check(unsigned nib) {
  if (ed_writing && ed_wr_state == 2) {
    if ((nib & 1) != 1) ed_fail(91, "missing end bit after the write CRC", nib);
    ed_wr_state = 0; ed_write_block_done();
  }
}
static unsigned ed_dat_clock_in(void) {
  if (ed_dat_out_pos < ed_dat_out_n) return ed_dat_out[ed_dat_out_pos++];
  return 0xF;
}
static void ed_dat_write(uint16_t v) {
  unsigned clocks = ed_clocks();
  uint8_t nib[4]; unsigned n = 0;
  if (clocks == 1) nib[n++] = (v & 1) ? 0xF : 0x0;
  else if (clocks == 2) { nib[n++] = (v >> 4) & 15; nib[n++] = v & 15; }
  else {
    nib[n++] = (v >> 4) & 15; nib[n++] = v & 15; nib[n++] = (v >> 12) & 15; nib[n++] = (v >> 8) & 15;
    if (ed_ram_fill < 512) { ed_ram[ed_ram_fill++] = (uint8_t)v; ed_ram[ed_ram_fill++] = (uint8_t)(v >> 8); }
  }
  for (unsigned i = 0; i < n; i++) {
    if (ed_writing && ed_wr_state == 2) ed_dat_end_check(nib[i]);
    else ed_dat_clock_out(nib[i]);
  }
}
static uint16_t ed_ram_read(void) {
  /* Lane order: 128 bytes per DAT line (line 0 first), bits MSB first. */
  if (ed_ram_pos >= 512) ed_fail(92, "SD_RAM read past 512 bytes", ed_ram_pos);
  uint8_t out[2];
  for (unsigned k = 0; k < 2; k++) {
    unsigned idx = ed_ram_pos + k, lane = idx / 128, byte = idx % 128, v = 0;
    for (unsigned b = 0; b < 8; b++) {
      unsigned j = byte * 8 + b;   /* nibble j of the sector */
      unsigned nibv = (j & 1) ? (ed_ram[j / 2] & 15) : (ed_ram[j / 2] >> 4);
      v = (v << 1) | ((nibv >> lane) & 1);
    }
    out[k] = (uint8_t)v;
  }
  ed_ram_pos += 2;
  return (uint16_t)(out[0] | (out[1] << 8));
}
static int ed_wait_block(void) {
  if (!ed_reading) return 0;
  ed_blocks_read++;
  if (ed_fail_read && ed_blocks_read >= ed_fail_read && ed_blocks_read < ed_fail_read + ed_fail_count) return 0;
  uint8_t data[512];
  ed_sector_io(ed_read_lba, data, 0);
  for (unsigned i = 0; i < 256; i++) ed_block[i] = (uint16_t)(data[2 * i] | (data[2 * i + 1] << 8));
  ed_stream_left = 256; ed_stream_pos = 0;
  sd_reads++;
  io_start = mTimingGlobalTime(model_core->timing);
  io_sample_start = model_stats ? model_core->busRead32(model_core, model_stats + 32) : 0;
  if (model_core->busRead16(model_core, 0x04000208)) io_audio_reads++;
  return 1;
}

/* ---- bus ---- */
static int ed_window(uint32_t a) { return a >= ED_BASE && a < ED_BASE + 0x200; }
static int ed_load16(uint32_t a, uint32_t *out) {
  if (!ed_regs_on || a < ED_BASE || a >= ED_BASE + 0x212) return 0;
  if (ed_stream_left && a >= ED_BASE + 0x12 && a < ED_BASE + 0x212) {
    if (a != ED_BASE + 0x12 + 2 * ed_stream_pos) ed_fail(93, "data read out of order", a);
    *out = ed_block[ed_stream_pos++];
    if (--ed_stream_left == 0) {
      ed_read_lba++;
      uint64_t e = mTimingGlobalTime(model_core->timing) - io_start; io_total += e; if (e > io_max) io_max = e;
      if (model_stats && model_core->busRead32(model_core, model_stats + 32) > io_sample_start) io_irq_overlaps++;
    }
    return 1;
  }
  if (a >= ED_BASE + 0x200) return 0;
  unsigned reg = (a - ED_BASE) / 2;
  switch (reg) {
  case 0x00: *out = ed_cfg; return 1;
  case 0x01: *out = ed_tout ? 2 : 0; return 1;
  case 0x05: *out = 0x0105; return 1;   /* FPGA version (model) */
  case 0x08: { *out = ed_cmd_latch; unsigned n = ed_clocks(); for (unsigned i = 0; i < n; i++) ed_cmd_latch = (uint8_t)((ed_cmd_latch << 1) | ed_cmd_clock_in()); return 1; }
  case 0x09: {
    if (ed_sd_cfg & 8) { ed_tout = !ed_wait_block(); *out = 0xFFFF; return 1; }   /* SD_WAIT_F0 */
    *out = (uint32_t)ed_dat_latch << 8 | 0xFF;
    unsigned n = ed_clocks();
    for (unsigned i = 0; i < n; i++) {
      unsigned nib = ed_dat_clock_in();
      ed_dat_latch = n == 1 ? (uint8_t)((ed_dat_latch << 1) | (nib & 1)) : (uint8_t)((ed_dat_latch << 4) | nib);
    }
    return 1;
  }
  case 0x0A: *out = ed_cmd_latch; return 1;
  case 0x0B: *out = ed_ram_read(); return 1;
  }
  *out = 0xFFFF; return 1;
}
static int ed_store16(uint32_t a, uint16_t v) {
  if (a < 0x08000000 || a >= 0x0E000000) return 0;
  if (!ed_window(a)) {
    if (a >= 0x09FE0000 && a < 0x09FE0400) ed_eep_writes++;
    ed_rom_writes++; return 1;   /* PSRAM writes off */
  }
  unsigned reg = (a - ED_BASE) / 2;
  if (reg == 0x5A) { ed_key_writes++; ed_key = v == 0xA5; return 1; }
  if (!ed_key) { ed_locked_writes++; return 1; }
  switch (reg) {
  case 0x00:
    ed_cfg_writes++;
    if (!(v & 2)) ed_fail(94, "CFG write unmaps PSRAM (the game would crash)", v);
    if (v & 4) ed_fail(95, "CFG write turns PSRAM writes on", v);
    ed_cfg = v; ed_regs_on = v & 1; return 1;
  case 0x08: { unsigned n = ed_clocks(); for (unsigned i = 0; i < n; i++) ed_cmd_clock_out((v >> (7 - i % 8)) & 1); return 1; }
  case 0x09: ed_dat_write(v); return 1;
  case 0x0A: ed_sd_cfg = v; return 1;
  case 0x0B: if (v == 0) { ed_ram_pos = 0; ed_ram_fill = 0; } return 1;
  }
  return 1;
}
static void ed_store16_hook(struct ARMCore *c, uint32_t a, int16_t v, int *cy) {
  if (ed_store16(a, (uint16_t)v)) { if (cy) *cy += 5; return; }
  original_memory.store16(c, a, v, cy);
}
static uint32_t ed_load16_hook(struct ARMCore *c, uint32_t a, int *cy) {
  uint32_t v;
  if (ed_load16(a, &v)) { if (cy) *cy += 5; return v; }
  return original_memory.load16(c, a, cy);
}
static void ed_store32_hook(struct ARMCore *c, uint32_t a, int32_t v, int *cy) {
  if (a >= 0x08000000 && a < 0x0E000000) { if (ed_window(a)) ed_fail(96, "32-bit register write", a); ed_rom_writes++; return; }
  original_memory.store32(c, a, v, cy);
}
static void ed_store8_hook(struct ARMCore *c, uint32_t a, int8_t v, int *cy) {
  if (a >= 0x08000000 && a < 0x0E000000) { if (ed_window(a)) ed_fail(96, "8-bit register write", a); ed_rom_writes++; return; }
  original_memory.store8(c, a, v, cy);
}
static uint32_t ed_load32_hook(struct ARMCore *c, uint32_t a, int *cy) {
  if (ed_regs_on && a >= ED_BASE && a < ED_BASE + 0x212) ed_fail(96, "32-bit register read", a);
  return original_memory.load32(c, a, cy);
}
static uint32_t ed_load8_hook(struct ARMCore *c, uint32_t a, int *cy) {
  if (ed_regs_on && a >= ED_BASE && a < ED_BASE + 0x212) ed_fail(96, "8-bit register read", a);
  return original_memory.load8(c, a, cy);
}
static void model_install(struct mCore *c) {
  model_core = c;
  const char *p = getenv("SD_IMAGE");
  if (p && *p) {
    image_file = fopen(p, getenv("SD_ALLOW_INDEX_WRITES") ? "r+b" : "rb");
    if (!image_file) exit(37);
    fseeko(image_file, 0, SEEK_END); image_size = ftello(image_file); rewind(image_file);
    sdimage = malloc(1); ed_present = 1;
  }
  sdhc = getenv("SDHC") != NULL; ed_v1 = getenv("SDV1") != NULL;
  if ((p = getenv("EDGBA_FAIL_READ"))) ed_fail_read = strtoul(p, 0, 0);
  if ((p = getenv("EDGBA_FAIL_COUNT"))) ed_fail_count = strtoul(p, 0, 0);
  ed_trace = getenv("EDGBA_TRACE") != NULL;
  struct ARMCore *cpu = c->cpu;
  original_memory = cpu->memory;
  cpu->memory.store16 = ed_store16_hook; cpu->memory.load16 = ed_load16_hook;
  cpu->memory.store32 = ed_store32_hook; cpu->memory.store8 = ed_store8_hook;
  cpu->memory.load32 = ed_load32_hook; cpu->memory.load8 = ed_load8_hook;
}
/* One line a test can parse; call at the end of a run. */
__attribute__((unused)) static void ed_report(FILE *f) {
  fprintf(f, "EDGBA cfg=%04X key=%u read_cmds=%u stop_cmds=%u sectors_read=%u sectors_written=%u cfg_writes=%u "
             "locked_writes=%u rom_writes=%u eeprom_window_writes=%u write_errors_sent=%u irq_overlaps=%u\n",
          ed_cfg, ed_key, read_cmds, stop_cmds, sd_reads, sd_writes, ed_cfg_writes, ed_locked_writes, ed_rom_writes,
          ed_eep_writes, ed_crc_errors_sent, io_irq_overlaps);
}
