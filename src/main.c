#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <doslib.h>
#include <iocslib.h>
#include "uart.h"
#include "rss.h"

#define PROGRAM_VERSION "0.1.1 (2023/08/22)"

//
//  helper: vdisp interrupt handler for progress bar display
//
static uint32_t vdisp_counter;
static void __attribute__((interrupt)) vdisp_handler() {

  int16_t c = vdisp_counter;

  if (c < 16) {
    B_PUTMES(7, 31 + c, 31, 0, ">");
  } else {
    B_PUTMES(7, 31 + c - 16, 31, 0, "_");
  }  

  vdisp_counter = ( vdisp_counter + 1 ) % 32;
}

//
//  helper: show help message
//
static void show_help_message() {
  printf("RSSNEX.X - RSSNEX client for X680x0/Human68k " PROGRAM_VERSION " by tantan\n");
  printf("usage: rssnex [options] <rss-url> [output-file]\n");
  printf("options:\n");
//  printf("     -s <speed> ... baud rate (9600/19200/38400) (default:38400)\n");
  printf("     -d     ... output in dshell format\n");
  printf("     -t[tz] ... sync date/time with rssn server\n");
  printf("     -h     ... show help message\n");
  printf("environment variables:\n");
  printf("     RSSN_SPEED   ... baud rate (9600/19200/38400)\n");
  printf("     RSSN_TIMEOUT ... timeout [sec]\n");
  printf("     RSSN_QUIET   ... 0 or none:show progress(default)  1:no progress\n");
  printf("     RSSN_STDOUT  ... 0 or none:write to file  1:write to stdout\n");
}

//
//  main
//
int32_t main(int32_t argc, uint8_t* argv[]) {

  // default return code
  int32_t rc = -1;

  // env variable access buffer
  uint8_t env_var_buffer[ 256 ];

  // baud rate
  int32_t baud_rate = GETENV("RSSN_SPEED", NULL, env_var_buffer) >= 0 ? atoi(env_var_buffer) : 38400;

  // timeout
  int32_t timeout = GETENV("RSSN_TIMEOUT", NULL, env_var_buffer) >= 0 ? atoi(env_var_buffer) : 60;

  // quiet mode
  int16_t quiet_mode = GETENV("RSSN_QUIET", NULL, env_var_buffer) >= 0 ? atoi(env_var_buffer) : 0;

  // stdout mode
  int16_t stdout_mode = GETENV("RSSN_STDOUT", NULL, env_var_buffer) >= 0 ? atoi(env_var_buffer) : 0;

  // date/time sync mode
  int16_t datetime_sync_mode = 0;
  int16_t tz = 9;

  // output format (0:tsv 1:dshell)
  int16_t output_format = 0;

  // rss url
  uint8_t* rss_url = NULL;

  // output file name
  uint8_t output_file_name[ 256 ];
  strcpy(output_file_name, "_R.D");   // default

  // output file handle
  FILE* fo = NULL;

  // uart instance
  UART uart = { 0 };

  // rss instance
  RSS rss = { 0 };

  // original function key mode
  int32_t func_mode = C_FNKMOD(-1);

  // parse command lines
  for (int16_t i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && strlen(argv[i]) >= 2) {
//      if (argv[i][1] == 's' && i+1 < argc) {
//        baud_rate = atoi(argv[i+1]);
//        i++;
//      } else 
      if (argv[i][1] == 'h') {
        show_help_message();
        goto exit;
      } else if (argv[i][1] == 'd') {
        output_format = 1;
      } else if (argv[i][1] == 't') {
        datetime_sync_mode = 1;
        quiet_mode = 1;
        if (strlen(argv[i]) > 2) {
          tz = atoi(argv[i]+2);
        }
      } else {
        printf("error: unknown option (%s).\n",argv[i]);
        goto exit;
      }
    } else {
      if (rss_url == NULL) {
        rss_url = argv[i];
      } else {
        strcpy(output_file_name, argv[i]);
      }
    }
  }

  if (baud_rate != 9600 && baud_rate != 19200 && baud_rate != 38400) {
    printf("error: unsupported baud rate. (%d)\n", baud_rate);
    goto exit;
  }

  if (!datetime_sync_mode && rss_url == NULL) {
    show_help_message();
    goto exit;
  }

try:

  // cursor off
  if (!quiet_mode) C_CUROFF();

  // function key off
  if (!quiet_mode) C_FNKMOD(3);

  // open uart  
  if (uart_open(&uart, baud_rate, timeout) != 0) {
    goto catch;
  }

  // open rss
  if (rss_open(&rss) != 0) {
    goto catch;
  }

  // datetime sync mode
  if (datetime_sync_mode) {

    uint8_t ts0[128];
    uint8_t ts1[128];

    // get the current time
    if (rss_datetime(&rss, tz, ts0, 127, &uart) != 0) {
      printf("error: datetime sync error.\n");
      goto catch;
    }
    ts0[19] = '\0';

    for (;;) {

      // check the next second timing
      if (rss_datetime(&rss, tz, ts1, 127, &uart) != 0) {
        printf("error: datetime sync error.\n");
        goto catch;
      }
      ts1[19] = '\0';
      if (strcmp(ts0, ts1) == 0) {
        for (uint32_t t0 = ONTIME(); ONTIME() < t0 + 10; ) {}   // wait 100msec
        continue;
      }

      printf("RSSN Server Date/Time: %s\n", ts1);
      ts1[4] = '\0';
      ts1[7] = '\0';
      ts1[10] = '\0';
      ts1[13] = '\0';
      ts1[16] = '\0';
      
      int16_t hour = atoi(ts1+11);
      int16_t min = atoi(ts1+14);
      int16_t sec = atoi(ts1+17);
      SETTIM2((hour << 16) | (min << 8) | sec);
      
      int16_t year = atoi(ts1) - 1980;
      int16_t month = atoi(ts1+5);
      int16_t day = atoi(ts1+8);
      SETDATE((year << 9) | (month << 5) | day);
      
      printf("Synchronized.\n");
      rc = 0;
      break;

    }
    goto catch;
  }

  // open output file in binary mode
  fo = stdout_mode ? stdout : fopen(output_file_name, "wb");
  if (fo == NULL) {
    goto catch;
  } 

  // message
  if (!quiet_mode) B_PUTMES(7, 0, 31, 32, "Now Loading... [ESC] to cancel ");

  // vsync interrupt for progress bar
  if (!quiet_mode) VDISPST((uint8_t*)vdisp_handler, 0, 55);

  // download channel items
  int32_t download_result = rss_download_channel(&rss, rss_url, fo, output_format, &uart);
    
  // check communication result
  if (download_result == UART_QUIT || download_result == UART_EXIT) {
    printf("error: canceled.\n");
  } else if (download_result == UART_TIMEOUT) { 
    printf("error: timeout.\n");
  } else {
    rc = 0;
  }

catch:

  // stop vsync interrupt handler
  if (!quiet_mode) VDISPST(0, 0, 0);

  // close output file handle
  if (!stdout_mode && fo != NULL) {
    fclose(fo);
    fo = NULL;
  }

  // close rss
  rss_close(&rss);

  // close uart
  uart_close(&uart);

  // remove file in error cases
  if (rc != 0) {
    DELETE(output_file_name);
  }

  // cursor on
  if (!quiet_mode) C_CURON();

  // resume function key mode
  if (!quiet_mode) C_FNKMOD(func_mode);

exit:

  return rc;
}