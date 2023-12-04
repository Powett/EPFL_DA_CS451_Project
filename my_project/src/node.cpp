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
    std::string rcv_s = std::string(buffer);
    Message rcv(rcv_s);

    if (rcv.isBebAck) {
      relayHost->addBebAcked(rcv.uniqAckID);
    } else {
      // Send an ack to the relay
      Message *ack = new Message(relayHost, rcv.laMsg,  rcv.uniqAckID, true);
      pending.push(ack);
      bebDeliver(rcv, relayHost);
#if DEBUG_MODE > 0
      ttyLog("[L] Received msg from " + std::to_string(relayHost->id) +
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
#if DEBUG_MODE > 0
      ttyLog("[S] Sending queue empty...");
#endif
      continue;
    }

    // Check the destination did not ack this message already (if not a bebAck)
    std::string mID = current->uniqAckID;
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
      ttyLog("[S] Sent: {" + std::string(buffer)+"}");
#endif
    }
    if (current->isBebAck) {
      // do not resend
      delete current;
    } else {
#if DEBUG_MODE > 0
      ttyLog("[S] Repushing message in line: " + current->uniqAckID);
#endif
      pending.push_back(current);
    }
  }
#if DEBUG_MODE > 0
  ttyLog("[S] Sender exit");
#endif
}

void Node::bebDeliver(Message &m, Parser::Host *fromH) {
#if DEBUG_MODE > 0
  ttyLog("[L] bebDelivered message " + m.uniqAckID + " from: " +
         std::to_string(fromH->id));
#endif

  // TODO
}

void Node::unsafe_bebBroadcast(LAMessage m, std::string uniqAckID) {
  for (auto &host : hosts) {
    Message *current = new Message(host, m, uniqAckID, false);
#if DEBUG_MODE > 2
  ttyLog("[M] bebBroadcasting message {" + current->to_string()+"}");
#endif
    pending.unsafe_push_back(current); // no multithreading
  }
}
void Node::bebBroadcast(LAMessage m, std::string uniqAckID) {
  pending.mut.lock();
  unsafe_bebBroadcast(m, uniqAckID);
  pending.mut.unlock();
}

void Node::logDecision(){
  return;
}