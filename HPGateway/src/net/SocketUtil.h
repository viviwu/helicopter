/**
 **************************************************************************
 * @file    :SocketUtil.h
 * @author  :viviwu
 * @brief   :Socket工具函数
 * @version :0.1
 * @date    :5/12/26 PM3:39
 * **************************************************************************
 */

#ifndef HELICOPTER_SOCKETUTIL_H
#define HELICOPTER_SOCKETUTIL_H

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstdint>
#include <string>

namespace gateway {
namespace sockets {

int CreateNonblockingOrDie();
void BindOrDie(int sockfd, const struct sockaddr_in& addr);
void ListenOrDie(int sockfd);
int Accept(int sockfd, struct sockaddr_in* addr);
void Close(int sockfd);
void ShutdownWrite(int sockfd);

void SetNonBlockAndCloseOnExec(int sockfd);
void SetReuseAddr(int sockfd, bool on);
void SetReusePort(int sockfd, bool on);
void SetTcpNoDelay(int sockfd, bool on);
void SetKeepAlive(int sockfd, bool on);

struct sockaddr_in GetLocalAddr(int sockfd);
struct sockaddr_in GetPeerAddr(int sockfd);
int GetSocketError(int sockfd);
bool IsSelfConnect(int sockfd);

std::string ToIp(const struct sockaddr_in& addr);
std::string ToIpPort(const struct sockaddr_in& addr);
void FromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr);

}
}

#endif  // HELICOPTER_SOCKETUTIL_H
