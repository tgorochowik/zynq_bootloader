/*
 * (c) 2013-2015 Antmicro Ltd <www.antmicro.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gelf.h>
#include <unistd.h>

#include "bif.h"
#include "bootrom.h"

/* This calculates the checksum up to (and including) end_addr */
uint32_t bootrom_calc_checksum(uint32_t *start_addr, uint32_t *end_addr ){
  uint32_t *ptr;
  uint32_t sum;

  sum = 0;
  ptr = start_addr;

  while( ptr <= end_addr){
    sum += *ptr;
    ptr++;
  }

  return ~sum;
}

int bootrom_prepare_header(bootrom_hdr_t *hdr){
  int i = 0;
  for (i = 0; i < sizeof(hdr->interrupt_table); i++) {
    /* TODO this probably can be done better than in loop */
    hdr->interrupt_table[i]=BOOTROM_INT_TABLE_DEFAULT;
  }
  hdr->width_detect = BOOTROM_WIDTH_DETECT;
  memcpy(&(hdr->img_id), BOOTROM_IMG_ID, strlen(BOOTROM_IMG_ID));
  hdr->encryption_status = BOOTROM_ENCRYPTED_NONE;
  /* BootROM does not interpret the field below */
  hdr->user_defined_0 = BOOTROM_USER_0;
  hdr->src_offset = 0x0; /* Will be filled elsewhere */
  hdr->img_len = 0x0; /* Will be filled elsewhere */
  hdr->reserved_0 = BOOTROM_RESERVED_0;
  hdr->start_of_exec = 0x0; /* Will be filled elsewhere */
  hdr->total_img_len = 0x0; /* Will be filled elsewhere */
  hdr->reserved_1 = BOOTROM_RESERVED_1_RL;
  hdr->checksum = bootrom_calc_checksum(&(hdr->width_detect),
                                        &(hdr->width_detect)+ 10);
  memset(hdr->user_defined_1, 0x0, sizeof(hdr->user_defined_1));

  /* TODO Not really sure what those do */
  hdr->user_defined_1[17] = 0x0;
  hdr->user_defined_1[18] = 0x0;
  hdr->user_defined_1[19] = 0x000008c0;
  hdr->user_defined_1[20] = 0x00000c80;

  /* Memory acces ranges - set to full (0x0 - 0xFFFFFFFF range) */
  for (i = 0; i < 256; i++) {
    hdr->reg_init[2*i]   = 0xFFFFFFFF;
    hdr->reg_init[2*i+1] = 0x0;
  }

  memset(hdr->user_defined_2, 0xFF, sizeof(hdr->user_defined_2));

  return 0;
}

char magic_hdr_cmp[] = { 0x00, 0x09, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x00, 0x00, 0x01 };
uint32_t append_bitstream(uint32_t *addr, FILE *bitfile){
  /* TODO cleanup this function */
  uint32_t *dest = addr;

  char magic_hdr[13];
  fread(&magic_hdr, 1, sizeof(magic_hdr), bitfile);
  if (memcmp(&magic_hdr, &magic_hdr_cmp, 13) != 0) {
    fclose(bitfile);
    printf("bit file seems to be incorrect.\n");
    exit(1);
  }
  while (1) {
    char section_hdr[2];
    fread(&section_hdr, 1, sizeof(section_hdr), bitfile);
    if (section_hdr[1] != 0x0) {
      fclose(bitfile);
      printf("bit file seems to have mismatched sections.\n");
      exit(1);
    }

    if (section_hdr[0] == 'e')
      break;

    uint8_t section_size;
    fread(&section_size, 1, sizeof(uint8_t), bitfile);
    char section_data[255];
    fread(&section_data, 1, section_size, bitfile);

    printf("Section '%c' size=%d : data = \"%s\"\n",
           section_hdr[0], section_size, section_data);
  }
  uint32_t bit_size;
  fread(&bit_size, 1, 3, bitfile);
  bit_size = ((bit_size >> 16) & 0xFF) | (bit_size & 0xFF00) | ((bit_size << 16) & 0xFF0000);
  printf("bitstream size is %u\n",bit_size);

  int i;
  char old_val[4];
  char new_val[4];
  for (i = 0; i <= bit_size; i+=sizeof(old_val)) {
    fread(&old_val, 1, sizeof(old_val), bitfile);
    new_val[0] = old_val[3];
    new_val[1] = old_val[2];
    new_val[2] = old_val[1];
    new_val[3] = old_val[0];
    memcpy(dest, new_val, 4);
    dest++;
  }

  return bit_size;
}

