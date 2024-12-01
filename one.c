// gcc -o one one.c -lncurses
//./one
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ncurses.h>
#include <time.h>
#include <string.h>


#define QUIT 'q'
#define NOKEY ' '

#define CARS_NUMBER 11
#define OBSTACLES_NUMBER 5

#define MAIN_COLOR 1
#define STAT_COLOR 2
#define PLAY_COLOR 3
#define FROG_COLOR 4
#define CAR_W_COLOR 5
#define CAR_B_COLOR 6
#define CAR_S_COLOR 7
#define CAR_T_COLOR 8
#define CAR_G_COLOR 9
#define STORK_COLOR 10

#define MAX_CAR_SPEED 1
#define MIN_CAR_SPEED 5

//const from file (universal game parameters)
int QUIT_TIME; // seconds to quit
int FRAME_TIME; // ms (base frame time) (time interval between frames)
int PASS_TIME; // s (time to die = no catched ball)
int MVF_FACTOR; // moving interval >= FRAME_TIME * MVC_FACTOR [ms] FROG
int ROWS, COLS, OFFY, OFFX, JUMPSIZE;

typedef struct{
    WINDOW *window;
    int x, y;
    int rows, cols;
    int color;
} WIN;

typedef struct{
    WIN *win;
    int color; // type(CAR_W_COLOR, CAR_B_COLOR, CAR_S_COLOR, CAR_T_COLOR, CAR_G_COLOR)
    int mv; //speed
    float acceleration;
    int x, y;
    int width, height;
    int og_width;
    int xmin, xmax;
    int turn; //left: -1, right: 1
    int ordered; //not ordered: 0, ordered(is going): 1, came to frog: 2, picked frog: 3
    int random; //not random: 0, counter:1/2
    char **shape;
} CAR;

typedef struct{
    WIN *win;
    int color;
    int mv;
    int x, y;
    int width, height;
    int og_width;
    int xmin, xmax;
    int ymin, ymax;
    char **shape;
} STORK;

typedef struct{
    WIN *win;
    int color;
    int mv;
    int x, y;
    int width, height;
    int xmin, xmax;
    int ymin, ymax;
    bool calls; //wants a taxi: true, does not want: false
    char **shape;
} FROG;

typedef struct{
    WIN *win;
    int x, y;
    int width, height;
    char **shape;
} OBSTACLE;

typedef struct{
    unsigned int frame_time;
    float pass_time;
    int frame_no;
} TIMER;

WINDOW *Start(){
    WINDOW *win;
    if ((win = initscr()) == NULL){
        fprintf(stderr, "Error initialising ncurses.\n");
        exit(EXIT_FAILURE);
    }
    start_color();
    init_pair(MAIN_COLOR, COLOR_WHITE, COLOR_BLACK);
    init_pair(STAT_COLOR, COLOR_WHITE, COLOR_BLUE);
    init_pair(PLAY_COLOR, COLOR_BLUE, COLOR_WHITE);
    init_pair(FROG_COLOR, COLOR_GREEN, COLOR_WHITE);
    init_pair(CAR_W_COLOR, COLOR_RED, COLOR_WHITE);
    init_pair(CAR_B_COLOR, COLOR_MAGENTA, COLOR_WHITE);
    init_pair(CAR_S_COLOR, COLOR_CYAN, COLOR_WHITE);
    init_pair(CAR_T_COLOR, COLOR_YELLOW, COLOR_WHITE);
    init_pair(CAR_G_COLOR, COLOR_WHITE, COLOR_WHITE);
    init_pair(STORK_COLOR, COLOR_BLACK, COLOR_WHITE);
    noecho();
    curs_set(0);
    return win;
}

void Welcome(WINDOW *win) { // Welcome screen (optional): press any key to continue
    mvwaddstr(win, 1, 1, "Do you want to play a game?");
    mvwaddstr(win, 2, 1, "Press any key to continue..");
    wgetch(win); // wait
    wclear(win); // clear (after next refresh)
    wrefresh(win);
}


