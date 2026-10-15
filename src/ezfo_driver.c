/* EZ-Flash Omega SD driver for GBA homebrew running from PSRAM.
 *
 * (C) 2026 Halim Jarrar. MIT licence:
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software, to deal in the software without restriction, including
 * the rights to use, copy, modify, merge, publish, distribute, sublicense and
 * sell copies, provided this notice is kept. THE SOFTWARE IS PROVIDED "AS IS",
 * WITHOUT WARRANTY OF ANY KIND.
 *
 * Written from the register protocol of the EZ-Flash Omega and Omega DE
 * kernels (ez-flash/omega-kernel and omega-de-kernel, Ezcard_OP.c, Apache-2.0)
 * and ezfo-disc_io by Felix Jones and ApacheThunder (Apache-2.0). Same
 * sequence, registers, timeouts and order as the kernel's Read_SD_sectors and
 * Write_SD_sectors, with these differences:
 *  - a failed read is reported instead of copying the busy buffer;
 *  - each command (at most 4 sectors) maps the kernel and maps the game back,
 *    so interrupts can be served between commands;
 *  - the SD window is read and written with 16-bit CPU accesses (the kernel's
 *    libgba dmaCopy is 16-bit as well); no DMA channel is borrowed;
 *  - the kernel's in-game IRQ hook (it lives in the game ROM) is bypassed
 *    while the kernel is mapped.
 *
 * Everything here runs from RAM: while the kernel is mapped the game ROM is
 * gone, so this file has no constant tables, switch tables, divisions or
 * library calls. tests/check_ram_code.py checks the linked ROM for that. */
#include "ezfo_driver.h"

#ifndef EZFO_SECTION
#define EZFO_SECTION ".iwram.ezfo"
#endif
#define EZFO_CODE __attribute__((section(EZFO_SECTION), target("arm"), noinline))

#define R16(a) (*(volatile uint16_t *)(a))
#define R32(a) (*(volatile uint32_t *)(a))
#define REG_IME 0x04000208u
#define REG_WAITCNT 0x04000204u

#define PAGE_PSRAM 0x0200u
#define PAGE_KERNEL 0x8002u
#define SD_WINDOW 0x09E00000u
#define SD_BUSY 0xEEE1u
#define SD_TIMEOUT 0x100000u /* kernel Wait_SD_Response */

struct ezfo_probe ezfo_probe_info;
volatile uint8_t ezfo_busy;
uint8_t ezfo_irq_live;
/* Built at run time: with its in-game patches on, the kernel rewrites every
 * literal 0x03007FFC in the ROM image (this code's copy included). */
static volatile uint32_t irq_vector_base = 0x03007F00u;

struct window {
  uint16_t ime, waitcnt;
  uint32_t vector, hooked;
};

static EZFO_CODE void spin(uint32_t n) {
  volatile uint32_t i = n;
  while (i) i--;
}

static EZFO_CODE void cart_reg(uint32_t reg, uint16_t value) {
  R16(0x09FE0000u) = 0xD200;
  R16(0x08000000u) = 0x1500;
  R16(0x08020000u) = 0xD200;
  R16(0x08040000u) = 0x1500;
  R16(reg) = value;
  R16(0x09FC0000u) = 0x1500;
}

static EZFO_CODE void sd_control(uint16_t c) { cart_reg(0x09400000u, c); }

static EZFO_CODE void sd_command(uint32_t lba, uint16_t count) {
  R16(0x09FE0000u) = 0xD200;
  R16(0x08000000u) = 0x1500;
  R16(0x08020000u) = 0xD200;
  R16(0x08040000u) = 0x1500;
  R16(0x09600000u) = (uint16_t)lba;
  R16(0x09620000u) = (uint16_t)(lba >> 16);
  R16(0x09640000u) = count;
  R16(0x09FC0000u) = 0x1500;
}

/* 0 = answered, 1 = timeout. Counts polls into *polls. */
static EZFO_CODE unsigned sd_wait(uint32_t *polls) {
  uint32_t n = 0;
  while (R16(SD_WINDOW) == SD_BUSY) {
    if (++n > SD_TIMEOUT) { *polls += n; return 1; }
  }
  *polls += n;
  return 0;
}

