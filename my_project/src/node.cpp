#include "node.hpp"
#include "defines.hpp"

void Node::bebListener() {
  while (!stopThreads) {
#ifdef DEBUG_MODE
    ttyLog("[L] Waiting for message, sleeping...");
    sleep(1);
#endif
    char buffer[MAX_PACKET_LENGTH];
    sockaddr_in from;
    ssize_t recvd_len = -1;

    // Receive a message
    while (recvd_len == -1 && !stopThreads) {
      recvd_len = sock->recv(from, buffer, MAX_PACKET_LENGTH);
#ifdef DEBUG_MODE
      ttyLog("[L] Sleeping...");
      sleep(1);
#endif
    }

    // Get source host
    auto relayHost = Parser::findHost(from, hosts);
    if (stopThreads || !relayHost || recvd_len < 2) {
#ifdef DEBUG_MODE
      ttyLog("[L] Error while receiving");
#endif
      continue;
    }

    // Unmarshal message
    Message rcv = unmarshal(relayHost, buffer);
    auto fromHost = Parser::findHostByID(rcv.fromID, hosts);

    relayHost->lastPing = std::time(nullptr);
    if (rcv.seq == 0) {
// special PING message, ignore
#ifdef DEBUG_MODE
      ttyLog("[L] Received PING from " + std::to_string(fromHost->id));
#endif
      continue;
    }
    if (rcv.ack) {
      // stop sending the message to the relay
      // size_t nb = pending.remove_acked_by(rcv, relayHost);
#ifdef DEBUG_MODE
      ttyLog("[L] Removed " + std::to_string(nb) +
             "messages to be sent that were acked");
#endif
    } else {
      // Send an ack to the relay
      Message *ack = new Message(relayHost, "ack", rcv.fromID, true, rcv.seq);
      pending.push(ack);
      bebDeliver(rcv, relayHost, fromHost);
#ifdef DEBUG_MODE
      ttyLog("[L] Received msg from " + std::to_string(fromHost->id) +
             ", relayed by: " + std::to_string(relayHost->id) +
             ", content: " + buffer);
#endif
    }
  }
#ifdef DEBUG_MODE
  ttyLog("[L] Listener exit");
#endif
}

void Node::messageManager(std::vector<std::string> &msgs) {
  size_t i = 0;
  bool finished_sending = false;
  bebPing();
  while (!stopThreads) {
    // put new messages in line, by chunks
    if (!finished_sending && i >= msgs.size()) {
      finished_sending = true;
      std::cout << "Finished putting messages in the sending queue"
                << std::endl;
    }
    while (!finished_sending && self_host->lastDelivered >= i) {
      // Put 10 more messages in queue
      for (int j = 0; i < msgs.size() && j < CHUNK_SIZE; j++) {
        bebBroadcast(msgs[i], i + 1, id);
        i++;
      }
    }
    // receive
    tryDeliver();
    // sleep
    usleep(250'000);
  }
}

void Node::bebSender() {
  char buffer[MAX_PACKET_LENGTH];
  while (!stopThreads) {
#ifdef DEBUG_MODE
    ttyLog("[S] Sleeping...");
    sleep(1);
#endif
#ifdef DEBUG_MODE
    ttyLog("[S] Ready to send");
#endif
    Message *current = pending.pop();
    if (!current) {
#ifdef DEBUG_MODE
      ttyLog("[S] Sending queue empty...");
#endif
      continue;
    }
    auto fromHost = Parser::findHostByID(current->fromID, hosts);
    if (current->destHost->crashed) {
#ifdef DEBUG_MODE
      ttyLog("[S] Skipping send to crashed node");
#endif
      delete current;
      continue;
    }
    // Marshal message
    ssize_t len = current->marshal(buffer);
    // Send message
    ssize_t sent = sock->unicast(current->destHost, buffer, len);

    if (sent < 0) {
#ifdef DEBUG_MODE
      ttyLog("[S] Error sending msg!");
#endif
      continue;
    } else {
#ifdef DEBUG_MODE
      ttyLog("[S] Sent: " + std::string(buffer));
#endif
    }
    if (current->ack) {
      // do not resend
      delete current;
    } else {
#ifdef DEBUG_MODE
      ttyLog("[S] Repushing message in line: " + std::to_string(current->seq));
#endif
      pending.push_last(current);
    }
  }
#ifdef DEBUG_MODE
  ttyLog("[S] Sender exit");
#endif
}

void Node::bebDeliver(Message &m, Parser::Host *relayH, Parser::Host *fromH) {
  // add fromHost to acknowledgers for message m (id:seq)
  fromH->addAcknowledger(m.seq, relayH->id);
#ifdef DEBUG_MODE
  ttyLog("[L] bebDelivered message " + std::to_string(m.seq) + " from: " +
         std::to_string(fromH->id) + " through: " + std::to_string(relayH->id));
#endif
  // if (relay, m) are not in forwarded, add and forward
  if (fromH->testSetForwarded(m.seq)) {
    bebBroadcast(m.msg, m.seq, fromH->id);
  }
}

void Node::unsafe_bebBroadcast(std::string m, size_t seq, size_t fromID) {
  for (auto &host : hosts) {
    Message *current = new Message(host, m, fromID, false, seq);
    pending.unsafe_push_last(current); // no multithreading yet
  }
}
void Node::bebBroadcast(std::string m, size_t seq, size_t fromID) {
  for (auto &host : hosts) {
    Message *current = new Message(host, m, fromID, false, seq);
    pending.push_last(current); // could be optimized
  }
}

void Node::bebPing() { bebBroadcast("ping", 0, id); }

void Node::tryDeliver() {
  // Check for deliverable messages
  for (auto &d_host : hosts) {
    while (d_host->acknowledgers[d_host->lastDelivered + 1].size() >
           hosts.size() / 2) {
      d_host->lastDelivered++;
      logFile << "d " << d_host->id << " " << d_host->lastDelivered
              << std::endl;
#ifdef DEBUG_MODE
      ttyLog("[L] Delivered message " + std::to_string(d_host->lastDelivered) +
             " from: " + std::to_string(d_host->id));
#endif
    }
  }
}