void CleanWin(WIN *W) {// cleaning window: writing " "
    wattron(W->window, COLOR_PAIR(W->color));
    box(W->window, 0, 0);
    for (int i = 1; i < W->rows - 1; i++){
        for (int j = 1; j < W->cols - 1; j++) {
            mvwprintw(W->window, i, j, " ");
        }
    }

}

WIN *Init(WINDOW *parent, int rows, int cols, int y, int x, int color, int delay) { // window initialization: position, colors etc
    WIN *W = (WIN *)malloc(sizeof(WIN));
    W->x = x;
    W->y = y;
    W->rows = rows;
    W->cols = cols;
    W->color = color;
    W->window = subwin(parent, rows, cols, y, x);
    CleanWin(W);
    if (delay == 0){
        nodelay(W->window, TRUE); // non-blocking reading of characters (for real-time game)
    }
    wrefresh(W->window);
    return W;
}

void EndGame(const char *info, WIN *W){
    CleanWin(W);
    for (int i = QUIT_TIME; i > 0; i--){
        mvwprintw(W->window, 1, 2, "%s Closing the game in %d seconds...", info, i);
        wrefresh(W->window);
        sleep(1);
    }
}

void ShowStatus(WIN *W, FROG *f, int pts){
    mvwprintw(W->window, 1, 45, "x: %d  y: %d  ", f->x, f->y);
    mvwprintw(W->window, 1, 25, "%d", pts);
    wrefresh(W->window);
}

void ShowTimer(WIN *W, float pass_time){
    mvwprintw(W->window, 1, 9, "%.2f", pass_time);
    wrefresh(W->window);
}

void ShowNewStatus(WIN *W, TIMER *T, FROG *f, int pts){
    box(W->window, 0, 0); // border
    mvwprintw(W->window, 1, 3, "Time: ");
    mvwprintw(W->window, 1, 17, "Lives: ");
    ShowTimer(W, T->pass_time);
    mvwprintw(W->window, 1, 35, "Position: ");
    mvwprintw(W->window, 1, 78, "Karolina Glaza 198193");
    ShowStatus(W, f, pts);
}


void PrintFrog(FROG *f){
    wattron(f->win->window, COLOR_PAIR(f->color));
    for (int i = 0; i < f->height; i++){
        for (int j = 0; j < f->width; j++){
            mvwprintw(f->win->window, f->y + i, f->x + j, "%c", f->shape[i][j]);
        }
    }
}

void DeleteFrog(FROG *f){
    for (int i = 0; i < f->height; i++){
        for (int j = 0; j < f->width; j++){
            mvwprintw(f->win->window, f->y + i, f->x + j, "%c", ' ');
        }
    }
}

void PrintCar(CAR *c){
    if(c->color!=CAR_G_COLOR){
        for (int i = 0; i < c->height; i++){
            for (int j = 0; j < c->width; j++){
                mvwprintw(c->win->window, c->y + i, c->x + j, "%c", c->shape[i][j]);
            }
        }
    }
}

void PrintStork(STORK *s){
    for (int i = 0; i < s->height; i++){
        for (int j = 0; j < s->width; j++){
            mvwprintw(s->win->window, s->y + i, s->x + j, "%c", s->shape[i][j]);
        }
    }
}

void DeleteCar(CAR *c){
    for (int i = 0; i < c->height; i++){
        for (int j = 0; j < c->width; j++){
            mvwprintw(c->win->window, c->y + i, c->x + j, "%c", ' ');
        }
    }
}

void DeleteStork(STORK *s){
    for (int i = 0; i < s->height; i++){
        for (int j = 0; j < s->width; j++){
            mvwprintw(s->win->window, s->y + i, s->x + j, "%c", ' ');
        }
    }
}

void PrintObstacle(OBSTACLE *o){
    wattron(o->win->window, COLOR_PAIR(PLAY_COLOR));
    for (int i = 0; i < o->height; i++){
        for (int j = 0; j < o->width; j++){
            mvwprintw(o->win->window, o->y + i, o->x + j, "%c", o->shape[i][j]);
        }
    }
}

