/**
 * File:   ftp_fs.h
 * Author: AWTK Develop Team
 * Brief:  ftp base fs
 *
 * Copyright (c) 2018 - 2023  Guangzhou ZHIYUAN Electronics Co.,Ltd.
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
 * 2023-10-28 Li XianJing <xianjimli@hotmail.com> created
 *
 */

#ifndef TK_FTP_FS_H
#define TK_FTP_FS_H

#include "tkc/fs.h"
#include "tkc/iostream.h"

BEGIN_C_DECLS

/**
 * @class ftp_fs_t
 * 将ftp客户端封装成文件系统。
 *
 */
typedef struct _ftp_fs_t {
  fs_t fs;

  /*private*/
  char *user;
  char *host;
  char *password;
  tk_iostream_t *ios;
  tk_iostream_t *data_ios;
  int data_port;
  int last_error_code;
  char last_error_message[256];
  const char* stat_cmd;
} ftp_fs_t;

/**
 * @method ftp_fs_create
 * 创建ftp文件系统。
 * @param {const char*} host 主机地址。
 * @param {uint32_t} port 监听的端口。
 * @param {const char*} user 用户名。
 * @param {const char*} password 密码。
 *
 * @return {fs_t*} 返回ftp文件系统对象。
 */
fs_t *ftp_fs_create(const char *host, uint32_t port, const char *user,
                    const char *password);

/**
 * @method ftp_fs_destroy
 * 销毁ftp文件系统。
 * @param {fs_t*} fs ftp文件系统对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t ftp_fs_destroy(fs_t *fs);

/**
 * @method ftp_fs_cast
 * 转换为ftp文件系统。
 * @param {fs_t*} fs ftp文件系统对象。
 *
 * @return {ftp_fs_t*} 返回ftp文件系统对象。
 */
ftp_fs_t *ftp_fs_cast(fs_t *fs);

#define FTP_FS(fs) ftp_fs_cast(fs)

END_C_DECLS

#endif /*TK_FTP_FS_H*/
