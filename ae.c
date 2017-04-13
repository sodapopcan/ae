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


/** init **/

int main()
{
  enable_raw_mode();

  while (1) {
    char c = '\0';

    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
      fail("read");

    if (iscntrl(c)) {
      if (c == 3) {
        printf("Type `q` to quit\r\n");
      } else {
        printf("%d\r\n", c);
      }
    } else {
      printf("%d ('%c')\r\n", c, c);
    }
    if (c == CTRL_KEY('q')) break;
  }

  return 0;
}
