// gcc -o frog frog.c -lncurses
//./frog
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>

#define QUIT_TIME 3 // .. seconds to quit
#define QUIT 'q'
#define NOKEY ' '
#define CARS_NUMBER 6

#define BET_TIME 2    // 2s (sleep time between two catches)
#define FRAME_TIME 25 // 25 ms (base frame time) (time interval between frames)
#define PASS_TIME 200 // 25 s (time to die = no catched ball)
#define MVB_FACTOR 5  // move every FRAME_TIME * MVB_FACTOR [ms] BALL
#define MVC_FACTOR 5  // moving interval >= FRAME_TIME * MVC_FACTOR [ms] CATCHER

#define MAIN_COLOR 1
#define STAT_COLOR 2
#define PLAY_COLOR 3
#define FROG_COLOR_N 4

#define CAR_W_COLOR 6
#define CAR_B_COLOR 7

#define ONE_LEFT -1
#define ONE_RIGHT 1

#define BORDER 1
#define DELAY_ON 1
#define DELAY_OFF 0

#define ROWS 25 // playwin (WIN) parameters
#define COLS 110
#define OFFY 4
#define OFFX 8

#define RA(min, max) ((min) + rand() % ((max) - (min) + 1)) // random number between min and max (inc)

typedef struct
{
    WINDOW *window; // ncurses window
    int x, y;
    int rows, cols;
    int color;
} WIN; // my window

typedef struct
{
    WIN *win;         
    int color;       //type also
    int mv;           // f
    int x, y;        
    int width, height; 
    int og_width; 
    int xmin, xmax;      
    int turn;         // w lewo: -1, w prawo: 1
    char **shape;     // kształt obiektu (dwuwymiarowa tablica znaków, która reprezentuje obraz obiektu)
} CAR;

typedef struct
{
    WIN *win;         
    int color;        
    int mv;           
    int x, y;         
    int width, height; 
    int xmin, xmax;   
    int ymin, ymax;  
    char **shape;     
} FROG;

typedef struct
{
    unsigned int frame_time;
    float pass_time;
    int frame_no;
} TIMER;

//------------------------------------------------
//----------------  WINDOW FUNCTIONS -------------
//------------------------------------------------

WINDOW *Start()
{
    WINDOW *win;

    if ((win = initscr()) == NULL)
    { // initialize ncurses
        fprintf(stderr, "Error initialising ncurses.\n");
        exit(EXIT_FAILURE);
    }

    start_color(); // initialize colors
    init_pair(MAIN_COLOR, COLOR_WHITE, COLOR_BLACK);
    init_pair(PLAY_COLOR, COLOR_BLUE, COLOR_WHITE);
    init_pair(STAT_COLOR, COLOR_WHITE, COLOR_BLUE);
    init_pair(FROG_COLOR_N, COLOR_GREEN, COLOR_WHITE);
    init_pair(CAR_W_COLOR, COLOR_RED, COLOR_WHITE);
    init_pair(CAR_B_COLOR, COLOR_CYAN, COLOR_WHITE);

    noecho(); // Switch off echoing, turn off cursor
    curs_set(0);
    return win;
}

void Welcome(WINDOW *win) // Welcome screen (optional): press any key to continue
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
void CleanWin(WIN *W, int bo) // bo(rder): 0 | 1
{
    int i, j;
    wattron(W->window, COLOR_PAIR(W->color));
    if (bo)
        box(W->window, 0, 0);
    for (i = bo; i < W->rows - bo; i++)
        for (j = bo; j < W->cols - bo; j++)
            mvwprintw(W->window, i, j, " ");
}

// window initialization: position, colors, border, etc
WIN *Init(WINDOW *parent, int rows, int cols, int y, int x, int color, int bo, int delay)
{
    // WIN* W = new WIN; // C++ version; compile with g++
    WIN *W = (WIN *)malloc(sizeof(WIN)); // C version; compile with gcc
    W->x = x;
    W->y = y;
    W->rows = rows;
    W->cols = cols;
    W->color = color;
    W->window = subwin(parent, rows, cols, y, x);
    CleanWin(W, bo);
    if (delay == DELAY_OFF)
        nodelay(W->window, TRUE); // non-blocking reading of characters (for real-time game)
    wrefresh(W->window);
    return W;
}

