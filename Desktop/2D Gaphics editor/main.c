/*
 * main.c
 * 2D Graphics Editor – Interactive ncurses Interface
 *
 * Layout:
 *   ┌─────────────────────────────────┬───────────────┐
 *   │          CANVAS (80x40)         │  SHAPE LIST   │
 *   │                                 │               │
 *   │                                 │  [id] type    │
 *   │                                 │  ...          │
 *   ├─────────────────────────────────┴───────────────┤
 *   │  STATUS BAR: keys …                             │
 *   └─────────────────────────────────────────────────┘
 *
 * Controls:
 *   a  → Add shape   (menu to pick type, then enter params)
 *   d  → Delete shape by ID
 *   m  → Modify shape by ID
 *   l  → List all shapes (summary overlay)
 *   c  → Clear all shapes
 *   e  → Export canvas to  canvas_export.txt
 *   h  → Help overlay
 *   q  → Quit
 */

#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "graphics.h"

/* ─── Layout constants ────────────────────────────────────────────── */
#define SIDEBAR_W   30
#define STATUS_H     3
#define TITLE_H      2

/* ncurses color pair indices */
#define CP_TITLE      1
#define CP_CANVAS     2
#define CP_BORDER     3
#define CP_SIDEBAR    4
#define CP_STATUS     5
#define CP_HIGHLIGHT  6
#define CP_ERROR      7
#define CP_SHAPE_STAR 8
#define CP_SHAPE_DASH 9
#define CP_MENU_SEL   10
#define CP_HELP       11
#define CP_SUCCESS    12

/* ─── Global canvas ───────────────────────────────────────────────── */
static Canvas canvas;

/* ─── Windows ─────────────────────────────────────────────────────── */
static WINDOW *win_title   = NULL;
static WINDOW *win_canvas  = NULL;
static WINDOW *win_sidebar = NULL;
static WINDOW *win_status  = NULL;

static const char *main_menu_items[] = {
    "1) Add Rectangle",
    "2) Add Circle",
    "3) Add Line",
    "4) Add Triangle",
    "5) Delete Object",
    "6) Modify Object",
    "7) List Objects",
    "8) Display Canvas",
    "9) Clear Canvas",
    "0) Quit",
};
#define N_MAIN_MENU 10

/* ═══════════════════════════════════════════════════════════════════
 *  Colour / init helpers
 * ═══════════════════════════════════════════════════════════════════ */

static void init_colors(void) {
    start_color();
    use_default_colors();

    init_pair(CP_TITLE,      COLOR_BLACK,   COLOR_CYAN);
    init_pair(CP_CANVAS,     COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_BORDER,     COLOR_CYAN,    COLOR_BLACK);
    init_pair(CP_SIDEBAR,    COLOR_CYAN,    COLOR_BLACK);
    init_pair(CP_STATUS,     COLOR_BLACK,   COLOR_BLUE);
    init_pair(CP_HIGHLIGHT,  COLOR_YELLOW,  COLOR_BLACK);
    init_pair(CP_ERROR,      COLOR_WHITE,   COLOR_RED);
    init_pair(CP_SHAPE_STAR, COLOR_GREEN,   COLOR_BLACK);
    init_pair(CP_SHAPE_DASH, COLOR_YELLOW,  COLOR_BLACK);
    init_pair(CP_MENU_SEL,   COLOR_BLACK,   COLOR_GREEN);
    init_pair(CP_HELP,       COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_SUCCESS,    COLOR_BLACK,   COLOR_GREEN);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Window creation / refresh
 * ═══════════════════════════════════════════════════════════════════ */

static void create_windows(void) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)cols;

    int canvas_w  = CANVAS_WIDTH  + 2; /* +2 for borders */
    int canvas_h  = CANVAS_HEIGHT + 2;
    int sidebar_h = rows - STATUS_H - TITLE_H;

    if (win_title)   { delwin(win_title);   win_title   = NULL; }
    if (win_canvas)  { delwin(win_canvas);  win_canvas  = NULL; }
    if (win_sidebar) { delwin(win_sidebar); win_sidebar = NULL; }
    if (win_status)  { delwin(win_status);  win_status  = NULL; }

    win_title   = newwin(TITLE_H,    canvas_w + SIDEBAR_W, 0,               0);
    win_canvas  = newwin(canvas_h,   canvas_w,             TITLE_H,         0);
    win_sidebar = newwin(sidebar_h,  SIDEBAR_W,            TITLE_H,         canvas_w);
    win_status  = newwin(STATUS_H,   canvas_w + SIDEBAR_W, rows - STATUS_H, 0);

    keypad(win_canvas,  TRUE);
    keypad(win_status,  TRUE);
    keypad(win_sidebar, TRUE);
}

