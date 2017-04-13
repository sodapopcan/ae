#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY(k) ((k) & 0x1f)

struct termios orig_termios;

/** terminal **/

void fail(const char *s)
{
  perror(s);
  exit(1);
}

void disable_raw_mode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    fail("tcsetattr");
}

void enable_raw_mode()
{
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    fail("tcgetattr");
  atexit(disable_raw_mode);

  struct termios raw = orig_termios;
  tcgetattr(STDIN_FILENO, &raw);

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= ~(CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    fail("tcsetattr");
}

/** editor **/

char editor_read_key()
{
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      fail("read");
  }
  return c;
}

void editor_process_keypress()
{
  char c = editor_read_key();

  switch (c) {
    case CTRL_KEY('q'):
      exit(0);
      break;
    case 3:
      printf("CTRL-Q to quit\r\n");
  }
}


/** init **/

int main()
{
  enable_raw_mode();

  while (1) {
    editor_process_keypress();
  }

  return 0;
}
