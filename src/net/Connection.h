#ifndef NET_CONNECTION
#define NET_CONNECTION

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "NetBuffer.h"

// One end of a TCP connection that never blocks: the game loop calls update() every frame, which
// sends what is waiting and receives what has arrived; whole messages are then taken with next().
// A message is a type byte and a payload (NetWriter); on the wire it is framed with its length.
//
// TCP (reliable, in order) keeps the protocol simple: a lost snapshot never has to be told apart
// from a lost "the player died". Nagle's algorithm is off, so small input messages leave at once.
class NetConnection {
public:
  struct Message {
    uint8_t type = 0;
    std::vector<uint8_t> payload;
  };

private:
  int fd = -1;
  bool connecting = false;
  bool open = false;
  std::string error;
  std::vector<uint8_t> inbox, outbox;
  size_t outboxSent = 0;

  explicit NetConnection(int fd, bool connecting) : fd(fd), connecting(connecting), open(true) {}
  void finishConnecting();

public:
  // The server's limit for a client that does not keep up: past this many bytes waiting to be
  // sent the connection is dropped
  static constexpr size_t MAX_OUTBOX = 4u << 20;
  static constexpr uint32_t MAX_MESSAGE = 1u << 20;

  NetConnection(const NetConnection &) = delete;
  NetConnection &operator=(const NetConnection &) = delete;
  ~NetConnection();

  // Starts connecting to host:port (a name or an address). nullptr, and `error` says why, if it
  // can't even start; else the connection is "connecting" until isConnecting() turns false (then
  // isOpen() says whether it worked)
  static std::unique_ptr<NetConnection> connectTo(const std::string &host, int port, std::string &error);

  bool isConnecting() const { return connecting; }
  bool isOpen() const { return open; }
  const std::string &getError() const { return error; }
  // Where the other end is (for the log)
  std::string peerName() const;

  void send(uint8_t type, const NetWriter &payload);
  void send(uint8_t type) { send(type, NetWriter()); }
  // Sends and receives what the network lets it, without waiting
  void update();
  // The next whole message that has arrived; false if there is none
  bool next(Message &message);
  // Bytes waiting to leave
  size_t pendingBytes() const { return outbox.size() - outboxSent; }
  void close();

  friend class NetListener;
};

// The server's door: accepts the clients that connect to its port
class NetListener {
private:
  int fd = -1;

public:
  ~NetListener();
  // false (and `error`) if the port can't be listened on
  bool listen(int port, std::string &error);
  // A client that has connected, or nullptr if none is waiting
  std::unique_ptr<NetConnection> accept();
};

#endif
