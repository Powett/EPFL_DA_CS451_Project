#include "lattice.hpp"
#include "defines.hpp"

std::string lavals_to_string(LAValue v) {
  std::string res = "";
  for (size_t i : v) {
    res += std::to_string(i) + " ";
  }
  if (res.length() > 0) {
    res = res.substr(0, res.length() - 1);
  }
  return res;
}

LAValue lavals_from_string(std::string const& s) {
  LAValue v;
  std::string sub=s;
  size_t i(0);
  while (i!=std::string::npos) {
    i = sub.find(" ");
    v.insert(std::stoul(sub.substr(0,i)));
    sub=sub.substr(i+1);
  }
  return v;
}

std::string LAMessage::to_string() const{
  // $LAMSG: $type|proposal|round|LAValue
  std::string payload = "";
  payload += std::to_string(unsigned(type));
  payload += LASEPARATOR;
  payload += std::to_string(proposal_number);
  payload += LASEPARATOR;
  payload += std::to_string(round_number);
  payload += LASEPARATOR;
  payload += lavals_to_string(v);
  payload += LASEPARATOR;
  return payload;
}