/* EverDrive GBA SD driver for GBA homebrew started from the EverDrive menu.
 *
 * Port of edio-gba (everdrive.c, disk.c) by krikzz,
 * https://github.com/krikzz/gba-ed-pub, MIT License:
 *
 *   Copyright (c) 2024 krikzz
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a
 *   copy of this software and associated documentation files (the
 *   "Software"), to deal in the Software without restriction, including
 *   without limitation the rights to use, copy, modify, merge, publish,
 *   distribute, sublicense, and/or sell copies of the Software, and to permit
 *   persons to whom the Software is furnished to do so, subject to the
 *   following conditions: The above copyright notice and this permission
 *   notice shall be included in all copies or substantial portions of the
 *   Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 *
 * Changes (C) 2026 Halim Jarrar, same licence:
 *  - the same register accesses in the same order, but CPU copies instead of
 *    DMA 3 (sector reads walk the same incrementing addresses as krikzz's
 *    DMA, sector writes and the CRC buffer use the same fixed address), so
 *    no DMA channel is borrowed and any buffer alignment works;
 *  - every busy wait is bounded; a failed multi-sector read is stopped
 *    (CMD12) and tried once more;
 *  - no 512-byte stack buffers: the write CRC is computed lane by lane while
 *    the CRC buffer is read;
 *  - the configuration register always keeps PSRAM mapped and PSRAM writes
 *    off (krikzz's sample runs from RAM and turns them on);
 *  - detection (edgba_detect): the registers must appear when enabled and
 *    disappear when disabled, then the SD card must start;
 *  - WS0 runs at the BIOS default 4/2 during SD access, like krikzz's sample.
 * The game ROM stays mapped while the SD card is used (the registers only
 * cover a small window at 0x09FC0000), so this code can run from ROM. */
#include "edgba_driver.h"

#define R16(a) (*(volatile uint16_t *)(a))
#define REG_BASE 0x09FC0000u
#define REG(r) (REG_BASE + (uint32_t)(r) * 2u)
#define REG_WAITCNT 0x04000204u

#define REG_CFG      0x00
#define REG_STATUS   0x01
#define REG_FPGA_VER 0x05
#define REG_SD_CMD   0x08
#define REG_SD_DAT   0x09
#define REG_SD_CFG   0x0A
#define REG_SD_RAM   0x0B
#define REG_KEY      0x5A

#define STAT_SD_BUSY  1
#define STAT_SDC_TOUT 2

#define CFG_REGS_ON  1
#define CFG_NROM_RAM 2   /* PSRAM (the game) instead of the boot ROM: always on */
#define CART_CFG (CFG_REGS_ON | CFG_NROM_RAM)

#define SD_SPD_LO 0
#define SD_SPD_HI 1
#define SD_MODE1 0
#define SD_MODE2 2
#define SD_MODE4 4
#define SD_MODE8 6
#define SD_WAIT_F0 8
#define SD_STRT_F0 16
#define SD_MODE_BITS 30
#define SD_SPD_BITS 1

#define CMD0  0x40
#define CMD2  0x42
#define CMD3  0x43
#define CMD6  0x46
#define CMD7  0x47
#define CMD8  0x48
#define CMD9  0x49
#define CMD12 0x4C
#define CMD18 0x52
#define CMD25 0x59
#define CMD41 0x69
#define CMD55 0x77
#define CMD58 0x7A
#define SD_V2 2
#define SD_HC 1
#define R1 1
#define R2 2
#define R3 3
#define R6 6
#define R7 7

#define DISK_ERR_INIT 0xC0
#define DISK_ERR_RD1 0xD2
#define DISK_ERR_RD2 0xD3
#define DISK_ERR_WR1 0xD4
#define DISK_ERR_WR2 0xD5
#define DISK_ERR_WR3 0xD6
#define DISK_ERR_WR4 0xD7
#define DISK_ERR_WR5 0xD8
#define DISK_ERR_CMD_TIMEOUT 0xD9
#define DISK_ERR_CRC_ERROR 0xDA
#define DISK_ERR_CLOSE_RW1 0xDB
#define DISK_ERR_CLOSE_RW2 0xDC
#define WAIT 2048
#define BUSY_LIMIT 0x100000u

struct edgba_state edgba_state;
#define S edgba_state

static const uint16_t crc_16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0};

/* ---- everdrive.c ---- */

