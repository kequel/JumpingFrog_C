//gcc -o frog frog.c -lncurses
//./frog
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>

#define QUIT_TIME 3 // .. seconds to quit
#define QUIT 'q'
#define NOKEY ' '

#define BET_TIME 2 // 2s (sleep time between two catches)
#define FRAME_TIME 25 // 25 ms (base frame time) (time interval between frames)
#define PASS_TIME 20 // 25 s (time to die = no catched ball)
#define MVB_FACTOR 2 // move every FRAME_TIME * MVB_FACTOR [ms] BALL
#define MVC_FACTOR 5 // moving interval >= FRAME_TIME * MVC_FACTOR [ms] CATCHER

#define MAIN_COLOR      1
#define STAT_COLOR      2
#define PLAY_COLOR      3
#define FROG_COLOR_N 4
#define FROG_COLOR_R 5
#define BALL_COLOR 6
#define BALR_COLOR 7

#define BORDER 1
#define DELAY_ON 1
#define DELAY_OFF 0

#define ROWS 25 // playwin (WIN) parameters
#define COLS 110
#define OFFY 4
#define OFFX 8

#define RA(min, max) ( (min) + rand() % ((max) - (min) + 1) ) // random number between min and max (inc)

//------------------------------------------------
//----------------  DATA STRUCTURES --------------
//------------------------------------------------

// window structure
typedef struct {
WINDOW* window; // ncurses window
int x, y;
int rows, cols;
int color;
} WIN; // my window

// moving object structure inside win(dow)
typedef struct {
WIN* win;
int color; // normal color
int revcol; // reverse color
int bflag; // background color flag = 1 (window), = 0 (own)
int mv; // move factor
int x, y; // top-left corner
int width, height; // sizes
int xmin, xmax; // min/max -> place for moving in win->window
int ymin, ymax;
char** shape; // shape of the object (2-dim characters array (box))
} OBJ;

typedef struct {
unsigned int frame_time;
float pass_time;
int frame_no;
} TIMER;

//------------------------------------------------
//----------------  WINDOW FUNCTIONS -------------
//------------------------------------------------

WINDOW* Start()
{
WINDOW* win;

if ((win = initscr()) == NULL) { // initialize ncurses
fprintf(stderr, "Error initialising ncurses.\n");
exit(EXIT_FAILURE);
}

start_color(); // initialize colors
init_pair(MAIN_COLOR, COLOR_WHITE, COLOR_BLACK);
init_pair(PLAY_COLOR, COLOR_BLUE, COLOR_WHITE);
init_pair(STAT_COLOR, COLOR_WHITE, COLOR_BLUE);
init_pair(FROG_COLOR_N, COLOR_GREEN, COLOR_WHITE);
init_pair(FROG_COLOR_R, COLOR_GREEN, COLOR_BLACK);
init_pair(BALL_COLOR, COLOR_RED, COLOR_WHITE);
init_pair(BALR_COLOR, COLOR_RED, COLOR_BLUE);

noecho(); // Switch off echoing, turn off cursor
curs_set(0);
return win;
}

void Welcome(WINDOW* win) // Welcome screen (optional): press any key to continue
{
mvwaddstr(win, 1, 1, "Do you want to play a game?");
mvwaddstr(win, 2, 1, "Press any key to continue..");
wgetch(win); // waiting here..
wclear(win); // clear (after next refresh)
wrefresh(win);
}

//------------------------------------------------
//----------------  WIN FUNCTIONS ----------------
//------------------------------------------------

// cleaning window: writing " "
void CleanWin(WIN* W, int bo) // bo(rder): 0 | 1
{
int i, j;
wattron(W->window, COLOR_PAIR(W->color));
if (bo) box(W->window, 0, 0);
for (i = bo; i < W->rows - bo; i++)
for (j = bo; j < W->cols - bo; j++)
mvwprintw(W->window, i, j, " ");
}

