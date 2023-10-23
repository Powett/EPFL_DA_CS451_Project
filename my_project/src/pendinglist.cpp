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
  if (empty()) {
    first = m;
    sem_post(&sem);
    return;
  }
  message *current = first;
  while (current->next) {
    current = current->next;
  }
  current->next = m;
  m->next = nullptr; // sanity
  sem_post(&sem);
}

int PendingList::remove_instances(const std::string str) {
  int nb = 0;
  sem_wait(&sem);
  if (empty()) {
    sem_post(&sem);
    return nb;
  }
  message *prev;
  while (first->msg == str) {
    prev = first;
    first = first->next;
    delete prev;
    nb++;
    if (!first) {
      sem_post(&sem);
      return nb;
    }
  }
  message *current = first->next;
  prev = first;
  while (current) {
    if (current->msg == str) {
      prev->next = current->next;
      delete current;
      nb++;
    } else {
      prev = current;
    }
    current = prev->next;
  }
  sem_post(&sem);
  return nb;
}

message *PendingList::pop() {
  sem_wait(&sem);
  message *prev = first;
  if (!empty()) {
    first = first->next;
    prev->next = nullptr;
  }
  sem_post(&sem);
  return prev;
}

bool PendingList::empty() { return first == nullptr; }

std::ostream &PendingList::display(std::ostream &out) {
  sem_wait(&sem);
  message *current = first;
  while (current) {
    out << "|to:" << current->destHost->fullAddressReadable()
        << (current->ack ? "a/" : "b/") << " \"" << current->msg << "\"["
        << std::to_string(current->len) << "]|";
    if (current->next) {
      out << "->";
    }
    current = current->next;
  }
  sem_post(&sem);
  return out;
}

PendingList::~PendingList() {
  message *current = first;
  message *prev;
  while (current) {
    prev = current;
    current = current->next;
    delete prev;
  }
}

std::ostream &operator<<(std::ostream &out, PendingList &pend) {
  return pend.display(out);
}