void EndGame(const char *info, WIN *W) // sth at the end
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

void ShowStatus(WIN *W, FROG *f, int pts)
{
    mvwprintw(W->window, 1, 45, "x: %d  y: %d  ", f->x, f->y);
    mvwprintw(W->window, 1, 25, "%d", pts);
    wrefresh(W->window);
}

void ShowTimer(WIN *W, float pass_time)
{
    mvwprintw(W->window, 1, 9, "%.2f", pass_time);
    wrefresh(W->window);
}

void ShowNewStatus(WIN *W, TIMER *T, FROG *f, int pts)
{
    box(W->window, 0, 0); // border
    mvwprintw(W->window, 1, 3, "Time: ");
    mvwprintw(W->window, 1, 17, "Points: ");
    ShowTimer(W, T->pass_time);
    mvwprintw(W->window, 1, 35, "Position: ");
    mvwprintw(W->window, 1, 78, ".J.U.M.P.I.N.G...F.R.O.G.");
    ShowStatus(W, f, pts);
}


void PrintFrog(FROG *f)
{
    for (int i = 0; i < f->height; i++)
    {
        for (int j = 0; j < f->width; j++)
        {
            mvwprintw(f->win->window, f->y + i, f->x + j, "%c", f->shape[i][j]);
        }
    }
}

void PrintCar(CAR *c)
{
    for (int i = 0; i < c->height; i++)
    {
        for (int j = 0; j < c->width; j++)
        {
            mvwprintw(c->win->window, c->y + i, c->x + j, "%c", c->shape[i][j]);
        }
    }
}

// common function for both: Catcher and Ball
void ShowCar(CAR *c, int dx)
{

    char *sw = (char *)malloc(sizeof(char) * c->width);
    memset(sw, ' ', c->width);


    wattron(c->win->window, COLOR_PAIR(c->color));

    if ((dx == 1) && ((c->x + c->width < c->xmax) || c->width != c->og_width))
    { // by szlo na druga strone noramlnie
        for (int i = 0; i < c->height; i++)
        {
            mvwprintw(c->win->window, c->y + i, c->x, " ");
        }
        c->x += dx;
    }

    if ((dx == -1) && (c->x > c->xmin))
    {
        c->x += dx;
        for (int i = 0; i < c->height; i++)
            mvwprintw(c->win->window, c->y + i, c->x + c->width, " ");
    }

    PrintCar(c);
    box(c->win->window, 0, 0); // obramowanie
    wrefresh(c->win->window);
}

void ShowFrog(FROG *f, int dx, int dy)
{

    char *sw = (char *)malloc(sizeof(char) * f->width);
    memset(sw, ' ', f->width);

    wattron(f->win->window, COLOR_PAIR(f->color));

    if ((dy == 1) && (f->y + f->height < f->ymax))
    {
        f->y += dy;
        mvwprintw(f->win->window, f->y - 1, f->x, sw);
    }

    if ((dy == -1) && (f->y > f->ymin))
    {
        f->y += dy;
        mvwprintw(f->win->window, f->y + f->height, f->x, sw);
    }

    if ((dx == 1) && (f->x + f->width < f->xmax))
    { 
        for (int i = 0; i < f->height; i++)
        {
            mvwprintw(f->win->window, f->y + i, f->x, " ");
        }
        f->x += dx;
    }

    if ((dx == -1) && (f->x > f->xmin))
    {
        f->x += dx;
        for (int i = 0; i < f->height; i++)
            mvwprintw(f->win->window, f->y + i, f->x + f->width, " ");
    }

    PrintFrog(f);
    box(f->win->window, 0, 0); // obramowanie
    wrefresh(f->win->window);
}

