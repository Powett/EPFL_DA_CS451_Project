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

void UDPSocket::listener(PendingList &pending, std::ofstream *logFile,
                         std::mutex &logMutex,
                         std::vector<Parser::Host *> &hosts,
                         std::atomic_bool &flagStop) {
  while (!flagStop) {
#ifdef DEBUG_MODE
    ttyLog("[L] Waiting for message");
#endif
    char buffer[MAX_PACKET_LENGTH];
    sockaddr_in from;
    ssize_t recvd_len = -1;
    while (recvd_len == -1 && !flagStop)
      recvd_len = recv(from, buffer, MAX_PACKET_LENGTH);
    auto fromHost = Parser::findHost(from, hosts);
    if (flagStop || !fromHost || recvd_len < 2) {
#ifdef DEBUG_MODE
      ttyLog("[L] Error while receiving");
#endif
      continue;
    }
#ifdef DEBUG_MODE
    ttyLog("[L] Received (full) from " + std::to_string(fromHost->id) + ": " +
           buffer);
#endif
    Message rcv = unmarshal(fromHost, buffer);
    if (rcv.ack) { // Ack
#ifdef DEBUG_MODE
      ttyLog("[L] Received ack for msg: " + rcv.msg);
#endif
      int nb = pending.remove_older(rcv.seq);
#ifdef DEBUG_MODE
      ttyLog("[L] Removed " + std::to_string(nb) + " with lower seq than " +
             std::to_string(rcv.seq));
#endif
    } else { // Normal message
      // If expected, increase and lock
      if (fromHost->lastAcked.compare_exchange_strong(rcv.seq, rcv.seq + 1)) {
#ifdef DEBUG_MODE
        ttyLog("[L] Was new: " + rcv.msg);
#endif
        logMutex.lock();
        (*logFile) << "d " << fromHost->id << " " << rcv.seq << std::endl;
        logMutex.unlock();
      }

      int last_seq = fromHost->lastAcked.load();
      // If expected or older, ACK with last known
      if (rcv.seq <= last_seq) {
        Message *ackMessage = new Message(fromHost, "", true, last_seq);
        pending.push(ackMessage);
#ifdef DEBUG_MODE
        ttyLog("[L] Pushed ack in sending queue for msg: " + ackMessage->msg);
#endif
      }
    }
  }
#ifdef DEBUG_MODE
  ttyLog("[L] Listener exit");
#endif
}

void UDPSocket::sender(PendingList &pending,
                       const std::vector<Parser::Host *> &hosts,
                       std::atomic_bool &flagStop) {
  char buffer[MAX_PACKET_LENGTH];
  while (!flagStop) {
#ifdef DEBUG_MODE
    ttyLog("[S] Ready to send");
#endif
    Message *current = pending.pop();
    if (!current) {
#ifdef DEBUG_MODE
      ttyLog("[S] Sending queue empty...");
#endif
      continue;
    }
    // Marshal message
    ssize_t len = current->marshal(buffer);

    ssize_t sent = unicast(current->destHost, buffer, len);

    if (sent < 0) {
#ifdef DEBUG_MODE
      ttyLog("[S] Error sending msg!");
#endif
      continue;
    } else {
#ifdef DEBUG_MODE
      ttyLog("[S] Sent: " + std::string(buffer));
#endif
    }
    if (!current->ack) {
#ifdef DEBUG_MODE
      ttyLog("[S] Repushing message in line");
#endif
      pending.push_last(current);
    } else {
      delete current;
    }
  }
#ifdef DEBUG_MODE
  ttyLog("[S] Sender exit");
#endif
}

void ttyLog(std::string message) {
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
  std::cout << "Thread "
            << ": " << message << std::endl;
#else
  std::cout << "Thread " << gettid() << ": " << message << std::endl;
#endif
}

// Format: $A:$N:$MSG
// return total size
ssize_t Message::marshal(char *buffer) {
  std::string payload;
  payload += (ack ? "a" : "b");
  payload += ":";
  payload += std::to_string(seq);
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
static Message unmarshal(Parser::Host *from, char *buffer) {
  std::string payload = std::string(buffer);
  bool ack = payload[0] == 'a';
  payload = payload.substr(2);
  auto separator = payload.find(":");
  if (separator == std::string::npos) {
    std::cerr << "Error unmarshalling raw message: " << payload << std::endl;
    exit(-1);
  }
  int seq = std::stoi(payload.substr(0, separator));
  std::string msg = payload.substr(separator + 1);
#ifdef DEBUG_MODE
  std::cout << "Unmarshalled msg: " << buffer << "{" << msg << "," << ack << ","
            << seq << "}" << std::endl;
#endif
  return Message(from, msg, ack, seq);
}