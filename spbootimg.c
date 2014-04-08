/* mkbootimg/spbootimg.c
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*
** Modified from tools/mkbootimg/mkbootimg.c by
** Bob Clary <bclary@mozilla.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define MAXNAMELEN 255

/* #include "mincrypt/sha.h" */
#include "bootimg.h"

unsigned pages(unsigned page_size, unsigned chunk_size)
{
  return (chunk_size + page_size - 1) / page_size;
}

int write_chunk(int bootimg_fd, char *bootimg_fn, unsigned size, char *output_type, unsigned offset)
{
  char *msg;
  char output_fn[MAXNAMELEN];
  char *data;
  int fd;
  unsigned count;

  sprintf(output_fn, "%s-%s", bootimg_fn, output_type);
  data = (char *)malloc(size);
  if (data == 0) {
    msg = "Unable to allocate memory";
    goto fail_chunk;
  }
  count = lseek(bootimg_fd, offset, SEEK_SET);
  if (count != offset) {
    msg = "Unable to seek";
    goto fail_chunk;
  }
  count = read(bootimg_fd, data, size);
  if (count != size) {
    msg = "Unable to read chunk";
    goto fail_chunk;
  }
  fd = open(output_fn, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if(fd < 0) {
    msg = "Unable to open output";
    goto fail_chunk;
  }
  count = write(fd, data, size);
  close(fd);
  if(count != size) {
    msg = "Could not write chunk";
    goto fail_chunk;
  }
  return 0;

 fail_chunk:
  close(bootimg_fd);
  fprintf(stderr,"error: %s %s %s %s\n",
          msg,
          bootimg_fn,
          output_type,
          strerror(errno));
  exit(1);

}

int usage(void)
{
  fprintf(stderr,"usage: spbootimg\n"
          "       -i|--input <filename>\n"
          );
  return 1;
}

int main(int argc, char **argv)
{
  boot_img_hdr hdr;

  int bootimg_fd;

  void *boot_data = 0;
  unsigned boot_size = 0;
  char boot_magic[BOOT_MAGIC_SIZE+1];
  char kernel_fn[MAXNAMELEN];
  void *kernel_data = 0;
  char ramdisk_fn[MAXNAMELEN];
  void *ramdisk_data = 0;
  char second_fn[MAXNAMELEN];
  void *second_data = 0;
  char header_fn[MAXNAMELEN];
  char *cmdline = "";
  char *bootimg_fn = 0;
  char *board = "";
  unsigned pagesize = 2048;
  int fd;
  /*SHA_CTX ctx;*/
  /*uint8_t* sha;*/
  unsigned long i;
  unsigned count;
  char *msg1 = "";
  char *msg2 = "";
  int offset;
  FILE *fp;
  unsigned kernel_offset =  0x00008000;
  unsigned ramdisk_offset = 0x01000000;
  unsigned second_offset =  0x00F00000;
  unsigned tags_offset =    0x00000100;
  unsigned kernel_base = 0;
  unsigned ramdisk_base = 0;
  unsigned second_base = 0;
  unsigned tags_base = 0;

  argc--;
  argv++;

  while(argc > 0){
    char *arg = argv[0];
    char *val = argv[1];
    if(argc < 2) {
      return usage();
    }
    argc -= 2;
    argv += 2;
    if(!strcmp(arg, "--input") || !strcmp(arg, "-i")) {
      bootimg_fn = val;
    } else {
      return usage();
    }
  }

  if(bootimg_fn == 0) {
    fprintf(stderr,"error: no input filename specified\n");
    return usage();
  }

  sprintf(kernel_fn, "%s-kernel", bootimg_fn);
  bootimg_fd = open(bootimg_fn, O_RDONLY);
  if (bootimg_fd < 0) {
    fprintf(stderr,"error: could not open %s\n", bootimg_fn);
    return 1;
  }

  count = read(bootimg_fd, (void *)&hdr, sizeof(hdr));
  if (count != sizeof(hdr)) {
    msg1 = "could not read hdr from input";
    goto fail;
  }
  for (i = 0; i < BOOT_MAGIC_SIZE; i++) {
    boot_magic[i] = hdr.magic[i];
  }
  boot_magic[BOOT_MAGIC_SIZE] = '\0';
  if (strcmp(BOOT_MAGIC, boot_magic)) {
    fprintf(stderr, "error: %s boot image file's magic value %s does not match %s\n",
            bootimg_fn, boot_magic, BOOT_MAGIC);
    goto fail;
  }
  if (hdr.page_size != 2048 && hdr.page_size != 4096 &&
      hdr.page_size != 8192 && hdr.page_size != 16384) {
    fprintf(stderr, "warning: hdr.page_size %d must be 2048, 4096, 8192 or 16384\n",
            hdr.page_size);
    goto fail;
  }
  count = lseek(bootimg_fd, hdr.page_size, SEEK_SET);
  if (count != hdr.page_size) {
    msg1 = "boot header is not a page";
    goto fail;
  }

  sprintf(header_fn, "%s-header", bootimg_fn);
  fp = fopen(header_fn, "w+");
  if(!fp) {
    msg1 = "could not create";
    msg2 = header_fn;
    goto fail;
  }
  kernel_base  = hdr.kernel_addr - kernel_offset;
  ramdisk_base = hdr.ramdisk_addr - ramdisk_offset;
  second_base  = hdr.second_addr - second_offset;
  tags_base    = hdr.tags_addr - tags_offset;

  fprintf(fp, "Assumptions\n");
  fprintf(fp, "kernel_offset = 0x%X\n", kernel_offset);
  fprintf(fp, "ramdisk_offset = 0x%X\n", ramdisk_offset);
  fprintf(fp, "second_offset = 0x%X\n", second_offset);
  fprintf(fp, "tags_offset = 0x%X\n", tags_offset);
  fprintf(fp, "Detected values\n");
  fprintf(fp, "kernel_size = %d\n", hdr.kernel_size);
  fprintf(fp, "kernel_addr = 0x%X\n", hdr.kernel_addr);
  fprintf(fp, "ramdisk_size = %d\n", hdr.ramdisk_size);
  fprintf(fp, "ramdisk_addr = 0x%X\n", hdr.ramdisk_addr);
  fprintf(fp, "second_size = %d\n", hdr.second_size);
  fprintf(fp, "second_addr = 0x%X\n", hdr.second_addr);
  fprintf(fp, "tags_addr = 0x%X\n", hdr.tags_addr);
  fprintf(fp, "page_size = %d\n", hdr.page_size);
  /*fprintf(fp, "unused = %d%d\n", hdr.unused[0], hdr.unused[1]);*/
  fprintf(fp, "name/board = %s\n", hdr.name);
  fprintf(fp, "cmdline = %s\n", hdr.cmdline);
  /*fprintf(fp, "id = %s\n", hdr.id);*/
  fprintf(fp, "Computed values\n");
  fprintf(fp, "kernel_base = 0x%X\n", kernel_base);
  fprintf(fp, "ramdisk_base = 0x%X, kernel_base - ramdisk_offset=0x%X\n", ramdisk_base,
          kernel_base - ramdisk_base);
  fprintf(fp, "second_base = 0x%X, kernel_base - second_offset=0x%X\n", second_base,
          kernel_base - second_base);
  fprintf(fp, "tags_base = 0x%X, kernel_base - tags_offset = 0x%X\n", tags_base,
          kernel_base - tags_base);
  if (kernel_base != ramdisk_base ||
      kernel_base != second_base ||
      kernel_base != tags_base) {
    fprintf(fp, "WARNING! base addresses do not match!\n");
    fprintf(stderr, "WARNING! base addresses do not match!\n");
  }

  fclose(fp);

  /* kernel */
  if(hdr.kernel_size == 0) {
    msg1 = "kernel size is zero";
    goto fail;
  }
  offset = hdr.page_size;
  write_chunk(bootimg_fd, bootimg_fn, hdr.kernel_size, "kernel", offset);

  /* ramdisk */
  if(hdr.ramdisk_size == 0) {
    msg1 = "ramdisk size is zero";
    goto fail;
  }
  offset += pages(hdr.page_size, hdr.kernel_size) * hdr.page_size;
  write_chunk(bootimg_fd, bootimg_fn, hdr.ramdisk_size, "first-ramdisk", offset);

  /* ramdisk 2 */
  if(hdr.second_size > 0) {
    offset += pages(hdr.page_size, hdr.ramdisk_size) * hdr.page_size;
    write_chunk(bootimg_fd, bootimg_fn, hdr.second_size, "second-ramdisk", offset);
  }

  return 0;

 fail:
  close(bootimg_fd);
  fprintf(stderr,"error: %s %s '%s': %s\n", msg1, msg2, bootimg_fn,
          strerror(errno));
  return 1;
}
