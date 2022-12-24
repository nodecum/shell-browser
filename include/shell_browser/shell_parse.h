#ifndef SHELL_PARSE_H__
#define SHELL_PARSE_H__

#include <zephyr/types.h>
#include <stdbool.h>
#include <shell_browser/strbuf.h>

enum vt_state {
  Unspec,
  Escape,
  EscapeBracket,
  EscapeNumArg,
  EscapeUnknown,
  Return,
  Newline,
  Separator,
};

enum ct_state {
  Prompt,        // test for prompt
  PromptMatched, // Prompt has matched
  Cmd,           // read commandline after PromptMatched  
  AltOrOut,      // Alternative or output, right after Cmd or PromptMatched   
  Alt,
  Out
};

struct shell_parse_t {
  const char *prompt;  // shell prompt, this signals an command line
  size_t prompt_i;     // actual index of prompt for comparing
  enum vt_state vt;    // virtual terminal state
  enum ct_state ct;    // actual context
  bool alt_or_out_before_prompt; // prompt followed directly after commandline (no output) 
  struct str_buf *cmd; // command line
  struct str_buf *alt; // alternatives
  struct str_buf *out; // command result output
  size_t alt_eq_part;  // length of equal leading characters in alternatives
  size_t alt_pos;      // the actual choosen alternative, position in alt->buffer
};

#define SHELL_PARSE_DEFINE(_name, _prompt, _cmd_buf_size,		\
                           _alt_buf_size, _out_buf_size)		\
  STR_BUF_DECLARE(_name##_cmd_buf, _cmd_buf_size);			\
  STR_BUF_DECLARE(_name##_alt_buf, _alt_buf_size);			\
  STR_BUF_DECLARE(_name##_out_buf, _out_buf_size);			\
  static struct shell_parse_t _name##_shell_parse = {		 	\
    .prompt = _prompt,							\
    .prompt_i = 0,							\
    .vt  = Unspec,							\
    .ct  = Out,								\
    .alt_or_out_before_prompt = false,					\
    .cmd = &_name##_cmd_buf,						\
    .alt = &_name##_alt_buf,						\
    .out = &_name##_out_buf,						\
    .alt_eq_part = 0,							\
    .alt_pos = 0							\
  };                                                                    

#define SHELL_PARSE_RESET_ALT( _p)  \
  _p->alt->size = 0;                \
  _p->alt_eq_part = 0;              \
  _p->alt_pos     = 0;

extern size_t shell_parse( const char* data, size_t length, 
			   struct shell_parse_t *p);

#endif // SHELL_PARSE_H__
