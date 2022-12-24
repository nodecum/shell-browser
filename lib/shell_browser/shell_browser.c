#include <shell_browser/shell_browser.h>
#include <string.h>
#include <shell_browser/strbuf.h>

enum endpoint {
  CONSOLE_EP,
  SHELL_EP
};

// copy current alternative to data with ommitting
// the letters in the last snippet of cmdline if present 
// begin to write at position *i in data 
// *i gets incremented as data will be writen
// *new_data is set to lowest position where data is different
// from the value before
void copy_alt( struct shell_parse_t *p, uint8_t *data, size_t *i, uint32_t max_len, 
                uint32_t *new_data) 
{   
  size_t j = p->alt_pos;
  // if cmd exists then copy only if last char is space 
  bool match = false;  
  if( p->cmd->size > 0 && p->alt_eq_part > 0 && p->alt_eq_part < p->alt->size ) {
    int i_ = p->alt_eq_part - 1;
    int j_ = p->cmd->size - 1;
    match = true;
    while( match && j_ >= 0 && p->cmd->buffer[ j_] != ' ') {
      match = i_ >= 0 && p->cmd->buffer[ j_] == p->alt->buffer[ i_];
      --i_; --j_;
    } 
    match = match && i_ < 0;
    if(match) {
      // printk("MATCH i_=%d, j_=%d, match=%d\n", i_, j_, match);
      j += p->alt_eq_part;
    }
  }
  if( p->cmd->size == 0 || p->cmd->buffer[ p->cmd->size - 1] == ' ' || match) {
    while( j < p->alt->size && *i < max_len && 
           p->alt->buffer[ j] != ' ' ) {
      const char c = p->alt->buffer[j];      
      if( data[ *i] != c) {
        data[ *i] = c;
        if( *i < *new_data) *new_data = *i;
      } 
      (*i)++; j++;
    }
  }
}

static void shell_browser_pend_on
( const struct shell_browser *sb, enum shell_browser_signal sig)
{
  struct k_poll_event event;
  k_poll_event_init
    ( &event, K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
      &sb->ctx->signals[ sig]);
  k_poll( &event, 1, K_FOREVER);
  k_poll_signal_reset( &sb->ctx->signals[ sig]);
}

static void write_to
( const struct shell_browser *sb, enum endpoint ep, const char *data, size_t length)
{
  __ASSERT_NO_MSG(sb && data);

  size_t offset = 0;
  size_t tmp_cnt;

  while (length) {
    int err;
    switch( ep) {
    case CONSOLE_EP : err = sb->iface->api->write_to_console
	( sb->iface, &data[offset], length, &tmp_cnt);
      break;
    case SHELL_EP :  err = sb->iface->api->write_to_shell
	( sb->iface, &data[offset], length, &tmp_cnt);
      break;
    }
    __ASSERT_NO_MSG( err == 0);
    __ASSERT_NO_MSG( length >= tmp_cnt);
    offset += tmp_cnt;
    length -= tmp_cnt;
    if (tmp_cnt == 0) {
      shell_browser_pend_on( sb, SHELL_BROWSER_SIGNAL_CONSOLE_TX_DONE + ep);
    }
  }
}

// write zero terminated string
static void write_to_console
( const struct shell_browser *sb, const char *data)
{ write_to( sb, CONSOLE_EP, data, strlen( data)); }

static void sent_event( const struct shell_browser* sb, enum shell_browser_signal s)
{ k_poll_signal_raise( &sb->ctx->signals[ s], 0); }

static void shell_browser_event_handler( enum shell_browser_event evt_type, void *ctx)
{
  struct shell_browser *sb = (struct shell_browser *) ctx;
  // caution, mapping of external event to internal signal enum
  // depend on ordering in enum defenitions
  sent_event( sb,
	      // the first corresponding event enum
	      SHELL_BROWSER_SIGNAL_CONSOLE_RX_RDY + evt_type);
}

typedef void (*shell_browser_signal_handler_t)(const struct shell_browser*);