void ShowStork(STORK *s, int dx, int dy){
    char *sw = (char *)malloc(sizeof(char) * s->width);
    memset(sw, ' ', s->width);
    wattron(s->win->window, COLOR_PAIR(s->color));
    DeleteStork(s);
    s->y=s->y+dy;
    s->x=s->x+dx;
    PrintStork(s);
    wattron(s->win->window, COLOR_PAIR(PLAY_COLOR));
    box(s->win->window, 0, 0);
    wrefresh(s->win->window);
}

void ShowCar(CAR *c, int dx){
    char *sw = (char *)malloc(sizeof(char) * c->width);
    memset(sw, ' ', c->width);
    wattron(c->win->window, COLOR_PAIR(c->color));

    if ((dx == 1) && ((c->x + c->width < c->xmax) || c->width != c->og_width)){ //so it can go to next side
        for (int i = 0; i < c->height; i++){
            mvwprintw(c->win->window, c->y + i, c->x, " ");
        }
        c->x += dx;
    }
    if ((dx == -1) && (c->x > c->xmin)){
        c->x += dx;
        for (int i = 0; i < c->height; i++){
            mvwprintw(c->win->window, c->y + i, c->x + c->width, " ");
        }
    }
    PrintCar(c);
    wattron(c->win->window, COLOR_PAIR(PLAY_COLOR));
    box(c->win->window, 0, 0);
    wrefresh(c->win->window);
}

int Collision_O(int y,int x, int w, int h,OBSTACLE *c){
    if (((c->y >= y && c->y < y + h) || (y >= c->y && y < c->y + c->height)) &&
        ((c->x >= x && c->x < x + w) || (x >= c->x && x < c->x + c->width)))
        return 1;
    else
        return 0;
}

int Collision_F_anyO(FROG *f, int dx, int dy,OBSTACLE* obstacles[]){
    for(int i=0; i<OBSTACLES_NUMBER; i++){
        if(Collision_O(f->y+dy*JUMPSIZE, f->x+dx*JUMPSIZE, f->width, f->height, obstacles[i])==1) return 1;
    }
    return 0;
}

int Collision_C_anyO(CAR *c, int dx, int dy,OBSTACLE* obstacles[]){
    for(int i=0; i<OBSTACLES_NUMBER; i++){
        if(Collision_O(c->y+dy, c->x+dx, c->width, c->height, obstacles[i])==1) return 1;
    }
    return 0;
}


