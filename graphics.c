/*
 * graphics.c  --  2D Graphics Editor
 *
 * Flow: Show menu -> User picks option -> Interactive cursor drawing
 *
 * Shape rules:
 *   Rectangle : 3 cursor clicks + 1 auto corner
 *               P1 = free, P2 = same row as P1 (horizontal only),
 *               P3 = same col as P2 (vertical only), P4 = auto
 *   Circle    : cursor picks centre, then TYPE radius as a number
 *   Line      : 2 free cursor points
 *   Triangle  : 3 free cursor points
 *
 * Keys (raw mode):
 *   Arrow keys   -- move cursor one cell
 *   Space/Enter  -- confirm current position
 *   ESC          -- cancel, return to menu
 */

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "graphics.h"

/* ================================================================== */
/*  Globals                                                             */
/* ================================================================== */
char canvas[ROWS][COLS];
GraphicObject objects[MAX_OBJECTS];
int objectCount = 0;
static int nextId = 1;

/* ================================================================== */
/*  Raw terminal                                                        */
/* ================================================================== */
static struct termios g_orig;
static int g_raw = 0;

static void disableRawMode(void) {
  if (g_raw)
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig);
  g_raw = 0;
}

static void enableRawMode(void) {
  tcgetattr(STDIN_FILENO, &g_orig);
  atexit(disableRawMode);
  g_raw = 1;

  struct termios r = g_orig;
  r.c_iflag &= (tcflag_t) ~(IXON | ICRNL);
  r.c_lflag &= (tcflag_t) ~(ECHO | ICANON | ISIG | IEXTEN);
  r.c_cc[VMIN] = 1;
  r.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &r);
}

/* ================================================================== */
/*  Key codes                                                           */
/* ================================================================== */
#define KEY_UP 1000
#define KEY_DOWN 1001
#define KEY_LEFT 1002
#define KEY_RIGHT 1003
#define KEY_ENTER 1004
#define KEY_SPACE 1005
#define KEY_ESC 1006

/*
 * readKey -- non-blocking ESC peek to distinguish bare ESC from
 *            ANSI escape sequences (arrow keys).
 */
static int readKey(void) {
  unsigned char c;
  if (read(STDIN_FILENO, &c, 1) != 1)
    return KEY_ESC;

  if (c == 27) {
    /* Try a non-blocking peek for the next byte */
    int fl = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, fl | O_NONBLOCK);
    unsigned char s0;
    ssize_t n = read(STDIN_FILENO, &s0, 1);
    fcntl(STDIN_FILENO, F_SETFL, fl); /* restore */

    if (n != 1)
      return KEY_ESC; /* bare ESC */

    unsigned char s1;
    if (read(STDIN_FILENO, &s1, 1) != 1)
      return KEY_ESC;

    if (s0 == '[') {
      switch (s1) {
      case 'A':
        return KEY_UP;
      case 'B':
        return KEY_DOWN;
      case 'C':
        return KEY_RIGHT;
      case 'D':
        return KEY_LEFT;
      }
    }
    return KEY_ESC;
  }

  if (c == '\r' || c == '\n')
    return KEY_ENTER;
  if (c == ' ')
    return KEY_SPACE;
  return (int)c;
}

/* ================================================================== */
/*  ANSI helpers                                                        */
/* ================================================================== */
static void clrscr(void) {
  printf("\033[2J\033[H");
  fflush(stdout);
}
static void gohome(void) { printf("\033[H"); }

/* ================================================================== */
/*  Canvas pixel helpers                                                */
/* ================================================================== */
static int inBounds(int r, int c) {
  return r >= 0 && r < ROWS && c >= 0 && c < COLS;
}

static void setPixel(int r, int c) {
  if (inBounds(r, c))
    canvas[r][c] = '*';
}

/* ================================================================== */
/*  clearCanvas / displayCanvas                                         */
/* ================================================================== */
void clearCanvas(void) {
  for (int i = 0; i < ROWS; i++)
    for (int j = 0; j < COLS; j++)
      canvas[i][j] = '_';
}

void displayCanvas(void) {
  printf("+");
  for (int j = 0; j < COLS; j++)
    putchar('-');
  printf("+\n");

  for (int i = 0; i < ROWS; i++) {
    putchar('|');
    for (int j = 0; j < COLS; j++)
      putchar(canvas[i][j]);
    printf("|\n");
  }

  printf("+");
  for (int j = 0; j < COLS; j++)
    putchar('-');
  printf("+\n");
}

/* ================================================================== */
/*  Drawing primitives                                                  */
/* ================================================================== */

/* Bresenham line between two points */
static void drawSegment(int r1, int c1, int r2, int c2) {
  int dr = abs(r2 - r1), dc = abs(c2 - c1);
  int sr = r1 < r2 ? 1 : -1, sc = c1 < c2 ? 1 : -1;
  int err = (dr > dc ? dr : -dc) / 2, e2;
  for (;;) {
    setPixel(r1, c1);
    if (r1 == r2 && c1 == c2)
      break;
    e2 = err;
    if (e2 > -dr) {
      err -= dc;
      r1 += sr;
    }
    if (e2 < dc) {
      err += dr;
      c1 += sc;
    }
  }
}

