#include "messaging.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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
UDPSocket::~UDPSocket(){
  close(sockfd);
}

ssize_t UDPSocket::unicast(const std::string &IP, unsigned short &port,
                       const char *buffer, ssize_t len, int flags) {
  sockaddr_in add;
  add.sin_family = AF_INET;
  add.sin_addr.s_addr = inet_addr(IP.c_str());
  // add.sin_addr.s_addr = inet_pton(AF_INET, "127.0.0.1", address.c_str());
  add.sin_port = htons(port);
  return sendto(this->sockfd, buffer, len, flags,
                reinterpret_cast<sockaddr *>(&add), sizeof(add)) > 0;
}

sockaddr_in UDPSocket::recv(char *buffer, ssize_t len, int flags) {
  sockaddr_in from;
  socklen_t sk_len;
  ssize_t ret = recvfrom(sockfd, buffer, len, flags,
                         reinterpret_cast<sockaddr *>(&from), &sk_len);
  buffer[ret] = 0;
  return from;
}

void UDPSocket::listener(std::ofstream *logFile, sem_t *logSem) {
  // while (1) {
  std::cout << "Thread " << gettid() << "Waiting for message" << std::endl;
  char buffer[MAX_PACKET_LENGTH];
  sockaddr_in from = recv(this->sockfd, buffer, MAX_PACKET_LENGTH);
  auto fromStr = std::string(inet_ntoa(from.sin_addr));
  std::cout << "Received from " << fromStr << ": " << buffer << std::endl;
  sem_wait(logSem);
  (*logFile) << "Received from " << fromStr << ": " << buffer << std::endl;
  sem_post(logSem);
  // }
}

void UDPSocket::sender(messageList* pending, sem_t *pendSem){
        std::cout << "Thread " << gettid() << "Ready to send" << std::endl;
    // while (1){
      sem_wait(pendSem);
      message current = **pending;
      ssize_t sent = unicast(current.dest, current.port, current.msg, current.len);
      if (sent<0){
        std::cout << "Error sending msg!" << std::endl;
        return;
      }
      *pending=current.next;
      delete current;
      sem_post(pendSem);
    // }
}