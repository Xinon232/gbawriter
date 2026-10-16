/* EverDrive GBA SD driver (Mini; X5/X3 expected). See edgba_driver.c.
 * Based on edio-gba by krikzz (MIT licence, Copyright (c) 2024 krikzz). */
#ifndef EDGBA_DRIVER_H
#define EDGBA_DRIVER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* edgba_detect() results */
#define EDGBA_NONE    0  /* not an EverDrive */
#define EDGBA_READY   1  /* EverDrive, SD card started */
#define EDGBA_NO_CARD 2  /* EverDrive, but the SD card did not start */

/* What detection saw, for a diagnostics screen; also the driver's state. */
struct edgba_state {
  uint32_t disk_addr;   /* next sector of an open multi-sector read, ~0 when none */
  uint16_t fpga_ver;    /* REG_FPGA_VER */
  uint8_t sd_cfg;       /* last value written to REG_SD_CFG */
  uint8_t card_type;    /* bit 1: SD v2, bit 0: SDHC (block addressing) */
  uint8_t init_err;     /* 0 or the DISK_ERR_INIT code of the card start */
  uint8_t last_err;     /* last read/write error code (0: none) */
};
extern struct edgba_state edgba_state;

/* Runs with the game ROM mapped (the EverDrive's registers only cover a
 * small window at 0x09FC0000), so these can live in ROM. */
unsigned edgba_detect(void);
/* 0 on success, nonzero on error. Any buffer alignment. */
unsigned edgba_read(void *buffer, uint32_t lba, unsigned count);
unsigned edgba_write(const void *buffer, uint32_t lba, unsigned count);

#ifdef __cplusplus
}
#endif
#endif
