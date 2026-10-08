#include "NetRole.h"

static NetRole role = NetRole::Server;

void setNetRole(NetRole r) { role = r; }
NetRole netRole() { return role; }
