#pragma once
// 只用于ARM指令测试的端口互斥锁替身；不编入正式固件。
#include <stdint.h>
struct mutex_t {
  bool locked = false;
};
inline bool mutex_try_enter(mutex_t *m, uint32_t *) {
  if (m->locked)
    return false;
  m->locked = true;
  return true;
}
inline void mutex_exit(mutex_t *m) { m->locked = false; }
