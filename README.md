# CPP Chat

Simple two-person TCP chat over the internet. One server accepts a single client. Messages are plaintext.

## Project layout

- bin/ - compiled binaries
- src/ - source code

## Build

From the repo root:

```bash
g++ -std=c++17 src/server.cpp -o bin/chatserver -pthread
g++ -std=c++17 src/client.cpp -o bin/chatclient -pthread
```

## Run

Start the server (choose a port):

```bash
./bin/chatserver 1500
```

Connect a client (same machine):

```bash
./bin/chatclient 127.0.0.1 1500
```

Connect a client (over the internet):

```bash
./bin/chatclient <server_public_ip> 1500
```

Type messages and press Enter. Use /quit to exit.

## Notes

- This is plain TCP without encryption. Do not use it for sensitive data.
- To chat over the internet, ensure the server port is reachable (VPS or port forwarding).
