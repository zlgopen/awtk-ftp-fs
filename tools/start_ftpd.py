from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer

def main():
    # 创建一个虚拟用户授权类
    authorizer = DummyAuthorizer()
    ftp_root = "/Users/jim/work/awtk-root/awtk-ftpd"

    # 添加用户权限
    authorizer.add_user("admin", "admin", ftp_root, perm="elradfmw")

    # 创建一个FTP处理类
    handler = FTPHandler
    handler.authorizer = authorizer

    # 启动FTP服务器
    server = FTPServer(("0.0.0.0", 2121), handler)
    server.serve_forever()

if __name__ == "__main__":
    main()
