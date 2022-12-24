#ifndef SHELL_BROWSER_CONSOLE_H__
#define SHELL_BROWSER_CONSOLE_H__

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <shell_browser/strbuf.h>
#include <shell_browser/shell_browser.h>
#ifdef __cplusplus
extern "C" {
#endif
  
  extern const struct shell_transport_api
  shell_browser_console_shell_transport_api;

  extern const struct shell_browser_transport_api
  shell_browser_console_shell_browser_transport_api;
  
  struct shell_browser_console {
    /** Handler function registered by shell. */
    shell_transport_handler_t shell_handler;
    /** Context registered by shell. */
    void *shell_context;
    shell_browser_handler_t shell_browser_handler;
    void *shell_browser_context;
    const struct shell *shell;
    const struct shell_browser *sb;
    struct ring_buf *shell_read_buf;
    struct ring_buf *shell_write_buf;
    char* console_read_char;
  };

  int shell_browser_console_putc( const struct shell_browser_console *sbc, char c);

#define SHELL_BROWSER_CONSOLE_DEFINE(_name, _prompt, _cmd_buf_size,	\
				     _alt_buf_size, _out_buf_size) 	\
  static struct shell_browser_console _name;				\
  struct shell_transport _name##_shell_transport = {			\
    .api = &shell_browser_console_shell_transport_api,			\
    .ctx = &_name							\
  };									\
  struct shell_browser_transport _name##_shell_browser_transport = {	\
    .api = &shell_browser_console_shell_browser_transport_api,		\
    .ctx = &_name							\
  };									\
  SHELL_BROWSER_DEFINE( _name##_sb, &_name##_shell_browser_transport,	\
   	 		_prompt, _cmd_buf_size,			 	\
   			_alt_buf_size, _out_buf_size);			\
  SHELL_DEFINE( _name##_shell, _prompt, &_name##_shell_transport, 	\
   		0, 0, SHELL_FLAG_OLF_CRLF);				\
  RING_BUF_DECLARE( _name##_shell_read_buf, 32);			\
  RING_BUF_DECLARE( _name##_shell_write_buf, 32);			\
  char _name##_console_read_char;					\
  static struct shell_browser_console _name = {				\
    .shell = &_name##_shell,						\
    .sb = &_name##_sb,							\
    .shell_read_buf = &_name##_shell_read_buf,				\
    .shell_write_buf = &_name##_shell_write_buf,			\
    .console_read_char = &_name##_console_read_char			\
  };									\
  
  int shell_browser_console_init( struct shell_browser_console* );

#ifdef __cplusplus
}
#endif

#endif /* SHELL_BROWSER_CONSOLE_H__ */