static void shell_browser_signal_handle
( const struct shell_browser *sb,
  enum shell_browser_signal sig_idx,
  shell_browser_signal_handler_t handler)
{
  struct k_poll_signal *signal = &sb->ctx->signals[sig_idx];
  int set;
  int res;

  k_poll_signal_check(signal, &set, &res);
  
  if (set) {
    k_poll_signal_reset(signal);
    handler(sb);
  }
}

bool sb_cycle_through_alternatives( const struct shell_browser* sb)
{
  bool refresh_display = false;
  struct shell_parse_t *p = sb->ctx->p;
  size_t alt_pos_ = p->alt_pos; // store where we started
  while( p->alt_pos < p->alt->size && p->alt->buffer[ p->alt_pos] != ' ')
    ++p->alt_pos;
  if( p->alt_pos < p->alt->size) {
    ++p->alt_pos; // advance to next entry
    refresh_display = true;
  }
  else if( p->alt_pos == p->alt->size) { // we are on the end
    if( alt_pos_ == 0) {
      // we had only one alternative or no at all
      // so we sent the tab key to the shell
      const char data = '\t';
       size_t count = 0;
      (void) sb->iface->api->write_to_shell
	( sb->iface, &data, sizeof( data), &count);
    } else {
      p->alt_pos = 0; // wrap around
      refresh_display = true;
    }
  }
  return refresh_display;
}

void sb_choose_current_alternative( const struct shell_browser* sb)
{
  // choose current alternative
  const uint32_t max_len = CONFIG_SHELL_BROWSER_MAX_LEN_CMD_PART;
  uint8_t data[ max_len];
  size_t i = 0; 
  uint32_t nd = max_len; // dummy, not used here
  copy_alt( sb->ctx->p, data, &i, max_len, &nd);
  if( i > 0 && i < max_len) data[i++] = ' ';
  if( i < max_len) data[i++] = '\t';
  write_to( sb, SHELL_EP, data, i);
  sb->ctx->p->alt->size = 0;
}

void sb_execute_command( const struct shell_browser* sb)
{
  // execute command
  const uint32_t max_len = CONFIG_SHELL_BROWSER_MAX_LEN_CMD_PART;
  uint8_t data[ max_len];
  size_t i = 0;
  uint32_t nd = max_len; // dummy, not used here
  copy_alt( sb->ctx->p, data, &i, max_len, &nd);
  if( i < max_len) data[i++] = '\r';
  write_to( sb, SHELL_EP, data, i);
}


void shell_browser_handle_console( const struct shell_browser* sb)
{
  size_t count = 0;
  char data;
  (void) sb->iface->api->read_from_console
    ( sb->iface, &data, sizeof( data), &count);
  if( count == 0) {
    return;
  }
  //printk("console:%c\n",data);
  bool refresh_display = false;
  switch( data) {
  case '1':
    refresh_display = sb_cycle_through_alternatives( sb); break;
  case '2':
    sb_choose_current_alternative( sb); break;
  case '3':
    sb_execute_command( sb); break;
  case '4':
    sb->ctx->mode = SB_MODE_PARSE; break;
  case '5':
    sb->ctx->mode = SB_MODE_PRINT; break;
  default:
    write_to( sb, SHELL_EP, &data, sizeof( data));
  }
  if ( refresh_display) {
    sent_event( sb, SHELL_BROWSER_SIGNAL_ALT_POS_CHANGED);
  }
}

void shell_browser_handle_shell( const struct shell_browser* sb)
{
 size_t count = 0;
  char data;
  (void) sb->iface->api->read_from_shell
    ( sb->iface, &data, sizeof( data), &count);
  if( count == 0) {
    return;
  }
  switch( sb->ctx->mode) {
  case SB_MODE_PARSE:
    shell_parse( &data, sizeof( data), sb->ctx->p);
    sent_event( sb, SHELL_BROWSER_SIGNAL_SHELL_PARSED);
    break;
  case SB_MODE_PRINT:
    write_to( sb, CONSOLE_EP, &data, sizeof( data));
    break;
  }
} 

enum intern_signal{ PARSED_INPUT, ALTPOS_CHANGED };

