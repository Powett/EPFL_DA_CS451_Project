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
  void push_back(Message *);
  void unsafe_push_back(Message *);
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