static uint16_t ed_reg_rd(unsigned reg) { return R16(REG(reg)); }
static void ed_reg_wr(unsigned reg, uint16_t data) { R16(REG(reg)) = data; }

static void ed_wait_busy(void) {
  uint32_t n = BUSY_LIMIT;
  while ((ed_reg_rd(REG_STATUS) & STAT_SD_BUSY) && --n) {}
}

static void ed_sd_mode(uint8_t mode) {
  S.sd_cfg = (uint8_t)((S.sd_cfg & ~SD_MODE_BITS) | (mode & SD_MODE_BITS));
  ed_reg_wr(REG_SD_CFG, S.sd_cfg);
}

static void ed_sd_speed(uint8_t speed) {
  S.sd_cfg = (uint8_t)((S.sd_cfg & ~SD_SPD_BITS) | (speed & SD_SPD_BITS));
  ed_reg_wr(REG_SD_CFG, S.sd_cfg);
}

static void ed_sd_cmd_wr(uint8_t data) {
  ed_reg_wr(REG_SD_CMD, data);
  ed_wait_busy();
}

static uint8_t ed_sd_cmd_rd(void) {
  uint8_t dat = (uint8_t)ed_reg_rd(REG_SD_CMD);
  ed_wait_busy();
  return dat;
}

static uint8_t ed_sd_cmd_val(void) { return (uint8_t)ed_reg_rd(REG_SD_CMD + 2); }

static void ed_sd_dat_wr(uint8_t data) {
  ed_reg_wr(REG_SD_DAT, (uint16_t)(0xff00 | data));
  ed_wait_busy();
}

static uint8_t ed_sd_dat_rd(void) {
  uint8_t dat = (uint8_t)(ed_reg_rd(REG_SD_DAT) >> 8);
  ed_wait_busy();
  return dat;
}

/* Waits for the start of the next data block (0: found). */
static unsigned ed_sd_wait_f0(void) {
  uint8_t mode = SD_MODE4 | SD_WAIT_F0 | SD_STRT_F0;
  for (unsigned i = 0; i < 65000; i++) {
    uint16_t resp;
    uint32_t n = BUSY_LIMIT;
    ed_sd_mode(mode);
    (void)ed_reg_rd(REG_SD_DAT);
    for (;;) {
      resp = ed_reg_rd(REG_STATUS);
      if ((resp & STAT_SD_BUSY) == 0 || !--n) break;
    }
    if ((resp & STAT_SDC_TOUT) == 0 && n) return 0;
    mode = SD_MODE4 | SD_WAIT_F0;
  }
  return 1;
}

/* One sector into dst: the same incrementing addresses krikzz's DMA reads. */
static unsigned ed_sd_read_sector(uint8_t *dst) {
  if (ed_sd_wait_f0()) return 1;
  volatile uint16_t *src = (volatile uint16_t *)REG(REG_SD_DAT);
  if (((uintptr_t)dst & 1u) == 0) {
    /* Unrolled: the loop runs from ROM, so fewer branches matter (gbamp3). */
    uint16_t *d = (uint16_t *)dst;
    for (unsigned i = 0; i < 256; i += 8, d += 8, src += 8) {
      d[0] = src[0]; d[1] = src[1]; d[2] = src[2]; d[3] = src[3];
      d[4] = src[4]; d[5] = src[5]; d[6] = src[6]; d[7] = src[7];
    }
  } else {
    for (unsigned i = 0; i < 256; i++) { uint16_t v = src[i]; dst[2 * i] = (uint8_t)v; dst[2 * i + 1] = (uint8_t)(v >> 8); }
  }
  return 0;
}

/* One sector out to the fixed data address, as krikzz's ed_sd_dma_wr. */
static void ed_sd_write_sector(const uint8_t *src) {
  ed_reg_wr(REG_SD_RAM, 0);
  ed_sd_mode(SD_MODE4);
  volatile uint16_t *dst = (volatile uint16_t *)REG(REG_SD_DAT);
  if (((uintptr_t)src & 1u) == 0) {
    const uint16_t *s = (const uint16_t *)src;
    for (unsigned i = 0; i < 256; i++) *dst = s[i];
  } else {
    for (unsigned i = 0; i < 256; i++) *dst = (uint16_t)(src[2 * i] | (src[2 * i + 1] << 8));
  }
}

/* ---- disk.c ---- */

