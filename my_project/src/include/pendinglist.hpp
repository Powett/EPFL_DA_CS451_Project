#pragma once
#include <mutex>
#include <ostream>

#include "parser.hpp"
class Message;

// Thread-safe LinkedList to store Messages
class PendingList {
public:
  PendingList() : mut(), first(nullptr), last(nullptr) {}
  void push(Message *);
  void push_last(Message *);
  void unsafe_push_last(Message *);
  size_t remove_acked_by(Message const&, Parser::Host*);
  bool empty();
  Message *pop();
  std::ostream &display(std::ostream &out);
  ~PendingList();
  std::mutex mut;

private:
  Message *first;
  Message *last;
  bool unsafe_empty();
};

std::ostream &operator<<(std::ostream &out, PendingList &pend);