#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "bif_parser.h"
#include "bootrom.h"

#define rpt() printf("%s:%d\n", __func__, __LINE__)

uint32_t bootrom_calc_checksum(uint32_t *start_addr, uint32_t *end_addr ){
  uint32_t *ptr;
  uint32_t sum;

  sum = 0;
  ptr = start_addr;

  while( ptr < end_addr){
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
  /* TODO the param below must be greater than 0x0 and less than 0x30000 */
  hdr->start_of_exec = 0x0;
  hdr->total_img_len = 0x0; /* Will be filled elsewhere */
  hdr->reserved_1 = BOOTROM_RESERVED_1_RL;
  hdr->checksum = bootrom_calc_checksum(&(hdr->width_detect),
                                        &(hdr->width_detect)+ 10);
  memset(hdr->user_defined_1, 0x0, sizeof(hdr->user_defined_1));
  memset(hdr->reg_init, 0xFF, sizeof(hdr->reg_init));
  memset(hdr->user_defined_2, 0x0, sizeof(hdr->user_defined_2));

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
  for (i = 0; i < bit_size; i+=sizeof(old_val)) {
    int read = fread(&old_val, 1, sizeof(old_val), bitfile);
    new_val[0] = old_val[3];
    new_val[1] = old_val[2];
    new_val[2] = old_val[1];
    new_val[3] = old_val[0];
    memcpy(dest, new_val, 4);
    dest++;
  }

  return bit_size;
}

/* Returns appended image lenght */
uint32_t append_file_to_image(uint32_t *addr, const char *filename){
  uint32_t file_header;
  struct stat cfile_stat;
  FILE *cfile;

  uint8_t elf_arch;
  uint32_t elf_phoff;
  uint32_t elf_shoff;
  uint16_t elf_shentsize;

  uint32_t total_size;

  /* TODO sanity checks */
  stat(filename, &cfile_stat);
  cfile = fopen(filename, "rb");

  /* Check file format */
  fread(&file_header, 1, sizeof(file_header), cfile);
  printf("File header: %08x\n", file_header);

  switch(file_header){
  case FILE_MAGIC_ELF:
    /* Check wheter it is 32b or 64b elf */
    fseek(cfile, 0, FILE_ELF_ARCH);
    fread(&elf_arch, 1, sizeof(elf_arch), cfile);

    /* Determine the program offset */
    switch(elf_arch){
      case FILE_ELF_ARCH_32:
        /* Read the actual program offset */
        fseek(cfile, FILE_ELF_PHOFF_32, SEEK_SET);
        fread(&elf_phoff, 1, sizeof(uint32_t), cfile);

        /* Read the section header table offset */
        fseek(cfile, FILE_ELF_SHOFF_32, SEEK_SET);
        fread(&elf_shoff, 1, sizeof(uint32_t), cfile);
        printf("SHOFF %08x\n", elf_shoff);

        /* Read the section header table size */
        fseek(cfile, FILE_ELF_SHENTSIZE_32, SEEK_SET);
        fread(&elf_shentsize, 1, sizeof(uint16_t), cfile);
        printf("SHENTSIZE %08x\n", elf_shentsize);
        break;
      case FILE_ELF_ARCH_64:
        printf("64b ELF architecture not supported yet.\n");
        exit(1);
      default:
        printf("ELF architecture not recognized.\n");
        exit(1);
    }

    /* Go to the program */
    fseek(cfile, elf_phoff * 4, SEEK_SET);

    /* Header table is the last section of elf file,
     * we use its offset and its size to calculate the
     * total length of the object inside elf.
     *
     * This is required to drop the useless trailing data
     */
    total_size = (elf_shoff + elf_shentsize - elf_phoff);
    /* TODO verify this as it doesnt seem to work correctly */
    /* Append the file to the image */
    fread(addr, 1, total_size, cfile);

    break;
  case FILE_MAGIC_XILINXBIT_0:
    /* Xilinx header is 64b, check the other half */
    fread(&file_header, 1, sizeof(file_header), cfile);
    if (file_header != FILE_MAGIC_XILINXBIT_1)
      exit(1); /* TODO better exit */

    /* It matches, append it to the image */
    fseek(cfile, 0, SEEK_SET);
    total_size = append_bitstream(addr, cfile);
    break;
  default: /* not supported - quit */
    exit(1);
  };

  /* Add 0xFF padding */
  while (total_size % 64){
    total_size++;
    memset(addr+total_size, 0xFF, 1);
  }

  /* TODO take other node parameters into account */

  /* Close the file */
  fclose(cfile);

  return total_size;
}

/* Returns total size of the created image */
uint32_t create_boot_image(uint32_t *img_ptr, bif_cfg_t *bif_cfg){
  /* declare variables */
  bootrom_hdr_t hdr;
  bif_node_t bootloader_node;

  uint32_t *coff = img_ptr; /* current offset/ptr */
  uint32_t img_size;
  uint16_t i;

  /* Prepare header of the image */
  bootrom_prepare_header(&hdr);

  /* move the offset to reserve the space for header */
  coff += sizeof(hdr);

  /* Look for the bootloader */
  for (i = 0; i < bif_cfg->nodes_num; i++) {
    if (bif_cfg->nodes[i].bootloader){
      bootloader_node = bif_cfg->nodes[i];
      /* Read the bootloader from disk */
      img_size = append_file_to_image(coff, bootloader_node.fname);

      /* Update the header to point at the correct bootloader */
      hdr.src_offset = (coff - img_ptr)*4;
      /* TODO check if size should be in words too (times 4?) */
      hdr.img_len = img_size;
      hdr.total_img_len = img_size;
      /* Recalculate the checksum */
      hdr.checksum = bootrom_calc_checksum(&(hdr.width_detect),
                                           &(hdr.width_detect)+ 10);

      /* Update the offset */
      coff += img_size;
      break;
    }
  }

  /* Finally write the header to the image */
  memcpy(img_ptr, &(hdr), sizeof(hdr));

  /* Iterate through the rest of images and write them */
  for (i = 0; i < bif_cfg->nodes_num; i++) {
    /* skip bootloader this time */
    if (!bif_cfg->nodes[i].bootloader){
      img_size = append_file_to_image(coff, bif_cfg->nodes[i].fname);

      /* Update the offset */
      coff += img_size;
    }
  }

  return coff - img_ptr;
}