/* Returns the offset by which the addr parameter should be moved
 * and partition header info via argument pointer */
uint32_t append_file_to_image(uint32_t *addr,
                              const char *filename,
                              bootrom_partition_hdr_t *part_hdr){
  uint32_t file_header;
  struct stat cfile_stat;
  FILE *cfile;
  Elf *elf;
  int fd_elf;
  size_t elf_hdr_n;
  GElf_Phdr elf_phdr;
  int i;

  uint32_t total_size = 0;

  if(stat(filename, &cfile_stat)) {
    printf("Could not stat file: %s\n", filename);
    exit(-1);
  }
  cfile = fopen(filename, "rb");

  if (cfile==NULL) {
    printf("Could not open file: %s\n", filename);
    exit(-1);
  }

  /* Check file format */
  fread(&file_header, 1, sizeof(file_header), cfile);

  switch(file_header){
  case FILE_MAGIC_ELF:
    /* init elf library */
    if(elf_version(EV_CURRENT) == EV_NONE ){
      printf("ELF library initialization failed\n");
      exit(-1);
    }

    /* open file descriptor used by elf library */
    if (( fd_elf = open(filename, O_RDONLY , 0)) < 0){
      printf("Elf could not open file %s.", filename);
      exit(-1);
    }

    /* init elf */
    if (( elf = elf_begin(fd_elf, ELF_C_READ , NULL )) == NULL ){
        printf("Elf file error\n");
        exit(-1);
    }

    /* make sure it is an elf (despite magic byte check) */
    if(elf_kind(elf) != ELF_K_ELF ){
        printf( "\"%s\" is not an ELF object.", filename);
        exit(-1);
    }

    /* get elf headers count */
    if(elf_getphdrnum(elf, &elf_hdr_n)!= 0){
         printf("Elf file header error.\n");
         exit(-1);
    }

    /* iterate through all headers to find the executable */
    for(i = 0; i < elf_hdr_n; i ++) {
        if(gelf_getphdr(elf, i, &elf_phdr) != &elf_phdr){
            printf("Elf file header error.\n");
            exit(-1);
        }


        /* check if the current one has executable flag set */
        if (elf_phdr.p_flags & PF_X){
          /* this is the one - prepare file for reading */
          fseek(cfile, elf_phdr.p_offset, SEEK_SET);

          /* append the data */
          total_size = fread(addr, 1, elf_phdr.p_filesz, cfile);

          /* set the load and execution address */
          part_hdr->dest_load_addr = elf_phdr.p_vaddr;
          part_hdr->dest_exec_addr = elf_phdr.p_vaddr;

          /* exit loop */
          break;
        }
    }
    /* close the elf file descriptor */
    elf_end(elf);
    close(fd_elf);

    /* set destination device as the only attribute */
    part_hdr->attributes =
      BOOTROM_PART_ATTR_DEST_DEV_PS << BOOTROM_PART_ATTR_DEST_DEV_OFF;

    break;
  case FILE_MAGIC_XILINXBIT_0:
    /* Xilinx header is 64b, check the other half */
    fread(&file_header, 1, sizeof(file_header), cfile);
    if (file_header != FILE_MAGIC_XILINXBIT_1){
      printf("Corrupted bit file: %s\n.", filename);
      exit(-1);
    }

    /* It matches, append it to the image */
    fseek(cfile, 0, SEEK_SET);
    total_size = append_bitstream(addr, cfile);

    /* set destination device as the only attribute */
    part_hdr->attributes =
      BOOTROM_PART_ATTR_DEST_DEV_PL << BOOTROM_PART_ATTR_DEST_DEV_OFF;

    /* no execution address for bitstream */
    part_hdr->dest_load_addr = 0x0;
    part_hdr->dest_exec_addr = 0x0;

    break;
  default: /* not supported - quit */
    printf("File format not recognized: %s.\n", filename);
    exit(1);
  };

  /* remove trailing zeroes */
  while ( *(addr+total_size) == 0x0 ){
    total_size--;
  }

  /* The output image needs to use the actual value +1B
   * for some reason */
  part_hdr->pd_word_len = total_size + 1;
  part_hdr->ed_word_len = total_size + 1;
  part_hdr->total_word_len = total_size + 1;

  /* Section count is always set to 1 */
  part_hdr->section_count = 0x1;

  /* Fill remaining fields that don't seem to be used */
  part_hdr->checksum_off = 0x0;
  part_hdr->cert_off = 0x0;
  memset(part_hdr->reserved, 0x00, sizeof(part_hdr->reserved));

  /* Add 0xFF padding */
  while (total_size % (BOOTROM_IMG_PADDING_SIZE / sizeof(uint32_t))){
    total_size++;
    memset(addr+total_size, 0xFF, sizeof(uint32_t));
  }

  /* Close the file */
  fclose(cfile);

  return total_size;
}

