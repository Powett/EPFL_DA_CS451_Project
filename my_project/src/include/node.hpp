#pragma once

#include "lattice.hpp"
#include "messaging.hpp"
#include "parser.hpp"

class Node {
public:
  Node() = default;
  UDPSocket *sock;
  PendingList pending;
  std::vector<Parser::Host *> hosts;
  Parser::Host *self_host;
  std::ofstream logFile;
  std::atomic_bool stopThreads;
  size_t id;

  void bebListener();
  void bebSender();

  void bebDeliver(Message &, Parser::Host *);
  void bebBroadcast(LAMessage, std::string);
  void unsafe_bebBroadcast(LAMessage, std::string);

  void laAck(size_t prop_number, size_t round_number);
  void laNack(size_t prop_number, LAValue val, size_t round_number);
  void laPropose(size_t prop_number, LAValue val, size_t round_number);

  void logDecision();
};