static void handle_intern_signal(const struct shell_browser* sb, enum intern_signal s)
{
  struct shell_parse_t *p = sb->ctx->p;
  struct str_buf *cmd_buf = sb->ctx->cmd_buf;
  struct str_buf *out_buf = sb->ctx->out_buf;
  enum ct_state *ct_ = &sb->ctx->ct_;
  enum ct_state *ct = &sb->ctx->ct;
 
  const size_t cmd_old_size = cmd_buf->size;
  uint32_t cmd_nd = cmd_buf->max_size;
  uint32_t out_nd = out_buf->max_size;
  *ct_ = *ct;  // save the context before the new one 
  *ct = p->ct;
  const bool newPrompt = 
    ( *ct == PromptMatched 
      && ! (cmd_buf->size > 0 && cmd_buf->buffer[ cmd_buf->size-1] == ' ') );
  if( ( (s == PARSED_INPUT) && (*ct == Cmd || newPrompt ) ) 
      || (s == ALTPOS_CHANGED) ) {
    size_t i = 0;  
    while( i < p->cmd->size && i < cmd_buf->max_size) { 
      const char c = p->cmd->buffer[ i];
      if( i < cmd_buf->size) {
	if( cmd_buf->buffer[ i] != c ) {
	  cmd_buf->buffer[ i] = c;
	  if( i < cmd_nd) cmd_nd = i;
	}
      } else {
	cmd_buf->buffer[i] = c;
	cmd_nd = i;
      } 
      ++i; 
    }
    copy_alt( p, cmd_buf->buffer, &i, cmd_buf->max_size, &cmd_nd);
    if( cmd_nd != cmd_buf->max_size ) {  
      if( i < cmd_buf->max_size) cmd_buf->buffer[ i] = '\0'; 
      else cmd_buf->buffer[ i-1] = '\0'; 
      cmd_buf->size = i;
    }
  } 
  else if( *ct == Out) {
    if( *ct_ != Out) 
      { 
	// if we had an output we clear the cmd_buf
	// and the out_buf
	cmd_buf->size=0; cmd_buf->buffer[0]= '\0'; 
	cmd_nd = cmd_buf->max_size; 
	out_buf->size=0; out_buf->buffer[0]= '\0';
	out_nd = out_buf->max_size;
      }
    size_t i = 0;
    while( i < p->out->size && i < out_buf->max_size) { 
      const char c = p->out->buffer[ i];
      if( out_buf->buffer[ i] != c ) {
	out_buf->buffer[ i] = c;
	if( i < out_nd) out_nd = i;
      }
      ++i; 
    }
    if( i < out_buf->max_size) out_buf->buffer[ i] = '\0'; 
    else out_buf->buffer[ i-1] = '\0'; 
    out_buf->size = i;
  }
  
  if( s == PARSED_INPUT && newPrompt) {
    write_to_console( sb, "\r$");
  }
  if( (s == PARSED_INPUT && (*ct == Cmd || *ct == PromptMatched) 
       && cmd_nd < cmd_buf->size)
      || s == ALTPOS_CHANGED ) {
    // we delete 
    int steps_back = cmd_old_size - cmd_nd;
    int j = 0;
    while( j < steps_back) { write_to_console( sb, "\b"); ++j; }
    write_to_console( sb, &cmd_buf->buffer[cmd_nd] );
    j = cmd_buf->size;
    while( j < cmd_old_size) { write_to_console( sb, " "); ++j; }
    j = cmd_buf->size;
    while( j < cmd_old_size) { write_to_console( sb, "\b"); ++j; }
  }
  else if ( !newPrompt && *ct == Out && out_nd < out_buf->size) {
    if( *ct_ != Out) write_to_console( sb, "\n");
    write_to_console( sb, &out_buf->buffer[out_nd]);
  }
}
  
static void handle_parsed_input(  const struct shell_browser* sb)
{ handle_intern_signal( sb, PARSED_INPUT); }

static void handle_altpos_changed( const struct shell_browser* sb)
{ handle_intern_signal( sb, ALTPOS_CHANGED); }  

static void kill_handler( const struct shell_browser *sb)
{
  //printk("KILL\n");
  sb->ctx->tid = NULL;
  k_thread_abort( k_current_get());
}

