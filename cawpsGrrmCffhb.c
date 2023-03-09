/* Copyright (c) 2023 Jefferson Venancius */

/* MIT License */

/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the */
/* "Software"), to deal in the Software without restriction, including */
/* without limitation the rights to use, copy, modify, merge, publish, */
/* distribute, sublicense, and/or sell copies of the Software, and to */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions: */

/* The above copyright notice and this permission notice shall be */
/* included in all copies or substantial portions of the Software. */

/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND */
/* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE */
/* LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION */
/* OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION */
/* WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// Huge thanks to snaptoken and antirez, snaptoken teaches you how to do a version of antirez's kilo and without that as a starting point I don't know if I would done it. I could, but I probably wouldn't.

// TODO - Count atleast how many days since George R R Martin said he was writing his new book.
// TODO - if there's text above, render 'More--', else, render '>'
// TODO - Tabstop should be configurable.

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE


#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <sys/ioctl.h> // gets window size //

/*** defines (constants) ***/

#define TAB_STOP 8
#define QUIT_TIMES 3
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,
};

typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
} erow;

struct editorConfig {
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int numrows;
  erow *row;
  int dirty;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct termios orig_termios;
};

struct editorConfig E;

/*** Header ***/

void editorSetStatusMessage(const char *fmt, ...);


void die(const char *s) {
write(STDOUT_FILENO, "\x1b[2J", 4);
write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1);
      die("tcsetattr");
};


void enableRawMode() {
if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  tcgetattr(STDIN_FILENO, &E.orig_termios);
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;

  // Disabling default keystrokes, the ones without description is just tradition.
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK| ISTRIP | IXON);
  // That one disables:
  // ICRNL - remove terminal translation from \r to \n so ^-m is exactly what it means (\r)
  // IXON - ^-s, ^q
  raw.c_cflag |= (CS8);
  // mask to read 8bit
  raw.c_oflag &= ~(OPOST);
// That one disables:
// OPOST - \r\n from ENTER and ^-m; \r go to first char in the line, \n go to line above (exactly like typewriters)

  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); 
// That one disables:
// ECHO - echo (terminal uses it all the time) ;
// ICANON - canonical mode (default terminal)
// IEXTEN - ^-o, ^-v, ^-c default
// ISIG - ^-c, ^-z (kill/suspend terminal process, so if you disable this your terminal will be stuck unless you kill it outside!)
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}


int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
      if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
      if (seq[2] == '~') {
        switch (seq[1]) {
        case '1': return HOME_KEY;
        case '3': return DEL_KEY;
        case '4': return END_KEY;
        case '5': return PAGE_UP;
        case '6': return PAGE_DOWN;
        case '7': return HOME_KEY;
        case '8': return END_KEY;
      }
    }
  } else {
      switch (seq[1]) {
        case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }
  }

    return '\x1b';

  } else {
  return c;
  }
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO,"\x1b[6n", 4) != 4) return -1;

  printf("\r\n");
  char c;
  while (i < sizeof(buf) -1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  printf("\r\n&buff[1]: '%s\r\n", &buf[1]);

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d,%d", rows, cols) != 2) return -1;
  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if(write(STDOUT_FILENO,"\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** row operations ***/

int editorRowCxToRx(erow *row, int cx) {

  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (TAB_STOP - 1) - (rx % TAB_STOP);
    rx++;
  }
  return rx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t') tabs++;

  free(row->render);
  row->render = malloc(row->size + tabs*(TAB_STOP - 1) + 1);

  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t'){
      row->render[idx++] = ' ';
      while (idx % TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
}



void editorAppendRow(char *s, size_t len) {
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
  }

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;

  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

/*** editor operations ***/

void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorAppendRow("", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}


/*** file i/o ****/

char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1;
  *buflen = totlen;

  char *buf = malloc(totlen);
  char *p = buf;
  for (j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
}

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);
  
  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;

    while ((linelen = getline(&line, &linecap, fp)) != -1) {
      while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen -1] == '\r'))
        linelen--;
        editorAppendRow(line, linelen);
    }

  free(line);
  fclose(fp);
  E.dirty = 0;
}

void editorSave() {
  if (E.filename == NULL) return;

  int len;
  char *buf = editorRowsToString(&len);

  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        E.dirty = 0;
        editorSetStatusMessage("You're saved!");
        return;
      }
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("You're doomed! %s", strerror(errno));
}

