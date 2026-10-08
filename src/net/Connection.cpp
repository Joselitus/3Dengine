#include "Connection.h"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

static void makeNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void noDelay(int fd) {
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
}

NetConnection::~NetConnection() { close(); }

void NetConnection::close() {
  if (fd >= 0)
    ::close(fd);
  fd = -1;
  open = false;
  connecting = false;
}

std::unique_ptr<NetConnection> NetConnection::connectTo(const std::string &host, int port,
                                                        std::string &error) {
  signal(SIGPIPE, SIG_IGN); // (a closed connection is an error code, not a signal)
  addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo *found = nullptr;
  int rc = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found);
  if (rc != 0) {
    error = std::string("No encuentro el servidor: ") + gai_strerror(rc);
    return nullptr;
  }
  int fd = -1;
  for (addrinfo *a = found; a; a = a->ai_next) {
    fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
    if (fd < 0)
      continue;
    makeNonBlocking(fd);
    noDelay(fd);
    if (::connect(fd, a->ai_addr, a->ai_addrlen) == 0 || errno == EINPROGRESS)
      break;
    ::close(fd);
    fd = -1;
  }
  freeaddrinfo(found);
  if (fd < 0) {
    error = std::string("No puedo conectar: ") + std::strerror(errno);
    return nullptr;
  }
  return std::unique_ptr<NetConnection>(new NetConnection(fd, true));
}

std::string NetConnection::peerName() const {
  sockaddr_storage addr;
  socklen_t length = sizeof(addr);
  if (fd < 0 || getpeername(fd, reinterpret_cast<sockaddr *>(&addr), &length) != 0)
    return "?";
  char text[INET6_ADDRSTRLEN] = "?";
  int port = 0;
  if (addr.ss_family == AF_INET) {
    auto *a = reinterpret_cast<sockaddr_in *>(&addr);
    inet_ntop(AF_INET, &a->sin_addr, text, sizeof(text));
    port = ntohs(a->sin_port);
  } else if (addr.ss_family == AF_INET6) {
    auto *a = reinterpret_cast<sockaddr_in6 *>(&addr);
    inet_ntop(AF_INET6, &a->sin6_addr, text, sizeof(text));
    port = ntohs(a->sin6_port);
  }
  return std::string(text) + ":" + std::to_string(port);
}

void NetConnection::finishConnecting() {
  int result = 0;
  socklen_t length = sizeof(result);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &length) != 0)
    result = errno;
  if (result == EINPROGRESS)
    return;
  if (result != 0) {
    error = std::string("No puedo conectar: ") + std::strerror(result);
    close();
    return;
  }
  connecting = false;
}

void NetConnection::send(uint8_t type, const NetWriter &payload) {
  if (!open)
    return;
  uint32_t length = (uint32_t)payload.size() + 1;
  const uint8_t *l = reinterpret_cast<const uint8_t *>(&length);
  outbox.insert(outbox.end(), l, l + sizeof(length));
  outbox.push_back(type);
  outbox.insert(outbox.end(), payload.data().begin(), payload.data().end());
  if (pendingBytes() > MAX_OUTBOX) {
    error = "El otro extremo no recibe lo bastante deprisa";
    close();
  }
}

void NetConnection::update() {
  if (fd < 0)
    return;
  if (connecting) {
    // A connect in progress is done when the socket becomes writable
    fd_set writable;
    FD_ZERO(&writable);
    FD_SET(fd, &writable);
    timeval none = {0, 0};
    if (select(fd + 1, nullptr, &writable, nullptr, &none) > 0)
      finishConnecting();
    if (connecting || fd < 0)
      return;
  }
  // Send
  while (outboxSent < outbox.size()) {
    ssize_t n = ::send(fd, outbox.data() + outboxSent, outbox.size() - outboxSent, MSG_NOSIGNAL);
    if (n > 0) {
      outboxSent += (size_t)n;
    } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
      break;
    } else {
      error = "Se ha perdido la conexion";
      close();
      return;
    }
  }
  if (outboxSent == outbox.size()) {
    outbox.clear();
    outboxSent = 0;
  } else if (outboxSent > (1u << 16)) { // (forget what has gone)
    outbox.erase(outbox.begin(), outbox.begin() + outboxSent);
    outboxSent = 0;
  }
  // Receive
  uint8_t chunk[16384];
  for (;;) {
    ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
    if (n > 0) {
      inbox.insert(inbox.end(), chunk, chunk + n);
    } else if (n == 0) {
      error = "El otro extremo ha cerrado la conexion";
      close();
      return;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;
    } else if (errno != EINTR) {
      error = std::string("Se ha perdido la conexion: ") + std::strerror(errno);
      close();
      return;
    }
  }
}

bool NetConnection::next(Message &message) {
  if (inbox.size() < sizeof(uint32_t))
    return false;
  uint32_t length;
  std::memcpy(&length, inbox.data(), sizeof(length));
  if (length == 0 || length > MAX_MESSAGE) {
    error = "Mensaje de red no valido";
    close();
    inbox.clear();
    return false;
  }
  if (inbox.size() < sizeof(length) + length)
    return false;
  message.type = inbox[sizeof(length)];
  message.payload.assign(inbox.begin() + sizeof(length) + 1, inbox.begin() + sizeof(length) + length);
  inbox.erase(inbox.begin(), inbox.begin() + sizeof(length) + length);
  return true;
}

// ---------------------------------------------------------------- listener
NetListener::~NetListener() {
  if (fd >= 0)
    ::close(fd);
}

bool NetListener::listen(int port, std::string &error) {
  signal(SIGPIPE, SIG_IGN);
  fd = socket(AF_INET6, SOCK_STREAM, 0);
  bool v6 = fd >= 0;
  if (!v6)
    fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    error = std::string("socket: ") + std::strerror(errno);
    return false;
  }
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  int rc;
  if (v6) {
    int off = 0; // (both IPv6 and IPv4 clients)
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    sockaddr_in6 a;
    std::memset(&a, 0, sizeof(a));
    a.sin6_family = AF_INET6;
    a.sin6_addr = in6addr_any;
    a.sin6_port = htons((uint16_t)port);
    rc = bind(fd, reinterpret_cast<sockaddr *>(&a), sizeof(a));
  } else {
    sockaddr_in a;
    std::memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons((uint16_t)port);
    rc = bind(fd, reinterpret_cast<sockaddr *>(&a), sizeof(a));
  }
  if (rc != 0 || ::listen(fd, 16) != 0) {
    error = std::string("No puedo escuchar en el puerto ") + std::to_string(port) + ": " +
            std::strerror(errno);
    ::close(fd);
    fd = -1;
    return false;
  }
  makeNonBlocking(fd);
  return true;
}

std::unique_ptr<NetConnection> NetListener::accept() {
  if (fd < 0)
    return nullptr;
  int client = ::accept(fd, nullptr, nullptr);
  if (client < 0)
    return nullptr;
  makeNonBlocking(client);
  noDelay(client);
  return std::unique_ptr<NetConnection>(new NetConnection(client, false));
}