static uint8_t disk_crc7(const uint8_t *buff, unsigned len) {
  unsigned a, crc = 0;
  while (len--) {
    crc ^= *buff++;
    a = 8;
    do {
      crc <<= 1;
      if (crc & (1 << 8)) crc ^= 0x12;
    } while (--a);
  }
  return (uint8_t)(crc & 0xfe);
}

static uint8_t disk_resp_type(uint8_t cmd) {
  if (cmd == CMD3) return R6;
  if (cmd == CMD8) return R7;
  if (cmd == CMD2 || cmd == CMD9) return R2;
  if (cmd == CMD58 || cmd == CMD41) return R3;
  return R1;
}

/* resp: 18 bytes. */
static uint8_t disk_cmd(uint8_t cmd, uint32_t arg, uint8_t *resp) {
  uint8_t resp_type = disk_resp_type(cmd);
  uint8_t buff[6];
  uint8_t resp_len = resp_type == R2 ? 17 : 6;
  uint8_t crc;
  unsigned i;
  buff[0] = cmd;
  buff[1] = (uint8_t)(arg >> 24);
  buff[2] = (uint8_t)(arg >> 16);
  buff[3] = (uint8_t)(arg >> 8);
  buff[4] = (uint8_t)arg;
  crc = disk_crc7(buff, 5) | 1;

  ed_sd_mode(SD_MODE8);
  ed_sd_cmd_wr(0xff);
  ed_sd_cmd_wr(cmd);
  ed_sd_cmd_wr((uint8_t)(arg >> 24));
  ed_sd_cmd_wr((uint8_t)(arg >> 16));
  ed_sd_cmd_wr((uint8_t)(arg >> 8));
  ed_sd_cmd_wr((uint8_t)arg);
  ed_sd_cmd_wr(crc);

  if (cmd == CMD18) return 0;

  ed_sd_cmd_rd();
  ed_sd_mode(SD_MODE1);
  i = 0;
  for (;;) {
    if ((ed_sd_cmd_val() & 192) == 0) break;
    if (i++ == WAIT) return DISK_ERR_CMD_TIMEOUT;
    ed_sd_cmd_rd();
  }

  ed_sd_mode(SD_MODE8);
  resp[0] = ed_sd_cmd_rd();
  for (i = 1; i < (unsigned)resp_len - 1; i++) resp[i] = ed_sd_cmd_rd();
  resp[i] = ed_sd_cmd_val();

  if (resp_type != R3) {
    if (resp_type == R2) crc = disk_crc7(resp + 1, (unsigned)resp_len - 2) | 1;
    else crc = disk_crc7(resp, (unsigned)resp_len - 1) | 1;
    if (crc != resp[resp_len - 1]) return DISK_ERR_CRC_ERROR;
  }
  return 0;
}

static uint8_t disk_init(void) {
  uint8_t resp[18];
  uint8_t r;
  unsigned i;
  uint32_t rca;

  S.card_type = 0;
  ed_sd_speed(SD_SPD_LO);
  ed_sd_mode(SD_MODE8);

  for (i = 0; i < 40; i++) ed_sd_dat_wr(0xff);
  (void)disk_cmd(CMD0, 0x1aa, resp);
  for (i = 0; i < 40; i++) ed_sd_dat_wr(0xff);

  r = disk_cmd(CMD8, 0x1aa, resp);
  if (r != 0 && r != DISK_ERR_CMD_TIMEOUT) return DISK_ERR_INIT + 0;
  if (r == 0) S.card_type |= SD_V2;

  if (S.card_type == SD_V2) {
    for (i = 0; i < WAIT; i++) {
      r = disk_cmd(CMD55, 0, resp);
      if (r) return DISK_ERR_INIT + 1;
      if ((resp[3] & 1) != 1) continue;
      r = disk_cmd(CMD41, 0x40300000, resp);
      if ((resp[1] & 128) == 0) continue;
      break;
    }
  } else {
    i = 0;
    do {
      r = disk_cmd(CMD55, 0, resp);
      if (r) return DISK_ERR_INIT + 2;
      r = disk_cmd(CMD41, 0x40300000, resp);
      if (r) return DISK_ERR_INIT + 3;
    } while (resp[1] < 1 && i++ < WAIT);
  }
  if (i == WAIT) return DISK_ERR_INIT + 4;
  if ((resp[1] & 64) && S.card_type != 0) S.card_type |= SD_HC;

  if (disk_cmd(CMD2, 0, resp)) return DISK_ERR_INIT + 5;
  if (disk_cmd(CMD3, 0, resp)) return DISK_ERR_INIT + 6;
  rca = ((uint32_t)resp[1] << 24) | ((uint32_t)resp[2] << 16) | ((uint32_t)resp[3] << 8) | resp[4];
  {
    /* krikzz's CMD7 with RCA 0 (no answer expected); its own buffer keeps
     * the RCA from CMD3, as krikzz's code relies on. */
    uint8_t ignored[18];
    (void)disk_cmd(CMD7, 0, ignored);
  }
  if (disk_cmd(CMD9, rca, resp)) return DISK_ERR_INIT + 7;
  if (disk_cmd(CMD7, rca, resp)) return DISK_ERR_INIT + 8;
  if (disk_cmd(CMD55, rca, resp)) return DISK_ERR_INIT + 9;
  if (disk_cmd(CMD6, 2, resp)) return DISK_ERR_INIT + 10;   /* ACMD6: 4-bit bus */

  ed_sd_speed(SD_SPD_HI);
  S.disk_addr = ~0u;
  return 0;
}