void drawLine(Point a, Point b) { drawSegment(a.row, a.col, b.row, b.col); }

/*
 * Rectangle is stored as 4 corners: p[0]=TL p[1]=TR p[2]=BR p[3]=BL
 * Drawn as 4 straight line segments.
 */
void drawRectangleCorners(Point tl, Point br) {
  /* Normalize */
  int r1 = tl.row, c1 = tl.col, r2 = br.row, c2 = br.col;
  if (r1 > r2) {
    int t = r1;
    r1 = r2;
    r2 = t;
  }
  if (c1 > c2) {
    int t = c1;
    c1 = c2;
    c2 = t;
  }
  for (int c = c1; c <= c2; c++) {
    setPixel(r1, c);
    setPixel(r2, c);
  }
  for (int r = r1; r <= r2; r++) {
    setPixel(r, c1);
    setPixel(r, c2);
  }
}

/*
 * drawCircleCP -- draws a circle at centre with given integer radius.
 * Radius is stored in p[1].row of the GraphicObject.
 */
void drawCircleCP(Point centre, int radius) {
  if (radius < 1)
    return;
  for (int dr = -radius; dr <= radius; dr++)
    for (int dc = -radius; dc <= radius; dc++) {
      double d = sqrt((double)dr * dr + (double)dc * dc);
      if (fabs(d - radius) <= 0.55)
        setPixel(centre.row + dr, centre.col + dc);
    }
}

void drawTrianglePts(Point p1, Point p2, Point p3) {
  drawSegment(p1.row, p1.col, p2.row, p2.col);
  drawSegment(p2.row, p2.col, p3.row, p3.col);
  drawSegment(p3.row, p3.col, p1.row, p1.col);
}

static int isObjectIdUsed(int id) {
  for (int i = 0; i < objectCount; i++) {
    if (objects[i].id == id)
      return 1;
  }
  return 0;
}

static int generateObjectId(void) {
  while (isObjectIdUsed(nextId))
    nextId++;
  int id = nextId++;
  return id;
}

static int placeObjectLabel(int row, int col, const char *label) {
  int offsets[8][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0},
                       {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};

  for (int i = 0; i < 8; i++) {
    int r = row + offsets[i][0];
    int c = col + offsets[i][1];
    int fits = 1;

    for (int j = 0; label[j] != '\0'; j++) {
      int rr = r;
      int cc = c + j;
      if (!inBounds(rr, cc) || canvas[rr][cc] == '*') {
        fits = 0;
        break;
      }
    }

    if (fits) {
      for (int j = 0; label[j] != '\0'; j++) {
        int rr = r;
        int cc = c + j;
        canvas[rr][cc] = label[j];
      }
      return 1;
    }
  }

  return 0;
}

static void drawObjectIdLabel(const GraphicObject *o) {
  if (o->id <= 0)
    return;

  char label[16];
  snprintf(label, sizeof(label), "%d", o->id);
  if (placeObjectLabel(o->p[0].row, o->p[0].col, label))
    return;

  int fallbackRow = o->p[0].row;
  int fallbackCol = o->p[0].col;
  if (fallbackCol + (int)strlen(label) >= COLS)
    fallbackCol = COLS - (int)strlen(label) - 1;
  if (fallbackCol < 0)
    fallbackCol = 0;
  if (fallbackRow < 0)
    fallbackRow = 0;
  for (int i = 0; label[i] != '\0'; i++) {
    int rr = fallbackRow;
    int cc = fallbackCol + i;
    if (inBounds(rr, cc))
      canvas[rr][cc] = label[i];
  }
}

/* ================================================================== */
/*  drawObject / redrawCanvas / addObject / deleteObject / listObjects  */
/* ================================================================== */
void drawObject(const GraphicObject *o) {
  switch (o->type) {
  case SHAPE_RECTANGLE:
    /* p[0..3] are the 4 corners stored during interactive pick */
    drawSegment(o->p[0].row, o->p[0].col, o->p[1].row, o->p[1].col);
    drawSegment(o->p[1].row, o->p[1].col, o->p[2].row, o->p[2].col);
    drawSegment(o->p[2].row, o->p[2].col, o->p[3].row, o->p[3].col);
    drawSegment(o->p[3].row, o->p[3].col, o->p[0].row, o->p[0].col);
    break;
  case SHAPE_CIRCLE:
    /* p[0] = centre, p[1].row = radius */
    drawCircleCP(o->p[0], o->p[1].row);
    break;
  case SHAPE_LINE:
    drawLine(o->p[0], o->p[1]);
    break;
  case SHAPE_TRIANGLE:
    drawTrianglePts(o->p[0], o->p[1], o->p[2]);
    break;
  }
}

