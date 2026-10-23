// 主机（x86 g++）软件检查：UTF-8模板名称校验、\uXXXX 线路编码与 JsonWriter::name。
// 链接的是正式 firmware 源文件，不是重写的算法；不是 ARM/Unicorn 模拟，也不是实板。
#include "common/DeviceConfig.h"
#include "config/ProfileManager.h"
#include "protocol/Json.h"
#include <cstdio>
#include <cstring>
#include <string>
using namespace fluxrail;
static int checks = 0, failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(cond)) {                                                             \
      ++failures;                                                              \
      std::printf("FAIL line %d: %s\n", __LINE__, #cond);                      \
    }                                                                          \
  } while (0)

static bool parseName(const std::string &json, char (&out)[32]) {
  protocol::Json j;
  if (!j.parse(json.data(), uint16_t(json.size())))
    return false;
  return j.utf8Value(j.field(0, "name"), out, sizeof(out));
}
static std::string written(const char *name) {
  char buffer[256]{};
  protocol::JsonWriter w(buffer, sizeof(buffer) - 1);
  w.name(name);
  return w.ok() ? std::string(buffer, w.size()) : std::string("<overflow/rejected>");
}

int main() {
  // ---- validName：保持原有ASCII行为 ----
  CHECK(config::validName("SDVX"));
  CHECK(config::validName("IN FALSUS"));
  CHECK(config::validName("1234567890123456789012345678901")); // 31 ASCII
  CHECK(!config::validName("12345678901234567890123456789012")); // 32 bytes, no NUL slot
  CHECK(!config::validName(""));
  CHECK(!config::validName("bad\"name"));
  CHECK(!config::validName("bad\\name"));
  CHECK(!config::validName("tab\tname"));
  CHECK(!config::validName("del\x7f"));
  // ---- 中文与其他 BMP 字符 ----
  CHECK(config::validName("\xE9\x9F\xB3\xE5\x87\xBB"));                 // 音击
  CHECK(config::validName("\xE6\x99\xAE\xE9\x80\x9A\xE5\xA4\x9A K"));   // 普通多 K
  CHECK(config::validName("\xC3\xA9"));                                  // é (2 bytes)
  // 10 个汉字 = 30 字节，可再加 1 个 ASCII 字符到 31 字节
  CHECK(config::validName("\xE4\xB8\x80\xE4\xBA\x8C\xE4\xB8\x89\xE5\x9B\x9B\xE4\xBA\x94\xE5\x85\xAD\xE4\xB8\x83\xE5\x85\xAB\xE4\xB9\x9D\xE5\x8D\x81"));
  CHECK(config::validName("\xE4\xB8\x80\xE4\xBA\x8C\xE4\xB8\x89\xE5\x9B\x9B\xE4\xBA\x94\xE5\x85\xAD\xE4\xB8\x83\xE5\x85\xAB\xE4\xB9\x9D\xE5\x8D\x81" "A"));
  CHECK(!config::validName("\xE4\xB8\x80\xE4\xBA\x8C\xE4\xB8\x89\xE5\x9B\x9B\xE4\xBA\x94\xE5\x85\xAD\xE4\xB8\x83\xE5\x85\xAB\xE4\xB9\x9D\xE5\x8D\x81" "AB")); // 32 bytes
  // ---- 拒绝：4字节（emoji）、非最短形式、代理对、续字节开头、截断 ----
  CHECK(!config::validName("\xF0\x9F\x98\x80"));
  CHECK(!config::validName("\xC0\xAF"));
  CHECK(!config::validName("\xC1\xBF"));
  CHECK(!config::validName("\xE0\x80\xAF"));
  CHECK(!config::validName("\xED\xA0\x80"));
  CHECK(!config::validName("\x80"));
  CHECK(!config::validName("\xE9\x9F"));
  CHECK(!config::validName("\xE9\x9F" "A"));
  CHECK(!config::validName("a\xFF"));
  // ---- 拒绝：控制/不可见/双向控制与非字符 ----
  CHECK(!config::validName("\xC2\x85"));          // U+0085 C1
  CHECK(!config::validName("\xE2\x80\x8B"));      // U+200B
  CHECK(!config::validName("\xE2\x80\xAE"));      // U+202E
  CHECK(!config::validName("\xE2\x80\xA8"));      // U+2028
  CHECK(!config::validName("\xE2\x81\xA6"));      // U+2066
  CHECK(!config::validName("\xEF\xBB\xBF"));      // U+FEFF
  CHECK(!config::validName("\xEF\xBF\xBF"));      // U+FFFF
  // ---- 重命名/新建经由正式管理器 ----
  static common::DeviceConfig c;
  config::defaults(c);
  CHECK(config::validDevice(c));
  // ---- 线路：\uXXXX 解码 ----
  char out[32]{};
  CHECK(parseName("{\"name\":\"abc\"}", out) && !std::strcmp(out, "abc"));
  CHECK(parseName("{\"name\":\"\\u97f3\\u51fb\"}", out) && !std::strcmp(out, "\xE9\x9F\xB3\xE5\x87\xBB"));
  CHECK(parseName("{\"name\":\"\\u97F3\\u51FB K\"}", out) && !std::strcmp(out, "\xE9\x9F\xB3\xE5\x87\xBB K"));
  CHECK(parseName("{\"name\":\"\\u0041\"}", out) && !std::strcmp(out, "A"));
  CHECK(parseName("{\"name\":\"\\u00e9\"}", out) && !std::strcmp(out, "\xC3\xA9"));
  CHECK(!parseName("{\"name\":\"\\ud83d\\ude00\"}", out)); // 代理对
  CHECK(!parseName("{\"name\":\"\\u0000\"}", out));
  CHECK(!parseName("{\"name\":\"\\u001f\"}", out));
  CHECK(!parseName("{\"name\":\"\\n\"}", out));
  CHECK(!parseName("{\"name\":\"\\\"\"}", out) || !config::validName(out));
  CHECK(!parseName("{\"name\":\"\\\\\"}", out) || !config::validName(out));
  CHECK(!parseName("{\"name\":\"\\u12\"}", out));
  CHECK(!parseName("{\"name\":\"\xE9\x9F\xB3\"}", out)); // 原始高位字节仍被词法器拒绝
  // 容量：31字节可放下，32字节放不下（需结尾0）
  CHECK(parseName("{\"name\":\"1234567890123456789012345678901\"}", out));
  CHECK(!parseName("{\"name\":\"12345678901234567890123456789012\"}", out));
  CHECK(parseName("{\"name\":\"\\u4e00\\u4e8c\\u4e09\\u56db\\u4e94\\u516d\\u4e03\\u516b\\u4e5d\\u5341A\"}", out) && std::strlen(out) == 31);
  CHECK(!parseName("{\"name\":\"\\u4e00\\u4e8c\\u4e09\\u56db\\u4e94\\u516d\\u4e03\\u516b\\u4e5d\\u5341AB\"}", out));
  // ---- 写出：非ASCII全部转义，线路纯ASCII；往返相等 ----
  CHECK(written("SDVX") == "SDVX");
  CHECK(written("\xE9\x9F\xB3\xE5\x87\xBB") == "\\u97f3\\u51fb");
  CHECK(written("\xC3\xA9") == "\\u00e9");
  CHECK(written("\xF0\x9F\x98\x80") == "<overflow/rejected>");
  CHECK(written("\xE9\x9F") == "<overflow/rejected>");
  {
    const std::string line = "{\"name\":\"" + written("\xE6\x99\xAE\xE9\x80\x9A\xE5\xA4\x9A K") + "\"}";
    for (unsigned char ch : line)
      CHECK(ch < 0x80);
    CHECK(parseName(line, out) && !std::strcmp(out, "\xE6\x99\xAE\xE9\x80\x9A\xE5\xA4\x9A K") && config::validName(out));
  }
  // ---- 已有管理器入口：新建/改名接受中文，非法名称不改动候选 ----
  CHECK(config::createProfile(c, 1, "\xE9\x9F\xB3\xE5\x87\xBB\xE8\x87\xAA\xE7\x94\xA8"));
  CHECK(config::validDevice(c));
  const uint32_t created = c.profiles[c.profile_count - 1].id;
  CHECK(created > 4 && config::renameProfile(c, created, "\xE6\x99\xAE\xE9\x80\x9A\xE5\xA4\x9A K"));
  CHECK(!std::strcmp(config::find(c, created)->name, "\xE6\x99\xAE\xE9\x80\x9A\xE5\xA4\x9A K"));
  CHECK(!config::renameProfile(c, created, "\xF0\x9F\x98\x80"));
  CHECK(!config::renameProfile(c, created, "\xE2\x80\xAE" "abc"));
  CHECK(!std::strcmp(config::find(c, created)->name, "\xE6\x99\xAE\xE9\x80\x9A\xE5\xA4\x9A K"));
  CHECK(!config::createProfile(c, 1, "\xF0\x9F\x98\x80"));
  CHECK(config::validDevice(c));
  CHECK(!std::strcmp(config::find(c, 1)->name, "SDVX") && !std::strcmp(config::find(c, 2)->name, "ONGEKI"));
  std::printf("utf8_name_tests: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
