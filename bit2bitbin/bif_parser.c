#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pcre.h>
#include "bif_parser.h"

int init_bif_cfg(bif_cfg_t *cfg){
  cfg->nodes_num = 0;
  cfg->nodes_avail = 100;
  /* TODO make it dynamic */

  return 0;
}

int deinit_bif_cfg(bif_cfg_t *cfg){
  cfg->nodes_num = 0;
  cfg->nodes_avail = 0;
  /* TODO make it dynamic */

  return 0;
}

int parse_bif(const char* fname, bif_cfg_t *cfg){
  FILE *bif_file;
  int bif_size;
  char *bif_content;
  pcre *re;

  if (!(bif_file = fopen(fname, "r"))){
    printf("Error opening file\n");
    exit(1);
  }

  /* Find file size */
  fseek(bif_file, 0, SEEK_END);
  bif_size = ftell(bif_file);
  fseek(bif_file, 0, SEEK_SET);

  /* allocate memory and read the whole file */
  bif_content = malloc(bif_size + 1);
  fread(bif_content, bif_size, 1, bif_file);

  /* Find the beginning and the end */
  char *beg;
  char *end;

  beg = strchr(bif_content, '{');
  end = strchr(bif_content, '}');

  /* extract the actual config */
  char *bif_cfg = malloc(sizeof *bif_cfg * (end-beg));
  memcpy(bif_cfg, beg+1, end-beg-1);

  //printf("\n\nInput:\n%s\n===============\n\n", bif_cfg);

  /* First extract the name and the parameter group if exists */
  char *pcre_regex = "(\\[(.*)\\])?([a-zA-Z.-]+)";
  const char *pcre_err;
  int pcre_err_off;
  re = pcre_compile(pcre_regex, 0, &pcre_err, &pcre_err_off, NULL);

  if (re == NULL){
    printf("Could not compile regex %s:%s", pcre_regex, pcre_err);
    exit(1);
  }

  /* Attributes regex */
  pcre *re_attr;
  char *pcre_attr_regex = "(([a-zA-Z0-9]+)=([a-zA-Z0-9]+))+";      //"(,?.*(=.*)?)+";
  re_attr = pcre_compile(pcre_attr_regex, 0, &pcre_err, &pcre_err_off, NULL);

  /* TODO CLEANUP CLEANUP CLEANUP */
  int ret;
  int ovec[30];
  int soff=0;
  int iovec[30];
  int isoff=0;
  char cattr[500];
  char pattr_n[500];
  char pattr_v[500];

  bif_node_t node;

  do {
    /* TODO better cleaning of the node */
    strcpy(node.fname, "");
    node.bootloader = 0;

    ret = pcre_exec(re, NULL, bif_cfg, strlen(bif_cfg), soff, 0, ovec, 30);
    if (ret < 4){
      free(bif_content);
      fclose(bif_file);
      return -1;
    }

    /* print matches */
    //printf("Filename: %.*s\n", ovec[7] - ovec[6], bif_cfg + ovec[6]);

    /* parse attributes */
    memcpy(cattr, bif_cfg + ovec[4], ovec[5] - ovec[4]);
    cattr[ovec[5] - ovec[4]] = '\0';

    isoff = 0;
    if (re_attr == NULL){
      printf("Could not compile regex %s:%s", pcre_attr_regex, pcre_err);
      exit(1);
    }
    int aret = 0;
    do {
      aret = pcre_exec(re_attr, NULL, cattr, strlen(cattr), isoff, 0, iovec, 30);
      if (aret < 1 && isoff == 0 && strlen(cattr) > 0){
        //printf("attr: %s\n", cattr);
        bif_node_set_attr(&node, cattr, NULL);
        break;
      }
      int i;
      for (i = 2; i < aret; i+=3) {
        //printf("attr: %.*s", iovec[2*i+1] - iovec[2*i], cattr + iovec[i*2]);

        memcpy(pattr_n, cattr + iovec[i*2], iovec[2*i+1] - iovec[2*i]);
        pattr_n[iovec[2*i+1] - iovec[2*i]] = '\0';

        //printf(" equal: %.*s\n", iovec[2*(i+1)+1] - iovec[2*(i+1)], cattr + iovec[(i+1)*2]);

        memcpy(pattr_v, cattr + iovec[(i+1)*2], iovec[2*(i+1)+1] - iovec[2*(i+1)]);
        pattr_v[iovec[2*(i+1)+1] - iovec[2*(i+1)]] = '\0';

        bif_node_set_attr(&node, pattr_n, pattr_v);
      }
      isoff = iovec[1];
    } while (aret > 3);

    //printf("\n");

    /* set filename of the node */
    memcpy(node.fname, bif_cfg + ovec[6], ovec[7] - ovec[6]);
    node.fname[ovec[7] - ovec[6]] = '\0';

    bif_cfg_add_node(cfg, &node);

    soff = ovec[1];
  } while (ret >3);


  free(bif_content);
  fclose(bif_file);

  return 0;
}

int bif_node_set_attr(bif_node_t *node, char *attr_name, char *value){
  /* TODO add defines!! */
  if (strcmp(attr_name, "bootloader") == 0){
    node->bootloader = 0xFF;
    return 0;
  }

  printf("Warning: node attribute not supported: \"%s\", skipping \n", attr_name);
  return -1;
}

int bif_cfg_add_node(bif_cfg_t *cfg, bif_node_t *node){

  /* TODO check node availability!! */
  cfg->nodes[cfg->nodes_num] = *node;

  (cfg->nodes_num)++;
  return 0;
}
