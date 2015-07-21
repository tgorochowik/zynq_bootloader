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

char magic_hdr_cmp[] = { 0x00, 0x09, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x00, 0x00, 0x01 };
int bootrom_append_bitstream(const char *fname, FILE *ofile){
  FILE *f_bit = fopen(fname, "r");
  if (!f_bit) {
    printf("file %s does not exist\n", fname);
    exit(1);
  }
  char magic_hdr[13];
  fread(&magic_hdr, 1, sizeof(magic_hdr), f_bit);
  if (memcmp(&magic_hdr, &magic_hdr_cmp, 13) != 0) {
    fclose(f_bit);
    printf("bit file seems to be incorrect.\n");
    exit(1);
  }
  while (1) {
    char section_hdr[2];
    fread(&section_hdr, 1, sizeof(section_hdr), f_bit);
    if (section_hdr[1] != 0x0) {
      fclose(f_bit);
      printf("bit file seems to have mismatched sections.\n");
      exit(1);
    }

    if (section_hdr[0] == 'e')
      break;

    uint8_t section_size;
    fread(&section_size, 1, sizeof(uint8_t), f_bit);
    char section_data[255];
    fread(&section_data, 1, section_size, f_bit);

    printf("Section '%c' size=%d : data = \"%s\"\n",
           section_hdr[0], section_size, section_data);
  }
  uint32_t bit_size;
  fread(&bit_size, 1, 3, f_bit);
  bit_size = ((bit_size >> 16) & 0xFF) | (bit_size & 0xFF00) | ((bit_size << 16) & 0xFF0000);
  printf("bitstream size is %u\n",bit_size);

  int i;
  char old_val[4];
  char new_val[4];
  for (i = 0; i < bit_size; i+=sizeof(old_val)) {
    int read = fread(&old_val, 1, sizeof(old_val), f_bit);
    new_val[0] = old_val[3];
    new_val[1] = old_val[2];
    new_val[2] = old_val[1];
    new_val[3] = old_val[0];
    fwrite(&new_val, 1, read, ofile);
  }
  fclose(f_bit);
}

int bootrom_write_file(const char *fname){
  FILE *file;
  bootrom_hdr_t bootrom_hdr;

  bootrom_prepare_header(&bootrom_hdr);

  /* TODO add sanity check */
  file = fopen(fname, "wb");

  printf("Will attempt writing\n");

  if (!fwrite(&bootrom_hdr, sizeof(bootrom_hdr_t), 1, file))
    printf("Error writing the binary file!\n");

  printf("Writing header file succesfull.\n");
  bootrom_append_bitstream("fpga.bit", file);
  printf("Writing bit file succesfull.\n");

  fclose(file);

  return 0;
}


