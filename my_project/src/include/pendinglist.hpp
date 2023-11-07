#pragma once
#include <mutex>
#include <ostream>

class Message;

// Thread-safe LinkedList to store Messages
class PendingList {
public:
  PendingList() : first(nullptr), last(nullptr), mut() {}
  void push(Message *);
  void push_last(Message *);
  void unsafe_push_last(Message *);
  int remove_instances(const std::string);
  int remove_older(const int);
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