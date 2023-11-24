#pragma once
#include <mutex>
#include <ostream>

#include "parser.hpp"
class Message;

// Thread-safe LinkedList to store Messages
class PendingList {
public:
  PendingList() : first(nullptr), last(nullptr), mut() {}
  void push(Message *);
  void push_last(Message *);
  void unsafe_push_last(Message *);
  size_t remove_acked_by(Message const&, Parser::Host*);
  bool safe_empty();
  Message *pop();
  std::ostream &display(std::ostream &out);
  ~PendingList();

private:
  Message *first;
  Message *last;
  std::mutex mut;
  bool empty();
};

std::ostream &operator<<(std::ostream &out, PendingList &pend);