/* ─── Draw title bar ─────────────────────────────────────────────── */
static void refresh_title(void) {
    wbkgd(win_title, COLOR_PAIR(CP_TITLE) | A_BOLD);
    werase(win_title);
    int w = getmaxx(win_title);
    const char *label = "  ★  2D GRAPHICS EDITOR  ★  [* and _ canvas]";
    int lx = (w - (int)strlen(label)) / 2;
    if (lx < 0) lx = 0;
    wattron(win_title, A_BOLD);
    mvwprintw(win_title, 0, lx, "%s", label);
    mvwhline(win_title, 1, 0, ACS_HLINE, w);
    wattroff(win_title, A_BOLD);
    wrefresh(win_title);
}

/* ─── Draw canvas window ─────────────────────────────────────────── */
static void refresh_canvas(void) {
    werase(win_canvas);
    wattron(win_canvas, COLOR_PAIR(CP_BORDER) | A_BOLD);
    box(win_canvas, ACS_VLINE, ACS_HLINE);
    wattroff(win_canvas, CP_BORDER | A_BOLD);

    wbkgd(win_canvas, COLOR_PAIR(CP_CANVAS));

    /* Render shapes onto grid */
    canvas_render(&canvas);

    /* Blit grid into window */
    for (int row = 0; row < CANVAS_HEIGHT; row++) {
        for (int col = 0; col < CANVAS_WIDTH; col++) {
            char ch = canvas.grid[row][col];
            if (ch == BG_CHAR && (row % 5 == 0 || col % 10 == 0)) {
                ch = '.';
            }
            if (ch == '*') {
                wattron(win_canvas, COLOR_PAIR(CP_SHAPE_STAR) | A_BOLD);
                mvwaddch(win_canvas, row + 1, col + 1, (chtype)ch);
                wattroff(win_canvas, COLOR_PAIR(CP_SHAPE_STAR) | A_BOLD);
            } else if (ch == '_') {
                wattron(win_canvas, COLOR_PAIR(CP_SHAPE_DASH) | A_BOLD);
                mvwaddch(win_canvas, row + 1, col + 1, (chtype)ch);
                wattroff(win_canvas, COLOR_PAIR(CP_SHAPE_DASH) | A_BOLD);
            } else {
                mvwaddch(win_canvas, row + 1, col + 1, (chtype)ch);
            }
        }
    }
    wrefresh(win_canvas);
}