// window initialization: position, colors, border, etc
WIN* Init(WINDOW* parent, int rows, int cols, int y, int x, int color, int bo, int delay)
{
// WIN* W = new WIN; // C++ version; compile with g++
WIN* W = (WIN*)malloc(sizeof(WIN)); // C version; compile with gcc
W->x = x; W->y = y; W->rows = rows; W->cols = cols; W->color = color;
W->window = subwin(parent, rows, cols, y, x);
CleanWin(W, bo);
if (delay == DELAY_OFF)  nodelay(W->window, TRUE); // non-blocking reading of characters (for real-time game)
wrefresh(W->window);
return W;
}

void EndGame(const char* info, WIN* W) // sth at the end
{
CleanWin(W, 1);
for (int i = QUIT_TIME; i > 0; i--)
{
mvwprintw(W->window, 1, 2, "%s Closing the game in %d seconds...", info, i);
wrefresh(W->window);
sleep(1);
}
}


//------------------------------------------------
//----------------  STATUS FUNCTIONS -------------
//------------------------------------------------

void ShowStatus(WIN* W, OBJ* o, int pts)
{
mvwprintw(W->window, 1, 45, "x: %d  y: %d  ", o->x, o->y);
mvwprintw(W->window, 1, 25, "%d", pts);
mvwprintw(W->window, 1, 68, "%s", o->bflag ? " normal  " : " reversed");
wrefresh(W->window);
}


void ShowTimer(WIN* W, float pass_time)
{
mvwprintw(W->window, 1, 9, "%.2f", pass_time);
wrefresh(W->window);
}

void ShowNewStatus(WIN* W, TIMER* T, OBJ* o, int pts)
{
box(W->window, 0, 0); // border
mvwprintw(W->window, 1, 3, "Time: ");
mvwprintw(W->window, 1, 17, "Points: ");
ShowTimer(W, T->pass_time);
mvwprintw(W->window, 1, 35, "Position: ");
mvwprintw(W->window, 1, 60, "Colors: ");
mvwprintw(W->window, 1, 78, ".C.A.T.C.H...T.H.E...B.A.L.L.");
ShowStatus(W, o, pts);
}

//------------------------------------------------
//----------------  OBJ+ FUNCTIONS ---------------
//------------------------------------------------

void Print(OBJ* ob)
{
for (int i = 0; i < ob->height; i++)
mvwprintw(ob->win->window, ob->y + i, ob->x, ob->shape[i]);
}

// common function for both: Catcher and Ball
void Show(OBJ* ob, int dx, int dy) {
char* sw = (char*)malloc(sizeof(char) * ob->width);
memset(sw, ' ', ob->width);

if (ob->bflag) wattron(ob->win->window, COLOR_PAIR(ob->color));
else wattron(ob->win->window, COLOR_PAIR(ob->revcol));

if ((dy == 1) && (ob->y + ob->height < ob->ymax)){
ob->y += dy;
mvwprintw(ob->win->window, ob->y - 1, ob->x, sw);
}

if ((dy == -1) && (ob->y > ob->ymin)){
ob->y += dy;
mvwprintw(ob->win->window, ob->y + ob->height, ob->x, sw);
}

if ((dx == 1) && (ob->x + ob->width < ob->xmax)){
ob->x += dx;
for (int i = 0; i < ob->height; i++) mvwprintw(ob->win->window, ob->y + i, ob->x - 1, " ");
}

if ((dx == -1) && (ob->x > ob->xmin)){
ob->x += dx;
for (int i = 0; i < ob->height; i++) mvwprintw(ob->win->window, ob->y + i, ob->x + ob->width, " ");
}

Print(ob);

if (ob->bflag) wattron(ob->win->window, COLOR_PAIR(ob->win->color));
wrefresh(ob->win->window);
}


void InitPos(OBJ* ob, int xs, int ys)
{
ob->x = xs;
ob->y = ys;
}

