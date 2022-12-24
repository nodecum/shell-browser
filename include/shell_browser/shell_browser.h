// -*- tab-width:2; indent-tabs-mode:nil;  -*-
#ifndef SHELL_BROWSER_H__
#define SHELL_BROWSER_H__

#include <shell_browser/strbuf.h>
#include <shell_browser/shell_parse.h>
#include <zephyr/kernel.h>
#ifdef __cplusplus
extern "C" {
#endif

  enum shell_browser_state {
    SHELL_BROWSER_STATE_UNINITIALIZED,
    SHELL_BROWSER_STATE_INITIALIZED,
    SHELL_BROWSER_STATE_ACTIVE
  };
  
  enum shell_browser_event {
    SHELL_BROWSER_EVENT_CONSOLE_RX_RDY,
    SHELL_BROWSER_EVENT_SHELL_RX_RDY,
    SHELL_BROWSER_EVENT_CONSOLE_TX_DONE,
    SHELL_BROWSER_EVENT_SHELL_TX_DONE,
  };

  typedef void (*shell_browser_handler_t)
  (enum shell_browser_event evt, void *context);

  struct shell_browser_transport;

  struct shell_browser_transport_api {
    int (*init)( const struct shell_browser_transport *transport,
                 const void *config,
                 shell_browser_handler_t evt_handler,
                 void *context);
    int (*write_to_console)( const struct shell_browser_transport *transport,
                             const char *data, size_t length, size_t *cnt);
    int (*read_from_console)( const struct shell_browser_transport *transport,
                              char *data, size_t length, size_t *cnt);
    int (*write_to_shell)( const struct shell_browser_transport *transport,
                            const char *data, size_t length, size_t *cnt);
    int (*read_from_shell)( const struct shell_browser_transport *transport,
                             char *data, size_t length, size_t *cnt);
  };
  
  struct shell_browser_transport {
    const struct shell_browser_transport_api *api;
    void *ctx;
  };

  // order depends on shell_browser_evt
  enum shell_browser_signal {
    SHELL_BROWSER_SIGNAL_KILL,
    SHELL_BROWSER_SIGNAL_CONSOLE_RX_RDY,
    SHELL_BROWSER_SIGNAL_SHELL_RX_RDY,
    SHELL_BROWSER_SIGNAL_CONSOLE_TX_DONE,
    SHELL_BROWSER_SIGNAL_SHELL_TX_DONE,
    SHELL_BROWSER_SIGNAL_SHELL_PARSED,
    SHELL_BROWSER_SIGNAL_ALT_POS_CHANGED,
    SHELL_BROWSER_SIGNALS
  };

  enum shell_browser_mode {
    SB_MODE_PARSE,
    SB_MODE_PRINT
  };
  
  struct shell_browser_ctx {
    const char* prompt;
    enum shell_browser_state state;
    struct shell_parse_t *p;
    struct str_buf *cmd_buf;
    struct str_buf *out_buf;
    bool refresh_display;
    enum ct_state ct;
    enum ct_state ct_;
    enum shell_browser_mode mode;
    struct k_poll_signal signals[SHELL_BROWSER_SIGNALS];
    struct k_poll_event events[SHELL_BROWSER_SIGNALS];
    struct k_mutex wr_mtx;
    k_tid_t tid;
  };
   
  /**
   * @brief Shell Browser instance.
   */
  struct shell_browser {
    const char *default_prompt; 
    const struct shell_browser_transport *iface;
    struct shell_browser_ctx *ctx;
    const char *thread_name;
    struct k_thread *thread;
    k_thread_stack_t *stack;
  };
  
#define SHELL_BROWSER_DEFINE( _name, _iface, _prompt, _cmd_buf_size,    \
                              _alt_buf_size, _out_buf_size )            \
  static const struct shell_browser _name;                              \
  SHELL_PARSE_DEFINE( _name##_p, _prompt, _cmd_buf_size,                \
                      _alt_buf_size, _out_buf_size);                    \
  STR_BUF_DECLARE( _name##_cmd_buf, _cmd_buf_size);                     \
  STR_BUF_DECLARE( _name##_out_buf, _out_buf_size);                     \
  static struct shell_browser_ctx _name##_ctx = {                       \
    .p = &_name##_p_shell_parse,                                        \
    .cmd_buf = &_name##_cmd_buf,                                        \
    .out_buf = &_name##_out_buf,                                        \
    .refresh_display = true,                                            \
    .ct = Prompt,                                                       \
    .ct_ = Prompt,                                                      \
    .mode = SB_MODE_PARSE                                               \
  };                                                                    \
  static K_KERNEL_STACK_DEFINE(_name##_stack, CONFIG_SHELL_BROWSER_STACK_SIZE); \
  /*static K_THREAD_STACK_DEFINE(_name##_stack, stack_size);*/          \
	static struct k_thread _name##_thread;                                \
  static const struct shell_browser _name  = {                          \
    .default_prompt = _prompt,                                          \
    .iface = _iface,                                                    \
    .ctx = &_name##_ctx,                                                \
    .thread_name = STRINGIFY(_name),                                    \
		.thread = &_name##_thread,                                          \
		.stack = _name##_stack                                              \
  };                                                                    \
  
  int shell_browser_init( const struct shell_browser *shell_browser,
                          const void *config);
  
#ifdef __cplusplus
}
#endif

#endif /* SHELL_BROWSER_H__ */

