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

/* Other file specific files */
#define FILE_MAGIC_ELF            0x464C457F
#define FILE_ELF_ARCH             0x05
#define FILE_ELF_ARCH_32          0x01
#define FILE_ELF_ARCH_64          0x02

#define FILE_ELF_PHOFF_32         0x1C
#define FILE_ELF_SHOFF_32         0x20
#define FILE_ELF_SHENTSIZE_32     0x2E

#define FILE_ELF_PHOFF_64         0x20
#define FILE_ELF_SHOFF_64         0x28
#define FILE_ELF_SHENTSIZE_64     0x36


#define FILE_MAGIC_XILINXBIT_0    0xf00f0900
#define FILE_MAGIC_XILINXBIT_1    0xf00ff00f

#endif /* BOOTROM_H */
