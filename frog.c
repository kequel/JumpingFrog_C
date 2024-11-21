// gcc -o frog frog.c -lncurses
//./frog
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <time.h>

#define QUIT_TIME 3 // .. seconds to quit
#define QUIT 'q'
#define NOKEY ' '

#define CARS_NUMBER 11
#define OBSTACLES_NUMBER 5
#define JUMPSIZE 2

#define BET_TIME 2    // 2s (sleep time between two catches)
#define FRAME_TIME 25 // 25 ms (base frame time) (time interval between frames)
#define PASS_TIME 200 // 25 s (time to die = no catched ball)
#define MVF_FACTOR 5  // moving interval >= FRAME_TIME * MVC_FACTOR [ms] CATCHER

#define MAIN_COLOR 1
#define STAT_COLOR 2
#define PLAY_COLOR 3
#define FROG_COLOR 4
#define CAR_W_COLOR 5
#define CAR_B_COLOR 6
#define CAR_S_COLOR 7
#define CAR_T_COLOR 8
#define CAR_G_COLOR 9

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
    int mv;           // czestotliwosc
    float acceleration; ///przyspieszenie
    int x, y;        
    int width, height; 
    int og_width; 
    int xmin, xmax;      
    int turn;         // w lewo: -1, w prawo: 1
    int ordered;    //0 - no, 1-is ordered 2- is going
    int random; //0-not random 1-first round 2-last
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
    bool calls;
    char **shape;     
} FROG;

typedef struct
{
    WIN *win;   
    int color;       
    int x, y;        
    int width, height;    
    char **shape;
} OBSTACLE;

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
    init_pair(STAT_COLOR, COLOR_WHITE, COLOR_BLUE);
    init_pair(PLAY_COLOR, COLOR_BLUE, COLOR_WHITE);
    init_pair(FROG_COLOR, COLOR_GREEN, COLOR_WHITE);
    init_pair(CAR_W_COLOR, COLOR_RED, COLOR_WHITE);
    init_pair(CAR_B_COLOR, COLOR_MAGENTA, COLOR_WHITE);
    init_pair(CAR_S_COLOR, COLOR_CYAN, COLOR_WHITE);
    init_pair(CAR_T_COLOR, COLOR_YELLOW, COLOR_WHITE);
    init_pair(CAR_G_COLOR, COLOR_WHITE, COLOR_WHITE);

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

void ShowStatus(WIN *W, FROG *f, int pts, CAR* cars[])
{
    mvwprintw(W->window, 1, 45, "x: %d  y: %d  ", f->x, f->y);
    mvwprintw(W->window, 1, 60, "xc: %d  yc: %d  ", cars[8]->x, cars[8]->y);
    mvwprintw(W->window, 1, 25, "%d", pts);
    wrefresh(W->window);
}

void ShowTimer(WIN *W, float pass_time)
{
    mvwprintw(W->window, 1, 9, "%.2f", pass_time);
    wrefresh(W->window);
}

void ShowNewStatus(WIN *W, TIMER *T, FROG *f, int pts, CAR* cars[])
{
    box(W->window, 0, 0); // border
    mvwprintw(W->window, 1, 3, "Time: ");
    mvwprintw(W->window, 1, 17, "Lives: ");
    ShowTimer(W, T->pass_time);
    mvwprintw(W->window, 1, 35, "Position: ");
    mvwprintw(W->window, 1, 78, "Karolina Glaza 198193");
    ShowStatus(W, f, pts, cars);
}


void PrintFrog(FROG *f)
{
    wattron(f->win->window, COLOR_PAIR(f->color));
    for (int i = 0; i < f->height; i++)
    {
        for (int j = 0; j < f->width; j++)
        {
            mvwprintw(f->win->window, f->y + i, f->x + j, "%c", f->shape[i][j]);
        }
    }
}

void DeleteFrog(FROG *f)
{
    for (int i = 0; i < f->height; i++)
    {
        for (int j = 0; j < f->width; j++)
        {
            mvwprintw(f->win->window, f->y + i, f->x + j, "%c", ' ');
        }
    }
}

