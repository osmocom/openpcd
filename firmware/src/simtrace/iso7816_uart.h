#ifndef SIMTRACE_ISO7816_UART_H
#define SIMTRACE_ISO7816_UART_H

struct simtrace_stats *iso_uart_stats_get(void);
void iso_uart_stats_dump(void);
void iso_uart_dump(void);
void iso_uart_rst(unsigned int state);
void iso_uart_rx_mode(void);
void iso_uart_clk_master(unsigned int master);
void iso_uart_init(void);
void iso_uart_report_overrun(void);

#endif