/* ─── Draw sidebar ───────────────────────────────────────────────── */
static void refresh_sidebar(void) {
    int h = getmaxy(win_sidebar);
    werase(win_sidebar);
    wbkgd(win_sidebar, COLOR_PAIR(CP_SIDEBAR));
    wattron(win_sidebar, COLOR_PAIR(CP_SIDEBAR) | A_BOLD);
    box(win_sidebar, ACS_VLINE, ACS_HLINE);
    mvwprintw(win_sidebar, 0, 2, " MENU ");
    wattroff(win_sidebar, COLOR_PAIR(CP_SIDEBAR) | A_BOLD);

    int row = 2;
    for (int i = 0; i < N_MAIN_MENU && row < h - 3; i++) {
        mvwprintw(win_sidebar, row, 2, "%s", main_menu_items[i]);
        row++;
    }

    if (row < h - 3) {
        wattron(win_sidebar, COLOR_PAIR(CP_BORDER));
        mvwhline(win_sidebar, row, 1, ACS_HLINE, SIDEBAR_W - 2);
        wattroff(win_sidebar, COLOR_PAIR(CP_BORDER));
        row += 2;
    }

    wattron(win_sidebar, A_BOLD);
    mvwprintw(win_sidebar, row++, 2, "Active Shapes:");
    wattroff(win_sidebar, A_BOLD);

    int active = 0;
    for (int i = 0; i < canvas.shape_count && row < h - 2; i++) {
        Shape *s = &canvas.shapes[i];
        if (!s->is_active) continue;
        active++;

        char desc[64];
        shape_describe(s, desc, sizeof(desc));
        mvwprintw(win_sidebar, row, 2, "%-*.*s", SIDEBAR_W - 4,
                  SIDEBAR_W - 4, desc);
        row++;
    }

    if (active == 0 && row < h - 2) {
        wattron(win_sidebar, A_DIM);
        mvwprintw(win_sidebar, row++, 2, "(no shapes)");
        wattroff(win_sidebar, A_DIM);
    }

    wattron(win_sidebar, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(win_sidebar, h - 2, 2, " Total: %d ", active);
    wattroff(win_sidebar, COLOR_PAIR(CP_TITLE) | A_BOLD);

    wrefresh(win_sidebar);
}

/* ─── Draw status / help bar ─────────────────────────────────────── */
static void refresh_status(const char *msg) {
    werase(win_status);
    wbkgd(win_status, COLOR_PAIR(CP_STATUS));

    int w = getmaxx(win_status);
    wattron(win_status, COLOR_PAIR(CP_STATUS) | A_BOLD);
    mvwhline(win_status, 0, 0, ACS_HLINE, w);

    /* Key hints */
    mvwprintw(win_status, 1, 1,
        "Menu: 1-9 actions, 0=Quit, h=Help | Use arrows + Enter to select points");

    /* Right-align status message */
    if (msg && msg[0]) {
        int mlen = (int)strlen(msg);
        int mx   = w - mlen - 2;
        if (mx < 1) mx = 1;
        wattron(win_status, COLOR_PAIR(CP_HIGHLIGHT) | A_BOLD);
        mvwprintw(win_status, 1, mx, "%s", msg);
        wattroff(win_status, COLOR_PAIR(CP_HIGHLIGHT) | A_BOLD);
    }
    wattroff(win_status, COLOR_PAIR(CP_STATUS) | A_BOLD);
    wrefresh(win_status);
}

/* ─── Full redraw ────────────────────────────────────────────────── */
static void redraw_all(const char *msg) {
    refresh_title();
    refresh_canvas();
    refresh_sidebar();
    refresh_status(msg);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Generic input helpers
 * ═══════════════════════════════════════════════════════════════════ */

/* Pop-up dialog: prompt for a single integer, return 0 on success */
static int prompt_int(const char *prompt, int *out) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int dw = 48, dh = 5;
    int dy = (rows - dh) / 2, dx = (cols - dw) / 2;
    if (dx < 0) dx = 0;

    WINDOW *dlg = newwin(dh, dw, dy, dx);
    wbkgd(dlg, COLOR_PAIR(CP_SIDEBAR));
    wattron(dlg, COLOR_PAIR(CP_BORDER) | A_BOLD);
    box(dlg, ACS_VLINE, ACS_HLINE);
    wattroff(dlg, COLOR_PAIR(CP_BORDER) | A_BOLD);

    wattron(dlg, A_BOLD | COLOR_PAIR(CP_HIGHLIGHT));
    mvwprintw(dlg, 1, 2, "%s", prompt);
    wattroff(dlg, A_BOLD | COLOR_PAIR(CP_HIGHLIGHT));

    mvwprintw(dlg, 2, 2, "> ");
    wrefresh(dlg);

    echo();
    curs_set(1);
    char buf[16] = {0};
    mvwgetnstr(dlg, 2, 4, buf, sizeof(buf) - 1);
    noecho();
    curs_set(0);
    delwin(dlg);

    char *end;
    long v = strtol(buf, &end, 10);
    if (end == buf) return -1;
    *out = (int)v;
    return 0;
}

/* Prompt for a single character */
static int prompt_char(const char *prompt, char *out) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int dw = 48, dh = 5;
    int dy = (rows - dh) / 2, dx = (cols - dw) / 2;
    if (dx < 0) dx = 0;

    WINDOW *dlg = newwin(dh, dw, dy, dx);
    wbkgd(dlg, COLOR_PAIR(CP_SIDEBAR));
    wattron(dlg, COLOR_PAIR(CP_BORDER) | A_BOLD);
    box(dlg, ACS_VLINE, ACS_HLINE);
    wattroff(dlg, COLOR_PAIR(CP_BORDER) | A_BOLD);

    wattron(dlg, A_BOLD | COLOR_PAIR(CP_HIGHLIGHT));
    mvwprintw(dlg, 1, 2, "%s", prompt);
    wattroff(dlg, A_BOLD | COLOR_PAIR(CP_HIGHLIGHT));

    mvwprintw(dlg, 2, 2, "Press a key (common: * _): ");
    wrefresh(dlg);

    echo();
    curs_set(1);
    int ch = wgetch(dlg);
    noecho();
    curs_set(0);
    delwin(dlg);

    if (ch == ERR || ch == 27) return -1;
    *out = (char)ch;
    return 0;
}

static int prompt_canvas_point(const char *title, int *out_x, int *out_y) {
    int x = CANVAS_WIDTH / 2;
    int y = CANVAS_HEIGHT / 2;
    if (*out_x >= 0 && *out_x < CANVAS_WIDTH) x = *out_x;
    if (*out_y >= 0 && *out_y < CANVAS_HEIGHT) y = *out_y;

    while (1) {
        /* Draw a blank canvas with a single selection marker */
        werase(win_canvas);
        wattron(win_canvas, COLOR_PAIR(CP_BORDER) | A_BOLD);
        box(win_canvas, ACS_VLINE, ACS_HLINE);
        wattroff(win_canvas, COLOR_PAIR(CP_BORDER) | A_BOLD);
        wbkgd(win_canvas, COLOR_PAIR(CP_CANVAS));

        for (int row = 0; row < CANVAS_HEIGHT; row++) {
            for (int col = 0; col < CANVAS_WIDTH; col++) {
                if (row == y && col == x) {
                    wattron(win_canvas, COLOR_PAIR(CP_MENU_SEL) | A_BOLD);
                    mvwaddch(win_canvas, row + 1, col + 1, 'C');
                    wattroff(win_canvas, COLOR_PAIR(CP_MENU_SEL) | A_BOLD);
                } else {
                    mvwaddch(win_canvas, row + 1, col + 1, (chtype)BG_CHAR);
                }
            }
        }
        wrefresh(win_canvas);

        /* Draw status line for point picker */
        werase(win_status);
        wbkgd(win_status, COLOR_PAIR(CP_STATUS));
        int w = getmaxx(win_status);
        wattron(win_status, COLOR_PAIR(CP_STATUS) | A_BOLD);
        mvwhline(win_status, 0, 0, ACS_HLINE, w);
        mvwprintw(win_status, 1, 1,
            "%s  Arrow keys = move, Enter = select, Esc = cancel",
            title);
        char coord_buf[64];
        snprintf(coord_buf, sizeof(coord_buf), "Row: %d  Col: %d", y, x);
        int mx = w - (int)strlen(coord_buf) - 2;
        if (mx < 1) mx = 1;
        wattron(win_status, COLOR_PAIR(CP_HIGHLIGHT) | A_BOLD);
        mvwprintw(win_status, 1, mx, "%s", coord_buf);
        wattroff(win_status, COLOR_PAIR(CP_HIGHLIGHT) | A_BOLD);
        wattroff(win_status, COLOR_PAIR(CP_STATUS) | A_BOLD);
        wrefresh(win_status);

        int ch = wgetch(win_canvas);
        switch (ch) {
            case KEY_UP:
                if (y > 0) y--;
                break;
            case KEY_DOWN:
                if (y < CANVAS_HEIGHT - 1) y++;
                break;
            case KEY_LEFT:
                if (x > 0) x--;
                break;
            case KEY_RIGHT:
                if (x < CANVAS_WIDTH - 1) x++;
                break;
            case '\n':
            case KEY_ENTER:
                *out_x = x;
                *out_y = y;
                return 0;
            case 27:
                return -1;
            case KEY_RESIZE:
                create_windows();
                break;
            default:
                break;
        }
    }
}

