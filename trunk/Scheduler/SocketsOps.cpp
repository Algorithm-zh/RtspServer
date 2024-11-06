#include "SocketsOps.h"
#include <fcntl.h>
#include <sys/types.h> /* See NOTES */
#ifndef WIN32
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <unistd.h>
#endif // !WIN32
#include "../Base/Log.h"
int sockets::createTcpSock() {

// SOCK_CLOEXEC。
// 作用：设置套接字文件描述符的 O_CLOEXEC 标志。这表示在执行 exec
// 系统调用时，该文件描述符将自动关闭，防止子进程继承不必要的文件描述符，提高安全性。
#ifndef WIN32
  int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                        IPPROTO_TCP);

#else
  int sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  unsigned long ul = 1;
  int ret = ioctlsocket(sockfd, FIONBIO, (unsigned long *)&ul);

  if (ret == SOCKET_ERROR) {
    LOGE("设置非阻塞失败");
  }
#endif

  return sockfd;
}

int sockets::createUdpSock() {

#ifndef WIN32
  int sockfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
#else
  int sockfd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  unsigned long ul = 1;
  int ret = ioctlsocket(sockfd, FIONBIO, (unsigned long *)&ul);

  if (ret == SOCKET_ERROR) {
    LOGE("设置非阻塞失败");
  }
#endif

  return sockfd;
}

bool sockets::bind(int sockfd, std::string ip, uint16_t port) {

  // SOCKADDR_IN server_addr;
  // server_addr.sin_family = AF_INET;
  // server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
  ////server_addr.sin_addr.s_addr = inet_addr("192.168.2.61");
  // server_addr.sin_port = htons(port);

  ////s_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  // if (::bind(sockfd, (SOCKADDR*)&server_addr, sizeof(SOCKADDR)) ==
  // SOCKET_ERROR) {
  //
  //     return false;
  // }
  // else {
  //     return true;
  // }

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(ip.c_str());
  //    addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (::bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    LOGE("::bind error,fd=%d,ip=%s,port=%d", sockfd, ip.c_str(), port);
    return false;
  }
  return true;
}

bool sockets::listen(int sockfd, int backlog) {
  if (::listen(sockfd, backlog) < 0) {
    LOGE("::listen error,fd=%d,backlog=%d", sockfd, backlog);
    return false;
  }
  return true;
}

int sockets::accept(int sockfd) {
  struct sockaddr_in addr = {0};
  socklen_t addrlen = sizeof(struct sockaddr_in);

  int connfd = ::accept(sockfd, (struct sockaddr *)&addr, &addrlen);
  setNonBlockAndCloseOnExec(connfd);
  ignoreSigPipeOnSocket(connfd);

  return connfd;
}

int sockets::write(int sockfd, const void *buf, int size) {

#ifndef WIN32
  return ::write(sockfd, buf, size);
#else
  return ::send(sockfd, (char *)buf, size, 0);
#endif
}

int sockets::sendto(int sockfd, const void *buf, int len,
                    const struct sockaddr *destAddr) {
  socklen_t addrLen = sizeof(struct sockaddr);
  return ::sendto(sockfd, (char *)buf, len, 0, destAddr, addrLen);
}

int sockets::setNonBlock(int sockfd) {
#ifndef WIN32
  int flags = fcntl(sockfd, F_GETFL, 0);
  fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

  return 0;
#else

  unsigned long ul = 1;
  int ret = ioctlsocket(sockfd, FIONBIO, (unsigned long *)&ul); // 设置非阻塞

  if (ret == SOCKET_ERROR) {
    return -1;
  } else {
    return 0;
  }
#endif
}

int sockets::setBlock(int sockfd, int writeTimeout) {
#ifndef WIN32
  int flags = fcntl(sockfd, F_GETFL, 0);
  fcntl(sockfd, F_SETFL, flags & (~O_NONBLOCK));

  if (writeTimeout > 0) {
    struct timeval tv = {writeTimeout / 1000, (writeTimeout % 1000) * 1000};
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
  }
#endif

  return 0;
}

void sockets::setReuseAddr(int sockfd, int on) {
  int optval = on ? 1 : 0;
  // 通用的套接字选项（不特定于某个协议），地址重用选项，启用地址重用
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval,
             sizeof(optval));
}

void sockets::setReusePort(int sockfd) {
#ifdef SO_REUSEPORT
  int on = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char *)&on, sizeof(on));
#endif
}

void sockets::setNonBlockAndCloseOnExec(int sockfd) {
#ifndef WIN32
  // 设置套接字为非阻塞模式
  {
    // 获取当前套接字的文件状态标志
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
      // 如果获取失败，输出错误信息
      perror("fcntl F_GETFL error");
      return;
    }

    // 设置非阻塞标志
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);
    if (ret == -1) {
      // 如果设置失败，输出错误信息
      perror("fcntl F_SETFL error");
      return;
    }
  }

  // 设置套接字在 exec 时自动关闭
  {
    // 获取当前套接字的文件描述符标志
    int flags = ::fcntl(sockfd, F_GETFD, 0);
    if (flags == -1) {
      // 如果获取失败，输出错误信息
      perror("fcntl F_GETFD error");
      return;
    }

    // 设置 close-on-exec 标志
    flags |= FD_CLOEXEC;
    int ret = ::fcntl(sockfd, F_SETFD, flags);
    if (ret == -1) {
      // 如果设置失败，输出错误信息
      perror("fcntl F_SETFD error");
      return;
    }
  }