/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};


#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}


// input

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch (key) {
  case ARROW_LEFT:
    if (E.cx != 0) {
    E.cx--;
    } else if (E.cy > 0) {
      E.cy--;
      E.cx = E.row[E.cy].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && E.cx < row->size) {
    E.cx++;
    } else if (row && E.cx == row->size) {
      E.cy++;
      E.cx = 0;
    }
    break;
  case ARROW_UP:
  if (E.cy != 0) {
    E.cy--;
    }
    break;
  case ARROW_DOWN:
  if (E.cy < E.numrows) {
    E.cy++;
    }
    break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}


void editorProcessKeyPress() {
  static int quit_times = QUIT_TIMES;

  int c = editorReadKey();

  switch (c) {
    case '\r':
      break;

    case CTRL_KEY('q'):
      if (E.dirty && quit_times > 0) {
        switch (quit_times) {
          case 3:
            editorSetStatusMessage("There are things to save, George.");
            break;
          case 2:
            editorSetStatusMessage("Don't leave me, George.");
            break;
          case 1:
            editorSetStatusMessage("WHY DO YOU HATE ME!?");
            break;
        }
        quit_times--;
        return;
      }
      write(STDOUT_FILENO,"\x1b[2J",4);
      write(STDOUT_FILENO,"\x1b[H",3);
      exit(0);
      break;

    case CTRL_KEY('s'):
      editorSave();
      break;


    case HOME_KEY:
      E.cx = 0;
      break;
    case END_KEY:
      if (E.cy < E.numrows)
        E.cx = E.row[E.cy].size;
      break;

    case BACKSPACE:
    /* case CTRL_KEY('h'): */
    case DEL_KEY:
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    /* case CTRL_KEY('l'): */
    case '\x1b':
      break;

    default:
      editorInsertChar(c);
      break;
  }
  quit_times = QUIT_TIMES;
}

#define WINDS_OF_WINTER_STARTS "4552"

// output

void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows){
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screenrows + 1;
  }
}




void editorDrawRows(struct abuf *ab) {// june 27th 2010 was the first anouncement about it. TODO - calculate how many days.
  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
        if (E.numrows == 0 && y == E.screenrows / 3) {
          char welcome[80];
          int welcomelen = snprintf(welcome, sizeof(welcome),
          "It's been %s days and George still didn't finish it.", WINDS_OF_WINTER_STARTS);
          if (welcomelen > E.screencols) welcomelen = E.screencols;
          int padding = (E.screencols - welcomelen) / 2;
          if (padding) {
            abAppend(ab, ">",1);
            padding--;
          }
          while (padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        } else {
          abAppend(ab, ">", 1);
      }
    } else {
        int len = E.row[filerow].rsize - E.coloff;
        if (len < 0) len = 0;
        if (len > E.screencols) len = E.screencols;
        abAppend(ab, &E.row[filerow].render[E.coloff], len);
      }
    abAppend(ab, "\x1b[K",3);
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab) { // Draw message bar goes here because I want the messages to be drawn in the center.
  char lstatus[80], rstatus[80], cstatus[80];
  int llen = snprintf(lstatus, sizeof(lstatus), "\x1b[7m%.20s%s\x1b[m", E.filename ? E.filename : "", E.dirty ? "*" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "\x1b[7m%s\x1b[m", "this many words.");
  int clen = snprintf(cstatus, sizeof(cstatus), "%s", E.statusmsg);

  if (llen > E.screencols) llen = E.screencols;
  abAppend(ab, lstatus, llen);

  while (llen < E.screencols) {
    if ((llen - 7) == (E.screencols) / 2) {
      abAppend(ab, cstatus, clen);
      llen += clen;
    } else if (E.screencols - (llen - 7) == (rlen-7)){
        abAppend(ab, rstatus, rlen);
        break;
    } else {
    abAppend(ab, " ", 1);
    llen++;
  }
  }
}

void editorRefreshScreen() {
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l",6);
  abAppend(&ab, "\x1b[H",3); // put the cursor at the top.

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, E.rx + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h",6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);

}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.dirty = 0;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows -= 1;
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
  editorOpen(argv[1]);
  }

  editorSetStatusMessage("Ctrl - Q.uit");

  char c;
  while (1) {
    editorRefreshScreen();
    editorProcessKeyPress();
  };
  return 0;
}