static EZFO_CODE void enter(struct window *w) {
  w->ime = R16(REG_IME);
  w->hooked = 0;
  uint32_t vector_addr = irq_vector_base + 0xFCu;
  w->vector = R32(vector_addr);
  uint16_t ime = ezfo_irq_live ? w->ime : 0;
  R16(REG_IME) = 0;
  if (w->vector >= 0x08000000u && w->vector < 0x0E000000u) {
    /* The kernel's IRQ hook sits in the game ROM and jumps through
     * 0x03007FF4. Point the BIOS straight at that handler for now. */
    uint32_t real = R32(0x03007FF4u);
    if (real >= 0x02000000u && real < 0x04000000u) { R32(vector_addr) = real; w->hooked = 1; }
    else ime = 0;
  }
  /* WS0 at the BIOS default 4/2, the kernel's own setting. */
  w->waitcnt = R16(REG_WAITCNT);
  R16(REG_WAITCNT) = (uint16_t)(w->waitcnt & ~0x001Cu);
  ezfo_busy = 1;
  cart_reg(0x09880000u, PAGE_KERNEL);
  R16(REG_IME) = ime;
}

static EZFO_CODE void leave(struct window *w) {
  R16(REG_IME) = 0;
  cart_reg(0x09880000u, PAGE_PSRAM);
  ezfo_busy = 0;
  R16(REG_WAITCNT) = w->waitcnt;
  if (w->hooked) R32(irq_vector_base + 0xFCu) = w->vector;
  R16(REG_IME) = w->ime;
}

static EZFO_CODE void copy_in(uint8_t *dst, unsigned halfwords) {
  volatile uint16_t *src = (volatile uint16_t *)SD_WINDOW;
  if (((uintptr_t)dst & 1u) == 0) {
    uint16_t *d = (uint16_t *)dst;
    while (halfwords--) *d++ = *src++;
  } else {
    while (halfwords--) { uint16_t v = *src++; dst[0] = (uint8_t)v; dst[1] = (uint8_t)(v >> 8); dst += 2; }
  }
}

static EZFO_CODE void copy_out(const uint8_t *s, unsigned halfwords) {
  volatile uint16_t *dst = (volatile uint16_t *)SD_WINDOW;
  if (((uintptr_t)s & 1u) == 0) {
    const uint16_t *p = (const uint16_t *)s;
    while (halfwords--) *dst++ = *p++;
  } else {
    while (halfwords--) { *dst++ = (uint16_t)(s[0] | (s[1] << 8)); s += 2; }
  }
}

/* One command of 1..4 sectors. */
static EZFO_CODE unsigned read_chunk(uint8_t *buf, uint32_t lba, unsigned n, uint32_t *polls) {
  struct window w;
  unsigned err = 1;
  enter(&w);
  sd_control(1);
  for (unsigned tries = 0; tries < 3 && err; tries++) {
    if (tries) spin(5000);
    sd_command(lba, (uint16_t)n);
    sd_control(3);
    err = sd_wait(polls);
    sd_control(1);
  }
  if (!err) copy_in(buf, n * 256u);
  sd_control(0);
  leave(&w);
  return err;
}

static EZFO_CODE unsigned write_chunk(const uint8_t *buf, uint32_t lba, unsigned n) {
  struct window w;
  unsigned err = 1;
  uint32_t polls = 0;
  enter(&w);
  for (unsigned tries = 0; tries < 2 && err; tries++) {
    /* Same sector, same data: repeating a timed-out write is harmless. */
    if (tries) spin(5000);
    sd_control(1);
    sd_control(3);
    copy_out(buf, n * 256u);
    sd_command(lba, (uint16_t)(0x8000u + n));
    err = sd_wait(&polls);
    spin(3000);
    sd_control(0);
  }
  leave(&w);
  return err;
}

