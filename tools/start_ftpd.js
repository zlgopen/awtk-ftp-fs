const FtpSrv = require('ftp-srv');

const hostname = '127.0.0.1'; // 本地地址，可以替换为你的服务器地址
const port = 2121;
const passiveUrl = 'ftp://your-public-ip'; // 你的服务器的公共 IP 地址

const ftpServer = new FtpSrv({
  url: `ftp://${hostname}:${port}`,
  anonymous: true, // 设置为 true 以允许匿名登录，或设置为 false 并添加用户验证逻辑
  pasv_url: passiveUrl, // 配置被动模式的 URL
  pasv_min: 1024, // 被动模式端口范围的最小值
  pasv_max: 1048 // 被动模式端口范围的最大值
});

ftpServer.on('login', ({connection, username, password}, resolve, reject) => {
  // 在这里你可以添加用户验证逻辑
  if (username === 'admin' && password === 'admin') {
    // 如果验证成功，提供用户的根目录
    resolve({root: './'});
  } else {
    // 如果验证失败，拒绝登录
    reject(new Error('Invalid username or password'));
  }
});

// 启动 FTP 服务器
ftpServer.listen()
  .then(() => {
    console.log(`FTP server is running at ftp://${hostname}:${port}`);
  })
  .catch(err => {
    console.error('Error starting FTP server:', err);
  });

