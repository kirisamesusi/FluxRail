#pragma once
#include <stddef.h>
#include <stdint.h>
namespace fluxrail {
namespace protocol {
// Fixed token arena, depth and line limits; no heap or null-terminated input
// dependency.
enum class JsonKind : uint8_t {
  Object,
  Array,
  String,
  Number,
  True,
  False,
  Null
};
struct JsonToken {
  JsonKind kind{};
  uint16_t start = 0, length = 0, next = 0;
};
class Json {
public:
  static constexpr uint16_t kMaxTokens = 384, kMissing = 0xffff;
  bool parse(const char *data, uint16_t length);
  bool text(uint16_t token, const char *literal) const;
  bool uintValue(uint16_t token, uint32_t &value) const;
  bool stringValue(uint16_t token, char *out, size_t capacity) const;
  bool floatValue(uint16_t token, float &value) const;
  bool doubleValue(uint16_t token, double &value) const;
  bool boolValue(uint16_t token, bool &value) const;
  bool shape(uint16_t object, const char *const *keys, uint16_t count) const;
  uint16_t field(uint16_t object, const char *key) const;
  uint16_t count(uint16_t container) const;
  const JsonToken &operator[](uint16_t token) const { return tokens_[token]; }

private:
  JsonToken tokens_[kMaxTokens]{};
  const char *data_ = nullptr;
  uint16_t length_ = 0, pos_ = 0, used_ = 0;
  void space();
  bool value(uint8_t depth);
  bool string();
};
class JsonWriter {
public:
  JsonWriter(char *data, size_t capacity) : data_(data), capacity_(capacity) {}
  void literal(const char *text);
  void number(uint32_t value);
  void real(double value);
  void boolean(bool value) { literal(value ? "true" : "false"); }
  bool ok() const { return ok_; }
  size_t size() const { return size_; }

private:
  char *data_;
  size_t capacity_, size_ = 0;
  bool ok_ = true;
  void put(char c);
};
} // namespace protocol
} // namespace fluxrail
