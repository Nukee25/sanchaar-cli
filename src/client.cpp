#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std;
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socket_len_t = int;
  #pragma comment(lib, "Ws2_32.lib")
#else
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <unistd.h>
  using socket_len_t = socklen_t;
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR (-1)
  using SOCKET = int;
#endif

namespace {
void close_socket(SOCKET s) {
#ifdef _WIN32
  closesocket(s);
#else
  close(s);
#endif
}

bool init_sockets() {
#ifdef _WIN32
  WSADATA wsa_data{};
  return WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0;
#else
  return true;
#endif
}

void cleanup_sockets() {
#ifdef _WIN32
  WSACleanup();
#endif
}
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    cerr << "Usage: chat_client <host> <port>\n";
    return 1;
  }

  if (!init_sockets()) {
    cerr << "Socket init failed.\n";
    return 1;
  }

  const char* host = argv[1];
  const char* port = argv[2];

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* result = nullptr;
  int rc = getaddrinfo(host, port, &hints, &result);
  if (rc != 0 || result == nullptr) {
#ifdef _WIN32
    cerr << "getaddrinfo failed: " << rc << "\n";
#else
    cerr << "getaddrinfo failed: " << gai_strerror(rc) << "\n";
#endif
    cleanup_sockets();
    return 1;
  }

  SOCKET sock = INVALID_SOCKET;
  for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
    sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (sock == INVALID_SOCKET) {
      continue;
    }
    if (connect(sock, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
      break;
    }
    close_socket(sock);
    sock = INVALID_SOCKET;
  }
  freeaddrinfo(result);

  if (sock == INVALID_SOCKET) {
    cerr << "Unable to connect.\n";
    cleanup_sockets();
    return 1;
  }

  cout << "Connected. Type messages. /quit to exit.\n> " << flush;

  atomic<bool> connected{true};
  thread recv_thread([&]() {
    vector<char> buffer(4096);
    while (connected) {
      int bytes = recv(sock, buffer.data(), static_cast<int>(buffer.size()), 0);
      if (bytes <= 0) {
        connected = false;
        break;
      }
      cout << "\n[server] " << string(buffer.data(), bytes) << "\n> " << flush;
    }
  });

  string line;
  while (connected && getline(cin, line)) {
    if (line == "/quit") {
      break;
    }
    line.push_back('\n');
    int sent = send(sock, line.data(), static_cast<int>(line.size()), 0);
    if (sent == SOCKET_ERROR) {
      cerr << "send failed.\n";
      break;
    }
    cout << "> " << flush;
  }

  connected = false;
  close_socket(sock);
  if (recv_thread.joinable()) {
    recv_thread.join();
  }

  cleanup_sockets();
  cout << "Client stopped.\n";
  return 0;
}
