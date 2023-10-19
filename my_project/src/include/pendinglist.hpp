#pragma once
#include <semaphore.h>

struct message;

// Thread-safe LinkedList to store messages
class PendingList {
public:
  PendingList() : first(nullptr), sem() { sem_post(&sem); }
  void push(message *);
  void push_last(message *);
  void remove_instances(const char *);
  message *pop();
  ~PendingList();

private:
  message *first;
  sem_t sem;
  bool empty();
};