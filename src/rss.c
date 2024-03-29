#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "himem.h"
#include "uart.h"
#include "rss.h"

// static buffers for RSSN APIs
static uint8_t request_buf[ 512 ];
static uint8_t response_buf[ 1024 * 128 ];
static uint8_t body_size_buf[ 16 ];

//
//  open rss
//
int32_t rss_open(RSS* rss) {

  // use high memory if we can
  rss->use_high_memory = himem_isavailable() ? 1 : 0;

  return 0;
}

//
//  close rss
//
void rss_close(RSS* rss) {
}

//
//  download rss channel content
//
int32_t rss_download_channel(RSS* rss, const char* rss_url, FILE* output_file, int16_t output_format, UART* uart) {

  // default return code
  int32_t rc = -1;

  // request
  if (output_format == 1) {
    strcpy(request_buf, ">|        /dshell?link=");
  } else {
    strcpy(request_buf, ">|        /openbbs?link=");    
  }
  strcat(request_buf, rss_url);
  size_t request_size = strlen(request_buf);
  sprintf(body_size_buf, "%08x", request_size - 10);
  memcpy(request_buf + 2, body_size_buf, 8);
  if (uart_write(uart, request_buf, request_size) != 0) {
    //printf("error: request write error.\n");
    goto exit;
  }

  // response
  int32_t uart_result = uart_read(uart, response_buf, 14);
  if (uart_result != UART_OK) {
    rc = uart_result;
    goto exit;
  }
  if (memcmp(response_buf, "<|0200", 6) != 0) {
    //printf("error: unexpected error code.\n");
    goto exit;
  }

  size_t response_size;
  sscanf(response_buf + 6, "%08x", &response_size);
  if (response_size > 1024 * 128 - 16) {
    //printf("error: too large response.\n");
    goto exit;
  }
  uart_result = uart_read(uart, response_buf + 14, response_size);
  if (uart_result != UART_OK) {
    rc = uart_result;
    goto exit;
  }

  size_t write_len = 0;
  do {
    size_t len = fwrite(response_buf + 14 + write_len, 1, response_size - write_len, output_file);
    if (len == 0) break;
    write_len += len;
  } while (write_len < response_size);

  rc = 0;

exit:

  return rc;
}

//
//  get rssn server date/time
//
int32_t rss_datetime(RSS* rss, int16_t tz, uint8_t* buf, size_t buf_len, UART* uart) {

  // default return code
  int32_t rc = -1;

  // request
  sprintf(request_buf, ">|        /datetime?tz=%d", tz);
  size_t request_size = strlen(request_buf);
  sprintf(body_size_buf, "%08x", request_size - 10);
  memcpy(request_buf + 2, body_size_buf, 8);
  if (uart_write(uart, request_buf, request_size) != 0) {
    //printf("error: request write error.\n");
    goto exit;
  }

  // response
  int32_t uart_result = uart_read(uart, response_buf, 14);
  if (uart_result != UART_OK) {
    rc = uart_result;
    goto exit;
  }
  if (memcmp(response_buf, "<|0200", 6) != 0) {
    printf("error: unexpected error code. (%c%c%c%c)\n", response_buf[2], response_buf[3], response_buf[4], response_buf[5]);
    goto exit;
  }

  size_t response_size;
  sscanf(response_buf + 6, "%08x", &response_size);
  if (response_size > buf_len) {
    printf("error: too large response.\n");
    goto exit;
  }
  uart_result = uart_read(uart, buf, response_size);
  if (uart_result != UART_OK) {
    rc = uart_result;
    goto exit;
  }

  rc = 0;

exit:

  return rc;
}
