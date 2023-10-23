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
  ttyLog("Sending (full) message to " + host->fullAddressReadable() +
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
    ttyLog("[L] Waiting for message");
    // ENDDEBUG
    char buffer[MAX_PACKET_LENGTH];
    sockaddr_in from;
    ssize_t recvd_len = -1;
    while (recvd_len == -1 && !(*stop))
      recvd_len = recv(from, buffer, MAX_PACKET_LENGTH);
    auto fromHost = Parser::findHost(from, hosts);
    if (*stop || !fromHost || recvd_len < 2) {
      continue;
    }
    // DEBUG
    ttyLog("[L] Received (full) from " + std::to_string(fromHost->id) + ": " +
           buffer);
    // ENDDEBUG

    std::string msg = std::string(buffer).substr(1);
    switch (buffer[0]) {
    case 'a': {
      // Ack
      // DEBUG
      ttyLog("[L] Received ack for msg: " + msg);
      // ENDDEBUG
      int nb = pending.remove_instances(msg);
      // DEBUG
      ttyLog("[L] Removed " + std::to_string(nb) + " instances of " + msg);
      // ENDDEBUG
      break;
    }

    case 'b': {
      // Normal
      message *ackMessage = new message{fromHost, msg, size_t(recvd_len), true};
      pending.push(ackMessage);
      // DEBUG
      ttyLog("[L] Pushed ack in sending queue for msg: " + ackMessage->msg);
      // ENDDEBUG

      // If new, log into file
      if (fromHost->markSeenNew(msg)) {
        sem_wait(logSem);
        (*logFile) << "d " << fromHost->id << " " << msg << std::endl;
        sem_post(logSem);
      }
      break;
    }
    default: {
      // DEBUG
      ttyLog("[L] Received weird message! Skipping...");
      continue;
      // ENDDEBUG
    }
    }
  }
  // DEBUG
  ttyLog("[L] Listener exit");
  // ENDDEBUG
}

void UDPSocket::sender(PendingList &pending,
                       const std::vector<Parser::Host> &hosts, bool *stop) {

  while (!(*stop)) {
    // DEBUG
    ttyLog("[S] Ready to send");
    // ENDDEBUG
    message *current = pending.pop();
    if (!current) {
      // DEBUG
      ttyLog("[S] Sending queue empty, sleeping for 1s...");
      // ENDDEBUG
      sleep(1);
      continue;
    }
    // Add correct directional byte
    auto full_text = (current->ack ? "a" : "b") + current->msg;
    ssize_t sent =
        unicast(current->destHost, full_text.c_str(), current->len + 1);

    if (sent < 0) {
      // DEBUG
      ttyLog("[S] Error sending msg!");
      // ENDDEBUG
      return;
    }
    if (!current->ack) {
      // DEBUG
      ttyLog("[S] Repushing message in line");
      // ENDDEBUG
      pending.push_last(current);
    } else {
      delete current;
    }
  }
  // DEBUG
  ttyLog("[S] Sender exit");
  // ENDDEBUG
}

void ttyLog(std::string message) {
  std::cout << "Thread " << gettid() << ": " << message << std::endl;
}