void shell_browser_process( const struct shell_browser *sb,
			    shell_browser_signal_handler_t handler)
{
  __ASSERT_NO_MSG( sb);
  __ASSERT_NO_MSG( sb->ctx);
  switch (sb->ctx->state) {  
  case SHELL_BROWSER_STATE_UNINITIALIZED:
  case SHELL_BROWSER_STATE_INITIALIZED: 
    break;
  case SHELL_BROWSER_STATE_ACTIVE: 
    handler( sb);
    break;  
  }
}

void shell_browser_process_console( const struct shell_browser* sb)
{ shell_browser_process( sb, shell_browser_handle_console); }

void shell_browser_process_shell( const struct shell_browser* sb)
{ shell_browser_process( sb, shell_browser_handle_shell); }

void shell_browser_process_parsed( const struct shell_browser* sb)
{ shell_browser_process( sb, handle_parsed_input); }

void shell_browser_process_altpos(  const struct shell_browser* sb)
{ shell_browser_process( sb, handle_altpos_changed); }

void shell_browser_thread(void *sb_handle, void *, void *)
{
  printk("shell_browser_thread\n");
  const struct shell_browser *sb = sb_handle;
  int err;
  sb->ctx->state = SHELL_BROWSER_STATE_ACTIVE;
  while (true) {
    /* waiting for all signals except
       SHELL_BROWSER_SIGNAL_CONSOLE_TX_DONE and
       SHELL_BROWSER_SIGNAL_SHELL_TX_DONE */
    err = k_poll( sb->ctx->events, SHELL_BROWSER_SIGNAL_CONSOLE_TX_DONE,
		  K_FOREVER);
    if (err != 0) {
       return;
    }
    k_mutex_lock(&sb->ctx->wr_mtx, K_FOREVER);
    shell_browser_signal_handle( sb, SHELL_BROWSER_SIGNAL_KILL, kill_handler);
    shell_browser_signal_handle( sb, SHELL_BROWSER_SIGNAL_CONSOLE_RX_RDY, shell_browser_process_console);
    shell_browser_signal_handle( sb, SHELL_BROWSER_SIGNAL_SHELL_RX_RDY, shell_browser_process_shell);
    shell_browser_signal_handle( sb, SHELL_BROWSER_SIGNAL_SHELL_PARSED, shell_browser_process_parsed);
    shell_browser_signal_handle( sb, SHELL_BROWSER_SIGNAL_ALT_POS_CHANGED, shell_browser_process_altpos);
    k_mutex_unlock(&sb->ctx->wr_mtx);
  }
}

static int instance_init( const struct shell_browser *sb,
			  const void *config)
{
  // printk("shell_browser instance_init\n");
  sb->ctx->prompt = sb->default_prompt;

  k_mutex_init( &sb->ctx->wr_mtx);
  
  for (int i = 0; i < SHELL_BROWSER_SIGNALS; i++) {
    k_poll_signal_init( &sb->ctx->signals[i]);
    k_poll_event_init( &sb->ctx->events[i],
		       K_POLL_TYPE_SIGNAL,
		       K_POLL_MODE_NOTIFY_ONLY,
		       &sb->ctx->signals[i]);
  }
  int ret = sb->iface->api->init
    ( sb->iface, config, shell_browser_event_handler, (void *)sb);
  if (ret == 0) {
    sb->ctx->state = SHELL_BROWSER_STATE_INITIALIZED;
  }
  return ret;
}


int shell_browser_init( const struct shell_browser *sb,
		        const void *config)
{
  __ASSERT_NO_MSG( sb);
  __ASSERT_NO_MSG( sb->ctx && sb->iface);
  
  if (sb->ctx->tid) {
    return -EALREADY;
  }
  int err = instance_init( sb, config);
  if (err != 0) { return err; }
  k_tid_t tid = k_thread_create
    ( sb->thread, sb->stack, CONFIG_SHELL_BROWSER_STACK_SIZE,
      shell_browser_thread, (void *)sb, UINT_TO_POINTER( 0), UINT_TO_POINTER( 0),
      CONFIG_SHELL_BROWSER_THREAD_PRIORITY, 0, K_NO_WAIT);
  
  sb->ctx->tid = tid;
  k_thread_name_set(tid, sb->thread_name);
  return 0;
}