void redrawCanvas(void) {
  clearCanvas();
  for (int i = 0; i < objectCount; i++) {
    drawObject(&objects[i]);
    drawObjectIdLabel(&objects[i]);
  }
}

int addObject(GraphicObject o) {
  if (objectCount >= MAX_OBJECTS)
    return -1;

  if (o.id <= 0 || isObjectIdUsed(o.id)) {
    o.id = generateObjectId();
  } else if (nextId <= o.id + 1) {
    nextId = o.id + 1;
  }

  objects[objectCount++] = o;
  redrawCanvas();
  return o.id;
}

int deleteObject(int id) {
  for (int i = 0; i < objectCount; i++)
    if (objects[i].id == id) {
      for (int j = i; j + 1 < objectCount; j++)
        objects[j] = objects[j + 1];
      objectCount--;
      redrawCanvas();
      return 1;
    }
  return 0;
}

void listObjects(void) {
  if (objectCount == 0) {
    printf("  No objects on canvas.\n");
    return;
  }
  printf("  %-4s  %-10s  Position        Parameters\n", "ID", "Type");
  printf("  -----------------------------------------------------------\n");
  for (int i = 0; i < objectCount; i++) {
    GraphicObject *o = &objects[i];
    switch (o->type) {
    case SHAPE_RECTANGLE:
      printf("  %-4d  %-10s  (%d,%d)          size=(%d,%d)\n", o->id, "Rectangle",
             o->p[0].row, o->p[0].col, abs(o->p[2].row - o->p[0].row) + 1,
             abs(o->p[2].col - o->p[0].col) + 1);
      break;
    case SHAPE_CIRCLE:
      printf("  %-4d  %-10s  centre=(%d,%d)  radius=%d\n", o->id, "Circle",
             o->p[0].row, o->p[0].col, o->p[1].row);
      break;
    case SHAPE_LINE:
      printf("  %-4d  %-10s  start=(%d,%d)   end=(%d,%d)\n", o->id, "Line",
             o->p[0].row, o->p[0].col, o->p[1].row, o->p[1].col);
      break;
    case SHAPE_TRIANGLE:
      printf("  %-4d  %-10s  P1=(%d,%d)      P2=(%d,%d) P3=(%d,%d)\n", o->id,
             "Triangle", o->p[0].row, o->p[0].col, o->p[1].row, o->p[1].col,
             o->p[2].row, o->p[2].col);
      break;
    }
  }
}

/* ================================================================== */
/*  Canvas Snapshot                                                     */
/* ================================================================== */
static void snapLine(FILE *f, const char *s) {
  fputs(s, stdout);
  fputs(s, f);
}

static const char *shapeName(ShapeType t) {
  switch (t) {
  case SHAPE_RECTANGLE:
    return "Rectangle";
  case SHAPE_CIRCLE:
    return "Circle";
  case SHAPE_LINE:
    return "Line";
  case SHAPE_TRIANGLE:
    return "Triangle";
  }
  return "Unknown";
}