static uint8_t disk_close_rw(void) {
  uint8_t resp[18];
  unsigned i;
  if (S.disk_addr == ~0u) return 0;
  S.disk_addr = ~0u;
  if (disk_cmd(CMD12, 0, resp)) return DISK_ERR_CLOSE_RW1;
  ed_sd_mode(SD_MODE1);
  ed_sd_dat_rd();
  ed_sd_dat_rd();
  ed_sd_dat_rd();
  ed_sd_mode(SD_MODE2);
  i = 65535;
  while (--i) {
    if (ed_sd_dat_rd() == 0xff) break;
  }
  if (i == 0) return DISK_ERR_CLOSE_RW2;
  return 0;
}

static uint8_t disk_read(uint32_t sd_addr, uint8_t *dst, unsigned slen) {
  uint8_t resp[18];
  uint8_t r;
  if (sd_addr != S.disk_addr) {
    r = disk_close_rw();
    if (r) return r;
    if (disk_cmd(CMD18, (S.card_type & SD_HC) ? sd_addr : sd_addr * 512u, resp)) return DISK_ERR_RD1;
    S.disk_addr = sd_addr;
  }
  while (slen) {
    if (ed_sd_read_sector(dst)) return DISK_ERR_RD2;
    dst += 512;
    slen--;
    S.disk_addr++;
  }
  return 0;
}

/* The CRC16 of the four data lines, from the cart's CRC buffer (one lane of
 * 128 bytes after the other: krikzz's crc16SD_HW without its 512-byte copy). */
static void disk_crc16(uint16_t *crc_out) {
  uint16_t crc_table[4];
  volatile uint16_t *ram = (volatile uint16_t *)REG(REG_SD_RAM);
  ed_reg_wr(REG_SD_RAM, 0);
  for (unsigned lane = 0; lane < 4; lane++) {
    uint16_t crc = 0;
    for (unsigned i = 0; i < 64; i++) {
      uint16_t v = *ram;
      uint16_t tmp = crc;
      crc = (uint16_t)(crc_16_table[(tmp >> 8) ^ (v & 0xff)] ^ (tmp << 8));
      tmp = crc;
      crc = (uint16_t)(crc_16_table[(tmp >> 8) ^ (v >> 8)] ^ (tmp << 8));
    }
    crc_table[lane] = crc;
  }
  for (unsigned i = 0; i < 4; i++) {
    for (unsigned u = 0; u < 16; u++) {
      crc_out[3 - i] >>= 1;
      crc_out[3 - i] |= (uint16_t)((crc_table[u % 4] & 1) << 15);
      crc_table[u % 4] >>= 1;
    }
  }
}