/* Show a brief error overlay (auto-dismiss) */
static void show_error(const char *msg) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int dw = (int)strlen(msg) + 8;
    int dh = 3;
    int dy = (rows - dh) / 2, dx = (cols - dw) / 2;
    if (dx < 0) dx = 0;

    WINDOW *err = newwin(dh, dw, dy, dx);
    wbkgd(err, COLOR_PAIR(CP_ERROR));
    wattron(err, A_BOLD);
    box(err, ACS_VLINE, ACS_HLINE);
    mvwprintw(err, 1, 3, "%s", msg);
    wattroff(err, A_BOLD);
    wrefresh(err);
    napms(1400);
    delwin(err);
}

/* Show a brief success overlay (auto-dismiss) */
static void show_success(const char *msg) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int dw = (int)strlen(msg) + 8;
    int dh = 3;
    int dy = (rows - dh) / 2, dx = (cols - dw) / 2;
    if (dx < 0) dx = 0;

    WINDOW *ok = newwin(dh, dw, dy, dx);
    wbkgd(ok, COLOR_PAIR(CP_SUCCESS));
    wattron(ok, A_BOLD);
    box(ok, ACS_VLINE, ACS_HLINE);
    mvwprintw(ok, 1, 3, "%s", msg);
    wattroff(ok, A_BOLD);
    wrefresh(ok);
    napms(900);
    delwin(ok);
}

static int compute_triangle_vertices(int cx, int cy,
                                     int a, int b, int c,
                                     Point *p1, Point *p2, Point *p3) {
    if (a <= 0 || b <= 0 || c <= 0) return -1;
    if (a + b <= c || a + c <= b || b + c <= a) return -1;

    double da = (double)a;
    double db = (double)b;
    double dc = (double)c;

    double xA = 0.0;
    double yA = 0.0;
    double xB = dc;
    double yB = 0.0;
    double xC = (db*db + dc*dc - da*da) / (2.0 * dc);
    double yC2 = db*db - xC*xC;
    if (yC2 < 0.0) return -1;
    double yC = sqrt(yC2);

    double centroid_x = (xA + xB + xC) / 3.0;
    double centroid_y = (yA + yB + yC) / 3.0;
    double shift_x = (double)cx - centroid_x;
    double shift_y = (double)cy - centroid_y;

    p1->x = (int)round(xA + shift_x);
    p1->y = (int)round(yA + shift_y);
    p2->x = (int)round(xB + shift_x);
    p2->y = (int)round(yB + shift_y);
    p3->x = (int)round(xC + shift_x);
    p3->y = (int)round(yC + shift_y);
    return 0;
}

static void add_rectangle_menu(char *status_out, int status_len) {
    int cx = 0, cy = 0, length = 0, width = 0;
    if (prompt_canvas_point("Select rectangle center:", &cx, &cy) < 0) {
        snprintf(status_out, status_len, "Add cancelled.");
        return;
    }
    if (prompt_int("Length (cm):", &length) < 0 || length <= 0) {
        show_error("Invalid length.");
        snprintf(status_out, status_len, "Add cancelled.");
        return;
    }
    if (prompt_int("Width (cm):", &width) < 0 || width <= 0) {
        show_error("Invalid width.");
        snprintf(status_out, status_len, "Add cancelled.");
        return;
    }

    int x = cx - length / 2;
    int y = cy - width / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + length > CANVAS_WIDTH) x = CANVAS_WIDTH - length;
    if (y + width > CANVAS_HEIGHT) y = CANVAS_HEIGHT - width;

    int id = shape_add_rectangle(&canvas, x, y, length, width, DRAW_CHAR_LINE);
    if (id > 0)
        snprintf(status_out, status_len, "Added rectangle ID=%d (%dx%d cm)", id, length, width);
    else
        snprintf(status_out, status_len, "Canvas full!");
}

