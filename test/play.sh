#!/bin/bash
# Starts the server on the desert map and a client that connects to it. Closing the client stops
# the server. Any argument goes to the client (./play.sh --windowed --name Pepe); PORT=N changes the port.
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT=${PORT:-7777}
"$DIR/server" --port "$PORT" --map "Desierto de dia" &
SERVER=$!
trap 'kill "$SERVER" 2>/dev/null; wait "$SERVER" 2>/dev/null' EXIT INT TERM
# wait until the server listens (it loads the map first)
for _ in $(seq 1 300); do
  kill -0 "$SERVER" 2>/dev/null || { echo "The server stopped" >&2; exit 1; }
  (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null && break
  sleep 0.2
done
"$DIR/client" --connect "127.0.0.1:$PORT" "$@"
