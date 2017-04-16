#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define AE_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define EMPTY_LINE_CHAR "~"

struct Ae {
	struct screen {
		int rows;
		int cols;
	} screen;
	struct termios orig_termios;
} Ae;


/** helpers **/

void print_char(char c)
{
	while (read(STDIN_FILENO, &c, 1) == 1) {
		if (iscntrl(c))
			printf("%d\r\n", c);
		else
			printf("%d ('%c')\r\n", c, c);
	}
}

/** terminal **/

void fail(const char *s)
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);
	exit(1);
}

void disable_raw_mode()
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &Ae.orig_termios) == -1)
		fail("tcsetattr");
}

void enable_raw_mode()
{
	if (tcgetattr(STDIN_FILENO, &Ae.orig_termios) == -1)
		fail("tcgetattr");
	atexit(disable_raw_mode);

	struct termios raw = Ae.orig_termios;
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

int cursor_get_position(int *rows, int *cols)
{
	char buf[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
		return -1;

	while (1 < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1)
			break;
		if (buf[i] == 'R')
			break;
		i++;
	}
	buf[i] = '\0';

	if (buf[0] != '\x1b' || buf[1] != '[')
		return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
		return -1;

	return 0;
}

int window_get_size(int *rows, int *cols)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
			return -1;
		return cursor_get_position(rows, cols);
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}


/** buffer **/

#define BUFFER_INIT {NULL, 0}

typedef struct Buffer {
	char *b;
	int len;
} Buffer;

void buffer_append(Buffer *b, const char *s, int len)
{
	char *new = realloc(b->b, b->len + len);

	if (new == NULL)
		return;
	memcpy(&new[b->len], s, len);
	b->b = new;
	b->len += len;
}

void buffer_free(Buffer *b)
{
	free(b->b);
}

/** editor **/

void editor_init()
{
	if (window_get_size(&Ae.screen.rows, &Ae.screen.cols) == -1)
		fail("window_get_size");
}

void editor_welcome_msg(Buffer *b)
{
	char welcome[80];
	int welcome_len = snprintf(welcome, sizeof welcome,
			"Andrew's Editor -- version %s", AE_VERSION);
	if (welcome_len > Ae.screen.cols)
		welcome_len = Ae.screen.cols;
	int padding = (Ae.screen.cols - welcome_len) / 2;
	if (padding) {
		buffer_append(b, EMPTY_LINE_CHAR, 1);
		padding--;
	}
	while (padding--)
		buffer_append(b, " ", 1);
	buffer_append(b, welcome, welcome_len);
}

void editor_draw_rows(Buffer *b)
{
	int y;
	for (y = 0; y < Ae.screen.rows; y++) {
		if (y == Ae.screen.rows / 3) {
			editor_welcome_msg(b);

		} else {
			buffer_append(b, EMPTY_LINE_CHAR, 1);
		}

		if (y < Ae.screen.rows - 1)
			buffer_append(b, "\r\n", 2);
	}
}

void editor_refresh_screen()
{
	Buffer b = BUFFER_INIT;

	buffer_append(&b, "\x1b[?25l", 6);
	buffer_append(&b, "\x1b[2J", 4);
	buffer_append(&b, "\x1b[H", 3);

	editor_draw_rows(&b);

	buffer_append(&b, "\x1b[H", 3);
	buffer_append(&b, "\x1b[?25h", 6);

	write(STDOUT_FILENO, b.b, b.len);

	buffer_free(&b);
}

void editor_process_keypress()
{
	char c = editor_read_key();

	switch (c) {
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
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
	editor_init();

	while (1) {
		editor_refresh_screen();
		editor_process_keypress();
	}

	return 0;
}
