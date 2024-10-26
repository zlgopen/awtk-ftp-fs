/**
 * File:   upload.c
 * Author: AWTK Develop Team
 * Brief:  ftp fs upload
 *
 * Copyright (c) 2023 - 2023  Guangzhou ZHIYUAN Electronics Co.,Ltd.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * License file for more details.
 *
 */

/**
 * History:
 * ================================================================
 * 2024-10-26 Li XianJing <lixianjing@zlg.cn> created
 *
 */

#include "tkc.h"
#include "ftp_fs.h"

int main(int argc, char* argv[]) {
  const char* local_file = "README.md";
  const char* remote_file = "test.md";
  const char* host = "localhost";
  int port = 2121;
  const char* user = "admin";
  const char* password = "admin";
  fs_t* fs = NULL;

  platform_prepare();

  if (argc < 7) {
    log_debug("Usage: %s remote_file local_file host port user password\n", argv[0]);
    log_debug("ex: %s %s %s %s %d %s %s\n", argv[0], remote_file, local_file, host, port, user, password);
    return 0;
  } else {
    remote_file = argv[1];
    local_file = argv[2];
    host =  argv[3];
    port = tk_atoi(argv[4]);
    user = argv[5];
    password = argv[6];
  }

  tk_socket_init();

  fs = ftp_fs_create(host, port, user, password);
  if (fs != NULL) {
    ret_t ret = ftp_fs_upload_file(fs, local_file, remote_file);
    log_debug("ret=%s\n", ret_code_to_name(ret));
    ftp_fs_destroy(fs);
  } else {
    log_debug("connect failed\n");
  }
  return 0;
}
