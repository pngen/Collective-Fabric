#pragma once
// Collective Fabric - minimal Winsock TCP transport for the real multiprocess
// coordinator/worker proof. Windows-only. Socket semantics are explicit: each
// connection is blocking; accept() does not inherit an unintended non-blocking
// state; writes are serialized per connection (one writer mutex); no lock is
// held across a blocking send of another global structure.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef ERROR
#undef ERROR
#endif
#ifdef FAILED
#undef FAILED
#endif
#ifdef DELETE
#undef DELETE
#endif
#ifdef DUPLICATE
#undef DUPLICATE
#endif
#ifdef ABORT
#undef ABORT
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef INVALID_HANDLE_VALUE
#undef INVALID_HANDLE_VALUE
#endif
#include <string>
#include <mutex>
#include <cstdint>

namespace collectivefabric_tools {

inline bool winsock_init() {
  WSADATA d;
  return WSAStartup(MAKEWORD(2, 2), &d) == 0;
}
inline void winsock_cleanup() { WSACleanup(); }

class TcpListener {
public:
  bool listen_port(unsigned short port, std::string& err) {
    sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) { err = "socket failed"; return false; }
    int one = 1;
    ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    if (::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
      err = "bind failed"; ::closesocket(sock_); sock_ = INVALID_SOCKET; return false;
    }
    if (::listen(sock_, 8) == SOCKET_ERROR) { err = "listen failed"; ::closesocket(sock_); sock_ = INVALID_SOCKET; return false; }
    return true;
  }
  SOCKET accept_client(std::string& err) {
    sockaddr_in caddr{};
    int clen = sizeof(caddr);
    SOCKET csock = ::accept(sock_, reinterpret_cast<sockaddr*>(&caddr), &clen);
    if (csock == INVALID_SOCKET) { err = "accept failed"; return INVALID_SOCKET; }
    int one = 1;
    u_long nb = 0;
    ::ioctlsocket(csock, FIONBIO, &nb);  // explicitly blocking, no inherited non-blocking
    ::setsockopt(csock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof(one));
    return csock;
  }
  void close() { if (sock_ != INVALID_SOCKET) { ::closesocket(sock_); sock_ = INVALID_SOCKET; } }
  ~TcpListener() { close(); }

private:
  SOCKET sock_ = INVALID_SOCKET;
};

class TcpConnection {
public:
  explicit TcpConnection(SOCKET s) : sock_(s) {}

  static TcpConnection connect(const std::string& host, unsigned short port, std::string& err) {
    SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { err = "socket failed"; return TcpConnection(INVALID_SOCKET); }
    u_long nb = 0; ::ioctlsocket(s, FIONBIO, &nb);  // blocking
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) { err = "bad host"; ::closesocket(s); return TcpConnection(INVALID_SOCKET); }
    if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
      err = "connect failed"; ::closesocket(s); return TcpConnection(INVALID_SOCKET);
    }
    return TcpConnection(s);
  }

  bool send_all(const std::uint8_t* data, std::size_t len) {
    std::lock_guard<std::mutex> l(write_mu_);
    std::size_t sent = 0;
    while (sent < len) {
      int n = ::send(sock_, reinterpret_cast<const char*>(data + sent), static_cast<int>(len - sent), 0);
      if (n == SOCKET_ERROR) return false;
      sent += static_cast<std::size_t>(n);
    }
    return true;
  }
  int recv_some(std::uint8_t* data, std::size_t len) {
    return ::recv(sock_, reinterpret_cast<char*>(data), static_cast<int>(len), 0);
  }
  void close() { if (sock_ != INVALID_SOCKET) { ::closesocket(sock_); sock_ = INVALID_SOCKET; } }
  ~TcpConnection() { close(); }
  SOCKET get() const noexcept { return sock_; }
  bool valid() const noexcept { return sock_ != INVALID_SOCKET; }

private:
  SOCKET sock_ = INVALID_SOCKET;
  std::mutex write_mu_;
};

} // namespace collectivefabric_tools
