#include "Json.h"
namespace fluxrail {
namespace protocol {
namespace {
bool digit(char c) { return c >= '0' && c <= '9'; }
bool hex(char c) {
  return digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
} // namespace
void Json::space() {
  while (pos_ < length_ && (data_[pos_] == ' ' || data_[pos_] == '\t' ||
                            data_[pos_] == '\r' || data_[pos_] == '\n'))
    ++pos_;
}
bool Json::string() {
  ++pos_;
  while (pos_ < length_) {
    const uint8_t c = static_cast<uint8_t>(data_[pos_++]);
    if (c == '"')
      return true;
    if (c < 0x20 || c >= 0x80)
      return false; // Current schema uses ASCII identifiers only.
    if (c == '\\') {
      if (pos_ == length_)
        return false;
      const char e = data_[pos_++];
      if (e == 'u') {
        for (uint8_t i = 0; i < 4; ++i) {
          if (pos_ == length_ || !hex(data_[pos_++]))
            return false;
        }
      } else if (e != '"' && e != '\\' && e != '/' && e != 'b' && e != 'f' &&
                 e != 'n' && e != 'r' && e != 't')
        return false;
    }
  }
  return false;
}
bool Json::value(uint8_t depth) {
  space();
  if (pos_ == length_ || used_ == kMaxTokens || depth > 12)
    return false;
  const uint16_t index = used_++, start = pos_;
  auto &t = tokens_[index];
  t.start = start;
  const char c = data_[pos_];
  if (c == '{' || c == '[') {
    t.kind = c == '{' ? JsonKind::Object : JsonKind::Array;
    const char close = c == '{' ? '}' : ']';
    ++pos_;
    space();
    if (pos_ < length_ && data_[pos_] == close)
      ++pos_;
    else
      for (;;) {
        if (c == '{') {
          if (pos_ == length_ || data_[pos_] != '"' || !value(depth + 1))
            return false;
          space();
          if (pos_ == length_ || data_[pos_++] != ':')
            return false;
        }
        if (!value(depth + 1))
          return false;
        space();
        if (pos_ == length_)
          return false;
        if (data_[pos_] == close) {
          ++pos_;
          break;
        }
        if (data_[pos_++] != ',')
          return false;
        space(); // Trailing comma is rejected by the next value.
      }
  } else if (c == '"') {
    t.kind = JsonKind::String;
    if (!string())
      return false;
  } else if (c == '-' || digit(c)) {
    t.kind = JsonKind::Number;
    if (c == '-') {
      ++pos_;
      if (pos_ == length_)
        return false;
    }
    if (data_[pos_] == '0')
      ++pos_;
    else {
      if (!digit(data_[pos_]))
        return false;
      while (pos_ < length_ && digit(data_[pos_]))
        ++pos_;
    }
    if (pos_ < length_ && data_[pos_] == '.') {
      ++pos_;
      const auto begin = pos_;
      while (pos_ < length_ && digit(data_[pos_]))
        ++pos_;
      if (begin == pos_)
        return false;
    }
    if (pos_ < length_ && (data_[pos_] == 'e' || data_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < length_ && (data_[pos_] == '+' || data_[pos_] == '-'))
        ++pos_;
      const auto begin = pos_;
      while (pos_ < length_ && digit(data_[pos_]))
        ++pos_;
      if (begin == pos_)
        return false;
    }
  } else {
    const char *word = nullptr;
    if (c == 't') {
      t.kind = JsonKind::True;
      word = "true";
    } else if (c == 'f') {
      t.kind = JsonKind::False;
      word = "false";
    } else if (c == 'n') {
      t.kind = JsonKind::Null;
      word = "null";
    } else
      return false;
    while (*word) {
      if (pos_ == length_ || data_[pos_++] != *word++)
        return false;
    }
  }
  t.length = pos_ - start;
  t.next = used_;
  return true;
}
bool Json::parse(const char *data, uint16_t length) {
  data_ = data;
  length_ = length;
  pos_ = used_ = 0;
  if (!data || !value(0))
    return false;
  space();
  return pos_ == length_;
}
bool Json::text(uint16_t i, const char *s) const {
  if (i >= used_ || tokens_[i].kind != JsonKind::String)
    return false;
  const auto &t = tokens_[i];
  uint16_t n = 0;
  while (s[n]) {
    if (n >= t.length - 2 || data_[t.start + 1 + n] != s[n])
      return false;
    ++n;
  }
  return n == t.length - 2;
}
bool Json::uintValue(uint16_t i, uint32_t &v) const {
  if (i >= used_ || tokens_[i].kind != JsonKind::Number)
    return false;
  uint32_t result = 0;
  const auto &t = tokens_[i];
  for (uint16_t n = 0; n < t.length; ++n) {
    const char c = data_[t.start + n];
    if (!digit(c))
      return false;
    const uint32_t d = c - '0';
    if (result > (UINT32_MAX - d) / 10)
      return false;
    result = result * 10 + d;
  }
  v = result;
  return true;
}
bool Json::boolValue(uint16_t i, bool &v) const {
  if (i >= used_)
    return false;
  if (tokens_[i].kind == JsonKind::True) {
    v = true;
    return true;
  }
  if (tokens_[i].kind == JsonKind::False) {
    v = false;
    return true;
  }
  return false;
}
uint16_t Json::count(uint16_t i) const {
  if (i >= used_ || (tokens_[i].kind != JsonKind::Object &&
                     tokens_[i].kind != JsonKind::Array))
    return kMissing;
  uint16_t n = 0;
  for (uint16_t p = i + 1; p < tokens_[i].next; p = tokens_[p].next)
    ++n;
  return tokens_[i].kind == JsonKind::Object ? n / 2 : n;
}
uint16_t Json::field(uint16_t i, const char *key) const {
  if (i >= used_ || tokens_[i].kind != JsonKind::Object)
    return kMissing;
  uint16_t found = kMissing;
  for (uint16_t p = i + 1; p < tokens_[i].next;) {
    const uint16_t v = p + 1;
    if (text(p, key)) {
      if (found != kMissing)
        return kMissing;
      found = v;
    }
    p = tokens_[v].next;
  }
  return found;
}
bool Json::shape(uint16_t i, const char *const *keys, uint16_t n) const {
  if (count(i) != n || i >= used_ || tokens_[i].kind != JsonKind::Object)
    return false;
  for (uint16_t k = 0; k < n; ++k)
    if (field(i, keys[k]) == kMissing)
      return false;
  return true;
}
bool Json::stringValue(uint16_t token, char *out, size_t capacity) const {
  if (token == kMissing || tokens_[token].kind != JsonKind::String ||
      tokens_[token].length < 2 ||
      size_t(tokens_[token].length - 2) >= capacity)
    return false;
  const auto &t = tokens_[token];
  const auto size = t.length - 2;
  for (uint16_t i = 0; i < size; ++i) {
    const char c = data_[t.start + 1 + i];
    if (c < 32 || c > 126 || c == 92 || c == '"')
      return false;
    out[i] = c;
  }
  out[size] = 0;
  return true;
}
bool Json::floatValue(uint16_t token, float &out) const {
  double value;
  if (!doubleValue(token, value) || value < -3.4028234e38 ||
      value > 3.4028234e38)
    return false;
  out = float(value);
  return true;
}
bool Json::doubleValue(uint16_t token, double &out) const {
  if (token == kMissing || tokens_[token].kind != JsonKind::Number)
    return false;
  const auto &t = tokens_[token];
  uint16_t p = t.start, end = p + t.length;
  bool neg = false;
  if (data_[p] == '-') {
    neg = true;
    ++p;
  }
  double v = 0;
  while (p < end && data_[p] >= '0' && data_[p] <= '9')
    v = v * 10 + double(data_[p++] - '0');
  if (p < end && data_[p] == '.') {
    ++p;
    double scale = 0.1;
    while (p < end && data_[p] >= '0' && data_[p] <= '9') {
      v += double(data_[p++] - '0') * scale;
      scale *= 0.1;
    }
  }
  if (p < end && (data_[p] == 'e' || data_[p] == 'E')) {
    ++p;
    bool down = false;
    if (p < end && (data_[p] == '+' || data_[p] == '-'))
      down = data_[p++] == '-';
    uint32_t e = 0;
    while (p < end) {
      e = e * 10 + uint32_t(data_[p++] - '0');
      if (e > 38)
        return false;
    }
    while (e--)
      v = down ? v / 10 : v * 10;
  }
  if (p != end || !(v >= 0 && v <= 3.4028234e38f))
    return false;
  out = neg ? -v : v;
  return true;
}
void JsonWriter::real(double value) {
  if (!(value >= -1000000 && value <= 1000000)) {
    ok_ = false;
    return;
  }
  if (value < 0) {
    put('-');
    value = -value;
  }
  uint32_t whole = uint32_t(value),
           fraction = uint32_t((value - double(whole)) * 1000000.0 + 0.5);
  if (fraction == 1000000) {
    ++whole;
    fraction = 0;
  }
  number(whole);
  put('.');
  for (uint32_t scale = 100000; scale; scale /= 10)
    put(char('0' + (fraction / scale) % 10));
}
void JsonWriter::put(char c) {
  if (size_ < capacity_)
    data_[size_++] = c;
  else
    ok_ = false;
}
void JsonWriter::literal(const char *s) {
  while (*s)
    put(*s++);
}
void JsonWriter::number(uint32_t v) {
  char digits[10];
  uint8_t n = 0;
  do {
    digits[n++] = static_cast<char>('0' + v % 10);
    v /= 10;
  } while (v);
  while (n)
    put(digits[--n]);
}
} // namespace protocol
} // namespace fluxrail
