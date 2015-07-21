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
#include "bif_parser.h"

int main(int argc, const char *argv[])
{
  bif_cfg_t cfg;
  init_bif_cfg(&cfg);

  /* TODO change to argv[1] */
  parse_bif("boot.bif", &cfg);

  int i;
  for (i = 0; i < cfg.nodes_num; i++) {
    printf("Node: %s, is bootloader? %d\n", cfg.nodes[i].fname, cfg.nodes[i].bootloader);
  }

  deinit_bif_cfg(&cfg);
  return 0;
}