FROG *InitFrog(WIN *w, int col)
{
    FROG *frog = (FROG *)malloc(sizeof(FROG)); // C
    frog->color = col;
    frog->win = w;
    frog->width = 3;
    frog->height = 2;
    frog->mv = 0;
    frog->x = (frog->win->cols - frog->width) / 2 ;
    frog->y = (frog->win->rows - frog->height - 1);

    frog->shape = (char **)malloc(sizeof(char *) * frog->height); // array of pointers (char*)
    for (int i = 0; i < frog->height; i++)
        frog->shape[i] = (char *)malloc(sizeof(char) * (frog->width + 1)); // +1: end-of-string (C): '\0'

    strcpy(frog->shape[0], "*.*");
    strcpy(frog->shape[1], "***");

    frog->xmin = 1;
    frog->xmax = w->cols - 1;
    frog->ymin = 1;
    frog->ymax = w->rows - 1;
    return frog;
}

CAR *InitCar(WIN *w, int type, int x0, int y0)
{
    CAR *car = (CAR *)malloc(sizeof(CAR)); // C
    car->x = x0;
    car->y = y0;
    car->color = type;
    car->win = w;
    car->width = 10;
    car->height = 2;
    car->og_width = 10;
    car->mv = MVB_FACTOR;
    car->turn = 1; //right - default

    car->shape = (char **)malloc(sizeof(char *) * car->height); // array of pointers (char*)
    for (int i = 0; i < car->height; i++)
        car->shape[i] = (char *)malloc(sizeof(char) * (car->width + 1)); // +1: end-of-string (C): '\0'

    strcpy(car->shape[0], "xxxxxxxxxx");
    strcpy(car->shape[1], "xxxxxxxxxx");

    car->xmin = 1;
    car->xmax = w->cols - 1;
    return car;
}

void MoveFrog(FROG *f, int ch, unsigned int frame)
{
    if (frame - f->mv >= MVC_FACTOR)
    {
        switch (ch)
        {
        case 'w':
            ShowFrog(f, 0, -1);
            break;
        case 's':
            ShowFrog(f, 0, 1);
            break;
        case 'a':
            ShowFrog(f, -1, 0);
            break;
        case 'd':
            ShowFrog(f, 1, 0);
            break;
        }
        f->mv = frame;
    }
}

void MoveWrapperCar(CAR *c, int frame, int* dx)
{
    if (frame % c->mv == 0)
    {
        if (c->x + c->og_width >= (c->xmax))
        {
            c->width--;
        }
        if (c->width == 0)
        {
            for (int i = 0; i < c->height; i++)
            {
                mvwprintw(c->win->window, c->y + i, c->x + c->width, " ");
            } // usuniecie pozostalosci
            c->x = 1;     // Wróć na początek
            c->width = 1; // Wracaj do dlugosci
        }
        else if (c->width < c->og_width && c->x < (c->xmax) - (c->og_width))
        {
            c->x = 1;
            c->width++;
        }
        else
        {
            *dx = 1;
        }
    }
}

void MoveBouncingCar(CAR *c, int frame, int* dx)
{
    if (frame % c->mv == 0)
    {
        if (c->x + c->og_width >= (c->xmax) || c->x == c->xmin)
        {
            c->turn=(c->turn)*(-1);
        }
    *dx = c->turn;
    }

}

void MoveCar(CAR * c, int frame)
{
    int dx = 0;

        switch (c->color)
        {
        case CAR_W_COLOR:
        {
            MoveWrapperCar(c, frame, &dx);
            break;
        }
        case CAR_B_COLOR:
        {
            MoveBouncingCar(c, frame, &dx);
            break;
        }
        }

        ShowCar(c, dx);
}


int Collision_F_C(FROG *b,CAR *c) 
{
    if (((c->y >= b->y && c->y < b->y + b->height) || (b->y >= c->y && b->y < c->y + c->height)) &&
        ((c->x >= b->x && c->x < b->x + b->width) || (b->x >= c->x && b->x < c->x + c->width)))
        return 1;
    else
        return 0;
}

//------------------------------------------------
//----------------  TIMER FUNCTIONS --------------
//------------------------------------------------

void Sleep(unsigned int tui) { usleep(tui * 1000); } // micro_sec = frame_time * 1000