static void add_circle_menu(char *status_out, int status_len) {
    int cx = 0, cy = 0, r = 0;
    if (prompt_canvas_point("Select circle center:", &cx, &cy) < 0) {
        snprintf(status_out, status_len, "Add cancelled.");
        return;
    }
    if (prompt_int("Radius:", &r) < 0 || r <= 0) {
        show_error("Invalid radius.");
        snprintf(status_out, status_len, "Add cancelled.");
        return;
    }

    int id = shape_add_circle(&canvas, cx, cy, r, DRAW_CHAR_LINE);
    if (id > 0)
        snprintf(status_out, status_len, "Added circle ID=%d", id);
    else
        snprintf(status_out, status_len, "Canvas full!");
}

static void add_line_menu(char *status_out, int status_len) {
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    if (prompt_canvas_point("Select line start:", &x1, &y1) < 0) {
        snprintf(status_out, status_len, "Add cancelled.");
        return;
    }
    if (prompt_canvas_point("Select line end:", &x2, &y2) < 0) {
        snprintf(status_out, status_len, "Add cancelled.");
        return;
    }
    int id = shape_add_line(&canvas, x1, y1, x2, y2, DRAW_CHAR_LINE);
    if (id > 0)
        snprintf(status_out, status_len, "Added line ID=%d", id);
    else
        snprintf(status_out, status_len, "Canvas full!");
}

static void add_triangle_menu(char *status_out, int status_len) {
    int cx = 0, cy = 0;
    if (prompt_canvas_point("Select triangle center:", &cx, &cy) < 0) {
        snprintf(status_out, status_len, "Add cancelled.");
        return;
    }
    int a = 0, b = 0, c = 0;
    if (prompt_int("Side A (cm):", &a) < 0 || a <= 0 ||
        prompt_int("Side B (cm):", &b) < 0 || b <= 0 ||
        prompt_int("Side C (cm):", &c) < 0 || c <= 0) {
        show_error("Invalid triangle sides.");
        snprintf(status_out, status_len, "Add cancelled.");
        return;
    }

    Point p1, p2, p3;
    if (compute_triangle_vertices(cx, cy, a, b, c, &p1, &p2, &p3) < 0) {
        show_error("Triangle sides do not form a valid triangle.");
        snprintf(status_out, status_len, "Add cancelled.");
        return;
    }

    int id = shape_add_triangle(&canvas, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, DRAW_CHAR_LINE);
    if (id > 0)
        snprintf(status_out, status_len, "Added triangle ID=%d (%d,%d,%d cm)", id, a, b, c);
    else
        snprintf(status_out, status_len, "Canvas full!");
}

static void display_canvas_menu(char *status_out, int status_len) {
    snprintf(status_out, status_len, "Canvas refreshed.");
}

/* ═══════════════════════════════════════════════════════════════════
 *  Add / Delete / Modify / List / Clear / Export / Help
 * ═══════════════════════════════════════════════════════════════════ */

static void handle_delete(char *status_out, int status_len) {
    int id;
    if (prompt_int("Delete – Enter shape ID:", &id) < 0) {
        snprintf(status_out, status_len, "Delete cancelled.");
        return;
    }
    if (shape_delete(&canvas, id) < 0)
        show_error("Shape ID not found!");
    else
        snprintf(status_out, status_len, "Deleted shape ID=%d", id);
}

