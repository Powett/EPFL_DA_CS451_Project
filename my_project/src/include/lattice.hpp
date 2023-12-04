#pragma once

#include <set>
#include <string>
#include <iostream>

#define LASEPARATOR '|'


typedef std::set<size_t> LAValue;
typedef enum LAMessageType { ACK = 0, NACK = 1, PROPOSE = 2 } LAMessageType;

std::string lavals_to_string(LAValue v);
LAValue lavals_from_string(std::string const& s);

class LAMessage {
public:
  LAMessage() : type(PROPOSE), round_number(0), v() {}
  LAMessage(LAMessageType t, size_t prop_n, size_t round_n, LAValue v = LAValue())
      : type(t), proposal_number(prop_n), round_number(round_n), v(v) {}
  // $LAMSG: $type|proposal|round|LAValue
  LAMessage(std::string const &s) {
    std::cout << "[L] Unmarshalling LAMessage: " << s << std::endl;
    std::string payload = s;
    // Get type
    size_t sep_i = payload.find(LASEPARATOR);
    type = LAMessageType(std::stoul(payload.substr(0, sep_i)));
    payload = payload.substr(sep_i + 1);
    // Get proposal number
    sep_i = payload.find(LASEPARATOR);
    proposal_number = std::stoul(payload.substr(0, sep_i));
    payload = payload.substr(sep_i + 1);
    // Get round number
    sep_i = payload.find(LASEPARATOR);
    round_number = std::stoul(payload.substr(0, sep_i));
    payload = payload.substr(sep_i + 1);
    // Get values
    v = lavals_from_string(payload);
  }
  LAMessageType type;
  size_t proposal_number;
  size_t round_number;
  LAValue v;
  
  std::string to_string() const;
};