void canvasSnapshot(void) {
  FILE *fp = fopen("snapshot.txt", "w");
  if (!fp) {
    fprintf(stderr, "Warning: cannot open snapshot.txt (%s)\n",
            strerror(errno));
    fp = fopen("/dev/null", "w");
    if (!fp)
      return;
  }

  char ln[256];
  const char *title = "2D Graphics Editor -- Canvas Snapshot";
  int totalW = 5 + COLS + 1; /* "RR | " + canvas + "|" */
  int pad = (totalW - (int)strlen(title)) / 2;
  if (pad < 0)
    pad = 0;

  /* === header === */
  for (int i = 0; i < totalW; i++) {
    fputc('=', stdout);
    fputc('=', fp);
  }
  fputc('\n', stdout);
  fputc('\n', fp);

  snprintf(ln, sizeof(ln), "%*s%s\n", pad, "", title);
  snapLine(fp, ln);

  time_t now = time(NULL);
  char ts[64];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
  int tpad = (totalW - (int)strlen(ts)) / 2;
  if (tpad < 0)
    tpad = 0;
  snprintf(ln, sizeof(ln), "%*s%s\n", tpad, "", ts);
  snapLine(fp, ln);

  for (int i = 0; i < totalW; i++) {
    fputc('=', stdout);
    fputc('=', fp);
  }
  fputc('\n', stdout);
  fputc('\n', fp);

  /* === column ruler (tens) === */
  snapLine(fp, "     ");
  for (int j = 0; j < COLS; j++) {
    char ch = (j % 10 == 0) ? (char)('0' + (j / 10) % 10) : ' ';
    fputc(ch, stdout);
    fputc(ch, fp);
  }
  fputc('\n', stdout);
  fputc('\n', fp);

  /* === column ruler (units) === */
  snapLine(fp, "     ");
  for (int j = 0; j < COLS; j++) {
    char ch = (char)('0' + j % 10);
    fputc(ch, stdout);
    fputc(ch, fp);
  }
  fputc('\n', stdout);
  fputc('\n', fp);

  /* === top border === */
  snapLine(fp, "    +");
  for (int j = 0; j < COLS; j++) {
    fputc('-', stdout);
    fputc('-', fp);
  }
  snapLine(fp, "+\n");

  /* === canvas rows === */
  for (int i = 0; i < ROWS; i++) {
    snprintf(ln, sizeof(ln), "%2d |", i);
    snapLine(fp, ln);
    for (int j = 0; j < COLS; j++) {
      fputc(canvas[i][j], stdout);
      fputc(canvas[i][j], fp);
    }
    snapLine(fp, "|\n");
  }

  /* === bottom border === */
  snapLine(fp, "    +");
  for (int j = 0; j < COLS; j++) {
    fputc('-', stdout);
    fputc('-', fp);
  }
  snapLine(fp, "+\n");

  /* === object list === */
  snprintf(ln, sizeof(ln), "\n  Objects on canvas: %d\n", objectCount);
  snapLine(fp, ln);

  if (objectCount == 0) {
    snapLine(fp, "  (none)\n");
  } else {
    snapLine(fp, "  ---- ID ---- Type -------- Details ----------------\n");
    for (int i = 0; i < objectCount; i++) {
      GraphicObject *o = &objects[i];
      switch (o->type) {
      case SHAPE_RECTANGLE:
        snprintf(ln, sizeof(ln),
                 "  %4d  %-10s  P1=(%d,%d) P2=(%d,%d) P3=(%d,%d) P4=(%d,%d)\n",
                 o->id, shapeName(o->type), o->p[0].row, o->p[0].col,
                 o->p[1].row, o->p[1].col, o->p[2].row, o->p[2].col,
                 o->p[3].row, o->p[3].col);
        break;
      case SHAPE_CIRCLE:
        snprintf(ln, sizeof(ln), "  %4d  %-10s  centre=(%d,%d)  radius=%d\n",
                 o->id, shapeName(o->type), o->p[0].row, o->p[0].col,
                 o->p[1].row);
        break;
      case SHAPE_LINE:
        snprintf(ln, sizeof(ln), "  %4d  %-10s  start=(%d,%d)  end=(%d,%d)\n",
                 o->id, shapeName(o->type), o->p[0].row, o->p[0].col,
                 o->p[1].row, o->p[1].col);
        break;
      case SHAPE_TRIANGLE:
        snprintf(ln, sizeof(ln),
                 "  %4d  %-10s  P1=(%d,%d)  P2=(%d,%d)  P3=(%d,%d)\n", o->id,
                 shapeName(o->type), o->p[0].row, o->p[0].col, o->p[1].row,
                 o->p[1].col, o->p[2].row, o->p[2].col);
        break;
      }
      snapLine(fp, ln);
    }
  }

  for (int i = 0; i < totalW; i++) {
    fputc('=', stdout);
    fputc('=', fp);
  }
  fputc('\n', stdout);
  fputc('\n', fp);

  fclose(fp);
  printf("\n  Snapshot saved to snapshot.txt\n");
}

/* ================================================================== */
/*  Interactive renderer  (raw mode, live cursor overlay)               */
/* ================================================================== */

/*
 * Renders the canvas with optional pixel-by-pixel overlay from a preview
 * buffer, confirmed points shown as 'O', live cursor as '+'.
 *
 * previewBuf: if non-NULL, replaces pixel values for preview drawing
 */
static void renderWithBuf(const char previewBuf[ROWS][COLS],
                          const Point *confirmed, int nConfirmed, int curRow,
                          int curCol, const char *status) {
  gohome();
  printf("+");
  for (int j = 0; j < COLS; j++)
    putchar('-');
  printf("+\r\n");

  for (int i = 0; i < ROWS; i++) {
    putchar('|');
    for (int j = 0; j < COLS; j++) {
      char ch = previewBuf ? previewBuf[i][j] : canvas[i][j];
      /* confirmed points -> 'O', cursor -> '+' */
      int isConfirmed = 0;
      for (int k = 0; k < nConfirmed; k++)
        if (confirmed[k].row == i && confirmed[k].col == j)
          isConfirmed = 1;
      if (isConfirmed)
        putchar('O');
      else if (i == curRow && j == curCol)
        putchar('+');
      else
        putchar(ch);
    }
    printf("|\r\n");
  }

  printf("+");
  for (int j = 0; j < COLS; j++)
    putchar('-');
  printf("+\r\n");
  printf("\033[K  %s\r\n", status);
  printf("\033[K  Cursor: row=%-3d col=%-3d\r\n", curRow, curCol);
  fflush(stdout);
}

/* Simple render without preview buf */
static void render(const Point *confirmed, int nConfirmed, int curRow,
                   int curCol, const char *status) {
  renderWithBuf(NULL, confirmed, nConfirmed, curRow, curCol, status);
}