void PrintCar(CAR *c)
{
    if(c->color!=CAR_G_COLOR){
    for (int i = 0; i < c->height; i++)
    {
        for (int j = 0; j < c->width; j++)
        {
            mvwprintw(c->win->window, c->y + i, c->x + j, "%c", c->shape[i][j]);
        }
    }
    }
    // else DeleteCar(c);
}

void DeleteCar(CAR *c)
{
    for (int i = 0; i < c->height; i++)
    {
        for (int j = 0; j < c->width; j++)
        {
            mvwprintw(c->win->window, c->y + i, c->x + j, "%c", ' ');
        }
    }
}

void PrintObstacle(OBSTACLE *o)
{
    wattron(o->win->window, COLOR_PAIR(o->color));
    for (int i = 0; i < o->height; i++)
    {
        for (int j = 0; j < o->width; j++)
        {
            mvwprintw(o->win->window, o->y + i, o->x + j, "%c", o->shape[i][j]);
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

    wattron(c->win->window, COLOR_PAIR(PLAY_COLOR));
    box(c->win->window, 0, 0); // obramowanie
    wrefresh(c->win->window);
}

int Collision_O(int y,int x, int w, int h,OBSTACLE *c) 
{
    if (((c->y >= y && c->y < y + h) || (y >= c->y && y < c->y + c->height)) &&
        ((c->x >= x && c->x < x + w) || (x >= c->x && x < c->x + c->width)))
        return 1;
    else
        return 0;
}

int Collision_F_anyO(FROG *f, int dx, int dy,OBSTACLE* obstacles[]) 
{
    for(int i=0; i<OBSTACLES_NUMBER; i++){
    if(Collision_O(f->y+dy*JUMPSIZE, f->x+dx*JUMPSIZE, f->width, f->height, obstacles[i])==1) return 1;
    }
    return 0;
}

int Collision_C_anyO(CAR *c, int dx, int dy,OBSTACLE* obstacles[]) 
{
    for(int i=0; i<OBSTACLES_NUMBER; i++){
    if(Collision_O(c->y+dy, c->x+dx, c->width, c->height, obstacles[i])==1) return 1;
    }
    return 0;
}


void ShowFrog(FROG *f, int dx, int dy, OBSTACLE* obstacles[])
{

    char *sw = (char *)malloc(sizeof(char) * f->width);
    memset(sw, ' ', f->width);

    wattron(f->win->window, COLOR_PAIR(f->color));

    DeleteFrog(f);

    if(Collision_F_anyO(f,dx,dy,obstacles)==0){
        if ((dy == 1) && (f->y + f->height < f->ymax)) f->y += dy*JUMPSIZE;
        if ((dy == -1) && (f->y > f->ymin)) f->y += dy*JUMPSIZE;
        if ((dx == 1) && (f->x + f->width < f->xmax))  f->x += dx*JUMPSIZE;
        if ((dx == -1) && (f->x > f->xmin)) f->x += dx*JUMPSIZE;
    }

    PrintFrog(f);
    box(f->win->window, 0, 0); // obramowanie
    wrefresh(f->win->window);
}

void orderTaxi(FROG *f, OBSTACLE* obstacles[], CAR *cars[])
{
    int bestCarNum=-1;
    for(int i=0; i<CARS_NUMBER; i++){
        if(cars[i]!=NULL &&((f->y)-(cars[i]->y))==JUMPSIZE && cars[i]->color==CAR_T_COLOR && (f->x)+3>cars[i]->x){
            if(bestCarNum==-1){
                bestCarNum=i;
            } 
            else if((f->x)-cars[bestCarNum]->x>(f->x)-cars[i]->x){
                bestCarNum=i;
            }
    }
    if(bestCarNum>=0){
        cars[bestCarNum]->ordered=1;
    }
}
}

FROG *InitFrog(WIN *w, int col)
{
    FROG *frog = (FROG *)malloc(sizeof(FROG)); // C
    frog->color = col;
    frog->win = w;
    frog->width = 3;
    frog->height = JUMPSIZE;
    frog->mv = 0;
    frog->x = (frog->win->cols - frog->width) / 2 ;
    frog->y = (frog->win->rows - frog->height - 1);
    frog->calls = false;

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

CAR *InitCar(WIN *w, int type, int x0, int y0, int speed, float a, int ran)
{
    CAR *car = (CAR *)malloc(sizeof(CAR)); // C
    car->x = x0;
    car->y = y0;
    car->color = type;
    car->win = w;
    car->width = 9;
    car->height = JUMPSIZE;
    car->og_width = 9;
    car->mv = speed;
    car->acceleration = a;
    car->turn = 1; //right - default
    car->ordered = 0;
    car->random = ran;

    car->shape = (char **)malloc(sizeof(char *) * car->height); // array of pointers (char*)
    for (int i = 0; i < car->height; i++)
        car->shape[i] = (char *)malloc(sizeof(char) * (car->width + 1)); // +1: end-of-string (C): '\0'

    for(int i=0; i<JUMPSIZE; i++){
    strcpy(car->shape[i], "xxxxxxxxx");
    }

    car->xmin = 1;
    car->xmax = w->cols - 1;
    return car;
}

CAR *InitRandomCar(int carnum,WIN *w, int y0)
{
    srand(time(NULL)*carnum);
    //type (color) - 5-9
    int random_color = rand() % 5 + 5; 
    //speed - 1-5
    int random_speed = rand() % 5 + 1;
    //acceleration - (-0.1)-0.1
    int random_a = ((float)rand() / RAND_MAX) * 0.2 - 0.1;
    return InitCar(w, random_color, 1, y0, random_speed, random_a, 1);
}

OBSTACLE *InitObstacle(WIN *w, int col, int x0, int y0, int width)
{
    OBSTACLE *obs = (OBSTACLE *)malloc(sizeof(OBSTACLE)); // C
    obs->x = x0;
    obs->y = y0;
    obs->color = col;
    obs->win = w;
    obs->width = width;
    obs->height = JUMPSIZE;

    obs->shape = (char **)malloc(sizeof(char *) * obs->height); // array of pointers (char*)
    for (int i = 0; i < obs->height; i++)
        obs->shape[i] = (char *)malloc(sizeof(char) * (obs->width + 1)); // +1: end-of-string (C): '\0'

    for (int i = 0; i < obs->height; i++) {
        for (int j = 0; j < obs->width; j++) {
            obs->shape[i][j] = '|';
        }
    }


    return obs;
}


void MoveFrog(FROG *f, int ch, unsigned int frame, OBSTACLE* obstacles[], CAR *cars[])
{
    if (frame - f->mv >= MVF_FACTOR)
    {
        switch (ch)
        {
        case 'w':
            f->calls=false;
            ShowFrog(f, 0, -1, obstacles);
            break;
        case 's':
            f->calls=false;
            ShowFrog(f, 0, 1, obstacles);
            break;
        case 'a':
            f->calls=false;
            ShowFrog(f, -1, 0, obstacles);
            break;
        case 'd':
            f->calls=false;
            ShowFrog(f, 1, 0, obstacles);
            break;
        case 't':
            f->calls=true;
            orderTaxi(f, obstacles, cars);
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

void MoveTaxiCar(CAR *c, int frame, int* dx, FROG *f)
{
    if(c->ordered==0){ //nie zamowiona taxi
        MoveWrapperCar(c, frame, dx);
    } 
    else if(c->ordered==1){ //zamowiona taxi (jedzie)
    if (f->x != c->x + 3) {
        MoveWrapperCar(c, frame, dx);
    }else {
        c->ordered = 2;  
    }       
    }
    else if(c->ordered==2){ //taxi dotarla do zaby
        DeleteFrog(f);
        strcpy(c->shape[0], "xxx*.*xxx");
        strcpy(c->shape[1], "xxx***xxx");
        MoveWrapperCar(c, frame, dx);
        c->ordered=3;
    }
    else if(c->ordered==3 && f->calls==true){ //taxi jedzie z zaba
        DeleteFrog(f);
        MoveWrapperCar(c, frame, dx);
    }
    else { //zaba chce wysiasc
        DeleteFrog(f);
        strcpy(c->shape[0], "xxxxxxxxx");
        strcpy(c->shape[1], "xxxxxxxxx");
        f->x=(c->x)+3;
        f->y=(c->y)-JUMPSIZE;
        PrintFrog(f);
        MoveWrapperCar(c, frame, dx);
        c->ordered=0;
    }
    wrefresh(f->win->window);  
}

void MoveBouncingCar(CAR *c, int frame, int* dx, OBSTACLE* obstacles[])
{
    if (frame % c->mv == 0)
    {
        if (c->x + c->og_width >= (c->xmax) || c->x == c->xmin || Collision_C_anyO(c,(c->turn),0,obstacles)==1)
        {
            c->turn=(c->turn)*(-1);
        }
    *dx = c->turn;
    }

}

void MoveStoppingCar(CAR* c, int frame, int* dx,  OBSTACLE* obstacles[], FROG* f){
    //it is bouncing car
        if (frame % c->mv == 0)
    {
        if ((c->turn==-1 && f->x - c->x == -4 && abs(f->y - c->y) <= JUMPSIZE) || (c->turn==1 && (c->x+c->width-3)-f->x == -4 && abs(f->y - c->y) <= JUMPSIZE)) {
            *dx=0;
            return;
        }
        if (c->x + c->og_width >= (c->xmax) || c->x == c->xmin || Collision_C_anyO(c,(c->turn),0,obstacles)==1)
        {
            c->turn=(c->turn)*(-1);
        }
    *dx = c->turn;
    }
}



void MoveCar(int i, int frame, OBSTACLE* obstacles[], FROG *f, CAR *cars[])
{
    int dx = 0;

    if (frame % 500 == 0 && cars[i]->acceleration > 0) { //0-brak, +0.x-przyspiesza, -0.x zwalnia
        cars[i]->mv = (int)(cars[i]->mv * (1 - cars[i]->acceleration)); 
        if (cars[i]->mv < 1) cars[i]->mv = 1; 
    }

    if(cars[i]->random==1 || cars[i]->random==2){
        if (frame % cars[i]->mv == 0 && cars[i]->x + cars[i]->og_width >= (cars[i]->xmax)){
            cars[i]->random++;
        }
    }
    else if(cars[i]->random==3){
        DeleteCar(cars[i]);
        cars[i]=InitRandomCar(i,cars[i]->win, cars[i]->y);
    }
        switch (cars[i]->color)
        {
        case CAR_W_COLOR:
        {
            MoveWrapperCar(cars[i], frame, &dx);
            break;
        }
        case CAR_B_COLOR:
        {
            MoveBouncingCar(cars[i], frame, &dx, obstacles);
            break;
        }
        case CAR_S_COLOR:
        {
            MoveStoppingCar(cars[i], frame, &dx, obstacles, f);
            break;
        }
        case CAR_T_COLOR:
        {
            MoveTaxiCar(cars[i], frame, &dx, f);
            break;
        }
        case CAR_G_COLOR:
        {
            MoveWrapperCar(cars[i], frame, &dx);
            break;
        }
        }

        ShowCar(cars[i], dx);
        PrintFrog(f);
}


int Collision_F_C(FROG *b,CAR *c) 
{
    if(c->color==CAR_G_COLOR) return 0;
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
    wrefresh(f->win->window);  
}

void ShowGoal(WIN *playwin) {
    wattron(playwin->window, COLOR_PAIR(COLOR_BLACK));
    for (int j = 1; j < playwin->cols - 1; j += 12) {
        mvwprintw(playwin->window, 1, j, "-NEXT-LEVEL-");
    }
    for (int i = 2; i < 2+JUMPSIZE; i++) { 
        for (int j = 1; j < playwin->cols - 1; j++) {
            mvwprintw(playwin->window, i, j, "|");
        }
    }
    wrefresh(playwin->window);
}



int MainLoop(WIN *status, FROG *frog, CAR *cars[], OBSTACLE *obstacles[], TIMER *timer) 
{
    int key;
    int pts = 3;
    while ((key = wgetch(status->window)) != QUIT && pts>0) 
    {
        if (key == ERR)
            key = NOKEY; 
        else
        {
         MoveFrog(frog, key, timer->frame_no, obstacles, cars);
         if(frog->y==2) return 1;
        }
        for (int i = 0; i < CARS_NUMBER; i++)
        {
            if (cars[i] != NULL)
            {
                MoveCar(i, timer->frame_no, obstacles, frog, cars);
                if (Collision_F_C(frog, cars[i]))
                {
                    pts--;
                    GameOver(frog);
                    if(pts<=0) return 2;
                }
            }
        }
        ShowStatus(status, frog, pts, cars);
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

void initCars(WIN* playwin, CAR* cars[]){
    int v1=5;
    int v2=3;
    int v3=2;
    cars[0] = InitCar(playwin, CAR_W_COLOR, 1, 18, v1, 0,0); // wrapper car
    cars[1] = InitCar(playwin, CAR_W_COLOR, 30, 18, v1,0,0); // wrapper car
    cars[2] = InitCar(playwin, CAR_W_COLOR, 70, 18, v1,0,0); // wrapper car
    cars[3] = InitCar(playwin, CAR_W_COLOR, 90, 18, v1,0,0); // wrapper car
    cars[4] = InitCar(playwin, CAR_B_COLOR, 1, 10, v2,0,0); // bouncing car
    cars[5] = InitCar(playwin, CAR_B_COLOR, 1, 12, v3,0.05,0); // bouncing car
    cars[6] = InitCar(playwin, CAR_B_COLOR, 85, 12, v3, 0,0); // bouncing car
    cars[7] = InitCar(playwin, CAR_S_COLOR, 1, 16, v1, 0,0); //stoppingcar
    cars[8] = InitCar(playwin, CAR_T_COLOR, 1, 20, v3, 0,0); //taxicar
    cars[9] = InitRandomCar(9, playwin, 8);
    cars[10] = InitRandomCar(8, playwin, 4);
}

void InitObstacles(WIN* playwin, OBSTACLE* obstacles[]){
    obstacles[0] = InitObstacle(playwin, PLAY_COLOR, 53, 12, 5);
    obstacles[1] = InitObstacle(playwin, PLAY_COLOR, 12, 14, 4);
    obstacles[2] = InitObstacle(playwin, PLAY_COLOR, 37, 14, 5);
    obstacles[3] = InitObstacle(playwin, PLAY_COLOR, 62, 14, 4);
    obstacles[4] = InitObstacle(playwin, PLAY_COLOR, 87, 14, 5);
}

int main()
{
    WINDOW *mainwin = Start();

    Welcome(mainwin);

    WIN *playwin = Init(mainwin, ROWS, COLS, OFFY, OFFX, PLAY_COLOR, BORDER, DELAY_ON);      // window for the playing area
    WIN *statwin = Init(mainwin, 3, COLS, ROWS + OFFY, OFFX, STAT_COLOR, BORDER, DELAY_OFF); // window for the status
    // DELAY_OFF = real-time game
    ShowGoal(playwin);

    TIMER *timer = InitTimer(statwin);
    
    FROG *frog = InitFrog(playwin, FROG_COLOR);

    CAR *cars[CARS_NUMBER] = {NULL};

    OBSTACLE *obstacles[OBSTACLES_NUMBER] = {NULL};

    initCars(playwin, cars);
    InitObstacles(playwin, obstacles);

    ShowNewStatus(statwin, timer, frog, 0, cars);
    ShowFrog(frog, 0, 0, obstacles);
    for (int i = 0; i < CARS_NUMBER; i++)
    {
        if (cars[i] != NULL)
            ShowCar(cars[i], 0);
    }

    for (int i = 0; i < OBSTACLES_NUMBER; i++)
    {
        if (obstacles[i] != NULL)
            PrintObstacle(obstacles[i]);
    }

    int result = MainLoop(statwin, frog, cars, obstacles, timer);
    if (result == 0)
        EndGame("End. 'Q' was pressed.", statwin);
    else if(result==1){
        //new level
        EndGame("You won.", statwin);
    }
    else if(result == 2)
        EndGame("End. No more lives.", statwin);
    else
    {
        char info[100];
        sprintf(info, " Timer is over");
        EndGame(info, statwin);
    }

    delwin(playwin->window); // Clean up (!)
    delwin(statwin->window);
    delwin(mainwin);
    endwin();
    refresh();
    return EXIT_SUCCESS;
}