void ShowFrog(FROG *f, int dx, int dy, OBSTACLE* obstacles[]){
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

void orderTaxi(FROG *f, OBSTACLE* obstacles[], CAR *cars[]){
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

int ConvertToInt(char *line) {
    int i = 0;
    int x = 0;
    while (line[i] != '\n' && line[i] != '\0') {
        if (line[i] >= '0' && line[i] <= '9') {
            x = x * 10 + (line[i] - '0');
        }
        i++;
    }
    return x;
}


FROG *InitFrog(WIN *w, int col, const char *filename) {
    FILE *file = fopen(filename, "r");
    FROG *frog = (FROG *)malloc(sizeof(FROG));
    frog->color = col;
    frog->win = w;
    frog->mv = 10;
    frog->calls = 0;
    frog->height = JUMPSIZE;
    frog->shape = (char **)malloc(sizeof(char *) * frog->height);

    char line[250];
    int count = 9+1+1; //const+width+shape begin
    while (count != 0 && fgets(line, sizeof(line), file)) {
        if(count==2){
            frog->width= ConvertToInt(line);
            for (int i = 0; i < frog->height; i++){
                frog->shape[i] = (char *)malloc(sizeof(char) * (frog->width + 1)); // +1: end-of-string '\0'
            }     
        }
        else if (count==1) {
            int count2=JUMPSIZE;
            while(count2){
                strcpy(frog->shape[JUMPSIZE-count2], line);
                fgets(line, sizeof(line), file);
                count2--;
            }
        }
        count--;
    }
    fclose(file); 

    frog->x = (frog->win->cols - frog->width) / 2;
    frog->y = (frog->win->rows - frog->height - 1);
    frog->xmin = 1;
    frog->xmax = w->cols - frog->width;
    frog->ymin = 1;
    frog->ymax = w->rows - frog->height;

    return frog;
}

STORK *InitStork(WIN *w, int col, const char *filename){
    FILE *file = fopen(filename, "r");
    STORK *stork = (STORK *)malloc(sizeof(STORK)); // C
    stork->color = col;
    stork->win = w;
    stork->mv = 1000;
    stork->x = 2;
    stork->y = 4;

    char line[250];
    int count =  9+1+JUMPSIZE+1+JUMPSIZE+2+1; //const+f.width+f.shape+c.width+c.shape+s.width and s.height+s.shape begin
    while (count != 0 && fgets(line, sizeof(line), file)) {
        if(count==3){
            stork->height= ConvertToInt(line);
            stork->shape = (char **)malloc(sizeof(char *) * stork->height);   
        }
        else if(count==2){
            stork->width= ConvertToInt(line);
            for (int i = 0; i < stork->height; i++){
                stork->shape[i] = (char *)malloc(sizeof(char) * (stork->width + 1)); // +1: end-of-string '\0'
            }  
        }
        else if (count==1) { //1=shape begins
            int count2=stork->height;
            while(count2){
                strcpy(stork->shape[stork->height-count2], line);
                fgets(line, sizeof(line), file);
                count2--;
            }
        }
        count--;
    }
    fclose(file); 

    stork->xmin = 1;
    stork->xmax = w->cols - 2;
    stork->ymin = 1;
    stork->ymax = w->rows - 4;
    return stork;
}

CAR *InitCar(WIN *w, int type, int x0, int y0, int speed, float a, int ran, const char *filename) {
    FILE *file = fopen(filename, "r");
    CAR *car = (CAR *)malloc(sizeof(CAR)); // C
    car->x = x0;
    car->y = y0;
    car->color = type;
    car->win = w;
    car->og_width = 9;
    car->mv = speed;
    car->acceleration = a;
    car->turn = 1; //right - default
    car->ordered = 0;
    car->random = ran;
    car->height = JUMPSIZE;
    car->shape = (char **)malloc(sizeof(char *) * car->height);

    char line[250];
    int count = 9+1+JUMPSIZE+1+1; //const+f.width+f.shape+c.width+c.shape beginning
    while (count != 0 && fgets(line, sizeof(line), file)) {
        if(count==2){
            car->width= ConvertToInt(line);
            for (int i = 0; i < car->height; i++){
                car->shape[i] = (char *)malloc(sizeof(char) * (car->width + 1)); // +1: end-of-string '\0'
            }     
        }
        else if (count==1) {
            int count2=JUMPSIZE;
            while(count2){
                strcpy(car->shape[JUMPSIZE-count2], line);
                fgets(line, sizeof(line), file);
                count2--;
            }
        }
        count--;
    }
    fclose(file); 

    car->xmin = 1;
    car->xmax = w->cols - 1;
    return car;
}

CAR *InitRandomCar(int carnum,WIN *w, int y0, const char *filename){
    srand(time(NULL)*carnum);
    //type (color) - 5-9
    int random_color = rand() % 5 + 5;
    //speed
    int random_speed = rand() % (MIN_CAR_SPEED) + MAX_CAR_SPEED;
    //acceleration - (-0.1)-0.1
    int random_a = ((float)rand() / RAND_MAX) * 0.2 - 0.1;
    return InitCar(w, random_color, 1, y0, random_speed, random_a, 1, filename);
}

OBSTACLE *InitObstacle(WIN *w, int x0, int y0, int width){
    OBSTACLE *obs = (OBSTACLE *)malloc(sizeof(OBSTACLE));
    obs->x = x0;
    obs->y = y0;
    obs->win = w;
    obs->width = width;
    obs->height = JUMPSIZE;
    obs->shape = (char **)malloc(sizeof(char *) * obs->height);
    for (int i = 0; i < obs->height; i++){
        obs->shape[i] = (char *)malloc(sizeof(char) * (obs->width + 1)); // +1: end-of-string '\0'
    }
    for (int i = 0; i < obs->height; i++) {
        for (int j = 0; j < obs->width; j++) {
            obs->shape[i][j] = '|';
        }
    }
    return obs;
}

void MoveFrog(FROG *f, int ch, unsigned int frame, OBSTACLE* obstacles[], CAR *cars[]){
    if (frame - f->mv >= MVF_FACTOR){
        switch (ch){
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

void MoveWrapperCar(CAR *c, int frame, int* dx){
    if (frame % c->mv == 0){
        if (c->x + c->og_width >= (c->xmax)){
            c->width--;
        }
        if (c->width == 0){
            for (int i = 0; i < c->height; i++){
                mvwprintw(c->win->window, c->y + i, c->x + c->width, " ");
            } // cleaning
            c->x = 1;     // go to the beginning
            c->width = 1; // go back to your length
        }
        else if (c->width < c->og_width && c->x < (c->xmax) - (c->og_width)){
            c->x = 1;
            c->width++;
        }
        else *dx = 1;
    }
}

void MoveTaxiCar(CAR *c, int frame, int* dx, FROG *f){
    //it is wrapping car
    if(c->ordered==0){ //not ordered
        MoveWrapperCar(c, frame, dx);
    }
    else if(c->ordered==1){ //ordered (is coming)
        if (f->x != c->x + 3) {
            MoveWrapperCar(c, frame, dx);
        }else {
            c->ordered = 2;
        }
    }
    else if(c->ordered==2){ //she came to the frog
        DeleteFrog(f);
        strcpy(c->shape[0], "xxx*.*xxx");
        strcpy(c->shape[1], "xxx***xxx");
        MoveWrapperCar(c, frame, dx);
        c->ordered=3;
    }
    else if(c->ordered==3 && f->calls==true){ //she is going with the frog
        DeleteFrog(f);
        MoveWrapperCar(c, frame, dx);
        f->x=c->x+3;
        f->y=c->y;
    }
    else { //frog wants to get out
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

void MoveBouncingCar(CAR *c, int frame, int* dx, OBSTACLE* obstacles[]){
    if (frame % c->mv == 0){
        if (c->x + c->og_width >= (c->xmax) || c->x == c->xmin || Collision_C_anyO(c,(c->turn),0,obstacles)==1){
            c->turn=(c->turn)*(-1);
        }
        *dx = c->turn;
    }
}

void MoveStoppingCar(CAR* c, int frame, int* dx,  OBSTACLE* obstacles[], FROG* f){
    //it is bouncing car
    if (frame % c->mv == 0){
        if ((c->turn==-1 && f->x - c->x == -4 && abs(f->y - c->y) <= JUMPSIZE) || (c->turn==1 && (c->x+c->width-3)-f->x == -4 && abs(f->y - c->y) <= JUMPSIZE)) {
            *dx=0;
            return;
        }
        if (c->x + c->og_width >= (c->xmax) || c->x == c->xmin || Collision_C_anyO(c,(c->turn),0,obstacles)==1){
            c->turn=(c->turn)*(-1);
        }
        *dx = c->turn;
    }
}



void MoveCar(int i, int frame, OBSTACLE* obstacles[], FROG *f, CAR *cars[], const char *filename){
    int dx = 0;
    if (frame % 500 == 0 && cars[i]->acceleration > 0) { //0-no, (+0.x)-faster, (-0.x)-slower
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
        cars[i]=InitRandomCar(i,cars[i]->win, cars[i]->y, filename);
    }
    switch (cars[i]->color){
        case CAR_W_COLOR:{
            MoveWrapperCar(cars[i], frame, &dx);
            break;
        }
        case CAR_B_COLOR:{
            MoveBouncingCar(cars[i], frame, &dx, obstacles);
            break;
        }
        case CAR_S_COLOR:{
            MoveStoppingCar(cars[i], frame, &dx, obstacles, f);
            break;
        }
        case CAR_T_COLOR:{
            MoveTaxiCar(cars[i], frame, &dx, f);
            break;
        }
        case CAR_G_COLOR:{
            MoveWrapperCar(cars[i], frame, &dx);
            break;
        }
    }
    ShowCar(cars[i], dx);
    PrintFrog(f); //only if she is not in a taxi
}

int MoveStork(int frame, STORK* s,FROG *f){
    int dx = 0;
    int dy = 0;
    if (frame % s->mv == 0){
        if((s->x+2)-(f->x+1)==0) dx=0;
        else if((s->x+2)-(f->x+1)<0) dx=2;
        else if((s->x+2)-(f->x+1)>0) dx=-2;

        if((s->y+2)-(f->y)==0) dy=0;
        else if((s->y+2)-(f->y)<0) dy=1;
        else if((s->y+2)-(f->y)>0) dy=-1;
    }
    if(s->y+dy<4){
        dy=0;
    }
    ShowStork(s, dx,dy);
    if(((s->x+2)-(f->x+1)>=0 && (s->y+2)-(f->y)>=0 && (s->x+2)-(f->x+1)<=1 && (s->y+2)-(f->y)<=1)) return 1;
    else if((s->x+2)-(f->x+1)>=0 && (s->x+2)-(f->x+1)<=1 && s->y==4 && f->y==4) return 1;
    else return 0;
}


int Collision_F_C(FROG *b,CAR *c){
    if(c->color==CAR_G_COLOR) return 0;
    if(c->ordered==3) return 0;
    if (((c->y >= b->y && c->y < b->y + b->height) || (b->y >= c->y && b->y < c->y + c->height)) &&
        ((c->x >= b->x && c->x < b->x + b->width) || (b->x >= c->x && b->x < c->x + c->width)))
        return 1;
    else
        return 0;
}

void Sleep(unsigned int tui) { // micro_sec = frame_time * 1000
    usleep(tui * 1000);
}

TIMER *InitTimer(WIN *status){
    TIMER *timer = (TIMER *)malloc(sizeof(TIMER));
    timer->frame_no = 1;
    timer->frame_time = FRAME_TIME;
    timer->pass_time = PASS_TIME / 1.0;
    return timer;
}

int UpdateTimer(TIMER *T, WIN *status){ // return 1: time is over; otherwise: 0
    T->frame_no++;
    T->pass_time = PASS_TIME - (T->frame_no * T->frame_time / 1000.0);
    if (T->pass_time < (T->frame_time / 1000.0)){
        T->pass_time = 0; // make this zero (floating point!)
    }
    else Sleep(T->frame_time);
    ShowTimer(status, T->pass_time);
    if (T->pass_time == 0) return 1;
    return 0;
}

void GameOver(FROG *f) {
    DeleteFrog(f);
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

int MainLoop(WIN *status, FROG *frog,STORK *stork, CAR *cars[], OBSTACLE *obstacles[], TIMER *timer, const char *filename){
    int key;
    int pts = 3;
    while ((key = wgetch(status->window)) != QUIT && pts>0){
        if (key == ERR)
            key = NOKEY;
        else{
            MoveFrog(frog, key, timer->frame_no, obstacles, cars);
            if(frog->y==2) return 1;
        }
        for (int i = 0; i < CARS_NUMBER; i++){
            if (cars[i] != NULL){
                MoveCar(i, timer->frame_no, obstacles, frog, cars, filename);
                if (Collision_F_C(frog, cars[i])){
                    pts--;
                    GameOver(frog);
                    if(pts<=0) return 2;
                }
            }
        }
        if(MoveStork(timer->frame_no, stork, frog)){
            pts=0;
            GameOver(frog);
            return 2;
        }
        for (int i = 0; i < OBSTACLES_NUMBER; i++){
            if (obstacles[i] != NULL)
                PrintObstacle(obstacles[i]);
        }
        ShowStatus(status, frog, pts);
        flushinp(); // clear input buffer (avoiding multiple key pressed)
        /* update timer */
        if (UpdateTimer(timer, status))
            return pts; // sleep inside
    }
    return 0;
}

void initCars(WIN* playwin, CAR* cars[], const char *filename){
    int v1=5;
    int v2=3;
    int v3=2;
    cars[0] = InitCar(playwin, CAR_W_COLOR, 1, 18, v1, 0,0, filename); // wrapper car
    cars[1] = InitCar(playwin, CAR_W_COLOR, 30, 18, v1,0,0, filename); // wrapper car
    cars[2] = InitCar(playwin, CAR_W_COLOR, 70, 18, v1,0,0, filename); // wrapper car
    cars[3] = InitCar(playwin, CAR_W_COLOR, 90, 18, v1,0,0, filename); // wrapper car
    cars[4] = InitCar(playwin, CAR_B_COLOR, 1, 10, v2,0,0, filename); // bouncing car
    cars[5] = InitCar(playwin, CAR_B_COLOR, 1, 12, v3,0.05,0,filename); // bouncing car
    cars[6] = InitCar(playwin, CAR_B_COLOR, 85, 12, v3, 0,0,filename); // bouncing car
    cars[7] = InitCar(playwin, CAR_S_COLOR, 1, 16, v1, 0,0,filename); //stoppingcar
    cars[8] = InitCar(playwin, CAR_T_COLOR, 1, 20, v3, 0,0,filename); //taxicar
    cars[9] = InitRandomCar(9, playwin, 8 , filename); //random car
    cars[10] = InitRandomCar(8, playwin, 4, filename); //random car
}

void InitObstacles(WIN* playwin, OBSTACLE* obstacles[]){
    obstacles[0] = InitObstacle(playwin, 53, 12, 5);
    obstacles[1] = InitObstacle(playwin, 12, 14, 4);
    obstacles[2] = InitObstacle(playwin, 37, 14, 5);
    obstacles[3] = InitObstacle(playwin, 62, 14, 4);
    obstacles[4] = InitObstacle(playwin, 87, 14, 5);
}

void LoadConstFromFile(const char *filename) {
    FILE *file = fopen(filename, "r");

    char line[250];
    int count = 9;
    while (count != 0 && fgets(line, sizeof(line), file)) {
        int value = ConvertToInt(line);
        switch (count) {
            case 9: QUIT_TIME = value; break;
            case 8: FRAME_TIME = value; break;
            case 7: PASS_TIME = value; break;
            case 6: MVF_FACTOR = value; break;
            case 5: ROWS = value; break;
            case 4: COLS = value; break;
            case 3: OFFX = value; break;
            case 2: OFFY = value; break;
            case 1: JUMPSIZE = value; break;
        }
        count--;
    }
    fclose(file);  
}


int main()
{
    LoadConstFromFile("const.txt");
    WINDOW *mainwin = Start();
    Welcome(mainwin);
    WIN *playwin = Init(mainwin, ROWS, COLS, OFFY, OFFX, PLAY_COLOR, 1);      // window for the playing area
    WIN *statwin = Init(mainwin, 3, COLS, ROWS + OFFY, OFFX, STAT_COLOR, 0); // window for the status
    ShowGoal(playwin);

    TIMER *timer = InitTimer(statwin);
    FROG *frog = InitFrog(playwin, FROG_COLOR, "const.txt");
    STORK *stork = InitStork(playwin, STORK_COLOR, "const.txt");
    CAR *cars[CARS_NUMBER] = {NULL};
    OBSTACLE *obstacles[OBSTACLES_NUMBER] = {NULL};
    initCars(playwin, cars, "const.txt");
    InitObstacles(playwin, obstacles);

    ShowNewStatus(statwin, timer, frog, 0);
    ShowFrog(frog, 0, 0, obstacles);
    for (int i = 0; i < CARS_NUMBER; i++){
        if (cars[i] != NULL)
            ShowCar(cars[i], 0);
    }

    for (int i = 0; i < OBSTACLES_NUMBER; i++){
        if (obstacles[i] != NULL)
            PrintObstacle(obstacles[i]);
    }

    int result = MainLoop(statwin, frog,stork, cars, obstacles, timer, "const.txt");
    if (result == 0)
        EndGame("End. 'Q' was pressed.", statwin);
    else if(result==1){
        //new level
        EndGame("You won.", statwin);
    }
    else if(result == 2)
        EndGame("End. No more lives.", statwin);
    else{
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