/* ================================================================== */
/*  RECTANGLE  (3 user clicks + 1 auto corner)                         */
/*                                                                      */
/*  P1 : free (first corner)                                            */
/*  P2 : LOCKED to same row as P1 -- only LEFT/RIGHT keys work          */
/*  P3 : LOCKED to same col as P2 -- only UP/DOWN keys work             */
/*  P4 : auto = (P3.row, P1.col)                                        */
/*                                                                      */
/*  Live preview shows edges building up as each point is confirmed.    */
/* ================================================================== */
int interactiveRectangle(void) {
  int curRow = ROWS / 2, curCol = COLS / 2;
  clrscr();

  /* ---- Phase 0: P1 free ---- */
  Point P1 = {0, 0};
  for (;;) {
    render(NULL, 0, curRow, curCol,
           "[Rect P1/3] Pick first corner  SPACE=confirm  ESC=cancel");
    int k = readKey();
    if (k == KEY_ESC)
      return -1;
    if (k == KEY_UP && curRow > 0)
      curRow--;
    if (k == KEY_DOWN && curRow < ROWS - 1)
      curRow++;
    if (k == KEY_LEFT && curCol > 0)
      curCol--;
    if (k == KEY_RIGHT && curCol < COLS - 1)
      curCol++;
    if (k == KEY_SPACE || k == KEY_ENTER) {
      P1.row = curRow;
      P1.col = curCol;
      break;
    }
  }

  /* ---- Phase 1: P2 -- same row as P1, only LEFT/RIGHT ---- */
  Point P2 = P1;
  curRow = P1.row; /* lock row */
  for (;;) {
    /* build preview buf: canvas + horizontal line P1->cursor */
    char buf[ROWS][COLS];
    memcpy(buf, canvas, sizeof(buf));
    int c1 = P1.col, c2 = curCol;
    if (c1 > c2) {
      int t = c1;
      c1 = c2;
      c2 = t;
    }
    for (int j = c1; j <= c2; j++)
      buf[P1.row][j] = '*';

    Point conf[1];
    conf[0] = P1;
    renderWithBuf(
        buf, conf, 1, curRow, curCol,
        "[Rect P2/3] TOP EDGE: move LEFT/RIGHT  SPACE=confirm  ESC=cancel");
    int k = readKey();
    if (k == KEY_ESC)
      return -1;
    /* Only horizontal movement allowed */
    if (k == KEY_LEFT && curCol > 0)
      curCol--;
    if (k == KEY_RIGHT && curCol < COLS - 1)
      curCol++;
    curRow = P1.row; /* always lock row */
    if (k == KEY_SPACE || k == KEY_ENTER) {
      P2.row = curRow;
      P2.col = curCol;
      break;
    }
  }

  /* ---- Phase 2: P3 -- same col as P2, only UP/DOWN ---- */
  Point P3 = P2;
  curCol = P2.col; /* lock col */
  curRow = P2.row;
  for (;;) {
    /* preview: top edge + right edge + closing lines */
    char buf[ROWS][COLS];
    memcpy(buf, canvas, sizeof(buf));

    int r1 = P1.row, r2 = curRow;
    /* Top edge P1->P2 */
    {
      int ca = P1.col, cb = P2.col;
      if (ca > cb) {
        int t = ca;
        ca = cb;
        cb = t;
      }
      for (int j = ca; j <= cb; j++)
        buf[P1.row][j] = '*';
    }
    /* Right edge P2->P3(current) */
    {
      int ra = P1.row, rb = curRow;
      if (ra > rb) {
        int t = ra;
        ra = rb;
        rb = t;
      }
      for (int i = ra; i <= rb; i++)
        buf[i][P2.col] = '*';
    }
    /* Bottom edge (preview) */
    {
      int ra = curRow;
      int ca = P1.col, cb = P2.col;
      if (ca > cb) {
        int t = ca;
        ca = cb;
        cb = t;
      }
      for (int j = ca; j <= cb; j++)
        buf[ra][j] = '*';
    }
    /* Left edge (preview) */
    {
      int ra = P1.row, rb = curRow;
      if (ra > rb) {
        int t = ra;
        ra = rb;
        rb = t;
      }
      for (int i = ra; i <= rb; i++)
        buf[i][P1.col] = '*';
    }
    (void)r1;
    (void)r2;

    Point conf[2];
    conf[0] = P1;
    conf[1] = P2;
    renderWithBuf(
        buf, conf, 2, curRow, curCol,
        "[Rect P3/3] HEIGHT: move UP/DOWN  SPACE=confirm  ESC=cancel");
    int k = readKey();
    if (k == KEY_ESC)
      return -1;
    /* Only vertical movement allowed */
    if (k == KEY_UP && curRow > 0)
      curRow--;
    if (k == KEY_DOWN && curRow < ROWS - 1)
      curRow++;
    curCol = P2.col; /* always lock col */
    if (k == KEY_SPACE || k == KEY_ENTER) {
      P3.row = curRow;
      P3.col = curCol;
      break;
    }
  }

  /* P4 auto: (P3.row, P1.col) */
  Point P4 = {P3.row, P1.col};

  GraphicObject obj;
  memset(&obj, 0, sizeof(obj));
  obj.type = SHAPE_RECTANGLE;
  obj.p[0] = P1;
  obj.p[1] = P2;
  obj.p[2] = P3;
  obj.p[3] = P4;
  return addObject(obj);
}

