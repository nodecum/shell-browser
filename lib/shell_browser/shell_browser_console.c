#include <shell_browser/shell_browser_console.h>
#include <shell_browser/shell_browser.h>
#include <zephyr/kernel.h>
#include <zephyr/console/console.h>

static int shell_init_cb
( const struct shell_transport *transport,
  const void *config,
  shell_transport_handler_t handler,
  void *context)
{
  //  printk("shell_init_cb\n");
  struct shell_browser_console *sbc =
    (struct shell_browser_console *) transport->ctx;  
  sbc->shell_handler = handler;
  sbc->shell_context = context;
  return 0;
}

static int shell_browser_console_shell_browser_init
( const struct shell_browser_transport *transport,
  const void *config,
  shell_browser_handler_t handler,
  void *context)
{
  struct shell_browser_console *sbc =
    (struct shell_browser_console *) transport->ctx;  
  sbc->shell_browser_handler = handler;
  sbc->shell_browser_context = context;
  return 0;
} 

static int shell_uninit_cb(const struct shell_transport *transport)
{
  //printk("shell_uninit\n");
  /*if ( sbc == NULL) { return -ENODEV; } */
  return 0;
}

static int shell_enable_cb(const struct shell_transport *transport, bool blocking)
{
  //printk("shell_enable\n");
  /*if ( sbc == NULL) { return -ENODEV; } */
  return 0;
}

static int shell_write_cb( const struct shell_transport *transport,
			   const void *data, size_t length, size_t *cnt)
{
  //printk("->shell_write\n");
  if ( transport == NULL || transport->ctx == NULL) { *cnt = 0; return -ENODEV; }
  struct shell_browser_console *sbc =
    (struct shell_browser_console *) transport->ctx;  
  *cnt = ring_buf_put( sbc->shell_write_buf, (const uint8_t*) data, length);
  sbc->shell_browser_handler( SHELL_BROWSER_EVENT_SHELL_RX_RDY,
			      sbc->shell_browser_context);
  //printk("<-shell_write\n");
  return 0;
}

static int read_from_shell( const struct shell_browser_transport *transport,
			    char *data, size_t length, size_t *cnt)
{
  //printk("->read_from_shell\n");
  if ( transport == NULL || transport->ctx == NULL) { *cnt = 0; return -ENODEV; }
  struct shell_browser_console *sbc =
    (struct shell_browser_console *) transport->ctx;  
  *cnt = ring_buf_get( sbc->shell_write_buf, data, length);
  if( ring_buf_is_empty( sbc->shell_write_buf)) {
    sbc->shell_handler( SHELL_TRANSPORT_EVT_TX_RDY, sbc->shell_context);
  } else {
    // read more bytes from the buffer
    sbc->shell_browser_handler( SHELL_BROWSER_EVENT_SHELL_RX_RDY,
				sbc->shell_browser_context);
  }
  //printk("<-read_from_shell\n");
  return 0;
}

static int write_to_shell( const struct shell_browser_transport *transport,
			   const char *data, size_t length, size_t *cnt)
{
  //printk("->write_to_shell\n");
  if ( transport == NULL || transport->ctx == NULL) { *cnt = 0; return -ENODEV; }
  struct shell_browser_console *sbc =
    (struct shell_browser_console *) transport->ctx;  
  if ( sbc == NULL) { *cnt = 0; return -ENODEV; }
  *cnt = ring_buf_put( sbc->shell_read_buf, data, length);
  sbc->shell_handler( SHELL_TRANSPORT_EVT_RX_RDY, sbc->shell_context);
  //printk("<-write_to_shell\n");
  return 0;
}

static int shell_read_cb( const struct shell_transport *transport,
			  void *data, size_t length, size_t *cnt)
{
  //printk("->shell_read\n");
  if ( transport == NULL || transport->ctx == NULL) { *cnt = 0; return -ENODEV; }
  struct shell_browser_console *sbc =
    (struct shell_browser_console *) transport->ctx;  
  if ( sbc == NULL) { *cnt=0; return -ENODEV; }
  *cnt = ring_buf_get( sbc->shell_read_buf, (uint8_t*) data, length);
  //printk("<-shell_read\n");
  return 0;
}

static int write_to_console( const struct shell_browser_transport *transport,
                             const char *data, size_t length, size_t *cnt)
{
  //printk("->write_to_console\n");
  if ( transport == NULL || transport->ctx == NULL) { *cnt = 0; return -ENODEV; }
  struct shell_browser_console *sbc =
    (struct shell_browser_console *) transport->ctx;
  const size_t buf_size = 31;
  char buf[buf_size+1];
  size_t j = 0;
  while (length) {
    size_t i = 0;
    while( i < length && i < buf_size)
      buf[ i++] = data[ j++];
    buf[i] = '\0';
    printk( "%s", buf);
    length -= i;
  }
  *cnt = j;
  sbc->shell_browser_handler( SHELL_BROWSER_EVENT_CONSOLE_TX_DONE,
			      sbc->shell_browser_context);
  //printk("<-write_to_console\n");
  return 0;
}

static int read_from_console( const struct shell_browser_transport *transport,
                              char *data, size_t length, size_t *cnt)
{
  //printk("->read_from_console\n");
  if ( transport == NULL || transport->ctx == NULL) { *cnt = 0; return -ENODEV; }
  struct shell_browser_console *sbc =
    (struct shell_browser_console *) transport->ctx;
  if( length > 0) {
    data[0] = *(sbc->console_read_char);
    *cnt = 1;
  } else {
    *cnt = 0;
  }
  //printk("<-read_from_console\n");
  return 0;
}

int shell_browser_console_putc( const struct shell_browser_console *sbc, char c)
{
  if ( sbc == NULL ) { return -ENODEV; }
  *(sbc->console_read_char) = c;
  sbc->shell_browser_handler( SHELL_BROWSER_EVENT_CONSOLE_RX_RDY,
			      sbc->shell_browser_context);
  return 0;
}

const struct shell_transport_api shell_browser_console_shell_transport_api = {
  .init = shell_init_cb,
  .uninit = shell_uninit_cb,
  .enable = shell_enable_cb,
  .write = shell_write_cb,
  .read = shell_read_cb
};

const struct shell_browser_transport_api shell_browser_console_shell_browser_transport_api = {
  .init = shell_browser_console_shell_browser_init,
  .write_to_console = write_to_console, 
  .read_from_console = read_from_console,
  .write_to_shell = write_to_shell,
  .read_from_shell = read_from_shell
};

int shell_browser_console_init( struct shell_browser_console *sbc)
{
  static const struct shell_backend_config_flags cfg_flags =
    {						
      .insert_mode	= 0,
      .echo		= 1,
      .obscure	        = 0,				
      .mode_delete	= 0,			
      .use_colors	= 0,			
      .use_vt100	= 1,			
    };
  shell_browser_init( sbc->sb, NULL);
  shell_init( sbc->shell, NULL, cfg_flags, false, LOG_LEVEL_ERR);
  return 0;
}



