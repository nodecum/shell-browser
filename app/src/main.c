#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/console/console.h>
#include <shell_browser/shell_browser_console.h>
#include "app_version.h"

#ifdef CONFIG_SHELL_BROWSER

SHELL_BROWSER_CONSOLE_DEFINE( sbc, "$", 128, 128, 1024 );

#endif

void main(void)
{
  console_init();
  shell_browser_console_init( &sbc);

  printk("Zephyr Shell Browser Example Application %s\n", APP_VERSION_STR);
  printk("----------\n");
  printk("Button '1' - cycle through alternative commands / arguments\n");
  printk("Button '2' - choose current command / argument\n");
  printk("Button '3' - execute command\n");
  printk("use only this buttons if in browser mode\n");
  printk("----------\n");
  printk("Button '4' - browser mode (default) - parse shell result\n");
  printk("Button '5' - shell mode - print shell output\n");
  printk("You can use all keys except '1'-'5' if in shell mode\n"); 
  
  char c = '\0';
  while ( 1) {
    c = console_getchar();
    shell_browser_console_putc( &sbc, c);
  }
}
