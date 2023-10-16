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

  ttyLog("Sending message to " + host->fullAddressReadable() +
         ", msg: " + buffer);
  sockaddr_in add;
  add.sin_family = AF_INET;
  add.sin_addr.s_addr = host->ip;
  // add.sin_addr.s_addr = inet_pton(AF_INET, "127.0.0.1", address.c_str());
  add.sin_port = host->port;
  return sendto(this->sockfd, buffer, len, flags,
                reinterpret_cast<sockaddr *>(&add), sizeof(add));
}

sockaddr_in UDPSocket::recv(char *buffer, ssize_t len, int flags) {
  sockaddr_in from;
  socklen_t sk_len;
  ssize_t ret = recvfrom(sockfd, buffer, len, flags,
                         reinterpret_cast<sockaddr *>(&from), &sk_len);
  buffer[ret] = 0;
  return from;
}

void UDPSocket::listener(std::ofstream *logFile, sem_t *logSem,
                         const std::vector<Parser::Host> &hosts) {
  signal(SIGTERM, listener_stop);
  signal(SIGINT, listener_stop);
  while (1) {
    ttyLog("Waiting for message");
    char buffer[MAX_PACKET_LENGTH];
    sockaddr_in from = recv(buffer, MAX_PACKET_LENGTH);
    auto fromStr = std::string(inet_ntoa(from.sin_addr));
    std::cout << "Received from " << fromStr << ": " << buffer << std::endl;
    sem_wait(logSem);
    (*logFile) << "d " << (Parser::findHost(from, hosts))->id << " " << buffer
               << std::endl;
    sem_post(logSem);
  }
}

void UDPSocket::sender(messageList *pending, sem_t *pendSem,
                       std::ofstream *logFile, sem_t *logSem,
                       const std::vector<Parser::Host> &hosts) {
  signal(SIGTERM, sender_stop);
  signal(SIGINT, sender_stop);
  ttyLog("Ready to send");
  while (1) {
    sem_wait(pendSem);
    message *current = *pending;
    if (current == NULL) {
      sem_post(pendSem);
      sleep(1);
      continue;
    }
    ssize_t sent =
        unicast(current->destHost, current->msg.c_str(), current->len);
    if (sent < 0) {
      ttyLog("Error sending msg!");
      return;
    }
    ttyLog("Sending success");
    sem_wait(logSem);
    (*logFile) << "b " << current->msg << std::endl;
    sem_post(logSem);
    *pending = current->next;
    delete current;
    sem_post(pendSem);
  }
}

void UDPSocket::sender_stop(int) { exit(0); }
void UDPSocket::listener_stop(int) { exit(0); }

void cleanup(messageList m) {
  while (m) {
    messageList next = m->next;
    delete m;
    m = next;
  }
}

void ttyLog(std::string message) {
  std::cout << "Thread " << gettid() << ": " << message << std::endl;
}