TIMER *InitTimer(WIN *status)
{
    TIMER *timer = (TIMER *)malloc(sizeof(TIMER));
    timer->frame_no = 1;
    timer->frame_time = FRAME_TIME;
    timer->pass_time = PASS_TIME / 1.0;
    return timer;
}

int UpdateTimer(TIMER *T, WIN *status) // return 1: time is over; otherwise: 0
{
    T->frame_no++;
    T->pass_time = PASS_TIME - (T->frame_no * T->frame_time / 1000.0);
    if (T->pass_time < (T->frame_time / 1000.0))
        T->pass_time = 0; // make this zero (floating point!)
    else
        Sleep(T->frame_time);
    ShowTimer(status, T->pass_time);
    if (T->pass_time == 0)
        return 1;
    return 0;
}

void GameOver(FROG *f) {
    // clr
    for (int i = 0; i < f->height; i++) {
        for (int j = 0; j < f->width; j++) {
            mvwprintw(f->win->window, f->y + i, f->x + j, " ");
        }
    }
    f->x = (f->win->cols - f->width) / 2;
    f->y = (f->win->rows - f->height - 1);

    wattron(f->win->window, COLOR_PAIR(f->color));
    PrintFrog(f);
    box(f->win->window, 0, 0);
    wrefresh(f->win->window);  
}


int MainLoop(WIN *status, FROG *frog, CAR *cars[], TIMER *timer) // 1: timer is over, 0: quit the game
{
    int key;
    int pts = 3;
    while ((key = wgetch(status->window)) != QUIT) // NON-BLOCKING! (nodelay=TRUE)
    {
        if (key == ERR)
            key = NOKEY; 
        else
        {
         MoveFrog(frog, key, timer->frame_no);
        }
        for (int i = 0; i < CARS_NUMBER; i++)
        {
            if (cars[i] != NULL)
            {
                MoveCar(cars[i], timer->frame_no);
                if (Collision_F_C(frog, cars[i]))
                {
                    pts--;
                 //   if(pts==0) 
                  //  else{
                        GameOver(frog);
                  //  }
                }
            }
        }
        ShowStatus(status, frog, pts);
        flushinp(); // clear input buffer (avoiding multiple key pressed)
        /* update timer */
        if (UpdateTimer(timer, status))
            return pts; // sleep inside
    }
    return 0;
}

//------------------------------------------------
//----------------  MAIN FUNCTION ----------------
//------------------------------------------------

int main()
{
    WINDOW *mainwin = Start();

    Welcome(mainwin);

    WIN *playwin = Init(mainwin, ROWS, COLS, OFFY, OFFX, PLAY_COLOR, BORDER, DELAY_ON);      // window for the playing area
    WIN *statwin = Init(mainwin, 3, COLS, ROWS + OFFY, OFFX, STAT_COLOR, BORDER, DELAY_OFF); // window for the status
    // DELAY_OFF = real-time game

    TIMER *timer = InitTimer(statwin);
    
    FROG *frog = InitFrog(playwin, FROG_COLOR_N);
    CAR *cars[CARS_NUMBER] = {NULL};
    cars[0] = InitCar(playwin, CAR_W_COLOR, 1, 10); // wrapper car
    cars[1] = InitCar(playwin, CAR_W_COLOR, 30, 10); // wrapper car
    cars[2] = InitCar(playwin, CAR_B_COLOR, 1, 14); // bouncing car
    cars[3] = InitCar(playwin, CAR_B_COLOR, 20, 7); // bouncing car
    cars[4] = InitCar(playwin, CAR_W_COLOR, 40, 4); // wrapper car

    ShowNewStatus(statwin, timer, frog, 0);
    ShowFrog(frog, 0, 0);
    for (int i = 0; i < CARS_NUMBER; i++)
    {
        if (cars[i] != NULL)
            ShowCar(cars[i], 0);
    }

    int result;
    if ((result = MainLoop(statwin, frog, cars, timer)) == 0)
        EndGame("You have decided to quit the game.", statwin);
    else
    {
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
