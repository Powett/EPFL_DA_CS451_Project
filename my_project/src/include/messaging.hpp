#pragma once

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

class UDPSocket {
public:
  UDPSocket(in_addr_t, unsigned short = DEFAULTPORT);
  ~UDPSocket();
  ssize_t unicast(const Parser::Host *, const char *, ssize_t, int = 0);
  ssize_t unicast(sockaddr_in *, const char *, ssize_t, int = 0);
  ssize_t recv(sockaddr_in &, char *, ssize_t, int = 0);

private:
  int sockfd;
};

class Message {
public:
  Message() = default;
  Message(Parser::Host *d, std::string m, size_t fromID, bool ack = false,
          size_t seq = 0, Message *next = nullptr)
      : destHost(d), msg(m), isBebAck(ack), seq(seq), fromID(fromID),
        next(next){};
  Parser::Host *destHost;
  std::string msg;
  bool isBebAck;
  size_t seq;
  size_t fromID;
  Message *next;
  ssize_t marshal(char *buffer);
  std::string uniqAckID();
};

Message unmarshal(char *buffer);
void ttyLog(std::string message);