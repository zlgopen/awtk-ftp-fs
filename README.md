# awtk-ftp-fs

## 1. 介绍

将 FTP 服务封装成 AWTK 文件系统，通过文件系统的方式访问 FTP 服务器。

由于其中使用了 XSTAT 扩展命令，目前只能和 [AWTK-FTPD](https://github.com/zlgopen/awtk-ftpd.git) 配合使用。

## 2 准备

### 2.1 获取 awtk 并编译

```
git clone https://github.com/zlgopen/awtk.git
cd awtk; scons; cd -
```

### 2.2 获取 awtk-ftp-fs 并编译

```
git clone https://github.com/zlgopen/awtk-ftp-fs.git
cd awtk-ftp-fs
```

* 编译 PC 版本

```
scons
```

* 编译 LINUX FB 版本

```
scons LINUX_FB=true
```

> 完整编译选项请参考 [编译选项](https://github.com/zlgopen/awtk-widget-generator/blob/master/docs/build_options.md)

## 3. 运行

```
./bin/ftp_cli data/download.ini
```

## 4. 相关项目

* [嵌入式 WEB 服务器  awtk-restful-httpd](https://github.com/zlgopen/awtk-restful-httpd)
* [嵌入式 FTP 服务器  awtk-restful-httpd](https://github.com/zlgopen/awtk-ftpd)