#endif
}

// 忽略 SIGPIPE 信号：防止因对端关闭连接而引发的 SIGPIPE 信号。
// 启用 Nagle 算法禁用：禁用 Nagle 算法，减少延迟，适用于需要快速响应的场景。
// 启用 TCP
// 保活机制：定期发送探测包以检测连接是否仍然有效，防止长时间无数据传输导致的连接断开。
// 禁止发送 SIGPIPE 信号：防止因对端关闭连接而引发的 SIGPIPE
// 信号，适用于需要优雅处理连接关闭的场景。

// 忽略 SIGPIPE 信号，防止因对端关闭连接而引发的 SIGPIPE 信号
void sockets::ignoreSigPipeOnSocket(int socketfd) {
#ifndef WIN32
  int option = 1;
  setsockopt(socketfd, SOL_SOCKET, MSG_NOSIGNAL, &option, sizeof(option));
#endif
}

// 启用 Nagle 算法禁用，即开启 TCP_NODELAY 选项
void sockets::setNoDelay(int sockfd) {
#ifdef TCP_NODELAY
  int on = 1;
  int ret =
      setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on));
#endif
}

// 启用 TCP 保活机制
void sockets::setKeepAlive(int sockfd) {
  int on = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on));
}

// 禁止发送 SIGPIPE 信号
void sockets::setNoSigpipe(int sockfd) {
#ifdef SO_NOSIGPIPE
  int on = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, (char *)&on, sizeof(on));
#endif
}

void sockets::setSendBufSize(int sockfd, int size) {
  setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size));
}

void sockets::setRecvBufSize(int sockfd, int size) {
  setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size));
}

// 获取对端(即与当前服务器通信的另一方)ip地址
std::string sockets::getPeerIp(int sockfd) {
  struct sockaddr_in addr = {0};
  socklen_t addrlen = sizeof(struct sockaddr_in);
  if (getpeername(sockfd, (struct sockaddr *)&addr, &addrlen) == 0) {
    return inet_ntoa(addr.sin_addr);
  }

  return "0.0.0.0";
}

int16_t sockets::getPeerPort(int sockfd) {
  struct sockaddr_in addr = {0};
  socklen_t addrlen = sizeof(struct sockaddr_in);
  if (getpeername(sockfd, (struct sockaddr *)&addr, &addrlen) == 0) {
    return ntohs(addr.sin_port);
  }

  return 0;
}
// 获取与套接字连接的对端的完整地址信息（包括 IP 地址和端口号）
int sockets::getPeerAddr(int sockfd, struct sockaddr_in *addr) {
  socklen_t addrlen = sizeof(struct sockaddr_in);
  return getpeername(sockfd, (struct sockaddr *)addr, &addrlen);
}

void sockets::close(int sockfd) {
#ifndef WIN32
  int ret = ::close(sockfd);
#else
  int ret = ::closesocket(sockfd);
#endif
}

bool connect(int sockfd, std::string ip, uint16_t port, int timeout) {
  bool isConnected = true;

  // 如果设置了超时时间，将套接字设置为非阻塞模式
  if (timeout > 0) {
    sockets::setNonBlock(sockfd);
  }

  // 初始化服务器地址结构
  struct sockaddr_in addr = {0};
  socklen_t addrlen = sizeof(addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(ip.c_str());

  // 尝试连接到服务器
  if (::connect(sockfd, (struct sockaddr *)&addr, addrlen) < 0) {
    // 如果连接失败
    if (timeout > 0) {
      // 设置连接标志为失败
      isConnected = false;

      // 使用 select 实现超时控制
      fd_set fdWrite;
      FD_ZERO(&fdWrite);
      FD_SET(sockfd, &fdWrite);

      // 设置超时时间
      struct timeval tv = {timeout / 1000, timeout % 1000 * 1000};

      // 调用 select 等待套接字可写
      int ret = select(sockfd + 1, NULL, &fdWrite, NULL, &tv);
      if (ret < 0) {
        perror("select failed");
      } else if (ret == 0) {
        std::perror("Connection timed out");
      } else {
        // 检查套接字是否可写
        if (FD_ISSET(sockfd, &fdWrite)) {
          isConnected = true;
        }
      }

      // 将套接字恢复为阻塞模式,(阻塞模式方便操作，非阻塞模式还得加各种判断处理等)
      sockets::setBlock(sockfd, O_NONBLOCK);
    } else {
      // 如果没有设置超时时间，直接返回连接失败
      isConnected = false;
    }
  }

  return isConnected;
}

std::string sockets::getLocalIp() { return "0.0.0.0"; }
