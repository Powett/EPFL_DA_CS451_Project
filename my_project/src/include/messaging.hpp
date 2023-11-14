#pragma once

#include <atomic>
#include <csignal>
#include <mutex>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "parser.hpp"
#include "pendinglist.hpp"

#define MAX_PACKET_LENGTH 1024
#define LOCALHOST "127.0.0.1"
#define DEFAULTPORT 0

class Message {
public:
  Message(Parser::Host *d, std::string m, bool ack = false, int seq = 0,
          Message *next = nullptr)
      : destHost(d), msg(m), ack(ack), seq(seq), next(next){};
  Parser::Host *destHost;
  std::string msg;
  bool ack;
  int seq;
  Message *next;
  ssize_t marshal(char *buffer);
};
static Message unmarshal(Parser::Host *from, char *buffer);

class UDPSocket {
public:
  UDPSocket(in_addr_t, unsigned short = DEFAULTPORT);
  ~UDPSocket();
  ssize_t unicast(const Parser::Host *, const char *, ssize_t, int = 0);
  ssize_t unicast(sockaddr_in *, const char *, ssize_t, int = 0);
  ssize_t recv(sockaddr_in &, char *, ssize_t, int = 0);
  void listener(PendingList &, std::ofstream &, std::vector<Parser::Host *> &,
                std::atomic_bool &);
  void sender(PendingList &, const std::vector<Parser::Host *> &,
              std::atomic_bool &);

private:
  int sockfd;
};

void ttyLog(std::string message);