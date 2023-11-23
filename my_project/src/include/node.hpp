#pragma once

#include "messaging.hpp"
#include "parser.hpp"

class Node {
public:
  Node() = default;
  UDPSocket *sock;
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