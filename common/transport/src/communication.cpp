#include "communication.h"

constexpr char LOWER_XDIGITS[] = "0123456789abcdef";

string format_hex(uint8_t c) {
  string s;
  s.resize(2);

  s[0] = LOWER_XDIGITS[c >> 4];
  s[1] = LOWER_XDIGITS[c & 0xf];

  return s;
}

string format_hex(std::span<const uint8_t> s) {
  string res;
  res.resize(s.size() * 2);

  auto p = std::begin(res);

  for (auto c : s) {
    *p++ = LOWER_XDIGITS[c >> 4];
    *p++ = LOWER_XDIGITS[c & 0x0f];
  }
  return res;
}

ostream &operator<<(ostream &os, const ngtcp2_cid &cid) {
  return os << "0x" << format_hex({cid.data, cid.datalen});
}

bool operator<=(ngtcp2_cid const &cid_A, ngtcp2_cid const &cid_B) {
    size_t min_len = min(cid_A.datalen, cid_B.datalen);
    for(size_t i = 0; i < min_len; i++)
    {
        if (cid_A.data[i] > cid_B.data[i])
            return false;
        if (cid_A.data[i] < cid_B.data[i])
            return true;
    }
    if(cid_A.datalen > cid_B.datalen)
        return false;
    return true;
}

bool operator>=(ngtcp2_cid const &cid_A, ngtcp2_cid const &cid_B) {
    return cid_B <= cid_A;
}

bool operator==(ngtcp2_cid const &cid_A, ngtcp2_cid const &cid_B) {
    if (cid_A.datalen != cid_B.datalen)
        return false;
    for(size_t i = 0; i < cid_A.datalen; i++)
    {
        if (cid_A.data[i] != cid_B.data[i])
            return false; 
    }
    return true;
}

bool operator<(ngtcp2_cid const &cid_A, ngtcp2_cid const &cid_B)
{
    return cid_B <= cid_A;
}

bool operator>(ngtcp2_cid const &cid_A, ngtcp2_cid const &cid_B)
{
    return cid_B >= cid_A;
}
