/* EZ-Flash Omega SD driver for GBA homebrew running from PSRAM.
 * (C) 2026 Halim Jarrar. MIT licence (see ezfo_driver.c).
 * Register protocol from the EZ-Flash Omega kernel (Apache-2.0) and
 * ezfo-disc_io by Felix Jones / ApacheThunder (Apache-2.0). */
#ifndef EZFO_DRIVER_H
#define EZFO_DRIVER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* All entry points run from RAM (IWRAM unless EZFO_SECTION says otherwise):
 * while the SD card is used the Omega maps its kernel over the game ROM. */
#define EZFO_LONG_CALL __attribute__((long_call))

/* ezfo_detect() results */
#define EZFO_NONE    0  /* not an Omega (or forced off) */
#define EZFO_READY   1  /* Omega, SD card answered */
#define EZFO_NO_CARD 2  /* Omega, but the SD card did not answer */

/* What detection saw, for a diagnostics screen. */
struct ezfo_probe {
  uint16_t nor_id;        /* S98 NOR ID read in kernel mode (0x223D on known boards) */
  uint8_t header_gone;    /* our ROM header disappeared in kernel mode */
  uint8_t header_back;    /* and came back on the PSRAM page */
  uint8_t sd_answer;      /* sector 0 read with a 0x55AA signature */
  uint8_t irq_hook;       /* the kernel's in-game IRQ hook was found */
  uint16_t sd_polls;      /* busy polls for that read */
  uint8_t write_failed;   /* a write timed out: writes are refused from then on */
};
extern struct ezfo_probe ezfo_probe_info;

/* 1 while the kernel page is mapped. An interrupt handler that runs during
 * SD access must not touch ROM; it can use this to postpone ROM work. */
extern volatile uint8_t ezfo_busy;

/* 0: interrupts are masked for each SD command (default).
 * 1: interrupts stay as they are; every handler must live in RAM. */
extern uint8_t ezfo_irq_live;

EZFO_LONG_CALL unsigned ezfo_detect(void);
/* 0 on success, nonzero on error. Any buffer alignment. */
EZFO_LONG_CALL unsigned ezfo_read(void *buffer, uint32_t lba, unsigned count);
EZFO_LONG_CALL unsigned ezfo_write(const void *buffer, uint32_t lba, unsigned count);

#ifdef __cplusplus
}
#endif
#endif
