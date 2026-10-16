/* Which cart the SD card is on: SuperCard SD, EZ-Flash Omega or EverDrive GBA.
 * (C) 2026 Halim Jarrar. MIT licence (see ezfo_driver.c). */
#ifndef CART_IO_H
#define CART_IO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define CART_UNKNOWN 0
#define CART_SUPERCARD 1      /* not an Omega: the SuperCard driver is used */
#define CART_EZFO 2           /* EZ-Flash Omega, SD card ready */
#define CART_EZFO_NO_CARD 3   /* EZ-Flash Omega, SD card did not answer */
#define CART_EDGBA 4          /* EverDrive GBA, SD card ready */
#define CART_EDGBA_NO_CARD 5  /* EverDrive GBA, SD card did not start */

extern unsigned cart_kind;

/* Checks the cart once per boot (later calls return the first result):
 * EverDrive first, then EZ-Flash Omega, otherwise SuperCard. Holding L+R
 * while it runs skips both checks (SuperCard forced). */
unsigned cart_detect(void);

/* Butano apps: detect, then start the SuperCard driver if needed (SDRAM
 * mapped, SD interface on, SDRAM writes as before). 0 = SD card ready. */
unsigned cart_sd_init(void);

/* 0 on success. */
unsigned cart_read(void *buffer, uint32_t lba, unsigned count);
unsigned cart_write(const void *buffer, uint32_t lba, unsigned count);

#ifdef __cplusplus
}
#endif
#endif