/* Returns total size of the created image */
uint32_t create_boot_image(uint32_t *img_ptr, bif_cfg_t *bif_cfg){
  /* declare variables */
  bootrom_hdr_t hdr;

  uint32_t *coff = img_ptr; /* current offset/ptr */
  uint32_t *poff; /* current partiton offset */
  uint32_t *hoff; /* partition header table offset */
  uint16_t i, j;
  int img_term_n = 0;
  uint8_t img_name[BOOTROM_IMG_MAX_NAME_LEN];

  bootrom_partition_hdr_t part_hdr[BIF_MAX_NODES_NUM];
  bootrom_img_hdr_t img_hdr[BIF_MAX_NODES_NUM];
  uint32_t img_size;

  /* Prepare header of the image */
  bootrom_prepare_header(&hdr);

  /* Move the offset to reserve the space for headers */
  poff = (BOOTROM_IMG_HDR_OFF) / sizeof(uint32_t) + img_ptr;
  coff = (BOOTROM_BINS_OFF) / sizeof(uint32_t) + img_ptr;

  /* Iterate through the images and write them */
  for (i = 0; i < bif_cfg->nodes_num; i++) {

    /* Append file content to memory */
    img_size = append_file_to_image(coff,
                                    bif_cfg->nodes[i].fname,
                                    &(part_hdr[i]));

    /* Check if dealing with bootloader */
    if (bif_cfg->nodes[i].bootloader){
      /* If so - update the header to point at the correct bootloader */
      hdr.src_offset = (coff - img_ptr) * sizeof(uint32_t);

      /* Image length needs to be in words not bytes */
      hdr.img_len = part_hdr[i].pd_word_len * sizeof(uint32_t);
      hdr.total_img_len = hdr.img_len;

      /* Recalculate the checksum */
      hdr.checksum = bootrom_calc_checksum(&(hdr.width_detect),
                                           &(hdr.reserved_1));
    }

    /* Fill the offset */
    part_hdr[i].data_off = (coff - img_ptr);


    /* Update the offset, skip padding for the last image */
    if (i == bif_cfg->nodes_num - 1){
      coff += part_hdr[i].pd_word_len;
    } else {
      coff += img_size;
    }

    /* Create image headers for all of them */
    img_hdr[i].part_count = 0x0;

    /* filling this field as a helper */
    img_hdr[i].name_len = strlen(bif_cfg->nodes[i].fname);

    /* Fill the name variable with zeroes */
    for (j = 0; j < BOOTROM_IMG_MAX_NAME_LEN; j++) {
      img_name[j] = 0x0;
    }
    /* Temporarily read the name */
    memcpy(img_name, bif_cfg->nodes[i].fname, img_hdr[i].name_len);

    /* Make the name len be divisible by 4 */
    while(img_hdr[i].name_len % 4)
      img_hdr[i].name_len++;

    /* The name is packed in big-endian order. To reconstruct
     * the string, unpack 4 bytes at a time, reverse
     * the order, and concatenate. */
    for (j = 0; j < img_hdr[i].name_len; j+=4) {
      img_hdr[i].name[j+0] = img_name[j+3];
      img_hdr[i].name[j+1] = img_name[j+2];
      img_hdr[i].name[j+2] = img_name[j+1];
      img_hdr[i].name[j+3] = img_name[j+0];
    }

    /* Add string terminator, the documentation says that this has
     * to be 32b long, however the bootgen binary makes it 64b if
     * the length of the name is greater than 8 and that's what
     * we're going to do here */
    if ( img_hdr[i].name_len > 8 ){
      img_term_n = 1;
    } else {
      img_term_n = 2;
    }
    memset(&(img_hdr[i].name[img_hdr[i].name_len]),
           0x00, img_term_n * sizeof(uint32_t));

    /* Fill the rest with 0xFF padding */
    for (j = img_hdr[i].name_len + img_term_n * sizeof(uint32_t);
         j < BOOTROM_IMG_MAX_NAME_LEN; j++) {
      img_hdr[i].name[j] = 0xFF;
    }

    /* Name length is not really the length of the name.
     * According to the documentation it is the value of the
     * actual partition count, however the bootgen binary
     * always sets this field to 1. */
    img_hdr[i].name_len = 0x1;
  }

  /* Prepare image header table */
  bootrom_img_hdr_tab_t img_hdr_tab;

  img_hdr_tab.version = BOOTROM_IMG_VERSION;
  img_hdr_tab.hdrs_count = i;
  img_hdr_tab.part_hdr_off = 0x0; /* filled below */
  img_hdr_tab.part_img_hdr_off = 0x0; /* filled below */
  img_hdr_tab.auth_hdr_off = 0x0; /* auth not implemented */

  /* The data will be copied to the reserved space later
   * when we know all the required offsets,
   * save the pointer for that */
  hoff = poff;

  /* Add 0xFF padding */
  uint32_t img_hdr_size = 0;
  img_hdr_size = sizeof(img_hdr_tab) / sizeof(uint32_t);
  while (img_hdr_size % (BOOTROM_IMG_PADDING_SIZE / sizeof(uint32_t))){
    memset(poff + img_hdr_size, 0xFF, sizeof(uint32_t));
    img_hdr_size++;
  }

  poff += img_hdr_size;

  for (i = 0; i < img_hdr_tab.hdrs_count; i++) {
    /* Write 0xFF padding first - will use offset info later */
    img_hdr_size = sizeof(img_hdr[i]) / sizeof(uint32_t);
    while (img_hdr_size % (BOOTROM_IMG_PADDING_SIZE / sizeof(uint32_t))){
      memset(poff + img_hdr_size, 0xFF, sizeof(uint32_t));
      img_hdr_size++;
    }

    /* calculate the next img hdr offsets */
    if (i + 1 == img_hdr_tab.hdrs_count) {
      img_hdr[i].next_img_off = 0x0;
    } else {
      img_hdr[i].next_img_off = poff + img_hdr_size - img_ptr;
    }

    img_hdr[i].part_hdr_off =
      (BOOTROM_PART_HDR_OFF / sizeof(uint32_t)) +
      (i * sizeof(bootrom_partition_hdr_t) / sizeof(uint32_t));

    /* Write the actual img_hdr data */
    memcpy(poff, &(img_hdr[i]), sizeof(img_hdr[i]));

    /* Keep the offset for later use */
    part_hdr[i].img_hdr_off = (poff - img_ptr);

    /* Calculate the checksum */
    part_hdr[i].checksum =
      bootrom_calc_checksum(&(part_hdr[i].pd_word_len),
                            &part_hdr[i].reserved[3]);

    if (i == 0){
      img_hdr_tab.part_img_hdr_off = (poff - img_ptr);
    }

    poff += img_hdr_size;
  }

  /* Fill the partition header offset in img header */
  img_hdr_tab.part_hdr_off = BOOTROM_PART_HDR_OFF / sizeof(uint32_t);

  /* Copy the image header as all the fields should be filled by now */
  memcpy(hoff, &(img_hdr_tab), sizeof(img_hdr_tab));

  /* Add 0xFF padding until BOOTROM_PART_HDR_OFF */
  while ( poff - img_ptr < BOOTROM_PART_HDR_OFF / sizeof(uint32_t) ){
    memset(poff, 0xFF, sizeof(uint32_t));
    poff++;
  }

  /* Write the partition headers */
  for (i = 0; i < img_hdr_tab.hdrs_count; i++) {
    memcpy(poff, &(part_hdr[i]), sizeof(part_hdr[i]));

    /* Partition header is aligned, so no padding needed */
    poff += sizeof(part_hdr[i]) / sizeof(uint32_t);
  }

  /* Add 0x00 padding until BOOTROM_PART_HDR_END_OFF */
  while ( poff - img_ptr < BOOTROM_PART_HDR_END_OFF / sizeof(uint32_t) ){
    memset(poff, 0x00, sizeof(uint32_t));
    poff++;
  }

  /* Add 0xFF padding until BOOTROM_BINS_OFF */
  while ( poff - img_ptr < BOOTROM_BINS_OFF / sizeof(uint32_t) ){
    memset(poff, 0xFF, sizeof(uint32_t));
    poff++;
  }

  /* Finally write the header to the image */
  memcpy(img_ptr, &(hdr), sizeof(hdr));

  return coff - img_ptr;
}