OBJ* InitFrog(WIN* w, int col, int rev)
{
// OBJ* ob    = new OBJ; // C++
OBJ* ob = (OBJ*)malloc(sizeof(OBJ)); // C
ob->bflag = 1; // normal colors (initially)
ob->revcol = rev;
ob->color = col;
ob->win = w;
ob->width = 3;
ob->height = 3;
ob->mv = 0;

ob->shape = (char**)malloc(sizeof(char*) * ob->height); // array of pointers (char*)
for (int i = 0; i < ob->height; i++)
ob->shape[i] = (char*)malloc(sizeof(char) * (ob->width + 1)); // +1: end-of-string (C): '\0'

strcpy(ob->shape[0], "***");
strcpy(ob->shape[1], "***");
strcpy(ob->shape[2], "***");

InitPos(ob, (ob->win->cols - ob->width) / 2, (ob->win->rows - ob->height -1));
ob->xmin = 1;
ob->xmax = w->cols - 1;
ob->ymin = 1;
ob->ymax = w->rows - 1;
return ob;
}

OBJ* InitBall(WIN* w, int col, int rev)
{
OBJ* ob = (OBJ*)malloc(sizeof(OBJ)); // C
ob->bflag = 1;
ob->color = col;
ob->revcol = rev;
ob->win = w;
ob->width = 10;
ob->height = 3;
ob->mv = MVB_FACTOR;

ob->shape = (char**)malloc(sizeof(char*) * ob->height); // array of pointers (char*)
for (int i = 0; i < ob->height; i++)
ob->shape[i] = (char*)malloc(sizeof(char) * (ob->width + 1)); // +1: end-of-string (C): '\0'

strcpy(ob->shape[0], "xxxxxxxxxx");
strcpy(ob->shape[1], "xxxxxxxxxx");
strcpy(ob->shape[2], "xxxxxxxxxx");

ob->xmin = 1;
ob->xmax = w->cols - 1;
ob->ymin = 1;
ob->ymax = w->rows - 1;
InitPos(ob, (ob->win->cols - ob->width) / 2, (ob->win->rows - ob->height) / 2); //ob->xmin, ob->ymin);
return ob;
}

void MoveFrog(OBJ* ob, int ch, unsigned int frame)
{
    if (frame - ob->mv >= MVC_FACTOR) {
        switch (ch) {
            case 'w': Show(ob, 0, -1); break; 
            case 's': Show(ob, 0, 1); break;  
            case 'a': Show(ob, -1, 0); break; 
            case 'd': Show(ob, 1, 0); break;  
        }
        ob->mv = frame; 
    }
}


void MoveCar(OBJ* ob, int Cx, int Cy, int frame)
{
int dx = 0, dy = 0;
if (frame % ob->mv == 0) // every ob->mv-th frame make a move
{
if (ob->x + ob->width >= ob->xmax) { 
        for (int i = 0; i < ob->height; i++) {
            for (int j = 0; j < ob->width; j++) {
                mvwprintw(ob->win->window, ob->y + i, ob->x + j, " "); // Czyści całą szerokość piłki
            }
        }
        ob->x = 1; // Wróć na początek
    } else {
        dx = 1; // Ruch w prawo
    }

Show(ob, dx, dy);
}
}

int Collision(OBJ* c, OBJ* b) // collision of two boxes
{
if (((c->y >= b->y && c->y < b->y + b->height) || (b->y >= c->y && b->y < c->y + c->height)) &&
((c->x >= b->x && c->x < b->x + b->width) || (b->x >= c->x && b->x < c->x + c->width)))
return 1;
else return 0;
}

void ChangeColors(OBJ* c) { c->bflag = !c->bflag; }

