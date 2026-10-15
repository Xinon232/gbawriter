/* Which cart the SD card is on: SuperCard SD or EZ-Flash Omega.
 * (C) 2026 Halim Jarrar. MIT licence (see ezfo_driver.c). */
#include "cart_io.h"
#include "ezfo_driver.h"
#include "supercard_driver.h"

#define REG_KEYINPUT (*(volatile uint16_t *)0x04000130)
#define REG_WAITCNT_ (*(volatile uint16_t *)0x04000204)

unsigned cart_kind = CART_UNKNOWN;

unsigned cart_detect(void) {
  if (cart_kind != CART_UNKNOWN) return cart_kind;
  unsigned held = ~REG_KEYINPUT & 0x3FFu;
  if ((held & 0x300u) == 0x300u) return cart_kind = CART_SUPERCARD; /* L+R: SuperCard forced */
  switch (ezfo_detect()) {
  case EZFO_READY: cart_kind = CART_EZFO; break;
  case EZFO_NO_CARD: cart_kind = CART_EZFO_NO_CARD; break;
  default: cart_kind = CART_SUPERCARD; break;
  }
  return cart_kind;
}

unsigned cart_sd_init(void) {
  unsigned kind = cart_detect();
  if (kind == CART_EZFO) return 0;
  if (kind == CART_EZFO_NO_CARD) return 1;
  REG_WAITCNT_ = 0x40c0;
  set_supercard_mode(MAPPED_SDRAM, true, true);
  t_card_info info;
  return sdcard_init(&info);
}

unsigned cart_read(void *buffer, uint32_t lba, unsigned count) {
  if (cart_kind == CART_EZFO) return ezfo_read(buffer, lba, count);
  return sdcard_read_blocks((uint8_t *)buffer, lba, count);
}

unsigned cart_write(const void *buffer, uint32_t lba, unsigned count) {
  if (cart_kind == CART_EZFO) return ezfo_write(buffer, lba, count);
  return sdcard_write_blocks((const uint8_t *)buffer, lba, count);
}
