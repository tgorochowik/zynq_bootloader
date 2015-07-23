#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gelf.h>
#include <unistd.h>

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
  Elf *elf;
  int fd_elf;
  size_t elf_hdr_n;
  GElf_Phdr elf_phdr;
  int i;

  uint32_t total_size = 0;

  /* TODO sanity checks */
  stat(filename, &cfile_stat);
  cfile = fopen(filename, "rb");

  /* Check file format */
  fread(&file_header, 1, sizeof(file_header), cfile);
  printf("File header: %08x\n", file_header);

  switch(file_header){
  case FILE_MAGIC_ELF:
    /* init elf library */
    if(elf_version(EV_CURRENT) == EV_NONE ){
      printf("ELF library initialization failed\n");
      exit(1);
    }

    /* open file descriptor used by elf library */
    if (( fd_elf = open(filename, O_RDONLY , 0)) < 0){
      printf("Elf could not open file %s.", filename);
      exit(1);
    }

    /* init elf */
    if (( elf = elf_begin(fd_elf, ELF_C_READ , NULL )) == NULL ){
        printf("elf_begin() failed : %s.");
        exit(1);
    }

    /* make sure it is an elf (despite magic byte check */
    if(elf_kind(elf) != ELF_K_ELF ){
        printf( "\"%s\" is not an ELF object.", filename);
        exit(1);
    }

    /* get elf headers count */
    if(elf_getphdrnum(elf, &elf_hdr_n)!= 0){
         printf("elf_getphdrnum() failed.");
         exit(1);
    }

    /* iterate through all headers to find the executable */
    for(i = 0; i < elf_hdr_n; i ++) {
        if(gelf_getphdr(elf, i, &elf_phdr) != &elf_phdr){
            printf("gelf_getphdr() failed.\n");
            exit(1);
        }

        /* check if the current one has executable flag set */
        if (elf_phdr.p_flags & PF_X){
          /* this is the one - prepare file for reading */
          fseek(cfile, elf_phdr.p_offset, SEEK_SET);

          /* append the data */
          total_size = fread(addr, 1, elf_phdr.p_filesz, cfile);

          /* exit loop */
          break;
        }
    }
    /* close the elf file descriptor */
    elf_end(elf);
    close(fd_elf);
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
    memset(addr+total_size, 0xFF, sizeof(uint32_t));
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
  uint32_t *poff; /* partition table offset */
  uint32_t img_size;
  uint16_t i, j;
  uint8_t img_name[BOOTROM_IMG_MAX_NAME_LEN];

  int img_term_n = 0;

  /* Prepare header of the image */
  bootrom_prepare_header(&hdr);

  /* move the offset to reserve the space for headers */
  poff = (0x000008c0)/4 + img_ptr;
  coff = (0x00001700)/4 + img_ptr;

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

  bootrom_img_hdr_t img_hdr[10]; /* TODO make dynamic */
  /* Iterate through the rest of images and write them */
  for (i = 0; i < bif_cfg->nodes_num; i++) {
    /* skip bootloader this time */
    if (!bif_cfg->nodes[i].bootloader){
      img_size = append_file_to_image(coff, bif_cfg->nodes[i].fname);

      /* Update the offset */
      coff += img_size;
    }

    /* Create image headers for all of them */
    img_hdr[i].next_img_off = 0xAABBCCDD;
    img_hdr[i].part_hdr_off = 0x55775577;
    img_hdr[i].part_count = 0x0;
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
     * to be 32b long, however the bootgen binary makes it 64b
     * and that's what we're going to do here */
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

  }

  /* Prepare image header table */
  bootrom_img_hdr_tab_t img_hdr_tab;

  img_hdr_tab.version = BOOTROM_IMG_VERSION;
  img_hdr_tab.hdrs_count = i;
  img_hdr_tab.part_hdr_off = 0x0; /* TODO fill */
  img_hdr_tab.img_hdr_off = 0x0; /* TODO fill */
  img_hdr_tab.auth_hdr_off = 0x0; /* TODO fill */

  /* Copy data to the image header */
  memcpy(poff, &(img_hdr_tab), sizeof(img_hdr_tab));

  /* Add 0xFF padding */
  uint32_t img_hdr_size = 0;
  img_hdr_size = sizeof(img_hdr_tab) / sizeof(uint32_t);
  while (img_hdr_size % (64 / sizeof(uint32_t))){
    memset(poff + img_hdr_size, 0xFF, sizeof(uint32_t));
    img_hdr_size++;
  }

  poff += img_hdr_size;

  /* Copy image headers data to the image header */
  for (i = 0; i < img_hdr_tab.hdrs_count; i++) {
    printf("JESTEM TU: %08x\n", (poff - img_ptr)/4);
    memcpy(poff, &(img_hdr[i]), sizeof(img_hdr[i]));

    /* Add 0xFF padding */
    img_hdr_size = sizeof(img_hdr[i]) / sizeof(uint32_t);
    while (img_hdr_size % (64 / sizeof(uint32_t))){
      memset(poff + img_hdr_size, 0xFF, sizeof(uint32_t));
      img_hdr_size++;
    }
    poff += img_hdr_size;
  }

  /* Finally write the header to the image */
  memcpy(img_ptr, &(hdr), sizeof(hdr));

  return coff - img_ptr;
}