void Restart(OBJ* catcher, OBJ* ball)
{

ChangeColors(catcher); ChangeColors(ball); Show(catcher, 0, 0); Show(ball, 0, 0);
sleep(BET_TIME);
CleanWin(catcher->win, 1);
InitPos(catcher, (catcher->win->cols - catcher->width) / 2, (catcher->win->rows - catcher->height) / 2);
InitPos(ball, ball->xmin, ball->ymin);
ChangeColors(catcher); ChangeColors(ball); Show(catcher, 0, 0); Show(ball, 0, 0);
}

//------------------------------------------------
//----------------  TIMER FUNCTIONS --------------
//------------------------------------------------

void Sleep(unsigned int tui) { usleep(tui * 1000); } // micro_sec = frame_time * 1000

TIMER* InitTimer(WIN* status)
{
TIMER* timer = (TIMER*)malloc(sizeof(TIMER));
timer->frame_no = 1;
timer->frame_time = FRAME_TIME;
timer->pass_time = PASS_TIME / 1.0;
return timer;
}

int UpdateTimer(TIMER* T, WIN* status) // return 1: time is over; otherwise: 0
{
T->frame_no++;
T->pass_time = PASS_TIME - (T->frame_no * T->frame_time / 1000.0);
if (T->pass_time < (T->frame_time / 1000.0)) T->pass_time = 0; // make this zero (floating point!)
else Sleep(T->frame_time);
ShowTimer(status, T->pass_time);
if (T->pass_time == 0) return 1;
return 0;
}

//------------------------------------------------
//----------------  MainLoop FUNCTION ------------
//------------------------------------------------
// objects OBJ have info about the windows containing it
int MainLoop(WIN* status, OBJ* catcher, OBJ* ball, TIMER* timer) // 1: timer is over, 0: quit the game
{
int ch;
int pts = 0;
while ((ch = wgetch(status->window)) != QUIT) // NON-BLOCKING! (nodelay=TRUE)
{
if (ch == ERR) ch = NOKEY; // ERR is ncurses predefined
/* change background or move; update status */
else
{
if (ch == 'b') { ChangeColors(catcher); CleanWin(catcher->win, 1); Show(catcher, 0, 0); Show(ball, 0, 0); }
else MoveFrog(catcher, ch, timer->frame_no);
}
MoveCar(ball, catcher->x + catcher->width / 2, catcher->y + catcher->height / 2, timer->frame_no);
if (Collision(catcher, ball))
{
Restart(catcher, ball); pts++;
} // for testing purposes
ShowStatus(status, catcher, pts);
flushinp();                     // clear input buffer (avoiding multiple key pressed)
/* update timer */
if (UpdateTimer(timer, status)) return pts; // sleep inside
}
return 0;
}

//------------------------------------------------
//----------------  MAIN FUNCTION ----------------
//------------------------------------------------

int main()
{
WINDOW* mainwin = Start();

Welcome(mainwin);

WIN* playwin = Init(mainwin, ROWS, COLS, OFFY, OFFX, PLAY_COLOR, BORDER, DELAY_ON); // window for the playing area
WIN* statwin = Init(mainwin, 3, COLS, ROWS + OFFY, OFFX, STAT_COLOR, BORDER, DELAY_OFF); // window for the status
// DELAY_OFF = real-time game

TIMER* timer = InitTimer(statwin);

OBJ* frog = InitFrog(playwin, FROG_COLOR_N, FROG_COLOR_R);
OBJ* ball = InitBall(playwin, BALL_COLOR, BALR_COLOR);

ShowNewStatus(statwin, timer, frog, 0);
Show(frog, 0, 0);
Show(ball, 0, 0);

int result;
if ((result = MainLoop(statwin, frog, ball, timer)) == 0)  EndGame("You have decided to quit the game.", statwin);
else{
char info[100];
sprintf(info, " Timer is over, Your points = %d.", result);
EndGame(info, statwin);
}

delwin(playwin->window); // Clean up (!)
delwin(statwin->window);
delwin(mainwin);
endwin();
refresh();
return EXIT_SUCCESS;
}