/* ================================================================== */
/*  CIRCLE  (cursor -> centre, then TYPE radius)                        */
/* ================================================================== */
int interactiveCircle(void) {
  int curRow = ROWS / 2, curCol = COLS / 2;
  clrscr();

  /* Phase 0: pick centre with cursor */
  Point centre = {0, 0};
  for (;;) {
    render(NULL, 0, curRow, curCol,
           "[Circle] Pick CENTRE  SPACE=confirm  ESC=cancel");
    int k = readKey();
    if (k == KEY_ESC)
      return -1;
    if (k == KEY_UP && curRow > 0)
      curRow--;
    if (k == KEY_DOWN && curRow < ROWS - 1)
      curRow++;
    if (k == KEY_LEFT && curCol > 0)
      curCol--;
    if (k == KEY_RIGHT && curCol < COLS - 1)
      curCol++;
    if (k == KEY_SPACE || k == KEY_ENTER) {
      centre.row = curRow;
      centre.col = curCol;
      break;
    }
  }

  /* Phase 1: type the radius (back to cooked mode) */
  disableRawMode();
  clrscr();
  printf("  Centre confirmed at row=%d, col=%d\n\n", centre.row, centre.col);
  printf("  Enter radius (1 - %d): ", COLS / 3);
  fflush(stdout);

  int radius = 0;
  while (scanf("%d", &radius) != 1 || radius < 1) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
    printf("  Invalid. Enter radius (1 - %d): ", COLS / 3);
    fflush(stdout);
  }
  int ch;
  while ((ch = getchar()) != '\n' && ch != EOF) {
  }

  GraphicObject obj;
  memset(&obj, 0, sizeof(obj));
  obj.type = SHAPE_CIRCLE;
  obj.p[0] = centre;
  obj.p[1].row = radius; /* store radius directly */
  obj.p[1].col = 0;
  return addObject(obj);
}

/* ================================================================== */
/*  LINE  (2 free cursor points)                                        */
/* ================================================================== */
int interactiveLine(void) {
  int curRow = ROWS / 2, curCol = COLS / 2;
  clrscr();

  /* Point 1 */
  Point P1 = {0, 0};
  for (;;) {
    render(NULL, 0, curRow, curCol,
           "[Line P1/2] Pick START  SPACE=confirm  ESC=cancel");
    int k = readKey();
    if (k == KEY_ESC)
      return -1;
    if (k == KEY_UP && curRow > 0)
      curRow--;
    if (k == KEY_DOWN && curRow < ROWS - 1)
      curRow++;
    if (k == KEY_LEFT && curCol > 0)
      curCol--;
    if (k == KEY_RIGHT && curCol < COLS - 1)
      curCol++;
    if (k == KEY_SPACE || k == KEY_ENTER) {
      P1.row = curRow;
      P1.col = curCol;
      break;
    }
  }

  /* Point 2 with live line preview */
  Point P2 = {0, 0};
  Point conf1[1];
  conf1[0] = P1;
  for (;;) {
    /* preview: draw line from P1 to cursor */
    char buf[ROWS][COLS];
    memcpy(buf, canvas, sizeof(buf));
    int r1 = P1.row, c1 = P1.col, r2 = curRow, c2 = curCol;
    /* Bresenham into buf */
    {
      int dr = abs(r2 - r1), dc = abs(c2 - c1);
      int sr = r1 < r2 ? 1 : -1, sc = c1 < c2 ? 1 : -1;
      int err = (dr > dc ? dr : -dc) / 2, e2;
      int rr = r1, cc = c1;
      for (;;) {
        if (inBounds(rr, cc))
          buf[rr][cc] = '*';
        if (rr == r2 && cc == c2)
          break;
        e2 = err;
        if (e2 > -dr) {
          err -= dc;
          rr += sr;
        }
        if (e2 < dc) {
          err += dr;
          cc += sc;
        }
      }
    }
    renderWithBuf(buf, conf1, 1, curRow, curCol,
                  "[Line P2/2] Pick END  SPACE=confirm  ESC=cancel");
    int k = readKey();
    if (k == KEY_ESC)
      return -1;
    if (k == KEY_UP && curRow > 0)
      curRow--;
    if (k == KEY_DOWN && curRow < ROWS - 1)
      curRow++;
    if (k == KEY_LEFT && curCol > 0)
      curCol--;
    if (k == KEY_RIGHT && curCol < COLS - 1)
      curCol++;
    if (k == KEY_SPACE || k == KEY_ENTER) {
      P2.row = curRow;
      P2.col = curCol;
      break;
    }
  }

  GraphicObject obj;
  memset(&obj, 0, sizeof(obj));
  obj.type = SHAPE_LINE;
  obj.p[0] = P1;
  obj.p[1] = P2;
  return addObject(obj);
}

