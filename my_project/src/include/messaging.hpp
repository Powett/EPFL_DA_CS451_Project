#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "parser.hpp"

#define MAX_PACKET_LENGTH 1024
#define LOCALHOST "127.0.0.1"
#define DEFAULTPORT 0

typedef struct message *messageList;
struct message {
  Parser::Host *destHost;
  std::string msg;
  ssize_t len;
  message *next;
};

void cleanup(messageList);

class UDPSocket {
public:
  UDPSocket(in_addr_t, unsigned short = DEFAULTPORT);
  ~UDPSocket();
  ssize_t unicast(const Parser::Host *, const char *, ssize_t, int = 0);
  ssize_t unicast(sockaddr_in *, const char *, ssize_t, int = 0);
  sockaddr_in recv(char *, ssize_t, int = 0);
  void listener(messageList *, sem_t *, std::ofstream *, sem_t *,
                std::vector<Parser::Host> &);
  void sender(messageList *, sem_t *, std::ofstream *, sem_t *,
              const std::vector<Parser::Host> &);

private:
  int sockfd;
  static void sender_stop(int);
  static void listener_stop(int);
};

void ttyLog(std::string message);