static void handle_modify(char *status_out, int status_len) {
    int id;
    if (prompt_int("Modify – Enter shape ID:", &id) < 0) {
        snprintf(status_out, status_len, "Modify cancelled.");
        return;
    }

    Shape *s = shape_find(&canvas, id);
    if (!s || !s->is_active) {
        show_error("Shape ID not found or deleted!");
        snprintf(status_out, status_len, "Modify failed.");
        return;
    }

    /* Sub-menu: what to modify */
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int dw = 38, dh = 7;
    int dy = (rows - dh) / 2, dx = (cols - dw) / 2;
    if (dx < 0) dx = 0;

    WINDOW *m = newwin(dh, dw, dy, dx);
    keypad(m, TRUE);
    wbkgd(m, COLOR_PAIR(CP_SIDEBAR));

    const char *opts[] = { "Geometry (coordinates / size)", "Draw character" };
    int sel = 0;
    int nopt = 2;
    int chosen = -1;

    for (;;) {
        werase(m);
        wattron(m, COLOR_PAIR(CP_BORDER) | A_BOLD);
        box(m, ACS_VLINE, ACS_HLINE);
        wattroff(m, COLOR_PAIR(CP_BORDER) | A_BOLD);
        wattron(m, COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvwprintw(m, 0, 2, " Modify [%d] ", id);
        wattroff(m, COLOR_PAIR(CP_TITLE) | A_BOLD);

        for (int i = 0; i < nopt; i++) {
            if (i == sel) {
                wattron(m, COLOR_PAIR(CP_MENU_SEL) | A_BOLD);
                mvwprintw(m, i + 2, 4, " %-30s", opts[i]);
                wattroff(m, COLOR_PAIR(CP_MENU_SEL) | A_BOLD);
            } else {
                mvwprintw(m, i + 2, 4, " %-30s", opts[i]);
            }
        }
        wattron(m, A_DIM);
        mvwprintw(m, dh - 2, 2, "↑↓←→ Move  Enter=OK  Esc=Cancel");
        wattroff(m, A_DIM);
        wrefresh(m);

        int ch = wgetch(m);
        if (ch == KEY_UP || ch == KEY_LEFT)   sel = (sel - 1 + nopt) % nopt;
        if (ch == KEY_DOWN || ch == KEY_RIGHT) sel = (sel + 1) % nopt;
        if (ch == 27) { delwin(m); snprintf(status_out, status_len, "Modify cancelled."); return; }
        if (ch == '\n' || ch == KEY_ENTER) { chosen = sel; delwin(m); break; }
    }

    int x1 = 0, y1 = 0, x2 = 0, y2 = 0, w = 0, h = 0, cx = 0, cy = 0, r = 0;
    char drawch;

    if (chosen == 1) {
        /* Modify draw character */
        if (prompt_char("New draw char:", &drawch) < 0) {
            snprintf(status_out, status_len, "Modify cancelled.");
            return;
        }
        shape_modify_char(&canvas, id, drawch);
        snprintf(status_out, status_len, "Modified ID=%d draw char", id);
        return;
    }

    /* Modify geometry */
    switch (s->type) {
        case SHAPE_LINE:
            if (prompt_canvas_point("Select new start point:", &x1, &y1) < 0) goto mod_cancel;
            if (prompt_canvas_point("Select new end point:", &x2, &y2) < 0) goto mod_cancel;
            shape_modify_line(&canvas, id, x1, y1, x2, y2);
            break;

        case SHAPE_RECTANGLE:
            if (prompt_canvas_point("Select new top-left:", &x1, &y1) < 0) goto mod_cancel;
            if (prompt_canvas_point("Select new bottom-right:", &x2, &y2) < 0) goto mod_cancel;
            if (x2 < x1) { int tmp = x1; x1 = x2; x2 = tmp; }
            if (y2 < y1) { int tmp = y1; y1 = y2; y2 = tmp; }
            w = x2 - x1 + 1;
            h = y2 - y1 + 1;
            shape_modify_rectangle(&canvas, id, x1, y1, w, h);
            break;

        case SHAPE_CIRCLE:
            if (prompt_canvas_point("Select new center:", &cx, &cy) < 0) goto mod_cancel;
            if (prompt_canvas_point("Select new radius point:", &x2, &y2) < 0) goto mod_cancel;
            {
                int dx = x2 - cx;
                int dy = y2 - cy;
                r = (int)(sqrt((double)dx*dx + (double)dy*dy) + 0.5);
                if (r < 0) r = 0;
            }
            shape_modify_circle(&canvas, id, cx, cy, r);
            break;

        case SHAPE_TRIANGLE:
            if (prompt_canvas_point("Select new P1:", &x1, &y1) < 0) goto mod_cancel;
            if (prompt_canvas_point("Select new P2:", &x2, &y2) < 0) goto mod_cancel;
            if (prompt_canvas_point("Select new P3:", &cx, &cy) < 0) goto mod_cancel;
            shape_modify_triangle(&canvas, id, x1, y1, x2, y2, cx, cy);
            break;
    }
    snprintf(status_out, status_len, "Modified ID=%d geometry", id);
    return;

mod_cancel:
    snprintf(status_out, status_len, "Modify cancelled.");
}

/* ─── List shapes overlay ────────────────────────────────────────── */
static void handle_list(void) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int dw = 62, dh = rows - 6;
    if (dh < 6) dh = 6;
    int dy = (rows - dh) / 2, dx = (cols - dw) / 2;
    if (dx < 0) dx = 0;

    WINDOW *lw = newwin(dh, dw, dy, dx);
    keypad(lw, TRUE);
    wbkgd(lw, COLOR_PAIR(CP_SIDEBAR));
    wattron(lw, COLOR_PAIR(CP_BORDER) | A_BOLD);
    box(lw, ACS_VLINE, ACS_HLINE);
    wattroff(lw, COLOR_PAIR(CP_BORDER) | A_BOLD);
    wattron(lw, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(lw, 0, 2, " All Shapes ");
    wattroff(lw, COLOR_PAIR(CP_TITLE) | A_BOLD);

    /* Column header */
    wattron(lw, A_UNDERLINE | A_BOLD);
    mvwprintw(lw, 1, 2, "%-4s %-9s %-4s %-40s", "ID", "Type", "Ch", "Parameters");
    wattroff(lw, A_UNDERLINE | A_BOLD);

    int row = 2;
    int active = 0;
    for (int i = 0; i < canvas.shape_count && row < dh - 2; i++) {
        Shape *s = &canvas.shapes[i];
        if (!s->is_active) continue;
        active++;

        char desc[80];
        shape_describe(s, desc, sizeof(desc));

        char tname[16];
        shape_type_name(s->type, tname, sizeof(tname));

        char params[50] = "";
        switch (s->type) {
            case SHAPE_LINE:
                snprintf(params, sizeof(params), "(%d,%d)->(%d,%d)",
                    s->data.line.start.x, s->data.line.start.y,
                    s->data.line.end.x,   s->data.line.end.y);
                break;
            case SHAPE_RECTANGLE:
                snprintf(params, sizeof(params), "(%d,%d) w=%d h=%d",
                    s->data.rect.top_left.x, s->data.rect.top_left.y,
                    s->data.rect.width,       s->data.rect.height);
                break;
            case SHAPE_CIRCLE:
                snprintf(params, sizeof(params), "center(%d,%d) r=%d",
                    s->data.circle.center.x, s->data.circle.center.y,
                    s->data.circle.radius);
                break;
            case SHAPE_TRIANGLE:
                snprintf(params, sizeof(params), "(%d,%d) (%d,%d) (%d,%d)",
                    s->data.triangle.p1.x, s->data.triangle.p1.y,
                    s->data.triangle.p2.x, s->data.triangle.p2.y,
                    s->data.triangle.p3.x, s->data.triangle.p3.y);
                break;
        }

        wattron(lw, COLOR_PAIR(CP_HIGHLIGHT));
        mvwprintw(lw, row, 2, "%-4d", s->id);
        wattroff(lw, COLOR_PAIR(CP_HIGHLIGHT));
        mvwprintw(lw, row, 6, "%-9s %-4c %s", tname, s->draw_char, params);
        row++;
    }

    if (active == 0) {
        wattron(lw, A_DIM);
        mvwprintw(lw, 2, 2, "(no active shapes)");
        wattroff(lw, A_DIM);
    }

    wattron(lw, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(lw, dh - 2, 2, " %d shape(s)  –  Press any key to close ", active);
    wattroff(lw, COLOR_PAIR(CP_TITLE) | A_BOLD);

    wrefresh(lw);
    wgetch(lw);
    delwin(lw);
}

/* ─── Clear all shapes ───────────────────────────────────────────── */
static void handle_clear(char *status_out, int status_len) {
    /* Confirm */
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int dw = 40, dh = 4;
    int dy = (rows - dh) / 2, dx = (cols - dw) / 2;
    if (dx < 0) dx = 0;

    WINDOW *cw = newwin(dh, dw, dy, dx);
    keypad(cw, TRUE);
    wbkgd(cw, COLOR_PAIR(CP_ERROR));
    wattron(cw, A_BOLD);
    box(cw, ACS_VLINE, ACS_HLINE);
    mvwprintw(cw, 1, 2, "Clear ALL shapes?  [y] Yes  [n] No");
    wattroff(cw, A_BOLD);
    wrefresh(cw);

    int ch = wgetch(cw);
    delwin(cw);

    if (ch == 'y' || ch == 'Y') {
        canvas_init(&canvas);
        snprintf(status_out, status_len, "Canvas cleared.");
    } else {
        snprintf(status_out, status_len, "Clear cancelled.");
    }
}

/* ─── Export canvas to text file ─────────────────────────────────── */
static void handle_export(char *status_out, int status_len) {
    canvas_render(&canvas);

    const char *fname = "canvas_export.txt";
    FILE *fp = fopen(fname, "w");
    if (!fp) {
        show_error("Cannot open canvas_export.txt!");
        snprintf(status_out, status_len, "Export failed.");
        return;
    }

    /* Timestamp header */
    time_t t = time(NULL);
    char tbuf[64];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    fprintf(fp, "2D Graphics Editor – Canvas Export\n");
    fprintf(fp, "Exported: %s\n", tbuf);
    fprintf(fp, "Shapes  : %d active\n\n", canvas.shape_count);

    /* Canvas grid */
    fprintf(fp, "+");
    for (int col = 0; col < CANVAS_WIDTH; col++) fprintf(fp, "-");
    fprintf(fp, "+\n");
    for (int row = 0; row < CANVAS_HEIGHT; row++)
        fprintf(fp, "|%s|\n", canvas.grid[row]);
    fprintf(fp, "+");
    for (int col = 0; col < CANVAS_WIDTH; col++) fprintf(fp, "-");
    fprintf(fp, "+\n\n");

    /* Shape list */
    fprintf(fp, "Shape List:\n");
    for (int i = 0; i < canvas.shape_count; i++) {
        Shape *s = &canvas.shapes[i];
        if (!s->is_active) continue;
        char desc[128];
        shape_describe(s, desc, sizeof(desc));
        fprintf(fp, "  %s\n", desc);
    }

    fclose(fp);
    show_success("Exported to canvas_export.txt!");
    snprintf(status_out, status_len, "Canvas exported.");
}

/* ─── Help overlay ───────────────────────────────────────────────── */
static void handle_help(void) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int dw = 56, dh = 22;
    int dy = (rows - dh) / 2, dx = (cols - dw) / 2;
    if (dx < 0) dx = 0;

    WINDOW *hw = newwin(dh, dw, dy, dx);
    keypad(hw, TRUE);
    wbkgd(hw, COLOR_PAIR(CP_SIDEBAR));
    wattron(hw, COLOR_PAIR(CP_BORDER) | A_BOLD);
    box(hw, ACS_VLINE, ACS_HLINE);
    wattroff(hw, COLOR_PAIR(CP_BORDER) | A_BOLD);
    wattron(hw, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(hw, 0, 2, " Help – 2D Graphics Editor ");
    wattroff(hw, COLOR_PAIR(CP_TITLE) | A_BOLD);

    const char *lines[] = {
        "",
        "  KEY BINDINGS",
        "  ─────────────────────────────────────────",
        "  1    Add Rectangle",
        "  2    Add Circle",
        "  3    Add Line",
        "  4    Add Triangle",
        "  5    Delete Object",
        "  6    Modify Object",
        "  7    List Objects",
        "  8    Display Canvas",
        "  9    Clear Canvas",
        "  0    Quit the editor",
        "",
        "  MENU NAVIGATION",
        "  ─────────────────────────────────────────",
        "  ↑/↓/←/→  Move selection cursor",
        "  Enter   Select point on canvas when prompted",
        "",
        "  DRAWING CHARS",
        "  ─────────────────────────────────────────",
        "  *    Star  (shown in GREEN)",
        "  _    Dash  (shown in YELLOW)",
        "  Any printable char is accepted.",
        "",
        "  SHAPES: Line · Rectangle · Circle · Triangle",
        "",
        "  Press any key to close...",
    };
    int nlines = (int)(sizeof(lines) / sizeof(lines[0]));
    for (int i = 0; i < nlines && i + 1 < dh - 1; i++)
        mvwprintw(hw, i + 1, 0, "%-*s", dw, lines[i]);

    wrefresh(hw);
    wgetch(hw);
    delwin(hw);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Main entry point
 * ═══════════════════════════════════════════════════════════════════ */

int main(void) {
    /* Initialise canvas */
    canvas_init(&canvas);

    /* Seed the canvas with a demo scene showcasing all four shape types */
    shape_add_rectangle(&canvas,  2,  1, 22, 12, '*');  /* id 1 */
    shape_add_circle   (&canvas, 58, 20,  9, '*');       /* id 2 */
    shape_add_line     (&canvas,  0, 39, 79,  0, '_');   /* id 3 – diagonal */
    shape_add_triangle (&canvas, 30,  2, 60,  2, 45, 18, '*'); /* id 4 */

    /* Initialise ncurses */
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    if (!has_colors()) {
        endwin();
        fprintf(stderr, "Error: terminal does not support colors.\n");
        return 1;
    }
    init_colors();

    create_windows();
    redraw_all("Welcome! Press h for help, a/d/m/q to work.");

    char status_msg[80] = "";
    int running = 1;

    while (running) {
        int ch = wgetch(win_status);
        switch (ch) {
            case '1':
                add_rectangle_menu(status_msg, sizeof(status_msg));
                break;
            case '2':
                add_circle_menu(status_msg, sizeof(status_msg));
                break;
            case '3':
                add_line_menu(status_msg, sizeof(status_msg));
                break;
            case '4':
                add_triangle_menu(status_msg, sizeof(status_msg));
                break;
            case '5':
                handle_delete(status_msg, sizeof(status_msg));
                break;
            case '6':
                handle_modify(status_msg, sizeof(status_msg));
                break;
            case '7':
                handle_list();
                snprintf(status_msg, sizeof(status_msg), "");
                break;
            case '8':
                display_canvas_menu(status_msg, sizeof(status_msg));
                break;
            case '9':
                handle_clear(status_msg, sizeof(status_msg));
                break;
            case '0':
            case 'q': case 'Q':
                running = 0;
                break;
            case 'h': case 'H':
                handle_help();
                snprintf(status_msg, sizeof(status_msg), "");
                break;
            case KEY_RESIZE:
                create_windows();
                break;
            default:
                status_msg[0] = '\0';
                break;
        }
        if (running)
            redraw_all(status_msg);
    }

    /* Clean up */
    if (win_title)   delwin(win_title);
    if (win_canvas)  delwin(win_canvas);
    if (win_sidebar) delwin(win_sidebar);
    if (win_status)  delwin(win_status);
    endwin();

    /* Print final canvas to stdout after ncurses quits */
    printf("\n=== Final Canvas State ===\n");
    canvas_render(&canvas);
    canvas_display(&canvas);

    printf("\n=== Active Shapes ===\n");
    for (int i = 0; i < canvas.shape_count; i++) {
        Shape *s = &canvas.shapes[i];
        if (!s->is_active) continue;
        char desc[128];
        shape_describe(s, desc, sizeof(desc));
        printf("  %s\n", desc);
    }
    return 0;
}
