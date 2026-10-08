#ifndef NET_PROTOCOL
#define NET_PROTOCOL

#include <cstdint>

// The network protocol between the game's server and its clients (TCP: see NetConnection). Every
// message is a type byte and a payload written with NetWriter, in the order given here. The
// server rules the world; the clients send what their players do (their controls and actions)
// and are told what happened (snapshots of the world).
//
// Client -> server
//   Hello     u32 version, string name
//   Input     u32 seq, f32 moveX, f32 moveY, f32 up, f32 yaw, f32 pitch, u8 flags (1: running)
//             what the controls say now, about thirty times a second
//   Use       u16 index, vec3 where   the player used the interactable number `index` of the map
//             (E), standing at `where` (his feet)
//   Action    u8 Action
//   Fire      vec3 eye, vec3 direction   the fire button
//   Command   string text   a console command ("/day"...)
// Server -> client
//   Welcome   u32 version, i32 playerId, u16 map, string mapName, u32 objects (how many dynamic
//             objects the map makes: the client checks its own map has as many), u16 n, n x
//             (i32 id, string name, i32 bodyNetId)   (the players there are, the new one included)
//   Reject    string reason
//   Joined    i32 id, string name, i32 bodyNetId      a new player
//   Left      i32 id
//   Snapshot  see NetServer::writeSnapshot
//   Spawn     i32 netId, u8 kind (NetKind), vec3 where, f32 arg    a mosquito or an egg appeared
//   Despawn   i32 netId      ...went away
//   Notice    string text    something to show the player
namespace Net {

constexpr uint32_t PROTOCOL_VERSION = 1;
constexpr int DEFAULT_PORT = 7777;
constexpr int MAX_PLAYERS = 16;

// The server works in steps of this many seconds, and sends a snapshot every few of them
constexpr double TICK = 1.0 / 60.0;
constexpr int TICKS_PER_SNAPSHOT = 2;
// The clients show the world this many seconds behind the latest snapshot, so that there are two
// of them to move between
constexpr double INTERPOLATION_DELAY = 0.1;
// The clients send their controls at most this often
constexpr double INPUT_INTERVAL = 1.0 / 60.0;

enum ClientMessage : uint8_t {
  C_HELLO = 1,
  C_INPUT,
  C_USE,
  C_ACTION,
  C_FIRE,
  C_COMMAND,
};

enum ServerMessage : uint8_t {
  S_WELCOME = 1,
  S_REJECT,
  S_JOINED,
  S_LEFT,
  S_SNAPSHOT,
  S_NOTICE,
  S_SPAWN,
  S_DESPAWN,
};

// What a player can ask for with a key
enum Action : uint8_t {
  A_LEAVE = 1,       // leave the vehicle / struggle
  A_HEADLIGHTS,      // flashlight, headlights, ray gun
  A_ENGINE,
  A_HANDBRAKE,
  A_VEHICLE_CAMERA,
  A_SHIP_LEGS,
  A_REFUEL,          // fill the RV's tank at a pump
};

// The flags of an object in a snapshot
enum ObjectFlag : uint8_t {
  OBJ_VISIBLE = 1,
  OBJ_COLLIDABLE = 2,
};

// The flags of a player in a snapshot
enum PlayerFlag : uint8_t {
  PLAYER_DEAD = 1,
  PLAYER_ABDUCTED = 2,
  PLAYER_IN_VEHICLE = 4,
  PLAYER_IN_SAUCER = 8,
};

} // namespace Net

#endif
