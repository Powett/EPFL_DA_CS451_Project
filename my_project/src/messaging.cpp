#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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
  return;
}

UDPSocket::~UDPSocket() { close(sockfd); }

ssize_t UDPSocket::unicast(const Parser::Host *host, const char *buffer,
                           ssize_t len, int flags) {

  // DEBUG
  ttyLog("Sending message to " + host->fullAddressReadable() +
         ", msg: " + buffer);
  // ENDDEBUG
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
  socklen_t sk_len;
  ssize_t ret = recvfrom(sockfd, buffer, len, flags,
                         reinterpret_cast<sockaddr *>(&from), &sk_len);
  buffer[ret] = 0;
  return ret;
}

void UDPSocket::listener(PendingList &pending, std::ofstream *logFile,
                         sem_t *logSem, std::vector<Parser::Host> &hosts,
                         bool *stop) {

  while (!(*stop)) {
    // DEBUG
    ttyLog("Waiting for message");
    // ENDDEBUG
    char buffer[MAX_PACKET_LENGTH];
    sockaddr_in from;
    while (recv(from, buffer, MAX_PACKET_LENGTH) == -1 && !(*stop))
      ;
    auto fromHost = Parser::findHost(from, hosts);
    if (!fromHost) {
      continue;
    }

    //  check if ack
    if (buffer[0] == 'a') {
      // if ack: cleanup list
      std::string ackedMsg = std::string(buffer).substr(1);
      pending.remove_instances(ackedMsg.c_str());
    } else {
      // else: send ack
      auto ack = "a" + std::string(buffer);

      // put in sending list ?
      unicast(&from, ack.c_str(), ssize_t(std::strlen(ack.c_str())));

      // DEBUG
      ttyLog("Sent ack: " + ack);
      // ENDDEBUG

      // Only log if new ?
      if (fromHost->markSeenNew(buffer)) {
        // DEBUG
        ttyLog("Received from " + std::to_string(fromHost->id) + ": " + buffer);
        // ENDDEBUG
        sem_wait(logSem);
        (*logFile) << "d " << fromHost->id << " " << buffer << std::endl;
        sem_post(logSem);
      }
    }
  }
  // DEBUG
  ttyLog("Listener exit");
  // ENDDEBUG
}

void UDPSocket::sender(PendingList &pending, std::ofstream *logFile,
                       sem_t *logSem, const std::vector<Parser::Host> &hosts,
                       bool *stop) {
  // DEBUG
  ttyLog("Ready to send");
  // ENDDEBUG

  while (!(*stop)) {
    message *current = pending.pop();
    if (!current) {
      sleep(1);
      continue;
    }

    ssize_t sent =
        unicast(current->destHost, current->msg.c_str(), current->len);

    if (sent < 0) {
      // DEBUG
      ttyLog("Error sending msg!");
      // ENDDEBUG
      return;
    }
    // DEBUG
    ttyLog("Sending success");
    // ENDDEBUG

    sem_wait(logSem);
    (*logFile) << "b " << current->msg << std::endl;
    sem_post(logSem);

    delete current;
  }
  // DEBUG
  ttyLog("Sender exit");
  // ENDDEBUG
}

void ttyLog(std::string message) {
  std::cout << "Thread " << gettid() << ": " << message << std::endl;
}