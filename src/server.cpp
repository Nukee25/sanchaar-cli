#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

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
std::atomic<bool> g_running{true};

void handle_signal(int) {
  g_running = false;
}

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
  if (argc < 2) {
    std::cerr << "Usage: chat_server <port>\n";
    return 1;
  }

  std::signal(SIGINT, handle_signal);

  if (!init_sockets()) {
    std::cerr << "Socket init failed.\n";
    return 1;
  }

  const char* port = argv[1];

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  addrinfo* result = nullptr;
  int rc = getaddrinfo(nullptr, port, &hints, &result);
  if (rc != 0 || result == nullptr) {
#ifdef _WIN32
    std::cerr << "getaddrinfo failed: " << rc << "\n";
#else
    std::cerr << "getaddrinfo failed: " << gai_strerror(rc) << "\n";
#endif
    cleanup_sockets();
    return 1;
  }

  SOCKET listen_sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (listen_sock == INVALID_SOCKET) {
    std::cerr << "socket failed.\n";
    freeaddrinfo(result);
    cleanup_sockets();
    return 1;
  }

  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
#ifdef _WIN32
             reinterpret_cast<const char*>(&opt),
#else
             &opt,
#endif
             sizeof(opt));

  if (bind(listen_sock, result->ai_addr, static_cast<int>(result->ai_addrlen)) == SOCKET_ERROR) {
    std::cerr << "bind failed.\n";
    freeaddrinfo(result);
    close_socket(listen_sock);
    cleanup_sockets();
    return 1;
  }
  freeaddrinfo(result);

  if (listen(listen_sock, 1) == SOCKET_ERROR) {
    std::cerr << "listen failed.\n";
    close_socket(listen_sock);
    cleanup_sockets();
    return 1;
  }

  std::cout << "Waiting for a client on port " << port << "...\n";

  sockaddr_storage client_addr{};
  socket_len_t client_len = sizeof(client_addr);
  SOCKET client_sock = accept(listen_sock, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
  if (client_sock == INVALID_SOCKET) {
    std::cerr << "accept failed.\n";
    close_socket(listen_sock);
    cleanup_sockets();
    return 1;
  }

  char host[NI_MAXHOST]{};
  char serv[NI_MAXSERV]{};
  if (getnameinfo(reinterpret_cast<sockaddr*>(&client_addr), client_len,
                  host, sizeof(host), serv, sizeof(serv),
                  NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
    std::cout << "Client connected from " << host << ":" << serv << "\n";
  }

  std::atomic<bool> connected{true};

  std::thread recv_thread([&]() {
    std::vector<char> buffer(4096);
    while (connected && g_running) {
      int bytes = recv(client_sock, buffer.data(), static_cast<int>(buffer.size()), 0);
      if (bytes <= 0) {
        connected = false;
        break;
      }
      std::cout << "\n[client] " << std::string(buffer.data(), bytes) << "\n> " << std::flush;
    }
  });

  std::cout << "Type messages to send. Ctrl+C to quit.\n> " << std::flush;
  std::string line;
  while (g_running && connected && std::getline(std::cin, line)) {
    if (line == "/quit") {
      break;
    }
    line.push_back('\n');
    int sent = send(client_sock, line.data(), static_cast<int>(line.size()), 0);
    if (sent == SOCKET_ERROR) {
      std::cerr << "send failed.\n";
      break;
    }
    std::cout << "> " << std::flush;
  }

  connected = false;
  close_socket(client_sock);
  close_socket(listen_sock);
  if (recv_thread.joinable()) {
    recv_thread.join();
  }

  cleanup_sockets();
  std::cout << "Server stopped.\n";
  return 0;
}
