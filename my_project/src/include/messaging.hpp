#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>

#define MAX_PACKET_LENGTH 1024
#define LOCALHOST "127.0.0.1"
#define DEFAULTPORT 0

class UDPSocket {
public:
  UDPSocket(in_addr_t, unsigned short = DEFAULTPORT);
  ~UDPSocket();
  ssize_t unicast(const std::string &, unsigned short &, const char *,
              ssize_t , int = 0);
  sockaddr_in recv(char *, ssize_t , int  = 0);
  void listener(std::ofstream *, sem_t *);
  void UDPSocket::sender(messageList*, sem_t *);
private:
  int sockfd;
};

struct message{
  std::string dest;
  unsigned short port;
  char* msg;
  ssize_t len;
  message* next;
};

typedef struct message* messageList;