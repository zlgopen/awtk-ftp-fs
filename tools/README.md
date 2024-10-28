# 启动 ftp 服务器

> 本文介绍了一些启动 ftp 服务器的方法，用于测试 ftp 客户端的功能。下面介绍的任意一种都可以，选择一种适合自己的即可，不需要全部都启动。

## 1. python 提供的 ftp 服务器

* 安装

```
pip install pyftpdlib
```

* 修改配置

编辑 start_ftpd.py， 根据需要修改 ftp_root 和 port，默认端口为 2121。

* 运行(在当前目录下)

> Linux 下需要使用 python3 运行。

```
python3 start_ftpd.py
```

> Windows 下需要使用 python 运行。

```
python start_ftpd.py
```

## 2. nodejs 提供的  ftp 服务器

> 先安装 nodejs，然后安装 ftp-srv。

* 安装

```
npm install
```

* 修改配置

编辑 start_ftpd.js， 根据需要修改 ftp_root 和 port，默认端口为 2121。

* 运行(在当前目录下)

```
node start_ftpd.js
```

## 3. Linux 的 vsftpd 服务器

* 安装

```bash
sudo apt install vsftpd
```

* 根据需要修改配置

```
/etc/vsftpd.conf
```

默认端口为 21，如果需要修改端口，可以修改配置文件中的 listen_port。

```ini
listen_port=2121
```

* 启动服务

````
sudo systemctl restart vsftpd
```