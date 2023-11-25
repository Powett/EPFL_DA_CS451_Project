#include "node.hpp"
#include "defines.hpp"

void Node::bebListener() {
  while (!stopThreads) {
#if DEBUG_MODE > 1
    ttyLog("[L] Waiting for message, sleeping...");
    sleep(1);
#endif
    char buffer[MAX_PACKET_LENGTH];
    sockaddr_in from;
    ssize_t recvd_len = -1;

    // Receive a message
    while (recvd_len == -1 && !stopThreads) {
      recvd_len = sock->recv(from, buffer, MAX_PACKET_LENGTH);
    }

    // Get source host
    auto relayHost = Parser::findHost(from, hosts);
    if (stopThreads || !relayHost || recvd_len < 2) {
#if DEBUG_MODE > 0
      ttyLog("[L] Error while receiving");
#endif
      continue;
    }

    // Unmarshal message
    Message rcv = unmarshal(relayHost, buffer);
    auto fromHost = Parser::findHostByID(rcv.fromID, hosts);

    if (rcv.isBebAck) {
      relayHost->addBebAcked(rcv.uniqAckID());
    } else {
      // Send an ack to the relay
      Message *ack = new Message(relayHost, "ack", rcv.fromID, true, rcv.seq);
      pending.push(ack);

      // If the message was already delivered, no need to continue
      if (canDeliver(fromHost, rcv.seq)) {
#if DEBUG_MODE > 0
        ttyLog("[L] Skipping deliverable message: " + rcv.uniqAckID());
#endif
        continue;
      }

      bebDeliver(rcv, relayHost, fromHost);
#if DEBUG_MODE > 0
      ttyLog("[L] Received msg from " + std::to_string(fromHost->id) +
             ", relayed by: " + std::to_string(relayHost->id) +
             ", content: " + buffer);
#endif
    }
  }
#if DEBUG_MODE > 0
  ttyLog("[L] Listener exit");
#endif
}

void Node::bebSender() {
  char buffer[MAX_PACKET_LENGTH];
  while (!stopThreads) {
#if DEBUG_MODE > 1
    ttyLog("[S] Sleeping...");
    sleep(1);
#endif
#if DEBUG_MODE > 1
    ttyLog("[S] Ready to send");
#endif
    Message *current = pending.pop();
    if (!current) {
      // #if DEBUG_MODE > 0
      ttyLog("[S] Sending queue empty...");
      // #endif
      continue;
    }
    auto fromHost = Parser::findHostByID(current->fromID, hosts);

    // Check the destination did not ack this message already (if not a bebAck)
    std::string mID = current->uniqAckID();
    if (!current->isBebAck && current->destHost->hasBebAcked(mID)) {
#if DEBUG_MODE > 0
      ttyLog("[S] Deleting acked message: " + mID);
#endif
      delete current;
      continue;
    }

    // Marshal message
    ssize_t len = current->marshal(buffer);
    // Send message
    ssize_t sent = sock->unicast(current->destHost, buffer, len);

    if (sent < 0) {
#if DEBUG_MODE > 0
      ttyLog("[S] Error sending msg!");
#endif
      continue;
    } else {
#if DEBUG_MODE > 0
      ttyLog("[S] Sent: " + std::string(buffer));
#endif
    }
    if (current->isBebAck) {
      // do not resend
      delete current;
    } else {
#if DEBUG_MODE > 0
      ttyLog("[S] Repushing message in line: " + std::to_string(current->seq));
#endif
      pending.push_back(current);
    }
  }
#if DEBUG_MODE > 0
  ttyLog("[S] Sender exit");
#endif
}

void Node::bebDeliver(Message &m, Parser::Host *relayH, Parser::Host *fromH) {
  // add fromHost to acknowledgers for message m (id:seq)
  fromH->addAcknowledger(m.seq, relayH->id);
#if DEBUG_MODE > 1
  ttyLog("[L] bebDelivered message " + std::to_string(m.seq) + " from: " +
         std::to_string(fromH->id) + " through: " + std::to_string(relayH->id));
#endif

  // if (relay, m) are not in forwarded, add and forward
  if (fromH->addForwarded(m.seq)) {
#if DEBUG_MODE > 0
    ttyLog("[L] forwarded message: " + m.uniqAckID());
#endif
    bebBroadcast(m.msg, m.seq, m.fromID);
  }
}

void Node::unsafe_bebBroadcast(std::string m, size_t seq, size_t fromID) {
  for (auto &host : hosts) {
    Message *current = new Message(host, m, fromID, false, seq);
    pending.unsafe_push_back(current); // no multithreading yet
  }
}
void Node::bebBroadcast(std::string m, size_t seq, size_t fromID) {
  pending.mut.lock();
  unsafe_bebBroadcast(m, seq, fromID);
  pending.mut.unlock();
}

bool Node::canDeliver(Parser::Host *host, size_t seq) {
  return host->sizeAcknowledgers(seq) > hosts.size() / 2;
}

void Node::tryDeliver() {
  // Check for deliverable messages
  for (auto &d_host : hosts) {
    size_t seq = 1;
    while (canDeliver(d_host, seq)) {
      seq++;
      logFile << "d " << d_host->id << " " << seq << std::endl;
#if DEBUG_MODE > 0
      ttyLog("[L] Delivered message " + std::to_string(seq) +
             " from: " + std::to_string(d_host->id));
#endif
    }
    for (size_t i = 1; i <= 1000; i++) {
      std::cout << i << ", nb of forwarders: " << d_host->sizeAcknowledgers(i)
                << std::endl;
    }
  }
}