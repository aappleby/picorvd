#include "gdb_server.h"

#ifdef __riscv

void put_byte(char b) {
}

char get_byte() {
  return 0;
}

#else

#include <fcntl.h>
#include <stdio.h>
#include <sys/poll.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>


static int serial_fd = 0;
static char rbuf[256];
static int rcursor = 0;
static int rsize = 0;

//------------------------------------------------------------------------------

void put_byte(char b) {
  //printf("%c", b);
  fflush(stdout);
  int result = write(serial_fd, &b, 1);
}

//------------------------------------------------------------------------------

char get_byte() {
  if (rcursor == rsize) {
    rcursor = 0;
    rsize = 0;
    pollfd fds = {serial_fd, POLLIN, 0};
    auto poll_result = poll(&fds, 1, 10000);
    rsize = read(serial_fd, rbuf, sizeof(rbuf));
  }

  // Exit app if we can't read a byte
  if (rsize == 0) {
    printf("Failed to get a byte, exiting\n");
    exit(0);
  }

  auto result = rbuf[rcursor++];
  return result;
}
#endif

//------------------------------------------------------------------------------

int main(int argc, char** argv) {

#ifndef __riscv
  if (argc < 2) {
    printf("gdb_server <pty path>\n");
    return 1;
  }

  serial_fd = open(argv[1], O_RDWR | O_NOCTTY);
  printf("File descriptor is %d\n", serial_fd);
#endif

  GDBServer s(get_byte, put_byte);
  s.loop();

#ifndef __riscv
  close(serial_fd);
  serial_fd = 0;
#endif

  return 0;
}

//------------------------------------------------------------------------------
