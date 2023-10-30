#pragma once
#include <mutex>
#include <ostream>

struct message;

// Thread-safe LinkedList to store messages
class PendingList {
public:
  PendingList() : first(nullptr), last(nullptr), mut() {}
  void push(message *);
  void push_last(message *);
  void unsafe_push_last(message *);
  int remove_instances(const std::string);
  int remove_older(const int);
  message *pop();
  std::ostream &display(std::ostream &out);
  ~PendingList();

private:
  message *first;
  message *last;
  std::mutex mut;
  bool empty();
};

std::ostream &operator<<(std::ostream &out, PendingList &pend);