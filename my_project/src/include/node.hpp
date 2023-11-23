#pragma once

#include "messaging.hpp"
#include "parser.hpp"

class Node {
public:
  UDPSocket sock;
  PendingList pending;
  std::vector<Parser::Host *> hosts;
  std::ofstream logFile;
  std::atomic_bool stopThreads;
  size_t id;

  void bebListener();
  void bebSender();
  void failureDetector();
  void bebDeliver(Message &, Parser::Host *, Parser::Host *);
  void bebBroadcast(std::string, size_t, size_t);
  void unsafe_bebBroadcast(std::string, size_t, size_t);
};

class Message {
public:
  Message() = default;
  Message(Parser::Host *d, std::string m, size_t fromID, bool ack = false,
          size_t seq = 0, Message *next = nullptr)
      : destHost(d), msg(m), ack(ack), seq(seq), fromID(fromID), next(next){};
  Parser::Host *destHost;
  std::string msg;
  bool ack;
  size_t seq;
  size_t fromID;
  Message *next;
  ssize_t marshal(char *buffer);
};
static Message unmarshal(Parser::Host *from, char *buffer);

void ttyLog(std::string message);