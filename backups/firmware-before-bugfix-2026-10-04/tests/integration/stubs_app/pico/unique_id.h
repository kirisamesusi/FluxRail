#pragma once
#include <stddef.h>
inline void pico_get_unique_board_id_string(char *out,size_t n){const char *s="0123456789ABCDEF";for(size_t i=0;i<n;++i)out[i]=i<16?s[i]:0;}
