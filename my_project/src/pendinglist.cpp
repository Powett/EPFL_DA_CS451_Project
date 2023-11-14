#include "pendinglist.hpp"
#include "messaging.hpp"

void PendingList::push(Message *m) {
  m->next = nullptr; // sanity
  mut.lock();
  if (empty()) {
    first = m;
    last = m;
  } else {
    m->next = first;
    first = m;
  }
  mut.unlock();
}
void PendingList::unsafe_push_last(Message *m) {
  m->next = nullptr; // sanity
  if (empty()) {
    first = m;
    last = m;
    return;
  }
  last->next = m;
  last = m;
}

void PendingList::push_last(Message *m) {
  mut.lock();
  unsafe_push_last(m);
  mut.unlock();
}

int PendingList::remove_instances(const std::string str) {
  int nb = 0;
  mut.lock();
  if (empty()) {
    mut.unlock();
    return nb;
  }
  Message *prev;
  while (first->msg == str) {
    prev = first;
    first = first->next;
    delete prev;
    nb++;
    if (!first) {
      last = nullptr; // we removed the whole list
      mut.unlock();
      return nb;
    }
  }
  Message *current = first->next;
  prev = first;
  while (current) {
    if (current->msg == str) {
      prev->next = current->next;
      if (current == last) { // we removed the last element
        last = prev;
      }
      delete current;
      nb++;
    } else {
      prev = current;
    }
    current = prev->next;
  }
  mut.unlock();
  return nb;
}

int PendingList::remove_older(const int seq, unsigned long destID) {
  int nb = 0;
  mut.lock();
  if (empty()) {
    mut.unlock();
    return nb;
  }
  Message *prev;
  while (first->seq <= seq && first->destHost->id == destID) {
    prev = first;
    first = first->next;
    delete prev;
    nb++;
    if (!first) {
      last = nullptr; // we removed the whole list
      mut.unlock();
      return nb;
    }
  }
  Message *current = first->next;
  prev = first;
  while (current) {
    if (current->seq <= seq && first->destHost->id == destID) {
      prev->next = current->next;
      if (current == last) { // we removed the last element
        last = prev;
      }
      delete current;
      nb++;
    } else {
      prev = current;
    }
    current = prev->next;
  }
  mut.unlock();
  return nb;
}

Message *PendingList::pop() {
  mut.lock();
  if (empty()) {
    mut.unlock();
    return nullptr;
  }
  Message *prev = first;
  if (first == last) { // only one element
    last = nullptr;
    first = nullptr;
  } else {
    first = first->next;
  }
  mut.unlock();
  return prev;
}

bool PendingList::empty() { return first == nullptr; }

std::ostream &PendingList::display(std::ostream &out) {
  mut.lock();
  Message *current = first;
  while (current) {
    out << "|to:" << current->destHost->fullAddressReadable()
        << (current->ack ? " a" : " b") << ":" << current->seq << ":"
        << "\"" << current->msg;
    if (current != last) {
      out << "->";
    }
    current = current->next;
  }
  mut.unlock();
  return out;
}

PendingList::~PendingList() {
  Message *current = first;
  Message *prev;
  while (current) {
    prev = current;
    current = current->next;
    delete prev;
  }
}

std::ostream &operator<<(std::ostream &out, PendingList &pend) {
  return pend.display(out);
}