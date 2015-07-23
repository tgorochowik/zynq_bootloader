#ifndef BOOTROM_H
#define BOOTROM_H

#include "bif_parser.h"

uint32_t create_boot_image(uint32_t *img_ptr, bif_cfg_t *bif_cfg);

/* BootROM Header based on ug585 */
typedef struct bootrom_hdr_t {
  uint32_t interrupt_table[8];
  uint32_t width_detect;
  uint32_t img_id;
  uint32_t encryption_status;
  union {
    uint32_t user_defined_0;
    uint32_t fsbl_defined_0;
  };
  uint32_t src_offset;
  uint32_t img_len;
  uint32_t reserved_0; /* set to 0 */
  uint32_t start_of_exec;
  uint32_t total_img_len;
  uint32_t reserved_1;
  uint32_t checksum;
  union {
    uint32_t user_defined_1[21];
    uint32_t fsbl_defined_1[21];
  };
  uint32_t reg_init[512];
  union {
    uint32_t user_defined_2[8];
    uint32_t fsbl_defined_2[8];
  };
} bootrom_hdr_t;

/* BootROM image header table based on ug821 */
typedef struct bootrom_img_hdr_tab_t{
  uint32_t version;
  uint32_t hdrs_count;
  uint32_t part_hdr_off; /* word offset to the partition header */
  uint32_t img_hdr_off; /* word offset to first image header */
  uint32_t auth_hdr_off; /* word offset to header authentication */
  uint32_t padding;
} bootrom_img_hdr_tab_t;

/* BootROM partition header based on ug821 */
/* All offsets are relative to the start of the boot image */
typedef struct bootrom_partition_hdr_t{
  uint32_t pd_word_len; /* encrypted partiton data length */
  uint32_t ed_word_len; /* unecrypted data length */
  uint32_t total_word_len; /* total encrypted,padding,expansion, auth length */

  uint32_t dest_load_addr; /* RAM addr where the part will be loaded */
  uint32_t dest_exec_addr;

  uint32_t data_off;
  uint32_t attributes;
  uint32_t section_count;

  uint32_t checksum_off;
  uint32_t img_hdr_off;
  uint32_t cert_off;

  uint32_t reserved[4]; /* set to 0 */

  uint32_t checksum;
} bootrom_partition_hdr_t;

/* BootROM image header based on ug821 */
typedef struct bootrom_img_hdr_t{
  uint32_t next_img_off; /* 0 if last */
  uint32_t part_hdr_off;
  uint32_t part_count; /* set to 0 */
  uint32_t name_len;
  uint32_t name;
  uint32_t terminator; /* set to 0 */
} bootrom_img_hdr_t;

/* attributes of the bootrom partition header */
#define BOOTROM_PART_ATTR_OWNER_OFF      16
#define BOOTROM_PART_ATTR_OWNER_MASK     (3 << BOOTROM_PART_ATTR_OWNER_OFF)
#define BOOTROM_PART_ATTR_OWNER_FSBL     0
#define BOOTROM_PART_ATTR_OWNER_UBOOT    1

#define BOOTROM_PART_ATTR_RSA_USED_OFF   15
#define BOOTROM_PART_ATTR_RSA_USED_MASK  (1 << BOOTROM_PATR_ATTR_RSA_USED_OFF)
#define BOOTROM_PART_ATTR_RSA_USED       1
#define BOOTROM_PART_ATTR_RSA_NOT_USED   0

#define BOOTROM_PART_ATTR_DEST_DEV_OFF   4
#define BOOTROM_PART_ATTR_DEST_DEV_MASK  (7 << BOOTRM_PART_ATTR_DEST_DEV_OFF)
#define BOOTROM_PART_ATTR_DEST_DEV_NONE  0
#define BOOTROM_PART_ATTR_DEST_DEV_PS    1
#define BOOTROM_PART_ATTR_DEST_DEV_PL    2
#define BOOTROM_PART_ATTR_DEST_DEV_INT   3

/* values taken from boot.bin generated with bootgen */
#define BOOTROM_INT_TABLE_DEFAULT 0xEAFFFFFE
#define BOOTROM_USER_0            0x01010000 /* probably not needed */
#define BOOTROM_RESERVED_1_RL     0x00000001 /* MUST be set to 0 but is not */

/* values from the documentation */
#define BOOTROM_WIDTH_DETECT      0xAA995566
#define BOOTROM_IMG_ID            "XNLX"
#define BOOTROM_ENCRYPTED_EFUSE   0xA5C3C5A3
#define BOOTROM_ENCRYPTED_RAMKEY  0x3A5C3C5A
#define BOOTROM_ENCRYPTED_NONE    0x00000000 /* anything but efuse, ramkey*/
#define BOOTROM_MIN_SRC_OFFSET    0x000008C0
#define BOOTROM_RESERVED_0        0x00000000 /* MUST be set to 0 */
#define BOOTROM_RESERVED_1        0x00000000 /* MUST be set to 0 */

#define BOOTROM_IMG_VERSION       0x01020000

/* Other file specific files */
#define FILE_MAGIC_ELF            0x464C457F

#define FILE_MAGIC_XILINXBIT_0    0xf00f0900
#define FILE_MAGIC_XILINXBIT_1    0xf00ff00f

#endif /* BOOTROM_H */
