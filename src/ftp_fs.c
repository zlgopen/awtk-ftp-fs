/**
 * File:   ftp_fs.c
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

#include "tkc/buffer.h"
#include "tkc/darray.h"
#include "tkc/fs.h"
#include "tkc/mem.h"
#include "tkc/path.h"
#include "tkc/tokenizer.h"
#include "tkc/utils.h"

#include "ftp_fs.h"

#include "streams/inet/iostream_tcp.h"

static ret_t ftp_fs_pasv(ftp_fs_t* ftp_fs);
static ret_t ftp_fs_cmd(ftp_fs_t* ftp_fs, const char* cmd, int32_t* ret_code, char* ret_data,
                        uint32_t ret_data_size);

static ret_t ftp_fs_expect226(ftp_fs_t* ftp_fs) {
  char buf[1024] = {0};
  int32_t ret = tk_iostream_read(ftp_fs->ios, buf, sizeof(buf)-1);
  return_value_if_fail(ret > 0, RET_IO);

  buf[ret] = '\0';
  ret = tk_atoi(buf);
  if (ret >= 200 && ret < 300) {
    return RET_OK;
  } else {
    return RET_FAIL;
  }
}

static ret_t fs_ftp_binary_mode(ftp_fs_t* ftp_fs) {
  char cmd[1024] = {0};
  return_value_if_fail(ftp_fs != NULL, RET_BAD_PARAMS);

  tk_snprintf(cmd, sizeof(cmd), "TYPE I\r\n");
  return ftp_fs_cmd(ftp_fs, cmd, NULL, NULL, 0);
}

static ret_t ftp_fs_read_data(ftp_fs_t* ftp_fs, wbuffer_t* wb) {
  int32_t ret = 0;
  char buf[1024] = {0};
  return_value_if_fail(ftp_fs != NULL && wb != NULL, RET_BAD_PARAMS);

  while ((ret = tk_iostream_read(ftp_fs->data_ios, buf, sizeof(buf)-1)) > 0) {
    if (wbuffer_write_binary(wb, buf, ret) != RET_OK) {
      TK_OBJECT_UNREF(ftp_fs->data_ios);
      return RET_OOM;
    }
  }

  TK_OBJECT_UNREF(ftp_fs->data_ios);

  return ftp_fs_expect226(ftp_fs);
}

static fs_item_t* fs_item_parse(const char* line) {
  tokenizer_t t;
  char skey[128] = {0};
  char svalue[128] = {0};
  fs_item_t* item = NULL;
  return_value_if_fail(line != NULL && *line != '\0', NULL);
  item = fs_item_create();
  return_value_if_fail(item != NULL, NULL);

  tokenizer_init(&t, line, strlen(line), ";");
  while (tokenizer_has_more(&t)) {
    const char* kv = tokenizer_next(&t);
    if (kv != NULL) {
      const char* p = strchr(kv, '=');
      if (p != NULL) {
        int len = tk_min_int(p - kv, sizeof(skey));

        tk_strncpy(skey, kv, len);
        tk_strncpy(svalue, p + 1, sizeof(svalue) - 1);
        if (tk_str_eq(skey, "type")) {
          if (strstr(svalue, "dir")) {
            item->is_dir = TRUE;
          } else if (strstr(svalue, "file")) {
            item->is_reg_file = TRUE;
          } else {
            item->is_link = TRUE;
          }
        }
      } else {
        kv++;
        tk_strncpy(item->name, kv, sizeof(item->name));
      }
    }
  }
  tokenizer_deinit(&t);

  return item;
}

static ret_t ftp_fs_cmd_list(ftp_fs_t* ftp_fs, const char* path, darray_t* items) {
  wbuffer_t wb;
  char cmd[1024] = {0};
  ret_t ret = RET_FAIL;
  char buf[1024] = {0};
  return_value_if_fail(path != NULL && items != NULL, RET_BAD_PARAMS);
  return_value_if_fail(fs_change_dir((fs_t*)ftp_fs, path) == RET_OK, RET_FAIL);
  return_value_if_fail(ftp_fs_pasv(ftp_fs) == RET_OK, RET_FAIL);

  tk_snprintf(cmd, sizeof(cmd), "MLSD %s\r\n", path);
  ret = ftp_fs_cmd(ftp_fs, cmd, NULL, buf, sizeof(buf)-1);
  return_value_if_fail(ret == RET_OK, ret);

  wbuffer_init_extendable(&wb);
  if (ftp_fs_read_data(ftp_fs, &wb) == RET_OK) {
    tokenizer_t t;
    tokenizer_init(&t, (const char*)(wb.data), wb.cursor, "\r\n");
    while (tokenizer_has_more(&t)) {
      const char* line = tokenizer_next(&t);
      if (line != NULL) {
        fs_item_t* item = fs_item_parse(line);
        if (item != NULL) {
          darray_push(items, item);
        }
      }
    }
    tokenizer_deinit(&t);
  }
  wbuffer_deinit(&wb);

  return RET_OK;
}

static ret_t ftp_fs_cmd(ftp_fs_t* ftp_fs, const char* cmd, int32_t* ret_code, char* ret_data,
                        uint32_t ret_data_size) {
  char buf[1024] = {0};
  int32_t len = strlen(cmd);
  int32_t ret = tk_iostream_write(ftp_fs->ios, cmd, len);
  return_value_if_fail(ret == len, RET_IO);

  ret = tk_iostream_read(ftp_fs->ios, buf, sizeof(buf)-1);
  return_value_if_fail(ret > 0, RET_IO);

  buf[ret] = '\0';
  ret = tk_atoi(buf);
  if (ret_code != NULL) {
    *ret_code = ret;
  }

  if (strstr(buf, "213-Status") != NULL && strstr(buf, "End of status") == NULL) {
    while (strstr(buf, "End of status") == NULL) {
      len = tk_strlen(buf);
      len += tk_iostream_read(ftp_fs->ios, buf + len, sizeof(buf)-1 - len);
    }
  }

  if (ret_data != NULL && ret_data_size > 0) {
    const char* p = strchr(buf, ' ');
    if (p != NULL) {
      tk_strncpy(ret_data, p + 1, ret_data_size);
    }
  }

  if (ret >= 100 && ret < 400) {
    ftp_fs->last_error_code = 0;
    ftp_fs->last_error_message[0] = '\0';
    return RET_OK;
  } else {
    ftp_fs->last_error_code = ret;
    tk_strncpy(ftp_fs->last_error_message, buf, sizeof(ftp_fs->last_error_message) - 1);
    return RET_FAIL;
  }
}

static ret_t ftp_fs_login(ftp_fs_t* ftp_fs) {
  char cmd[1024] = {0};
  ret_t ret = RET_FAIL;
  tk_snprintf(cmd, sizeof(cmd), "USER %s\r\n", ftp_fs->user);
  ret = ftp_fs_cmd(ftp_fs, cmd, NULL, NULL, 0);
  return_value_if_fail(ret == RET_OK, ret);

  tk_snprintf(cmd, sizeof(cmd), "PASS %s\r\n", ftp_fs->password);
  ret = ftp_fs_cmd(ftp_fs, cmd, NULL, NULL, 0);
  return_value_if_fail(ret == RET_OK, ret);

  fs_ftp_binary_mode(ftp_fs);

  return RET_OK;
}

static ret_t ftp_fs_pasv(ftp_fs_t* ftp_fs) {
  char cmd[1024] = {0};
  ret_t ret = RET_FAIL;
  char buf[1024] = {0};
  int ip0 = 0;
  int ip1 = 0;
  int ip2 = 0;
  int ip3 = 0;
  int port_hi = 0;
  int port_lo = 0;
  const char* p = NULL;

  if (ftp_fs->data_ios != NULL) {
    TK_OBJECT_UNREF(ftp_fs->data_ios);
    ftp_fs->data_ios = NULL;
  }

  tk_snprintf(cmd, sizeof(cmd), "PASV\r\n");
  ret = ftp_fs_cmd(ftp_fs, cmd, NULL, buf, sizeof(buf)-1);
  return_value_if_fail(ret == RET_OK, ret);

  p = strchr(buf, '(');
  return_value_if_fail(p != NULL, RET_FAIL);

  if (tk_sscanf(p, "(%d,%d,%d,%d,%d,%d)", &ip0, &ip1, &ip2, &ip3, &port_hi, &port_lo) == 6) {
    char ip[128] = {0};
    ftp_fs->data_port = port_hi * 256 + port_lo;
    tk_snprintf(ip, sizeof(ip), "%d.%d.%d.%d", ip0, ip1, ip2, ip3);
    ftp_fs->data_ios = tk_iostream_tcp_create_client(ip, ftp_fs->data_port);
    return_value_if_fail(ftp_fs->data_ios != NULL, RET_IO);

    return RET_OK;
  }

  return RET_FAIL;
}

static ret_t ftp_fs_cmd_get_size(ftp_fs_t* ftp_fs, const char* filename, int32_t* size) {
  char cmd[1024] = {0};
  ret_t ret = RET_FAIL;
  char buf[1024] = {0};
  return_value_if_fail(filename != NULL && size != NULL, RET_BAD_PARAMS);

  tk_snprintf(cmd, sizeof(cmd), "SIZE %s\r\n", filename);
  ret = ftp_fs_cmd(ftp_fs, cmd, NULL, buf, sizeof(buf)-1);
  return_value_if_fail(ret == RET_OK, ret);

  *size = tk_atoi(buf);

  return RET_OK;
}

static ret_t ftp_fs_cmd_stat(ftp_fs_t* ftp_fs, const char* filename, fs_stat_info_t* fst) {
  char cmd[1024] = {0};
  ret_t ret = RET_FAIL;
  char buf[1024] = {0};
  const char* p = NULL;
  tokenizer_t t;
  return_value_if_fail(filename != NULL && fst != NULL, RET_BAD_PARAMS);

  memset(fst, 0x00, sizeof(*fst));

  if (ftp_fs->stat_cmd != NULL) {
    tk_snprintf(cmd, sizeof(cmd), "%s %s\r\n", ftp_fs->stat_cmd, filename);
    ret = ftp_fs_cmd(ftp_fs, cmd, NULL, buf, sizeof(buf)-1);
  } else {
    tk_snprintf(cmd, sizeof(cmd), "XSTAT %s\r\n", filename);
    ret = ftp_fs_cmd(ftp_fs, cmd, NULL, buf, sizeof(buf)-1);
    if (ret != RET_OK) {
      if (ftp_fs->last_error_code == 500) {
        tk_snprintf(cmd, sizeof(cmd), "STAT %s\r\n", filename);
        ret = ftp_fs_cmd(ftp_fs, cmd, NULL, buf, sizeof(buf)-1);
      } else {
        return ret;
      }
    }
  }

  p = strstr(buf, ":\r\n");
  if (p != NULL) {
    p += strlen(":\r\n");
    // p = 213-Status of \"/test.bin\":\r\n-rw-r--r--   1 jim      staff        1650 Oct 25 13:12 test.bin\r\n213 End of status.\r\n
    tokenizer_init(&t, p, strlen(p), " ");
    if (tokenizer_has_more(&t)) {
      p = tokenizer_next(&t);  // -rw-r--r--
      if (tk_strcmp(p, "213") == 0) {
        fst->is_dir = TRUE;
        //空目录可能没有内容，比如："/abc":\r\n213 End of status.\r\n
      } else {
        fst->is_reg_file = p[0] == '-';
        fst->is_dir = p[0] == 'd';
        p = tokenizer_next(&t);                   // 1
        p = tokenizer_next(&t);                   // jim
        p = tokenizer_next(&t);                   // staff
        fst->size = tokenizer_next_int64(&t, 0);  // 1650
        p = tokenizer_next(&t);                   // Oct
        p = tokenizer_next(&t);                   // 25
        p = tokenizer_next(&t);                   // 13:12
      }
    }
    tokenizer_deinit(&t);
  } else {
    tokenizer_init(&t, buf, strlen(buf), " ");
    fst->size = tokenizer_next_int64(&t, 0);
    fst->mtime = tokenizer_next_int64(&t, 0);
    fst->ctime = tokenizer_next_int64(&t, 0);
    fst->atime = tokenizer_next_int64(&t, 0);
    fst->is_dir = tokenizer_next_int(&t, 0);
    fst->is_link = tokenizer_next_int(&t, 0);
    fst->is_reg_file = tokenizer_next_int(&t, 0);
    fst->uid = tokenizer_next_int(&t, 0);
    fst->gid = tokenizer_next_int(&t, 0);
    tokenizer_deinit(&t);
  }

  return RET_OK;
}

static ret_t ftp_fs_cmd_get_pwd(ftp_fs_t* ftp_fs, char* path, uint32_t path_size) {
  char cmd[1024] = {0};
  ret_t ret = RET_FAIL;
  char buf[1024] = {0};
  const char* p = NULL;
  const char* pend = NULL;
  return_value_if_fail(path != NULL && path_size > 0, RET_BAD_PARAMS);

  tk_snprintf(cmd, sizeof(cmd), "PWD\r\n");
  ret = ftp_fs_cmd(ftp_fs, cmd, NULL, buf, sizeof(buf)-1);
  return_value_if_fail(ret == RET_OK, ret);

  p = strchr(buf, '"');
  if (p != NULL) {
    p += 1;
    pend = strchr(p, '"');
    if (pend != NULL) {
      tk_strncpy(path, p, tk_min(pend - p, path_size));
      return RET_OK;
    }
  }

  return RET_FAIL;
}

typedef struct _fs_ftp_file_t {
  fs_file_t file;
  ftp_fs_t* ftp_fs;
  char name[MAX_PATH + 1];
  char temp_path[MAX_PATH + 1];
  fs_file_t* temp_file;
  bool_t changed;
} fs_ftp_file_t;

static ret_t ftp_fs_cmd_download_file(ftp_fs_t* ftp_fs, const char* remote_filename,
                                      const char* local_filename) {
  int ret = 0;
  char cmd[1024] = {0};
  char buf[1024] = {0};
  fs_file_t* file = NULL;
  return_value_if_fail(ftp_fs != NULL && remote_filename != NULL && local_filename != NULL,
                       RET_BAD_PARAMS);

  goto_error_if_fail(ftp_fs_pasv(ftp_fs) == RET_OK);
  tk_snprintf(cmd, sizeof(cmd), "RETR %s\r\n", remote_filename);

  if (ftp_fs_cmd(ftp_fs, cmd, NULL, NULL, 0) == RET_OK) {
    file = fs_open_file(os_fs(), local_filename, "wb+");
    if (file != NULL) {
      while ((ret = tk_iostream_read(ftp_fs->data_ios, buf, sizeof(buf)-1)) > 0) {
        break_if_fail(fs_file_write(file, buf, ret) == ret);
      }
      fs_file_close(file);
    }

    TK_OBJECT_UNREF(ftp_fs->data_ios);
    return ftp_fs_expect226(ftp_fs);
  }

  return RET_NOT_FOUND;
error:
  fs_file_close(file);
  TK_OBJECT_UNREF(ftp_fs->data_ios);

  return RET_OK;
}

ret_t ftp_fs_download_file(fs_t* fs, const char* remote_filename, const char* local_filename) {
  ftp_fs_t* ftp_fs = FTP_FS(fs);
  return_value_if_fail(ftp_fs != NULL, RET_BAD_PARAMS);
  return_value_if_fail(remote_filename != NULL && local_filename != NULL, RET_BAD_PARAMS);

  return ftp_fs_cmd_download_file(ftp_fs, remote_filename, local_filename);
}

static ret_t ftp_fs_cmd_upload_file(ftp_fs_t* ftp_fs, const char* local_filename,
                                    const char* remote_filename) {
  int ret = 0;
  char cmd[1024] = {0};
  char buf[1024] = {0};
  fs_file_t* file = NULL;
  return_value_if_fail(ftp_fs != NULL && remote_filename != NULL && local_filename != NULL,
                       RET_BAD_PARAMS);

  file = fs_open_file(os_fs(), local_filename, "rb");
  return_value_if_fail(file != NULL, RET_FAIL);

  goto_error_if_fail(ftp_fs_pasv(ftp_fs) == RET_OK);
  tk_snprintf(cmd, sizeof(cmd), "STOR %s\r\n", remote_filename);
  goto_error_if_fail(ftp_fs_cmd(ftp_fs, cmd, NULL, NULL, 0) == RET_OK);

  while ((ret = fs_file_read(file, buf, sizeof(buf)-1)) > 0) {
    break_if_fail(tk_iostream_write_len(ftp_fs->data_ios, buf, ret, 2000) == ret);
  }

  fs_file_close(file);
  TK_OBJECT_UNREF(ftp_fs->data_ios);

  return ftp_fs_expect226(ftp_fs);
error:
  fs_file_close(file);
  TK_OBJECT_UNREF(ftp_fs->data_ios);

  return RET_FAIL;
}

ret_t ftp_fs_upload_file(fs_t* fs, const char* local_filename, const char* remote_filename) {
  ftp_fs_t* ftp_fs = FTP_FS(fs);
  return_value_if_fail(ftp_fs != NULL, RET_BAD_PARAMS);
  return_value_if_fail(remote_filename != NULL && local_filename != NULL, RET_BAD_PARAMS);

  return ftp_fs_cmd_upload_file(ftp_fs, local_filename, remote_filename);
}

static int32_t fs_ftp_file_read(fs_file_t* file, void* buffer, uint32_t size) {
  fs_ftp_file_t* ftp_file = (fs_ftp_file_t*)file;
  return_value_if_fail(ftp_file != NULL, RET_BAD_PARAMS);

  return fs_file_read(ftp_file->temp_file, buffer, size);
}

static int32_t fs_ftp_file_write(fs_file_t* file, const void* buffer, uint32_t size) {
  fs_ftp_file_t* ftp_file = (fs_ftp_file_t*)file;
  return_value_if_fail(ftp_file != NULL, RET_BAD_PARAMS);
  ftp_file->changed = TRUE;

  return fs_file_write(ftp_file->temp_file, buffer, size);
}

static int32_t fs_ftp_file_printf(fs_file_t* file, const char* const format_str, va_list vl) {
  fs_ftp_file_t* ftp_file = (fs_ftp_file_t*)file;
  return_value_if_fail(ftp_file != NULL, RET_BAD_PARAMS);

  if (ftp_file->temp_file != NULL && ftp_file->temp_file->vt != NULL &&
      ftp_file->temp_file->vt->printf != NULL) {
    ftp_file->changed = TRUE;
    return ftp_file->temp_file->vt->printf(file, format_str, vl);
  }

  return RET_FAIL;
}

static ret_t fs_ftp_file_seek(fs_file_t* file, int32_t offset) {
  fs_ftp_file_t* ftp_file = (fs_ftp_file_t*)file;
  return_value_if_fail(ftp_file != NULL, RET_BAD_PARAMS);

  return fs_file_seek(ftp_file->temp_file, offset);
}

static int64_t fs_ftp_file_tell(fs_file_t* file) {
  fs_ftp_file_t* ftp_file = (fs_ftp_file_t*)file;
  return_value_if_fail(ftp_file != NULL, 0);

  return fs_file_tell(ftp_file->temp_file);
}

static int64_t fs_ftp_file_size(fs_file_t* file) {
  fs_ftp_file_t* ftp_file = (fs_ftp_file_t*)file;
  return_value_if_fail(ftp_file != NULL, 0);

  return fs_file_size(ftp_file->temp_file);
}

static ret_t fs_ftp_file_stat(fs_file_t* file, fs_stat_info_t* fst) {
  fs_ftp_file_t* ftp_file = (fs_ftp_file_t*)file;
  return_value_if_fail(ftp_file != NULL, RET_BAD_PARAMS);

  return fs_file_stat(ftp_file->temp_file, fst);
}

static ret_t fs_ftp_file_sync(fs_file_t* file) {
  fs_ftp_file_t* ftp_file = (fs_ftp_file_t*)file;
  return_value_if_fail(ftp_file != NULL, RET_BAD_PARAMS);

  if (ftp_file->changed) {
    return ftp_fs_cmd_upload_file(ftp_file->ftp_fs, ftp_file->temp_path, ftp_file->name);
  }

  return RET_OK;
}

static ret_t fs_ftp_file_truncate(fs_file_t* file, int32_t size) {
  fs_ftp_file_t* ftp_file = (fs_ftp_file_t*)file;
  return_value_if_fail(ftp_file != NULL, RET_BAD_PARAMS);

  ftp_file->changed = TRUE;
  return fs_file_truncate(ftp_file->temp_file, size);
}

static bool_t fs_ftp_file_eof(fs_file_t* file) {
  fs_ftp_file_t* ftp_file = (fs_ftp_file_t*)file;
  return_value_if_fail(ftp_file != NULL, TRUE);

  return fs_file_eof(ftp_file->temp_file);
}

static ret_t fs_ftp_file_close(fs_file_t* file) {
  fs_ftp_file_t* ftp_file = (fs_ftp_file_t*)file;
  return_value_if_fail(ftp_file != NULL, RET_BAD_PARAMS);
  if (ftp_file->temp_file != NULL) {
    fs_file_sync(ftp_file->temp_file);
    fs_file_close(ftp_file->temp_file);
    ftp_file->temp_file = NULL;
  }

  if (file_exist(ftp_file->temp_path)) {
    ftp_fs_cmd_upload_file(ftp_file->ftp_fs, ftp_file->temp_path, ftp_file->name);
    fs_remove_file(os_fs(), ftp_file->temp_path);
  }

  TKMEM_FREE(ftp_file);

  return RET_OK;
}

static const fs_file_vtable_t s_file_vtable = {.read = fs_ftp_file_read,
                                               .write = fs_ftp_file_write,
                                               .printf = fs_ftp_file_printf,
                                               .seek = fs_ftp_file_seek,
                                               .tell = fs_ftp_file_tell,
                                               .size = fs_ftp_file_size,
                                               .stat = fs_ftp_file_stat,
                                               .sync = fs_ftp_file_sync,
                                               .truncate = fs_ftp_file_truncate,
                                               .eof = fs_ftp_file_eof,
                                               .close = fs_ftp_file_close};

static fs_file_t* fs_ftp_open_file(fs_t* fs, const char* name, const char* mode) {
  fs_ftp_file_t* ftp_file = NULL;
  char temp_path[MAX_PATH + 1] = {0};
  ftp_fs_t* ftp_fs = FTP_FS(fs);
  return_value_if_fail(fs != NULL && name != NULL && mode != NULL, NULL);
  ftp_file = TKMEM_ZALLOC(fs_ftp_file_t);
  return_value_if_fail(ftp_file != NULL, NULL);

  tk_snprintf(temp_path, sizeof(temp_path) - 1, "%s_%d_%s", ftp_fs->host, ftp_fs->data_port, name);
  tk_replace_char(temp_path, '/', '_');
  tk_replace_char(temp_path, '\\', '_');

  path_prepend_temp_path(ftp_file->temp_path, temp_path);

  if (ftp_fs_cmd_download_file(ftp_fs, name, ftp_file->temp_path) != RET_OK) {
    if (strchr(mode, 'w') == NULL) {
      goto error;
    }
  }

  tk_strncpy(ftp_file->name, name, sizeof(ftp_file->name) - 1);
  ftp_file->temp_file = fs_open_file(os_fs(), ftp_file->temp_path, mode);
  if (ftp_file->temp_file != NULL) {
    ftp_file->file.vt = &s_file_vtable;
    ftp_file->ftp_fs = ftp_fs;
    return (fs_file_t*)ftp_file;
  }

error:
  TKMEM_FREE(ftp_file);
  return NULL;
}

typedef struct _fs_ftp_dir_t {
  fs_dir_t dir;
  darray_t items;
  ftp_fs_t* ftp_fs;
  uint32_t current;
} fs_ftp_dir_t;

static ret_t fs_ftp_dir_rewind(fs_dir_t* dir) {
  fs_ftp_dir_t* ftp_dir = (fs_ftp_dir_t*)dir;
  return_value_if_equal(dir != NULL, RET_OK);
  ftp_dir->current = 0;

  return RET_OK;
}

ret_t fs_ftp_dir_read(fs_dir_t* dir, fs_item_t* item) {
  fs_ftp_dir_t* ftp_dir = (fs_ftp_dir_t*)dir;
  return_value_if_equal(dir != NULL && item != NULL, RET_OK);

  if (ftp_dir->current < ftp_dir->items.size) {
    fs_item_t* i = (fs_item_t*)darray_get(&ftp_dir->items, ftp_dir->current);
    *item = *i;
    ftp_dir->current++;

    return RET_OK;
  } else {
    return RET_FAIL;
  }
}

static ret_t fs_ftp_dir_close(fs_dir_t* dir) {
  fs_ftp_dir_t* ftp_dir = (fs_ftp_dir_t*)dir;
  return_value_if_equal(dir != NULL, RET_OK);

  darray_deinit(&ftp_dir->items);
  TKMEM_FREE(ftp_dir);

  return RET_OK;
}

static const fs_dir_vtable_t s_dir_vtable = {
    .read = fs_ftp_dir_read, .rewind = fs_ftp_dir_rewind, .close = fs_ftp_dir_close};

static fs_dir_t* fs_ftp_open_dir(fs_t* fs, const char* name) {
  fs_ftp_dir_t* dir = NULL;
  return_value_if_fail(fs != NULL && name != NULL, NULL);
  dir = TKMEM_ZALLOC(fs_ftp_dir_t);
  return_value_if_fail(dir != NULL, NULL);

  darray_init(&dir->items, 10, (tk_destroy_t)fs_item_destroy, NULL);
  dir->dir.vt = &s_dir_vtable;
  dir->ftp_fs = FTP_FS(fs);

  if (ftp_fs_cmd_list(dir->ftp_fs, name, &dir->items) == RET_OK) {
    return (fs_dir_t*)dir;
  } else {
    fs_dir_close((fs_dir_t*)dir);
    return NULL;
  }
}

static ret_t fs_ftp_remove_file(fs_t* fs, const char* name) {
  char cmd[1024] = {0};
  ftp_fs_t* ftp_fs = FTP_FS(fs);
  return_value_if_fail(ftp_fs != NULL && name != NULL, RET_BAD_PARAMS);

  tk_snprintf(cmd, sizeof(cmd), "DELE %s\r\n", name);
  return ftp_fs_cmd(ftp_fs, cmd, NULL, NULL, 0);
}

static bool_t fs_ftp_file_exist(fs_t* fs, const char* name) {
  fs_stat_info_t info;
  return fs_stat(fs, name, &info) == RET_OK && info.is_reg_file;
}

static ret_t fs_ftp_file_rename(fs_t* fs, const char* name, const char* new_name) {
  char cmd[1024] = {0};
  ftp_fs_t* ftp_fs = FTP_FS(fs);
  return_value_if_fail(ftp_fs != NULL && name != NULL && new_name != NULL, RET_BAD_PARAMS);

  tk_snprintf(cmd, sizeof(cmd), "RNFR %s\r\n", name);
  return_value_if_fail(ftp_fs_cmd(ftp_fs, cmd, NULL, NULL, 0) == RET_OK, RET_FAIL);

  tk_snprintf(cmd, sizeof(cmd), "RNTO %s\r\n", new_name);
  return_value_if_fail(ftp_fs_cmd(ftp_fs, cmd, NULL, NULL, 0) == RET_OK, RET_FAIL);

  return RET_OK;
}

static ret_t fs_ftp_remove_dir(fs_t* fs, const char* name) {
  char cmd[1024] = {0};
  ftp_fs_t* ftp_fs = FTP_FS(fs);
  return_value_if_fail(ftp_fs != NULL && name != NULL, RET_BAD_PARAMS);

  tk_snprintf(cmd, sizeof(cmd), "RMD %s\r\n", name);
  return ftp_fs_cmd(ftp_fs, cmd, NULL, NULL, 0);
}

static ret_t fs_ftp_change_dir(fs_t* fs, const char* name) {
  char cmd[1024] = {0};
  ftp_fs_t* ftp_fs = FTP_FS(fs);
  return_value_if_fail(ftp_fs != NULL && name != NULL, RET_BAD_PARAMS);

  tk_snprintf(cmd, sizeof(cmd), "CWD %s\r\n", name);
  return ftp_fs_cmd(ftp_fs, cmd, NULL, NULL, 0);
}

static ret_t fs_ftp_create_dir(fs_t* fs, const char* name) {
  char cmd[1024] = {0};
  ftp_fs_t* ftp_fs = FTP_FS(fs);
  return_value_if_fail(ftp_fs != NULL && name != NULL, RET_BAD_PARAMS);

  tk_snprintf(cmd, sizeof(cmd), "MKD %s\r\n", name);
  return ftp_fs_cmd(ftp_fs, cmd, NULL, NULL, 0);
}

static bool_t fs_ftp_dir_exist(fs_t* fs, const char* name) {
  fs_stat_info_t info;
  return fs_stat(fs, name, &info) == RET_OK && info.is_dir;
}

static ret_t fs_ftp_dir_rename(fs_t* fs, const char* name, const char* new_name) {
  return fs_ftp_file_rename(fs, name, new_name);
}

static int32_t fs_ftp_get_file_size(fs_t* fs, const char* name) {
  int32_t size = 0;
  ftp_fs_t* ftp_fs = FTP_FS(fs);
  return_value_if_fail(ftp_fs != NULL, RET_BAD_PARAMS);

  return ftp_fs_cmd_get_size(ftp_fs, name, &size) == RET_OK ? size : 0;
}

static ret_t fs_ftp_get_disk_info(fs_t* fs, const char* volume, int32_t* free_kb,
                                  int32_t* total_kb) {
  return RET_NOT_IMPL;
}

static ret_t fs_ftp_stat(fs_t* fs, const char* name, fs_stat_info_t* fst) {
  ftp_fs_t* ftp_fs = FTP_FS(fs);
  return_value_if_fail(ftp_fs != NULL, RET_BAD_PARAMS);

  return ftp_fs_cmd_stat(ftp_fs, name, fst);
}

static ret_t fs_ftp_get_cwd(fs_t* fs, char path[MAX_PATH + 1]) {
  ftp_fs_t* ftp_fs = FTP_FS(fs);
  return_value_if_fail(ftp_fs != NULL, RET_BAD_PARAMS);

  return ftp_fs_cmd_get_pwd(ftp_fs, path, MAX_PATH);
}

static ret_t fs_ftp_get_exe(fs_t* fs, char path[MAX_PATH + 1]) {
  return fs_get_exe(os_fs(), path);
}

static ret_t fs_ftp_get_temp_path(fs_t* fs, char path[MAX_PATH + 1]) {
  return fs_get_temp_path(os_fs(), path);
}

static ret_t fs_ftp_get_user_storage_path(fs_t* fs, char path[MAX_PATH + 1]) {
  return fs_get_user_storage_path(os_fs(), path);
}

static ret_t ftp_fs_init(fs_t* fs) {
  fs->open_file = fs_ftp_open_file;
  fs->remove_file = fs_ftp_remove_file;
  fs->file_exist = fs_ftp_file_exist;
  fs->file_rename = fs_ftp_file_rename;

  fs->open_dir = fs_ftp_open_dir;
  fs->remove_dir = fs_ftp_remove_dir;
  fs->create_dir = fs_ftp_create_dir;
  fs->change_dir = fs_ftp_change_dir;
  fs->dir_exist = fs_ftp_dir_exist;
  fs->dir_rename = fs_ftp_dir_rename;

  fs->get_file_size = fs_ftp_get_file_size;
  fs->get_disk_info = fs_ftp_get_disk_info;
  fs->get_cwd = fs_ftp_get_cwd;
  fs->get_exe = fs_ftp_get_exe;
  fs->get_user_storage_path = fs_ftp_get_user_storage_path;
  fs->get_temp_path = fs_ftp_get_temp_path;
  fs->stat = fs_ftp_stat;

  return RET_OK;
}

static const char* ftp_fs_get_stat_cmd_from_welcome(const char* message) {
  if (strstr(message, "vsFTPd") != NULL) {
    return "STAT";
  } else if (strstr(message, "AWTK") != NULL) {
    return "XSTAT";
  } else if (strstr(message, "pyftpdlib") != NULL) {
    return "XSTAT";
  }

  return NULL;
}

fs_t* ftp_fs_create(const char* host, uint32_t port, const char* user, const char* password) {
  ftp_fs_t* ftp_fs = NULL;
  char buf[1024] = {0};
  return_value_if_fail(port > 0 && user != NULL && password != NULL, NULL);
  ftp_fs = TKMEM_ZALLOC(ftp_fs_t);
  return_value_if_fail(ftp_fs != NULL, NULL);

  ftp_fs_init(&ftp_fs->fs);
  ftp_fs->host = tk_str_copy(ftp_fs->host, host);
  ftp_fs->user = tk_str_copy(ftp_fs->user, user);
  ftp_fs->password = tk_str_copy(ftp_fs->password, password);
  ftp_fs->ios = tk_iostream_tcp_create_client(host, port);

  goto_error_if_fail(ftp_fs->ios != NULL);

  /*welcome message*/
  tk_iostream_read(ftp_fs->ios, buf, sizeof(buf)-1);
  log_debug("%s", buf);

  ftp_fs->stat_cmd = ftp_fs_get_stat_cmd_from_welcome(buf);
  goto_error_if_fail(ftp_fs_login(ftp_fs) == RET_OK);

  return (fs_t*)(ftp_fs);
error:
  if (ftp_fs != NULL) {
    ftp_fs_destroy((fs_t*)(ftp_fs));
  }

  return NULL;
}

ret_t ftp_fs_destroy(fs_t* fs) {
  ftp_fs_t* ftp_fs = FTP_FS(fs);
  return_value_if_fail(ftp_fs != NULL, RET_BAD_PARAMS);

  TKMEM_FREE(ftp_fs->user);
  TKMEM_FREE(ftp_fs->password);
  TKMEM_FREE(ftp_fs->host);
  TK_OBJECT_UNREF(ftp_fs->ios);
  TK_OBJECT_UNREF(ftp_fs->data_ios);

  TKMEM_FREE(ftp_fs);
  return RET_OK;
}

ftp_fs_t* ftp_fs_cast(fs_t* fs) {
  return_value_if_fail(fs != NULL && fs->change_dir == fs_ftp_change_dir, NULL);

  return (ftp_fs_t*)fs;
}
