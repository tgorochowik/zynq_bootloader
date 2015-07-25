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
#define rpt() printf("%s:%d \n", __func__, __LINE__)

int main(int argc, const char *argv[])
{
  bif_cfg_t cfg;
  init_bif_cfg(&cfg);

  /* TODO change to argv[1] */
  parse_bif("boot.bif", &cfg);

  int i;
  for (i = 0; i < cfg.nodes_num; i++) {
    printf("Node: %s\n", cfg.nodes[i].fname);
    printf(" load:   %08x\n", cfg.nodes[i].load);
    printf(" offset: %08x\n", cfg.nodes[i].offset);
  }

  /* Generate bin file */
  FILE *ofile;
  uint32_t ofile_size;
  uint32_t *file_data;
  file_data = malloc (sizeof *file_data * 10000000);

  ofile_size = create_boot_image(file_data, &cfg);
  /* TODO change to argv[2] */
  ofile = fopen("boot.bin", "wb");

  if (ofile == NULL ){
    printf("Could not open output file\n");
  }

  printf("Got %dB(=%dkB=%dMB)\n", ofile_size, ofile_size / 1024, ofile_size / 1024 / 1024);

  fwrite(file_data, sizeof(uint32_t), ofile_size, ofile);

  fclose(ofile);
  free(file_data);
  deinit_bif_cfg(&cfg);

  printf("All done, quitting\n");
  return 0;
}

