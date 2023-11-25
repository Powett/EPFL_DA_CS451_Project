#include "pendinglist.hpp"
#include "messaging.hpp"

void PendingList::push(Message *m) {
  m->next = nullptr; // sanity
  mut.lock();
  if (unsafe_empty()) {
    first = m;
    last = m;
  } else {
    m->next = first;
    first = m;
  }
  mut.unlock();
}
void PendingList::unsafe_push_back(Message *m) {
  m->next = nullptr; // sanity
  if (unsafe_empty()) {
    first = m;
    last = m;
    return;
  }
  last->next = m;
  last = m;
}

void PendingList::push_back(Message *m) {
  mut.lock();
  unsafe_push_back(m);
  mut.unlock();
}

Message *PendingList::pop() {
  mut.lock();
  if (unsafe_empty()) {
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

bool PendingList::unsafe_empty() { return first == nullptr; }
bool PendingList::empty() {
  mut.lock();
  bool val = unsafe_empty();
  mut.unlock();
  return val;
}
std::ostream &PendingList::display(std::ostream &out) {
  mut.lock();
  Message *current = first;
  while (current) {
    out << "|to:" << current->destHost->fullAddressReadable()
        << (current->isBebAck ? " a" : " b") << ":" << current->seq << ":"
        << "\"" << current->msg << "\"";
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