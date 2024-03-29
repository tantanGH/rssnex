#ifndef __H_RSS__
#define __H_RSS__

#include <stdio.h>
#include <stdint.h>
#include "uart.h"

#define RSS_OK       (0)
#define RSS_EXIT     (1)
#define RSS_QUIT     (2)
#define RSS_TIMEOUT  (3)

typedef struct {
  int16_t use_high_memory;
} RSS;

int32_t rss_open(RSS* rss);
void rss_close(RSS* rss);

int32_t rss_download_channel(RSS* rss, const char* rss_url, FILE* output_file, int16_t output_format, UART* uart);
int32_t rss_datetime(RSS* rss, int16_t tz, uint8_t* buf, size_t buf_len, UART* uart);

#endif