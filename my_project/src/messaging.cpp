#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "defines.hpp"
#include "messaging.hpp"
#include "pendinglist.hpp"

UDPSocket::UDPSocket(in_addr_t IP, unsigned short port) {
  struct sockaddr_in sk;

  // Creating socket file descriptor
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&sk, 0, sizeof(sk));

  // Filling server information
  sk.sin_family = AF_INET; // IPv4
  sk.sin_addr.s_addr = IP;
  sk.sin_port = port;

  // Bind the socket with the server address
  if (bind(this->sockfd, reinterpret_cast<sockaddr *>(&sk), sizeof(sk)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  // Set socket as non-blocking
  int flags = fcntl(this->sockfd, F_GETFL, 0);
  fcntl(this->sockfd, F_SETFL, flags | O_NONBLOCK);
  return;
}

UDPSocket::~UDPSocket() { close(sockfd); }

ssize_t UDPSocket::unicast(const Parser::Host *host, const char *buffer,
                           ssize_t len, int flags) {

#ifdef DEBUG_MODE
  ttyLog("Sending (full) message to " + host->fullAddressReadable() +
         ", msg: " + buffer);
#endif
  sockaddr_in add;
  add.sin_family = AF_INET;
  add.sin_addr.s_addr = host->ip;
  add.sin_port = host->port;
  return unicast(&add, buffer, len, flags);
}

ssize_t UDPSocket::unicast(sockaddr_in *dest, const char *buffer, ssize_t len,
                           int flags) {
  return sendto(this->sockfd, buffer, len, flags,
                reinterpret_cast<sockaddr *>(dest), sizeof(*dest));
}

ssize_t UDPSocket::recv(sockaddr_in &from, char *buffer, ssize_t len,
                        int flags) {
  socklen_t sk_len(sizeof(from));
  ssize_t ret = recvfrom(sockfd, buffer, len, flags,
                         reinterpret_cast<sockaddr *>(&from), &sk_len);
  if (ret > 0) {
    buffer[ret] = 0;
  } else if (ret == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
    std::cerr << "Error when receiving: " << std::strerror(errno) << std::endl;
  }
  return ret;
}

void ttyLog(std::string message) {
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
  std::cout << "Thread "
            << ": " << message << std::endl;
#else
  std::cout << "Thread " << gettid() << ": " << message << std::endl;
#endif
}

// Format: $Ack:$Nseq:$IDfrom:$MSG
// return total size
ssize_t Message::marshal(char *buffer) {
  std::string payload;
  payload += (ack ? "a" : "b");
  payload += ":";
  payload += std::to_string(seq);
  payload += ":";
  payload += std::to_string(fromID);
  payload += ":";
  payload += msg;
  ssize_t n = payload.length();
  strncpy(buffer, payload.c_str(), n + 1);
  buffer[n + 1] = '\0';
#ifdef DEBUG_MODE
  std::cout << "Marshalled msg: " << buffer << " size " << (n + 1) << std::endl;
#endif
  return n;
}

// Format: $A:$N:$MSG
Message unmarshal(Parser::Host *from, char *buffer) {
  std::string payload = std::string(buffer);
  bool ack = payload[0] == 'a';
  payload = payload.substr(2);
  auto separator = payload.find(":");
  if (separator == std::string::npos) {
    std::cerr << "Error unmarshalling raw message: " << payload << std::endl;
    return Message();
  }
  size_t seq = std::stoul(payload.substr(0, separator));
  payload = payload.substr(separator + 1);
  separator = payload.find(":");
  if (separator == std::string::npos) {
    std::cerr << "Error unmarshalling raw message: " << payload << std::endl;
    return Message();
  }
  size_t fromID = std::stoul(payload.substr(0, separator));
  std::string msg = payload.substr(separator + 1);
#ifdef DEBUG_MODE
  std::cout << "Unmarshalled msg: " << buffer << "-> {Msg:\"" << msg
            << "\", type:" << (ack ? "a" : "b") << ", seq:" << seq
            << ", fromID: " << fromID << "}" << std::endl;
#endif
  return Message(from, msg, fromID, ack, seq);
}