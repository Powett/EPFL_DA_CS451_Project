#pragma once

#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "parser.hpp"
#include "pendinglist.hpp"
#include "defines.hpp"

#define MAX_PACKET_LENGTH 1024
#define LOCALHOST "127.0.0.1"
#define DEFAULTPORT 0

#define BEBSEPARATOR ':'

class UDPSocket {
public:
  UDPSocket(in_addr_t, unsigned short = DEFAULTPORT);
  ~UDPSocket();
  ssize_t unicast(const Parser::Host *, const char *, ssize_t, int = 0);
  ssize_t unicast(sockaddr_in *, const char *, ssize_t, int = 0);
  ssize_t recv(sockaddr_in &, char *, ssize_t, int = 0);

private:
  int sockfd;
};

class Message {
public:
  Message(Parser::Host *d, LAMessage msg, std::string uniqAckID,
          bool ack = false, Message *next = nullptr)
      : destHost(d), laMsg(msg), uniqAckID(uniqAckID), isBebAck(ack), next(next){};
  
  // Format: $bebAck:$UniqID:$LAMSG
  // To unmarshall
  Message(std::string const& s): destHost(nullptr){
  #if DEBUG_MODE > 2
    std::cout << "Unmarshalling msg: {" << s << "}" << std::endl;
  #endif
    std::string payload = s;
    isBebAck = payload[0] == 'a';
    payload = payload.substr(2);
    size_t separator = payload.find(BEBSEPARATOR);
    if (separator == std::string::npos) {
      std::cerr << "Error unmarshalling raw message: " << payload << std::endl;
      return;  
    }
    uniqAckID = payload.substr(0,separator);
    payload=payload.substr(separator+1);
    laMsg = LAMessage(payload);
  #if DEBUG_MODE > 2
    std::cout << "Unmarshalled msg: " << (isBebAck ? "bebA" : "bebB") << ", ackID:" << uniqAckID
              << " LA: " << laMsg.to_string() << "}" << std::endl;
  #endif
  }
  Parser::Host *destHost;
  LAMessage laMsg;
  std::string uniqAckID;
  bool isBebAck;
  Message *next;
  ssize_t marshal(char *buffer);
  std::string to_string() const;
};

void ttyLog(std::string message);
std::ostream &operator<<(std::ostream &out, Message const&m);