#ifndef STRBUF_H__
#define STRBUF_H__

#include <zephyr/types.h>

struct str_buf {
  char  *buffer;
  size_t size; // current size of string
  size_t max_size;
};

#define STR_BUF_DECLARE(name, size_)			\
  static char __noinit _str_buf_data_##name[size_];	\
  struct str_buf name = {				\
    .buffer = _str_buf_data_##name,			\
    .size = 0,						\
    .max_size = size_					\
  }

#define STR_BUF_INC( s) if( s->size < (s->max_size-1)) ++(s->size); 

#endif // STRBUF_H__
