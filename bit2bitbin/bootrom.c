#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bootrom.h"

#define rpt() printf("%s:%d\n", __func__, __LINE__)

uint32_t bootrom_calc_header_checksum(bootrom_hdr_t *bootrom_hdr){
  uint32_t *ptr;
  uint32_t sum;
  int i;

  ptr = (uint32_t*) &bootrom_hdr->width_detect;

  for(i = 0; i < 10; i++) {
    sum += ptr[i];
  }

  return ~sum;
}

int bootrom_prepare_header(bootrom_hdr_t *bootrom_hdr){
  int i = 0;
  for (i = 0; i < sizeof(bootrom_hdr->interrupt_table); i++) {
    /* TODO this probably can be done better than in loop */
    bootrom_hdr->interrupt_table[i]=BOOTROM_INT_TABLE_DEFAULT;
  }
  bootrom_hdr->width_detect = BOOTROM_WIDTH_DETECT;
  memcpy(&(bootrom_hdr->img_id), BOOTROM_IMG_ID, strlen(BOOTROM_IMG_ID));
  bootrom_hdr->encryption_status = BOOTROM_ENCRYPTED_NONE;
  /* BootROM does not interpret the field below */
  bootrom_hdr->user_defined_0 = BOOTROM_USER_0;
  bootrom_hdr->src_offset = 0x0; /* TODO will have to be calulated */
  bootrom_hdr->img_len = 0x0; /* TODO remember to fill it */
  bootrom_hdr->reserved_0 = BOOTROM_RESERVED_0;
  /* TODO the param below must be greater than 0x0 and less than 0x30000 */
  bootrom_hdr->start_of_exec = 0x0;
  bootrom_hdr->total_img_len = 0x0; /* TODO remember to fill it */
  bootrom_hdr->reserved_1 = BOOTROM_RESERVED_1_RL;
  bootrom_hdr->checksum = bootrom_calc_header_checksum(bootrom_hdr);
  memset(bootrom_hdr->user_defined_1, 0x0,
         sizeof(bootrom_hdr->user_defined_1) * sizeof(uint32_t));
  memset(bootrom_hdr->reg_init, 0xFF,
         sizeof(bootrom_hdr->reg_init) * sizeof(uint32_t));
  memset(bootrom_hdr->user_defined_2, 0x0,
         sizeof(bootrom_hdr->user_defined_2) * sizeof(uint32_t));

  return 0;
}

int bootrom_write_file(const char *fname){
  FILE *file;
  bootrom_hdr_t bootrom_hdr;

  bootrom_prepare_header(&bootrom_hdr);

  file = fopen(fname, "wb");

  printf("Will attempt writing\n");

  if (!fwrite(&bootrom_hdr, sizeof(bootrom_hdr_t), 1, file))
    printf("Error writing the binary file!\n");

  printf("Writing the binary file succesfull.\n");

  fclose(file);

  return 0;
}
