#include "pendinglist.hpp"
#include "messaging.hpp"

void PendingList::push(message *m) {
  sem_wait(&sem);
  m->next = first;
  first = m;
  sem_post(&sem);
}
void PendingList::push_last(message *m) {
  sem_wait(&sem);
  message *current = first;
  if (empty()) {
    first = m;
    sem_post(&sem);
    return;
  }
  while (current->next) {
    current = current->next;
  }
  current->next = m;
  sem_post(&sem);
}

void PendingList::remove_instances(const char *str) {
  sem_wait(&sem);
  if (empty()) {
    sem_post(&sem);
    return;
  }
  message *prev;
  while (std::strcmp(first->msg.c_str(), str) == 0) {
    prev = first;
    first = first->next;
    delete prev;
  }
  message *current = first->next;
  prev = first;
  while (current) {
    if (std::strcmp(current->msg.c_str(), str) == 0) {
      prev->next = current->next;
      delete current;
    } else {
      prev = current;
    }
    current = prev->next;
  }
  sem_post(&sem);
}

message *PendingList::pop() {
  sem_wait(&sem);
  message *prev = first;
  if (first) {
    first = first->next;
  }
  sem_post(&sem);
  return prev;
}

bool PendingList::empty() { return first == nullptr; }

PendingList::~PendingList() {
  message *current = first;
  message *prev;
  while (current) {
    prev = current;
    current = current->next;
    delete prev;
  }
}