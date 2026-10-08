#ifndef NET_ROLE
#define NET_ROLE

// Which side of the network this program is. The server rules the world; a client keeps a copy of
// it that shows what the server says. The code that makes the world is the same on both (so that
// their objects match, see Stage::getNetId) and asks this where the two must differ: who decides
// who dies, who thinks for the creatures...
enum class NetRole { Server, Client };

// Set once, by main, before anything is made
void setNetRole(NetRole role);
NetRole netRole();

#endif
