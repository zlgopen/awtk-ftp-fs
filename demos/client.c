/**
 * File:   fs.c
 * Author: AWTK Develop Team
 * Brief:  ftp fs
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
 * 2023-10-28 Li XianJing <lixianjing@zlg.cn> created
 *
 */

#include "tkc.h"
#include "ftp_fs.h"
#include "conf_io/conf_node.h"
#include "conf_io/conf_ini.h"

static void run_script(conf_doc_t* doc, uint32_t times) {
  fs_t* fs = NULL;
  conf_node_t* iter = conf_node_get_first_child(doc->root);

  while (iter != NULL) {
    const char* name = conf_node_get_name(iter);

    if (tk_str_eq(name, "create")) {
      const char* host = conf_node_get_child_value_str(iter, "host", "localhost");
      const char* user = conf_node_get_child_value_str(iter, "user", "admin");
      const char* password = conf_node_get_child_value_str(iter, "password", "admin");
      int port = conf_node_get_child_value_int32(iter, "port", 2121);

      if (fs != NULL) {
        iter = iter->next;
        continue;
      }

      fs = ftp_fs_create(host, port, user, password);
      log_debug("create: %s:%d %s %s\n", host, port, user, password);
      iter = iter->next;
      continue;
    }

    if (fs == NULL) {
      log_debug("fs is null.\n");
    }

    if (tk_str_eq(name, "get_file_size")) {
      int32_t expected_size = conf_node_get_child_value_int32(iter, "size", -1);
      const char* name = conf_node_get_child_value_str(iter, "name", NULL);
      if (name != NULL) {
        int32_t size = fs_get_file_size(fs, name);
        if (expected_size >= 0 && expected_size != size) {
          log_debug("get_file_size failed: %s %d %d\n", name, expected_size, size);
        } else {
          log_debug("get_file_size: %s %d\n", name, size);
        }
      }
    } else if (tk_str_eq(name, "get_cwd")) {
      char cwd[MAX_PATH + 1] = {0};
      const char* expected_cwd = conf_node_get_child_value_str(iter, "cwd", NULL);
      ret_t ret = fs_get_cwd(fs, cwd);
      if (ret == RET_OK) {
        if (expected_cwd != NULL) {
          if (tk_str_eq(cwd, expected_cwd)) {
            log_debug("get_cwd: %s\n", cwd);
          } else {
            log_debug("get_cwd failed: %s %s\n", cwd, expected_cwd);
          }
        } else {
          log_debug("get_cwd: %s\n", cwd);
        }
      } else {
        log_debug("get_cwd failed: %d\n", ret);
      }
    } else if (tk_str_eq(name, "file_exist")) {
      const char* name = conf_node_get_child_value_str(iter, "name", NULL);
      bool_t expected_exist = conf_node_get_child_value_bool(iter, "exist", FALSE);
      bool_t ret = fs_file_exist(fs, name);
      if (ret == expected_exist) {
        log_debug("file_exist: %s %d\n", name, ret);
      } else {
        log_debug("file_exist failed: %s %d %d\n", name, ret, expected_exist);
      }
    } else if (tk_str_eq(name, "remove_file")) {
      const char* name = conf_node_get_child_value_str(iter, "name", NULL);
      bool_t ret = fs_remove_file(fs, name) == RET_OK;
      bool_t expected_result = conf_node_get_child_value_bool(iter, "result", FALSE);
      if (ret == expected_result) {
        log_debug("remove_file: %s %d\n", name, ret);
      } else {
        log_debug("remove_file failed: %s %d %d\n", name, ret, expected_result);
      }
    } else if (tk_str_eq(name, "file_rename")) {
      const char* from = conf_node_get_child_value_str(iter, "from", NULL);
      const char* to = conf_node_get_child_value_str(iter, "to", NULL);
      bool_t ret = fs_file_rename(fs, from, to) == RET_OK;
      bool_t expected_result = conf_node_get_child_value_bool(iter, "result", FALSE);
      if (ret == expected_result) {
        log_debug("file_rename: %s %s %d\n", from, to, ret);
      } else {
        log_debug("file_rename failed: %s %s %d %d\n", from, to, ret, expected_result);
      }
    } else if (tk_str_eq(name, "create_dir")) {
      const char* name = conf_node_get_child_value_str(iter, "name", NULL);
      bool_t ret = fs_create_dir(fs, name) == RET_OK;
      bool_t expected_result = conf_node_get_child_value_bool(iter, "result", FALSE);
      if (ret == expected_result) {
        log_debug("create_dir: %s %d\n", name, ret);
      } else {
        log_debug("create_dir failed: %s %d %d\n", name, ret, expected_result);
      }
    } else if (tk_str_eq(name, "remove_dir")) {
      const char* name = conf_node_get_child_value_str(iter, "name", NULL);
      bool_t ret = fs_remove_dir(fs, name) == RET_OK;
      bool_t expected_result = conf_node_get_child_value_bool(iter, "result", FALSE);
      if (ret == expected_result) {
        log_debug("remove_dir: %s %d\n", name, ret);
      } else {
        log_debug("remove_dir failed: %s %d %d\n", name, ret, expected_result);
      }
    } else if (tk_str_eq(name, "dir_rename")) {
      const char* from = conf_node_get_child_value_str(iter, "from", NULL);
      const char* to = conf_node_get_child_value_str(iter, "to", NULL);
      bool_t ret = fs_dir_rename(fs, from, to) == RET_OK;
      bool_t expected_result = conf_node_get_child_value_bool(iter, "result", FALSE);
      if (ret == expected_result) {
        log_debug("dir_rename: %s %s %d\n", from, to, ret);
      } else {
        log_debug("dir_rename failed: %s %s %d %d\n", from, to, ret, expected_result);
      }
    } else if (tk_str_eq(name, "change_dir")) {
      const char* name = conf_node_get_child_value_str(iter, "name", NULL);
      bool_t ret = fs_change_dir(fs, name) == RET_OK;
      bool_t expected_result = conf_node_get_child_value_bool(iter, "result", FALSE);
      if (ret == expected_result) {
        log_debug("change_dir: %s %d\n", name, ret);
      } else {
        log_debug("change_dir failed: %s %d %d\n", name, ret, expected_result);
      }
    } else if (tk_str_eq(name, "dir_exist")) {
      const char* name = conf_node_get_child_value_str(iter, "name", NULL);
      bool_t expected_exist = conf_node_get_child_value_bool(iter, "exist", FALSE);
      bool_t ret = fs_dir_exist(fs, name);
      if (ret == expected_exist) {
        log_debug("dir_exist: %s %d\n", name, ret);
      } else {
        log_debug("dir_exist failed: %s %d %d\n", name, ret, expected_exist);
      }
    } else if (tk_str_eq(name, "dir_list")) {
      const char* name = conf_node_get_child_value_str(iter, "name", NULL);
      const char* expected_items = conf_node_get_child_value_str(iter, "items", NULL);
      fs_dir_t* dir = fs_open_dir(fs, name);
      if (dir != NULL) {
        str_t str;
        fs_item_t item;
        str_init(&str, 10000);

        while (fs_dir_read(dir, &item) == RET_OK) {
          str_append_format(&str, 1024, "{%s:%s};", item.is_dir ? "dir" : "file", item.name);
        }

        if (expected_items != NULL) {
          if (!tk_str_eq(expected_items, str.str)) {
            log_debug("dir_list failed: %s.\n", name);
          }
        } else {
          log_debug("%s\n", str.str);
        }
        str_reset(&str);
        fs_dir_close(dir);
      } else {
        log_debug("dir_list failed: %s.\n", name);
      }
    } else if (tk_str_eq(name, "remove_local_file")) {
      const char* name = conf_node_get_child_value_str(iter, "name", NULL);
      if (fs_remove_file(os_fs(), name) == RET_OK) {
        log_debug("remove %s.\n", name);
      } else {
        log_debug("remove %s fail.\n", name);
      }
    } else if (tk_str_eq(name, "download")) {
      const char* remote = conf_node_get_child_value_str(iter, "remote", NULL);
      const char* local = conf_node_get_child_value_str(iter, "local", NULL);
      int32_t expected_size = conf_node_get_child_value_int32(iter, "size", -1);

      fs_file_t* from_file = fs_open_file(fs, remote, "rb");
      fs_file_t* to_file = fs_open_file(os_fs(), local, "wb+");
      if (from_file != NULL) {
        int ret = 0;
        char buff[1024] = {0};
        if (to_file != NULL) {
          while ((ret = fs_file_read(from_file, buff, sizeof(buff))) > 0) {
            fs_file_write(to_file, buff, ret);
          }
          fs_file_close(to_file);
        }
        fs_file_close(from_file);
        
        if(expected_size >= 0) {
          if(file_get_size(local) != expected_size) {
            log_debug("download %s failed\n", remote);
          } else {
            log_debug("download %s => %s\n", remote, local);
          }
        } else {
          log_debug("download %s => %s\n", remote, local);
        }
      }
    } else if (tk_str_eq(name, "upload")) {
      const char* remote = conf_node_get_child_value_str(iter, "remote", NULL);
      const char* local = conf_node_get_child_value_str(iter, "local", NULL);
      fs_file_t* from_file = fs_open_file(os_fs(), local, "rb");
      fs_file_t* to_file = fs_open_file(fs, remote, "wb+");
      if (from_file != NULL) {
        int ret = 0;
        char buff[1024] = {0};
        if (to_file != NULL) {
          while ((ret = fs_file_read(from_file, buff, sizeof(buff))) > 0) {
            fs_file_write(to_file, buff, ret);
          }
        }
        fs_file_close(from_file);
      }
      
      if (to_file != NULL) {
         fs_file_close(to_file);
      }
    } else if (tk_str_eq(name, "stat")) {
      fs_stat_info_t info;
      const char* name = conf_node_get_child_value_str(iter, "name", NULL);
      int32_t expected_size = conf_node_get_child_value_int32(iter, "size", -1);

      ret_t ret = fs_stat(fs, name, &info);
      if (ret == RET_OK) {
        if (expected_size >= 0 && expected_size != info.size) {
          log_debug("stat failed: %s %d %d\n", name, expected_size, (int)(info.size));
        } else {
          log_debug("stat: %s %d\n", name, (int)(info.size));
        }
      } else {
        log_debug("stat failed: %d\n", ret);
      }
    } else if (tk_str_eq(name, "sleep")) {
      int32_t time_ms = conf_node_get_child_value_int32(iter, "time", 1000);
      sleep_ms(time_ms);
    } else if (tk_str_eq(name, "close")) {
      ftp_fs_destroy(fs);
      fs = NULL;
    }

    iter = iter->next;

    if (iter == NULL) {
      iter = conf_node_get_first_child(doc->root);
      times--;
      log_debug("=============%u===============\n", times);
    }

    if (times == 0) {
      break;
    }
  }

  if (fs != NULL) {
    ftp_fs_destroy(fs);
  }
}

int main(int argc, char* argv[]) {
  char* data = NULL;
  conf_doc_t* doc = NULL;
  const char* input = argc > 1 ? argv[1] : "data/fs_default.ini";
  uint32_t times = argc > 2 ? tk_atoi(argv[2]) : 1;

  platform_prepare();

  if (argc < 2) {
    log_debug("Usage: %s config times\n", argv[0]);
    log_debug(" ex: %s data/tcp.ini\n", argv[0]);
    log_debug(" ex: %s data/tcp.ini 10\n", argv[0]);
    return 0;
  }

  tk_socket_init();

  data = (char*)file_read(input, NULL);
  if (data != NULL) {
    doc = conf_doc_load_ini(data);
    if (doc != NULL) {
      run_script(doc, times);
      conf_doc_destroy(doc);
    }
    TKMEM_FREE(data);
  }

  tk_socket_deinit();

  return 0;
}