/* ================================================================== */
/*  TRIANGLE  (3 free cursor points, with live edge preview)            */
/* ================================================================== */
int interactiveTriangle(void) {
  int curRow = ROWS / 2, curCol = COLS / 2;
  clrscr();

  /* Helper: draw a line segment into buf */
#define DRAW_SEG_BUF(BUF, r1, c1, r2, c2)                                      \
  do {                                                                         \
    int _dr = abs((r2) - (r1)), _dc = abs((c2) - (c1));                        \
    int _sr = (r1) < (r2) ? 1 : -1, _sc = (c1) < (c2) ? 1 : -1;                \
    int _err = (_dr > _dc ? _dr : -_dc) / 2, _e2;                              \
    int _rr = (r1), _cc = (c1);                                                \
    for (;;) {                                                                 \
      if (inBounds(_rr, _cc))                                                  \
        (BUF)[_rr][_cc] = '*';                                                 \
      if (_rr == (r2) && _cc == (c2))                                          \
        break;                                                                 \
      _e2 = _err;                                                              \
      if (_e2 > -_dr) {                                                        \
        _err -= _dc;                                                           \
        _rr += _sr;                                                            \
      }                                                                        \
      if (_e2 < _dc) {                                                         \
        _err += _dr;                                                           \
        _cc += _sc;                                                            \
      }                                                                        \
    }                                                                          \
  } while (0)

  /* P1 */
  Point P1 = {0, 0};
  for (;;) {
    render(NULL, 0, curRow, curCol,
           "[Tri P1/3] Pick POINT 1  SPACE=confirm  ESC=cancel");
    int k = readKey();
    if (k == KEY_ESC)
      return -1;
    if (k == KEY_UP && curRow > 0)
      curRow--;
    if (k == KEY_DOWN && curRow < ROWS - 1)
      curRow++;
    if (k == KEY_LEFT && curCol > 0)
      curCol--;
    if (k == KEY_RIGHT && curCol < COLS - 1)
      curCol++;
    if (k == KEY_SPACE || k == KEY_ENTER) {
      P1.row = curRow;
      P1.col = curCol;
      break;
    }
  }

  /* P2 with live edge P1->cursor */
  Point P2 = {0, 0};
  Point conf1[1];
  conf1[0] = P1;
  for (;;) {
    char buf[ROWS][COLS];
    memcpy(buf, canvas, sizeof(buf));
    DRAW_SEG_BUF(buf, P1.row, P1.col, curRow, curCol);
    renderWithBuf(buf, conf1, 1, curRow, curCol,
                  "[Tri P2/3] Pick POINT 2  SPACE=confirm  ESC=cancel");
    int k = readKey();
    if (k == KEY_ESC)
      return -1;
    if (k == KEY_UP && curRow > 0)
      curRow--;
    if (k == KEY_DOWN && curRow < ROWS - 1)
      curRow++;
    if (k == KEY_LEFT && curCol > 0)
      curCol--;
    if (k == KEY_RIGHT && curCol < COLS - 1)
      curCol++;
    if (k == KEY_SPACE || k == KEY_ENTER) {
      P2.row = curRow;
      P2.col = curCol;
      break;
    }
  }

  /* P3 with live full triangle preview */
  Point P3 = {0, 0};
  /* start cursor near midpoint offset */
  curRow = (P1.row + P2.row) / 2 + 4;
  curCol = (P1.col + P2.col) / 2;
  if (curRow >= ROWS)
    curRow = ROWS - 1;

  Point conf2[2];
  conf2[0] = P1;
  conf2[1] = P2;
  for (;;) {
    char buf[ROWS][COLS];
    memcpy(buf, canvas, sizeof(buf));
    DRAW_SEG_BUF(buf, P1.row, P1.col, P2.row, P2.col);
    DRAW_SEG_BUF(buf, P2.row, P2.col, curRow, curCol);
    DRAW_SEG_BUF(buf, curRow, curCol, P1.row, P1.col);
    renderWithBuf(buf, conf2, 2, curRow, curCol,
                  "[Tri P3/3] Pick APEX  SPACE=confirm  ESC=cancel");
    int k = readKey();
    if (k == KEY_ESC)
      return -1;
    if (k == KEY_UP && curRow > 0)
      curRow--;
    if (k == KEY_DOWN && curRow < ROWS - 1)
      curRow++;
    if (k == KEY_LEFT && curCol > 0)
      curCol--;
    if (k == KEY_RIGHT && curCol < COLS - 1)
      curCol++;
    if (k == KEY_SPACE || k == KEY_ENTER) {
      P3.row = curRow;
      P3.col = curCol;
      break;
    }
  }

#undef DRAW_SEG_BUF

  GraphicObject obj;
  memset(&obj, 0, sizeof(obj));
  obj.type = SHAPE_TRIANGLE;
  obj.p[0] = P1;
  obj.p[1] = P2;
  obj.p[2] = P3;
  return addObject(obj);
}

