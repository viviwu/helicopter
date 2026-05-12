/**
 **************************************************************************
 * @file    :SocketUtil.cpp
 * @author  :viviwu
 * @brief   :Socket工具实现
 * @version :0.1
 * @date    :5/12/26 PM3:39
 * **************************************************************************
 */

#include "SocketUtil.h"
#include "../base/Logger.h"
#include <assert.h>
#include <string.h>

namespace gateway {
namespace sockets {

int CreateNonblockingOrDie() {
#ifdef __linux__
  int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
#else
  int sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd >= 0) {
    SetNonBlockAndCloseOnExec(sockfd);
  }
#endif
  if (sockfd < 0) {
    LOG_FATAL("sockets::CreateNonblockingOrDie");
  }
  return sockfd;
}

void BindOrDie(int sockfd, const struct sockaddr_in& addr) {
  int ret = ::bind(sockfd, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr));
  if (ret < 0) {
    LOG_FATAL("sockets::BindOrDie");
  }
}

void ListenOrDie(int sockfd) {
  int ret = ::listen(sockfd, SOMAXCONN);
  if (ret < 0) {
    LOG_FATAL("sockets::ListenOrDie");
  }
}

int Accept(int sockfd, struct sockaddr_in* addr) {
  socklen_t addrlen = sizeof(*addr);
#ifdef __linux__
  int connfd = ::accept4(sockfd, reinterpret_cast<struct sockaddr*>(addr),
                         &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
  int connfd = ::accept(sockfd, reinterpret_cast<struct sockaddr*>(addr), &addrlen);
  if (connfd >= 0) {
    SetNonBlockAndCloseOnExec(connfd);
  }
#endif
  if (connfd < 0) {
    int savedErrno = errno;
    LOG_ERROR("Socket::accept");
    switch (savedErrno) {
      case EAGAIN:
      case ECONNABORTED:
      case EINTR:
      case EPROTO:
      case EPERM:
      case EMFILE:
        errno = savedErrno;
        break;
      case EBADF:
      case EFAULT:
      case EINVAL:
      case ENFILE:
      case ENOBUFS:
      case ENOMEM:
      case ENOTSOCK:
      case EOPNOTSUPP:
        LOG_FATAL("unexpected error of ::accept4 {}", savedErrno);
        break;
      default:
        LOG_FATAL("unknown error of ::accept4 {}", savedErrno);
        break;
    }
  }
  return connfd;
}

void Close(int sockfd) {
  if (::close(sockfd) < 0) {
    LOG_ERROR("sockets::close");
  }
}

void ShutdownWrite(int sockfd) {
  if (::shutdown(sockfd, SHUT_WR) < 0) {
    LOG_ERROR("sockets::shutdown");
  }
}

void SetNonBlockAndCloseOnExec(int sockfd) {
  int flags = ::fcntl(sockfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  int ret = ::fcntl(sockfd, F_SETFL, flags);
  if (ret < 0) {
    LOG_ERROR("SetNonBlock failed");
  }

  flags = ::fcntl(sockfd, F_GETFD, 0);
  flags |= FD_CLOEXEC;
  ret = ::fcntl(sockfd, F_SETFD, flags);
  if (ret < 0) {
    LOG_ERROR("SetCloseOnExec failed");
  }
}

void SetReuseAddr(int sockfd, bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void SetReusePort(int sockfd, bool on) {
#ifdef SO_REUSEPORT
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
  if (ret < 0 && on) {
    LOG_ERROR("SO_REUSEPORT failed.");
  }
#else
  (void)sockfd;
  (void)on;
#endif
}

void SetTcpNoDelay(int sockfd, bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void SetKeepAlive(int sockfd, bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

struct sockaddr_in GetLocalAddr(int sockfd) {
  struct sockaddr_in localaddr;
  memset(&localaddr, 0, sizeof(localaddr));
  socklen_t addrlen = sizeof(localaddr);
  if (::getsockname(sockfd, reinterpret_cast<struct sockaddr*>(&localaddr), &addrlen) < 0) {
    LOG_ERROR("sockets::getLocalAddr");
  }
  return localaddr;
}

struct sockaddr_in GetPeerAddr(int sockfd) {
  struct sockaddr_in peeraddr;
  memset(&peeraddr, 0, sizeof(peeraddr));
  socklen_t addrlen = sizeof(peeraddr);
  if (::getpeername(sockfd, reinterpret_cast<struct sockaddr*>(&peeraddr), &addrlen) < 0) {
    LOG_ERROR("sockets::getPeerAddr");
  }
  return peeraddr;
}

int GetSocketError(int sockfd) {
  int optval;
  socklen_t optlen = sizeof(optval);
  if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
    return errno;
  } else {
    return optval;
  }
}

bool IsSelfConnect(int sockfd) {
  struct sockaddr_in localaddr = GetLocalAddr(sockfd);
  struct sockaddr_in peeraddr = GetPeerAddr(sockfd);
  return localaddr.sin_port == peeraddr.sin_port &&
         localaddr.sin_addr.s_addr == peeraddr.sin_addr.s_addr;
}

std::string ToIp(const struct sockaddr_in& addr) {
  char buf[64] = "";
  ::inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
  return buf;
}

std::string ToIpPort(const struct sockaddr_in& addr) {
  char buf[64] = "";
  ::inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
  size_t end = ::strlen(buf);
  uint16_t port = ntohs(addr.sin_port);
  snprintf(buf + end, sizeof(buf) - end, ":%u", port);
  return buf;
}

void FromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr) {
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0) {
    LOG_ERROR("sockets::fromIpPort");
  }
}

}
}
