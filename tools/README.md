# 启动 ftp 服务器

## 1. python 提供的 ftp 服务器

* 安装

```
pip install pyftpdlib
```

* 修改配置

编辑 start_ftpd.py， 根据需要修改 ftp_root 和 port，默认端口为 2121。

* 运行

```
python3 start_ftpd.py
```

## 2. Linux 的 vsftpd 服务器

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

## 3. nodejs 提供的  ftp 服务器

* 安装

```
npm install -g ftp-srv
```

* 修改配置

编辑 start_ftpd.js， 根据需要修改 ftp_root 和 port，默认端口为 2121。

* 运行

```
node start_ftpd.js
```
