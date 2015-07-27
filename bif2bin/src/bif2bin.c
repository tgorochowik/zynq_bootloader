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


#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "bif.h"
#include "bootrom.h"

int main(int argc, const char *argv[])
{
  FILE *ofile;
  uint32_t ofile_size;
  uint32_t *file_data;
  bif_cfg_t cfg;
  int i;

  init_bif_cfg(&cfg);

  if (argc != 3) {
    printf("Zynq .bin file generator\n");
    printf("(c) 2013-2015 Antmicro Ltd.\n");
    printf("Usage: bif2bin <input_bif_file> <output_bit_file>\n");
    exit(-1);
  }

  parse_bif(argv[1], &cfg);

  for (i = 0; i < cfg.nodes_num; i++) {
    printf("Node: %s", cfg.nodes[i].fname);
    if (cfg.nodes[i].bootloader)
      printf(" (bootloader)\n");
    else
      printf("\n");
    if (cfg.nodes[i].load)
      printf(" load:   %08x\n", cfg.nodes[i].load);
    if (cfg.nodes[i].offset)
      printf(" offset: %08x\n", cfg.nodes[i].offset);
  }

  /* Generate bin file */
  file_data = malloc (sizeof *file_data * 10000000);

  ofile_size = create_boot_image(file_data, &cfg);
  ofile = fopen(argv[2], "wb");

  if (ofile == NULL ){
    printf("Could not open output file: %s\n", argv[2]);
    exit(-1);
  }

  fwrite(file_data, sizeof(uint32_t), ofile_size, ofile);

  fclose(ofile);
  free(file_data);
  deinit_bif_cfg(&cfg);

  printf("All done, quitting\n");
  return 0;
}