/* ================================================================== */
/*  Menu  (options only -- canvas shown AFTER pick during drawing)      */
/* ================================================================== */
static void printMenu(void) {
  clrscr();
  printf("  +==================================+\n");
  printf("  |      2D Graphics Editor          |\n");
  printf("  +==================================+\n");
  printf("  |  Objects on canvas : %-3d         |\n", objectCount);
  printf("  +----------------------------------+\n");
  printf("  |   1.  Add Rectangle              |\n");
  printf("  |   2.  Add Circle                 |\n");
  printf("  |   3.  Add Line                   |\n");
  printf("  |   4.  Add Triangle               |\n");
  printf("  |   5.  Delete object              |\n");
  printf("  |   6.  List objects               |\n");
  printf("  |   7.  Display canvas             |\n");
  printf("  |   8.  Clear canvas               |\n");
  printf("  |   9.  Quit                       |\n");
  printf("  |  10.  Canvas Snapshot            |\n");
  printf("  +==================================+\n");
  printf("\n  Choose (1-10): ");
  fflush(stdout);
}

static int readMenuChoice(void) {
  disableRawMode();
  int ch = 0;
  if (scanf("%d", &ch) != 1)
    ch = 0;
  int c;
  while ((c = getchar()) != '\n' && c != EOF) {
  }
  return ch;
}

/* ================================================================== */
/*  main                                                                */
/* ================================================================== */
int main(void) {
  clearCanvas();

  for (;;) {
    printMenu();
    int choice = readMenuChoice();

    switch (choice) {

    /* ---- Rectangle ---- */
    case 1: {
      enableRawMode();
      int id = interactiveRectangle();
      disableRawMode();
      clrscr();
      if (id > 0)
        printf("  Rectangle added successfully. ID = %d\n", id);
      else
        printf("  Cancelled.\n");
      printf("  Press Enter to continue...\n");
      getchar();
      break;
    }

    /* ---- Circle ---- */
    case 2: {
      enableRawMode();
      int id = interactiveCircle();
      /* interactiveCircle already disables raw mode after centre pick */
      clrscr();
      if (id > 0)
        printf("  Circle added successfully. ID = %d\n", id);
      else
        printf("  Cancelled.\n");
      printf("  Press Enter to continue...\n");
      getchar();
      break;
    }

    /* ---- Line ---- */
    case 3: {
      enableRawMode();
      int id = interactiveLine();
      disableRawMode();
      clrscr();
      if (id > 0)
        printf("  Line added successfully. ID = %d\n", id);
      else
        printf("  Cancelled.\n");
      printf("  Press Enter to continue...\n");
      getchar();
      break;
    }

    /* ---- Triangle ---- */
    case 4: {
      enableRawMode();
      int id = interactiveTriangle();
      disableRawMode();
      clrscr();
      if (id > 0)
        printf("  Triangle added successfully. ID = %d\n", id);
      else
        printf("  Cancelled.\n");
      printf("  Press Enter to continue...\n");
      getchar();
      break;
    }

    /* ---- Delete ---- */
    case 5: {
      if (objectCount == 0) {
        printf("  No objects available to delete.\n");
      } else {
        printf("  Existing object IDs:");
        for (int i = 0; i < objectCount; i++)
          printf(" %d", objects[i].id);
        printf("\n");
        printf("  Enter object ID to delete: ");
        fflush(stdout);
        int id = 0;
        if (scanf("%d", &id) == 1) {
          int c;
          while ((c = getchar()) != '\n' && c != EOF) {
          }
          if (deleteObject(id))
            printf("  Deleted object ID %d.\n", id);
          else
            printf("  No object found with ID %d.\n", id);
        } else {
          int c;
          while ((c = getchar()) != '\n' && c != EOF) {
          }
          printf("  Invalid input.\n");
        }
      }
      printf("  Press Enter to continue...\n");
      getchar();
      break;
    }

    /* ---- List ---- */
    case 6:
      printf("\n");
      listObjects();
      printf("\n  Press Enter to continue...\n");
      getchar();
      break;

    /* ---- Display canvas ---- */
    case 7:
      printf("\n");
      displayCanvas();
      printf("\n  Press Enter to continue...\n");
      getchar();
      break;

    /* ---- Clear ---- */
    case 8:
      objectCount = 0;
      nextId = 1;
      clearCanvas();
      printf("  Canvas cleared.\n");
      printf("  Press Enter to continue...\n");
      getchar();
      break;

    /* ---- Quit ---- */
    case 9:
      printf("  Goodbye!\n");
      return 0;

    /* ---- Canvas Snapshot ---- */
    case 10:
      printf("\n");
      canvasSnapshot();
      printf("\n  Press Enter to continue...\n");
      getchar();
      break;

    default:
      printf("  Invalid choice (1-10).\n");
      printf("  Press Enter to continue...\n");
      getchar();
      break;
    }
  }
}
