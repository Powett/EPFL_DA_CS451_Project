#pragma once
#include <ostream>
#include <semaphore.h>

struct message;

// Thread-safe LinkedList to store messages
class PendingList {
public:
  PendingList() : first(nullptr), last(nullptr), sem() { sem_post(&sem); }
  void push(message *);
  void push_last(message *);
  int remove_instances(const std::string);
  message *pop();
  std::ostream &display(std::ostream &out);
  ~PendingList();

private:
  message *first;
  message *last;
  sem_t sem;
  bool empty();
};

std::ostream &operator<<(std::ostream &out, PendingList &pend);