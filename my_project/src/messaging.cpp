#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "defines.hpp"
#include "messaging.hpp"
#include "pendinglist.hpp"

UDPSocket::UDPSocket(in_addr_t IP, unsigned short port) {
  struct sockaddr_in sk;

  // Creating socket file descriptor
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&sk, 0, sizeof(sk));

  // Filling server information
  sk.sin_family = AF_INET; // IPv4
  sk.sin_addr.s_addr = IP;
  sk.sin_port = port;

  // Bind the socket with the server address
  if (bind(this->sockfd, reinterpret_cast<sockaddr *>(&sk), sizeof(sk)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  // Set socket as non-blocking
  int flags = fcntl(this->sockfd, F_GETFL, 0);
  fcntl(this->sockfd, F_SETFL, flags | O_NONBLOCK);
  return;
}

UDPSocket::~UDPSocket() { close(sockfd); }

ssize_t UDPSocket::unicast(const Parser::Host *host, const char *buffer,
                           ssize_t len, int flags) {

#ifdef DEBUG_MODE
  ttyLog("Sending (full) message to " + host->fullAddressReadable() +
         ", msg: " + buffer);
#endif
  sockaddr_in add;
  add.sin_family = AF_INET;
  add.sin_addr.s_addr = host->ip;
  add.sin_port = host->port;
  return unicast(&add, buffer, len, flags);
}

ssize_t UDPSocket::unicast(sockaddr_in *dest, const char *buffer, ssize_t len,
                           int flags) {
  return sendto(this->sockfd, buffer, len, flags,
                reinterpret_cast<sockaddr *>(dest), sizeof(*dest));
}

ssize_t UDPSocket::recv(sockaddr_in &from, char *buffer, ssize_t len,
                        int flags) {
  socklen_t sk_len(sizeof(from));
  ssize_t ret = recvfrom(sockfd, buffer, len, flags,
                         reinterpret_cast<sockaddr *>(&from), &sk_len);
  if (ret > 0) {
    buffer[ret] = 0;
  } else if (ret == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
    std::cerr << "Error when receiving: " << std::strerror(errno) << std::endl;
  }
  return ret;
}

void UDPSocket::bebListener(PendingList &pending, std::ofstream &logFile,
                            std::vector<Parser::Host *> &hosts, size_t self_ID,
                            std::atomic_bool &flagStop) {
  while (!flagStop) {
#ifdef DEBUG_MODE
    ttyLog("[L] Waiting for message, sleeping...");
    sleep(1);
#endif
    char buffer[MAX_PACKET_LENGTH];
    sockaddr_in from;
    ssize_t recvd_len = -1;

    // Receive a message
    while (recvd_len == -1 && !flagStop) {
      recvd_len = recv(from, buffer, MAX_PACKET_LENGTH);
#ifdef DEBUG_MODE
      ttyLog("[L] Sleeping...");
      sleep(1);
#endif
    }

    // Get source host
    auto relayHost = Parser::findHost(from, hosts);
    if (flagStop || !relayHost || recvd_len < 2) {
#ifdef DEBUG_MODE
      ttyLog("[L] Error while receiving");
#endif
      continue;
    }

    // Unmarshal message
    Message rcv = unmarshal(relayHost, buffer);
    auto fromHost = Parser::findHostByID(rcv.fromID, hosts);

#ifdef DEBUG_MODE
    ttyLog("[L] Received (full) from " + std::to_string(fromHost->id) +
           ", relayed by: " + std::to_string(relayHost->id) ", content: " +
           buffer);
#endif
    relayHost->last_ping = std::time(nullptr);
    bebDeliver(pending, hosts, rcv, relayHost, fromHost, logFile);
  }
#ifdef DEBUG_MODE
  ttyLog("[L] Listener exit");
#endif
}

void UDPSocket::bebSender(PendingList &pending, std::atomic_bool &flagStop) {
  char buffer[MAX_PACKET_LENGTH];
  while (!flagStop) {
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

    // Marshal message
    ssize_t len = current->marshal(buffer);
    // Send message
    ssize_t sent = unicast(current->destHost, buffer, len);

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

void UDPSocket::bebDeliver(PendingList &pending,
                           std::vector<Parser::Host *> &hosts, Message &m,
                           Parser::Host *relayH, Parser::Host *fromH,
                           std::ofstream &logFile) {
  // add fromHost to acknowledgers for message m (id:seq)
  fromH->addAcknowledger(m.seq, relayH->id);

  // Check for deliverable messages
  for (auto &d_host : hosts) {
    bool all_ack = true;
    for (auto &host : hosts) {
      if (host->crashed) {
        continue;
      }
      if (!d_host->hasAcknowledger(d_host->lastDelivered + 1, host->id)) {
        all_ack = false;
        break;
      }
    }
    if (all_ack) {
      logFile << "d " << d_host->id << " " << d_host->lastDelivered
              << std::endl;
#ifdef DEBUG_MODE
      ttyLog("[L] Delivered message " + std::to_string(d_host->lastDelivered) +
             " from: " + std::to_string(d_host->id));
#endif
      d_host->lastDelivered++;
    }
  }

  // if (relay, m) are not in forwarded, add and forward
  if (fromH->testSetForwarded(m.seq)) {
    bebBroadcast(pending, hosts, m.msg, m.seq, fromH->id);
  }
}

void unsafe_bebBroadcast(PendingList &pending,
                         std::vector<Parser::Host *> &hosts, std::string m,
                         size_t seq, size_t fromID) {
  for (auto &host : hosts) {
    Message *current = new Message(host, m, fromID, false, seq);
    pending.unsafe_push_last(current); // no multithreading yet
  }
}
void safe_bebBroadcast(PendingList &pending, std::vector<Parser::Host *> &hosts,
                       std::string m, size_t seq, size_t fromID) {
  for (auto &host : hosts) {
    Message *current = new Message(host, m, fromID, false, seq);
    pending.push_last(current); // could be optimized
  }
}

void ttyLog(std::string message) {
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
  std::cout << "Thread "
            << ": " << message << std::endl;
#else
  std::cout << "Thread " << gettid() << ": " << message << std::endl;
#endif
}

// Format: $Ack:$Nseq:$IDfrom:$MSG
// return total size
ssize_t Message::marshal(char *buffer) {
  std::string payload;
  payload += (ack ? "a" : "b");
  payload += ":";
  payload += std::to_string(seq);
  payload += ":";
  payload += std::to_string(fromID);
  payload += ":";
  payload += msg;
  ssize_t n = payload.length();
  strncpy(buffer, payload.c_str(), n + 1);
  buffer[n + 1] = '\0';
#ifdef DEBUG_MODE
  std::cout << "Marshalled msg: " << buffer << " size " << (n + 1) << std::endl;
#endif
  return n;
}

// Format: $A:$N:$MSG
static Message unmarshal(Parser::Host *from, char *buffer) {
  std::string payload = std::string(buffer);
  bool ack = payload[0] == 'a';
  payload = payload.substr(2);
  auto separator = payload.find(":");
  if (separator == std::string::npos) {
    std::cerr << "Error unmarshalling raw message: " << payload << std::endl;
    return Message();
  }
  size_t seq = std::stoi(payload.substr(0, separator));
  payload = payload.substr(separator + 1);
  separator = payload.find(":");
  if (separator == std::string::npos) {
    std::cerr << "Error unmarshalling raw message: " << payload << std::endl;
    return Message();
  }
  int fromID = std::stoi(payload.substr(0, separator));
  std::string msg = payload.substr(separator + 1);
#ifdef DEBUG_MODE
  std::cout << "Unmarshalled msg: " << buffer << "-> {Msg:\"" << msg
            << "\", is_ack:" << ack << ", seq:" << seq << "}" << std::endl;
#endif
  return Message(from, msg, ack, seq, fromID);
}