static uint8_t disk_write(uint32_t sd_addr, const uint8_t *src, unsigned slen) {
  uint8_t resp[18];
  uint16_t crc16[4] = {0, 0, 0, 0};
  uint8_t r;
  unsigned i, u;

  r = disk_close_rw();
  if (r) return r;
  S.disk_addr = sd_addr;
  if (disk_cmd(CMD25, (S.card_type & SD_HC) ? sd_addr : sd_addr * 512u, resp)) return DISK_ERR_WR1;

  while (slen--) {
    ed_sd_mode(SD_MODE2);
    ed_sd_dat_wr(0xff);
    ed_sd_dat_wr(0xf0);
    ed_sd_write_sector(src);
    disk_crc16(crc16);
    src += 512;

    ed_sd_mode(SD_MODE2);
    for (i = 0; i < 4; i++) {
      ed_sd_dat_wr((uint8_t)(crc16[i] >> 8));
      ed_sd_dat_wr((uint8_t)(crc16[i] & 0xff));
    }
    ed_sd_mode(SD_MODE1);
    ed_sd_dat_wr(0xff);
    ed_sd_dat_rd();

    i = 1024;
    while ((ed_sd_dat_rd() & 1) != 0 && --i != 0) {}
    if (i == 0) return DISK_ERR_WR3;
    r = 0;
    for (i = 0; i < 3; i++) {
      r = (uint8_t)(r << 1);
      u = ed_sd_dat_rd();
      r |= (uint8_t)(u & 1);
    }
    r &= 7;
    if (r != 0x02) return r == 5 ? DISK_ERR_WR4 : DISK_ERR_WR5;

    ed_sd_mode(SD_MODE1);
    ed_sd_dat_rd();
    i = 65535;
    while (--i) {
      if (ed_sd_dat_rd() == 0xff) break;
    }
    if (i == 0) return DISK_ERR_WR2;
  }
  return disk_close_rw();
}

/* ---- this app's interface ---- */

static uint16_t ws_begin(void) {
  uint16_t old = R16(REG_WAITCNT);
  R16(REG_WAITCNT) = (uint16_t)(old & ~0x001Cu);   /* WS0 4/2 */
  return old;
}

unsigned edgba_read(void *buffer, uint32_t lba, unsigned count) {
  uint16_t ws = ws_begin();
  uint8_t r = disk_read(lba, (uint8_t *)buffer, count);
  if (r) {
    /* Stop the multi-sector read (CMD12) and try once more. */
    (void)disk_close_rw();
    r = disk_read(lba, (uint8_t *)buffer, count);
    if (r) { S.last_err = r; (void)disk_close_rw(); }
  }
  R16(REG_WAITCNT) = ws;
  return r;
}

unsigned edgba_write(const void *buffer, uint32_t lba, unsigned count) {
  uint16_t ws = ws_begin();
  uint8_t r = disk_write(lba, (const uint8_t *)buffer, count);
  if (r) { S.last_err = r; S.disk_addr = lba; (void)disk_close_rw(); S.disk_addr = ~0u; }
  R16(REG_WAITCNT) = ws;
  return r;
}

unsigned edgba_detect(void) {
  uint16_t off[3], on[3];
  S.disk_addr = ~0u; S.fpga_ver = 0; S.sd_cfg = 0; S.card_type = 0; S.init_err = 0; S.last_err = 0;
  /* SuperCard: SDRAM mapped, SD interface off, SDRAM writes off, so the
   * writes below cannot change a SuperCard's ROM image. */
  R16(0x09FFFFFEu) = 0xA55A; R16(0x09FFFFFEu) = 0xA55A;
  R16(0x09FFFFFEu) = 0x0001; R16(0x09FFFFFEu) = 0x0001;
  /* EverDrive: unlock the registers, then compare the register window with
   * the registers off and on. PSRAM stays mapped, PSRAM writes stay off. */
  ed_reg_wr(REG_KEY, 0xA5);
  ed_reg_wr(REG_CFG, CFG_NROM_RAM);
  off[0] = ed_reg_rd(REG_STATUS); off[1] = ed_reg_rd(REG_FPGA_VER); off[2] = ed_reg_rd(REG_SD_CMD + 2);
  ed_reg_wr(REG_CFG, CART_CFG);
  on[0] = ed_reg_rd(REG_STATUS); on[1] = ed_reg_rd(REG_FPGA_VER); on[2] = ed_reg_rd(REG_SD_CMD + 2);
  if (off[0] == on[0] && off[1] == on[1] && off[2] == on[2]) {
    ed_reg_wr(REG_CFG, CFG_NROM_RAM);   /* not an EverDrive: leave the window as it was */
    return EDGBA_NONE;
  }
  S.fpga_ver = on[1];
  S.sd_cfg = 0;
  ed_reg_wr(REG_SD_CFG, 0);
  uint16_t ws = ws_begin();
  S.init_err = disk_init();
  R16(REG_WAITCNT) = ws;
  return S.init_err ? EDGBA_NO_CARD : EDGBA_READY;
}
