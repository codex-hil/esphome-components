#pragma once
#include <memory>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
namespace esphome::socket {
class Socket {
 public:
  explicit Socket(int fd):fd_(fd) {}
  ~Socket() { ::close(fd_); }
  int setblocking(bool b) { return fcntl(fd_, F_SETFL, b ? 0 : O_NONBLOCK); }
  int setsockopt(int l,int o,const void *v,socklen_t n) { return ::setsockopt(fd_,l,o,v,n); }
  int bind(const sockaddr *a,socklen_t n) { return ::bind(fd_,a,n); }
  int listen(int n) { return ::listen(fd_,n); }
  std::unique_ptr<Socket> accept(sockaddr *a,socklen_t *n) { int f=::accept(fd_,a,n); return f<0 ? nullptr : std::make_unique<Socket>(f); }
  ssize_t read(void *b,size_t n) { return ::read(fd_,b,n); }
  ssize_t write(const void *b,size_t n) { return ::send(fd_,b,n,MSG_NOSIGNAL); }
 private:
  int fd_;
};
using ListenSocket=Socket;
inline std::unique_ptr<ListenSocket> socket_listen(int d,int t,int p) {
  int f=::socket(d,t,p); return f<0 ? nullptr : std::make_unique<ListenSocket>(f);
}
}