EZFO_CODE unsigned ezfo_read(void *buffer, uint32_t lba, unsigned count) {
  uint8_t *b = (uint8_t *)buffer;
  uint32_t polls = 0;
  while (count) {
    unsigned n = count > 4u ? 4u : count;
    if (read_chunk(b, lba, n, &polls)) return 1;
    b += n * 512u; lba += n; count -= n;
  }
  return 0;
}

EZFO_CODE unsigned ezfo_write(const void *buffer, uint32_t lba, unsigned count) {
  const uint8_t *b = (const uint8_t *)buffer;
  while (count) {
    unsigned n = count > 4u ? 4u : count;
    if (write_chunk(b, lba, n)) return 1;
    b += n * 512u; lba += n; count -= n;
  }
  return 0;
}

static EZFO_CODE unsigned header_equal(const uint16_t *saved) {
  for (unsigned i = 0; i < 16; i++)
    if (R16(0x080000A0u + i * 2u) != saved[i]) return 0;
  return 1;
}

/* Something is badly wrong (the ROM is not on the PSRAM page, e.g. it was
 * installed to NOR): stop on a red screen without touching ROM. */
static EZFO_CODE void halt_red(void) {
  R16(0x04000000u) = 0;
  R16(0x05000000u) = 0x001F;
  for (;;) {}
}

EZFO_CODE unsigned ezfo_detect(void) {
  uint16_t saved[16];
  uint8_t sector_tail[2] = {0, 0};
  struct window w;
  struct ezfo_probe *p = &ezfo_probe_info;
  p->nor_id = 0; p->header_gone = p->header_back = p->sd_answer = p->irq_hook = 0; p->sd_polls = 0;
  /* SuperCard: SDRAM mapped, SD interface off, SDRAM writes off. A SuperCard
   * then ignores the Omega's unlock writes below; an Omega ignores this. */
  R16(0x09FFFFFEu) = 0xA55A; R16(0x09FFFFFEu) = 0xA55A;
  R16(0x09FFFFFEu) = 0x0001; R16(0x09FFFFFEu) = 0x0001;
  for (unsigned i = 0; i < 16; i++) saved[i] = R16(0x080000A0u + i * 2u);

  unsigned ime = R16(REG_IME);
  R16(REG_IME) = 0;
  uint32_t vector = R32(irq_vector_base + 0xFCu);
  p->irq_hook = vector >= 0x08000000u && vector < 0x0E000000u;
  cart_reg(0x09880000u, PAGE_KERNEL);
  spin(5000);
  p->header_gone = !header_equal(saved);
  if (p->header_gone) {
    R16(0x09000000u) = 0xF0;
    R16(0x09000000u + 0x555u * 2u) = 0xAA;
    R16(0x09000000u + 0x2AAu * 2u) = 0x55;
    R16(0x09000000u + 0x555u * 2u) = 0x90;
    p->nor_id = R16(0x09000000u + 0xEu * 2u);
    R16(0x09000000u) = 0xF0;
  }
  cart_reg(0x09880000u, PAGE_PSRAM);
  spin(5000);
  p->header_back = header_equal(saved);
  R16(REG_IME) = ime;
  if (!p->header_gone) return EZFO_NONE;
  if (!p->header_back) halt_red();

  /* The SD card answers: sector 0 (MBR or boot sector) ends in 55 AA. */
  uint32_t polls = 0;
  unsigned saved_live = ezfo_irq_live;
  ezfo_irq_live = 0;
  enter(&w);
  sd_control(1);
  sd_command(0, 1);
  sd_control(3);
  unsigned err = sd_wait(&polls);
  sd_control(1);
  if (!err) { uint16_t v = R16(SD_WINDOW + 510u); sector_tail[0] = (uint8_t)v; sector_tail[1] = (uint8_t)(v >> 8); }
  sd_control(0);
  leave(&w);
  ezfo_irq_live = (uint8_t)saved_live;
  p->sd_polls = polls > 0xFFFFu ? 0xFFFFu : (uint16_t)polls;
  p->sd_answer = !err && sector_tail[0] == 0x55 && sector_tail[1] == 0xAA;
  return p->sd_answer ? EZFO_READY : EZFO_NO_CARD;
}
