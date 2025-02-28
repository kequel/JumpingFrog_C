// gcc -o one one.c -lncurses
//./one

/*******************************************/
/*Longest function tester - limit: 1024 bytes */
//https://www.spoj.com/PP24MMBS/problems/LONFLEN/
/*******************************************/

/*******************************************/
//project made using demo game Catch the Ball from dr hab. inż. Michał Małafiejski https://github.com/animima
/*******************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ncurses.h>
#include <time.h>
#include <string.h>

#define QUIT 'q'  
#define NOKEY ' '

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
        fprintf(stderr, "Error initialising.\n");
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

void Welcome(WINDOW *win, char* name) {
    mvwaddstr(win, 1, 1, "JUMPING FROG GAME");

    mvwaddstr(win, 4, 1, "RULES: ");
    mvwaddstr(win, 5, 2, "1) You have 200s and 3 lives to complete each level.");
    mvwaddstr(win, 6, 2, "2) Best 3 players (fastest) are displayed in the ranking.");
    mvwaddstr(win, 7, 2, "3) Cars take away one life from you.");
    mvwaddstr(win, 8, 2, "4) Stork takes away all your lives. ");
    mvwaddstr(win, 9, 2, "5) You can save your game to file (but it wont include your current level progress). ");
    mvwaddstr(win, 11, 1, "CARS: ");
    mvwaddstr(win, 12, 2, "1) BLUE - friendly car, will stop if it sees you.");
    mvwaddstr(win, 13, 2, "2) YELLOW - taxi car, click [T]  to order a taxi in a line in front of you.");
    mvwaddstr(win, 14, 2, "3) RED and PURPLE - try to kill you.");

    mvwaddstr(win, 17, 1, "Enter your name (max 8 char): ");

    wrefresh(win);
    echo();
    wgetnstr(win, name, 8);
    noecho();
    wclear(win);
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

void ShowStatus(WIN *W, FROG *f, int pts, int lvl, char name[]){
    mvwprintw(W->window, 1, 45, "x: %d  y: %d  ", f->x, f->y);
    mvwprintw(W->window, 1, 25, "%d", pts);
    mvwprintw(W->window, 1, 65, "LEVEL: %d", lvl);
    mvwprintw(W->window, 1, 90, "NOW PLAYING: %s", name);

    wrefresh(W->window);
}

void ShowTimer(WIN *W, float pass_time){
    mvwprintw(W->window, 1, 9, "%.2f", 200.0-pass_time);
    wrefresh(W->window);
}

void ShowNewStatus(WIN *W, TIMER *T, FROG *f, int pts, int lvl, char name[]){
    box(W->window, 0, 0); // border
    mvwprintw(W->window, 1, 3, "Time: ");
    mvwprintw(W->window, 1, 17, "Lives: ");
    ShowTimer(W, T->pass_time);
    mvwprintw(W->window, 1, 35, "Position: ");
    mvwprintw(W->window, 3, 2, "[Q] - Quit      [C] - Continue Saved Game     [X] - Save Game       [T] - order Taxi");
    mvwprintw(W->window, 3, 90, "Karolina Glaza 198193");
    mvwprintw(W->window, 1, 118, "TOP 3 RESULTS:");
    FILE *file = fopen("game_ranking.txt", "r");
    char line[10];
    int count = 3; 
    while (count != 0) {
    if (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        mvwprintw(W->window, 1+(4-count), 117, "%s", line);
    }
    if (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        mvwprintw(W->window, 1+(4-count), 132, "%s", line);
    }
    count--;
    }
    fclose(file);
    ShowStatus(W, f, pts, lvl, name);
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

int Collision_F_anyO(FROG *f, int dx, int dy,OBSTACLE* obstacles[], int obstacles_number){
    for(int i=0; i<obstacles_number; i++){
        if(Collision_O(f->y+dy*JUMPSIZE, f->x+dx*JUMPSIZE, f->width, f->height, obstacles[i])==1) return 1;
    }
    return 0;
}

int Collision_C_anyO(CAR *c, int dx, int dy,OBSTACLE* obstacles[], int obstacles_number){
    for(int i=0; i<obstacles_number; i++){
        if(Collision_O(c->y+dy, c->x+dx, c->width, c->height, obstacles[i])==1) return 1;
    }
    return 0;
}


void ShowFrog(FROG *f, int dx, int dy, OBSTACLE* obstacles[], int obstacles_number){
    char *sw = (char *)malloc(sizeof(char) * f->width);
    memset(sw, ' ', f->width);
    wattron(f->win->window, COLOR_PAIR(f->color));
    DeleteFrog(f);

    if(Collision_F_anyO(f,dx,dy,obstacles, obstacles_number)==0){
        if ((dy == 1) && (f->y + f->height < f->ymax)) f->y += dy*JUMPSIZE;
        if ((dy == -1) && (f->y > f->ymin)) f->y += dy*JUMPSIZE;
        if ((dx == 1) && (f->x + f->width < f->xmax))  f->x += dx*JUMPSIZE;
        if ((dx == -1) && (f->x > f->xmin)) f->x += dx*JUMPSIZE;
    }
    PrintFrog(f);
    box(f->win->window, 0, 0); // obramowanie
    wrefresh(f->win->window);
}

void orderTaxi(FROG *f, OBSTACLE* obstacles[], CAR *cars[], int cars_number){
    int bestCarNum=-1;
    for(int i=0; i<cars_number; i++){
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

STORK *InitStork(WIN *w, int col, const char *filename, int stork_speed){
    FILE *file = fopen(filename, "r");
    STORK *stork = (STORK *)malloc(sizeof(STORK)); // C
    stork->color = col;
    stork->win = w;
    stork->mv = stork_speed;
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

CAR *InitRandomCar(int carnum, WIN *w, int y0, const char *filename, int max_car_speed){
    srand(time(NULL)*carnum);
    //type (color) - 5-9
    int random_color = rand() % 5 + 5;
    //speed
    int random_speed = rand() % (max_car_speed*3) + max_car_speed;
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

void MoveFrog(FROG *f, int ch, unsigned int frame, OBSTACLE* obstacles[], int obstacles_number, CAR* cars[], int cars_number){
    if (frame - f->mv >= MVF_FACTOR){
        switch (ch){
            case 'w':
                f->calls=false;
                ShowFrog(f, 0, -1, obstacles, obstacles_number);
                break;
            case 's':
                f->calls=false;
                ShowFrog(f, 0, 1, obstacles, obstacles_number);
                break;
            case 'a':
                f->calls=false;
                ShowFrog(f, -1, 0, obstacles, obstacles_number);
                break;
            case 'd':
                f->calls=false;
                ShowFrog(f, 1, 0, obstacles, obstacles_number);
                break;
            case 't':
                f->calls=true;
                orderTaxi(f, obstacles, cars, cars_number);
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

void MoveBouncingCar(CAR *c, int frame, int* dx, OBSTACLE* obstacles[], int obstacles_number){
    if (frame % c->mv == 0){
        if (c->x + c->og_width >= (c->xmax) || c->x == c->xmin || Collision_C_anyO(c,(c->turn),0,obstacles, obstacles_number)==1){
            c->turn=(c->turn)*(-1);
        }
        *dx = c->turn;
    }
}

void MoveStoppingCar(CAR* c, int frame, int* dx,  OBSTACLE* obstacles[], int obstacles_number, FROG* f){
    //it is bouncing car
    if (frame % c->mv == 0){
        if ((c->turn==-1 && f->x - c->x == -4 && abs(f->y - c->y) <= JUMPSIZE) || (c->turn==1 && (c->x+c->width-3)-f->x == -4 && abs(f->y - c->y) <= JUMPSIZE)) {
            *dx=0;
            return;
        }
        if (c->x + c->og_width >= (c->xmax) || c->x == c->xmin || Collision_C_anyO(c,(c->turn),0,obstacles, obstacles_number)==1){
            c->turn=(c->turn)*(-1);
        }
        *dx = c->turn;
    }
}



void MoveCar(int i, int frame, OBSTACLE* obstacles[], int obstacles_number, FROG *f, CAR *cars[], int cars_number, const char *filename, int max_car_speed){
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
        cars[i]=InitRandomCar(i,cars[i]->win, cars[i]->y, filename, max_car_speed);
    }
    switch (cars[i]->color){
        case CAR_W_COLOR:{
            MoveWrapperCar(cars[i], frame, &dx);
            break;
        }
        case CAR_B_COLOR:{
            MoveBouncingCar(cars[i], frame, &dx, obstacles, obstacles_number);
            break;
        }
        case CAR_S_COLOR:{
            MoveStoppingCar(cars[i], frame, &dx, obstacles, obstacles_number, f);
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

int UpdateTimer(TIMER *T, WIN *status, int new_time){ // return 1: time is over; otherwise: 0
    T->frame_no++;
    // float new=new_time+0.0;
    T->pass_time = PASS_TIME - (T->frame_no * T->frame_time / 1000.0);
    if (T->pass_time < (T->frame_time / 1000.0)){
        T->pass_time = 0; // make this zero (floating point!)
    }
    else Sleep(T->frame_time);
    // if(new){
    //     // printf("before: %.2f  ", T->pass_time);
    //     // printf("new; %.2f  ", new);
    //     T->pass_time = new -(T->frame_no * T->frame_time / 1000.0);
    //     // printf("after %.2f", T->pass_time);
    // } 
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

void SaveGame(float* ran_pts, char name[], int* lvl){
    FILE *file = fopen("saved_game.txt", "w");
    if (file != NULL){
        fprintf(file, "%s\n", name);
        fprintf(file, "%d\n", *lvl);
        int ran=(int)(*ran_pts*(-1));
        fprintf(file, "%d", ran);
    }
    fclose(file);
}

void ContinueGame(float* ran_pts, char* name, int* lvl, TIMER *T, WIN *status){
    FILE *file = fopen("saved_game.txt", "r");

    char line[9];
    fgets(line, sizeof(line), file);
    for(int i=0; i<9; i++){
        name[i]=line[i];
        if(name[i]=='\n') name[i]='\0';
    }
    *lvl=ConvertToInt(fgets(line, sizeof(line), file)); 
    *ran_pts=(ConvertToInt(fgets(line, sizeof(line), file)));
    *ran_pts=*ran_pts*(-1)+200;
    UpdateTimer(T, status, (200-*ran_pts));
}

int MainLoop(float* ran_pts, char name[], int* lvl, WIN *status, FROG *frog,STORK *stork, CAR *cars[], int cars_number, OBSTACLE *obstacles[], int obstacles_number, TIMER *timer, const char *filename, int max_car_speed){
    int key;
    int pts = 3;
    while ((key = wgetch(status->window)) != QUIT && pts>0){
        if (key == ERR){ 
            key = NOKEY;
    } else if(key=='x'){ //save to file
        SaveGame(ran_pts, name, lvl);
    } else if(key=='c'){ //continue from file
        ContinueGame(ran_pts, name, lvl, timer, status);
        return 3;
    }
        else{
            nanosleep((const struct timespec[]){{0, 50L}}, NULL);
            MoveFrog(frog, key, timer->frame_no, obstacles, obstacles_number, cars, cars_number);
            if(frog->y==2) return 1; //1-end of board
        }
        for (int i = 0; i < cars_number; i++){
            if (cars[i] != NULL){
                MoveCar(i, timer->frame_no, obstacles, obstacles_number, frog, cars, cars_number,filename, max_car_speed);
                if (Collision_F_C(frog, cars[i])){
                    pts--;
                    GameOver(frog);
                    if(pts<=0) return 2; //2-died
                }
            }
        }
        if(MoveStork(timer->frame_no, stork, frog)){
            pts=0;
            GameOver(frog);
            return 2;
        }
        for (int i = 0; i < obstacles_number; i++){
            if (obstacles[i] != NULL)
                PrintObstacle(obstacles[i]);
        }
        ShowStatus(status, frog, pts, *lvl, name);
        flushinp();
        if (key != 'c' && UpdateTimer(timer, status, 0))
            return pts; 
    }
    return 0;
}

void initCars(WIN* playwin, CAR* cars[], int cars_number, const char *filename, int max_car_speed){
    FILE *file = NULL;
    switch(cars_number){
        case 13: file = fopen("cars_level1.txt", "r"); break;
        case 17: file = fopen("cars_level2.txt", "r"); break;
        case 24: file = fopen("cars_level3.txt", "r"); break;
    }
    char line[250];
    int count=0;
    while (count!=cars_number && fgets(line, sizeof(line), file)) {
        int color = ConvertToInt(line) ;
        if(color!=0){
            fgets(line, sizeof(line), file);
            int xa = ConvertToInt(line) ;
            fgets(line, sizeof(line), file);
            int xb = ConvertToInt(line) ;
            fgets(line, sizeof(line), file);
            int y = ConvertToInt(line) ;
            fgets(line, sizeof(line), file);
            int speed = ConvertToInt(line)*max_car_speed ;
            fgets(line, sizeof(line), file);
            float acc = ConvertToInt(line) * 0.01;
            cars[count] = InitCar(playwin, color, xa*playwin->cols/xb, y, speed, acc,0, filename); 
            count ++;
        } else{
            fgets(line, sizeof(line), file);
            int y = ConvertToInt(line) ;
            cars[count] = InitRandomCar(count, playwin, y , filename, max_car_speed);
            count++;
        }
        fgets(line, sizeof(line), file); //linia odstepu w pliku
    }
    fclose(file); 
}


void InitObstacles(WIN* playwin, OBSTACLE* obstacles[], int obstacles_number){
    switch(obstacles_number){
        case 10:{ //level 1
            for(int i=0; i<4; i++){
                obstacles[i] = InitObstacle(playwin, (i+1)*playwin->cols/5-3, 6, 3);
            }
            obstacles[4] = InitObstacle(playwin, playwin->cols/2-4, 10, 7);
            for(int i=5; i<9; i++){
                obstacles[i] = InitObstacle(playwin, (i-4)*playwin->cols/5-2, 14, 3);
            }
            obstacles[9] = InitObstacle(playwin, playwin->cols/2-4, 20, 7);
            break;
        }
        case 6:{ //level 2
            obstacles[0] = InitObstacle(playwin, playwin->cols/2-5, 8, 9);
            obstacles[1] = InitObstacle(playwin, playwin->cols/2-3, 16, 5);
            obstacles[2] = InitObstacle(playwin, playwin->cols/4-2, 20, 3);
            for(int i=3; i<6; i++){
                obstacles[i] = InitObstacle(playwin, (i-2)*playwin->cols/4-2, 20, 3);
            }
            break;
        }
        case 5:{ //level 3
            obstacles[0] = InitObstacle(playwin, playwin->cols/3-4, 10, 9);
            obstacles[1] = InitObstacle(playwin, 2*playwin->cols/3-4, 10, 9);
            for(int i=2; i<5; i++){
                obstacles[i] = InitObstacle(playwin, (i-1)*playwin->cols/4-4, 4, 6);
            }
            break;
        }
    }
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
            case 2:OFFY=value;break;
            case 1: JUMPSIZE = value; break;
        }
        count--;
    }
    fclose(file);  
}

void loadLevelConstFromFile(int num, int* cars_number, int* obstacles_number, int* max_car_speed, int* stork_speed){
    FILE *file = fopen("levels_const.txt", "r");

    char line[250];
    int count=4*num;
    while (count != 0 && fgets(line, sizeof(line), file)) {
        if(count==4) *cars_number= ConvertToInt(line);
        if(count==3) *obstacles_number= ConvertToInt(line);
        if(count==2) *max_car_speed= ConvertToInt(line);
        if(count==1) *stork_speed= ConvertToInt(line);
        count--;
    }
    fclose(file); 
}

int RunGame(float* ran_pts, char name[], int* level, WIN *statwin, WIN *playwin, WINDOW *mainwin, TIMER *timer, FROG *frog, STORK *stork, CAR **cars, int cars_number, OBSTACLE **obstacles, int obstacles_number, int max_car_speed) {
    ShowNewStatus(statwin, timer, frog, 0, *level, name);
    ShowFrog(frog, 0, 0, obstacles, obstacles_number);

    for (int i = 0; i < cars_number; i++) {
        if (cars[i] != NULL)
            ShowCar(cars[i], 0);
    }

    for (int i = 0; i < obstacles_number; i++) {
        if (obstacles[i] != NULL)
            PrintObstacle(obstacles[i]);
    }

    int result = MainLoop(ran_pts, name, level, statwin, frog, stork, cars, cars_number, obstacles, obstacles_number, timer, "const.txt", max_car_speed);

    if (result == 0) //0-quit
        EndGame("End. 'Q' was pressed.", statwin);
    else if (result == 1){ //1-new level
        (*level)++;
        if(*level>3) return 3; //3-done
        return 1;
    }
    else if (result == 2){ //2-died (return 0)
        EndGame("End. No more lives.", statwin);
        result=0;
    } else if (result == 3){ //3-continue from file
        return 1; 
    }
      
    else {
        char info[100];
        sprintf(info, "Timer is over.");
        EndGame(info, statwin);
    }
    refresh();
    return result;
}




void CleanupLevel(TIMER *t, float *ranking_points, WIN *playwin, WIN *statwin, WINDOW *mainwin, CAR ***cars, int cars_number, OBSTACLE ***obstacles, int obstacles_number) {
    for (int i = 0; i < cars_number; i++) {
        if ((*cars)[i] != NULL) {
            free((*cars)[i]);
            (*cars)[i] = NULL;  
        }
    }
    free(*cars);
    *cars = NULL;

    for (int i = 0; i < obstacles_number; i++) {
        if ((*obstacles)[i] != NULL) {
            free((*obstacles)[i]);
            (*obstacles)[i] = NULL;  
        }
    }
    free(*obstacles);
    *obstacles = NULL;
    *ranking_points=*ranking_points-(t->pass_time);
    CleanWin(playwin);
    refresh();
}

void InitGame(char name[],int level, int *cars_number, int *obstacles_number, int *max_car_speed, int *stork_speed, WINDOW **mainwin, WIN **playwin, WIN **statwin, TIMER **timer, FROG **frog, STORK **stork, CAR ***cars, OBSTACLE ***obstacles) {
    LoadConstFromFile("const.txt");
    loadLevelConstFromFile(level, cars_number, obstacles_number, max_car_speed, stork_speed);

    if (level==1 && !*playwin) {
        *mainwin = Start();
        Welcome(*mainwin, name);
        *playwin = Init(*mainwin, ROWS, COLS, OFFY, OFFX, PLAY_COLOR, 1);
        *statwin = Init(*mainwin, 6, COLS, ROWS + OFFY, OFFX, STAT_COLOR, 0);
    }
    ShowGoal(*playwin);

    *timer = InitTimer(*statwin);
    *frog = InitFrog(*playwin, FROG_COLOR, "const.txt");
    *stork = InitStork(*playwin, STORK_COLOR, "const.txt", *stork_speed);

    *cars = malloc(*cars_number * sizeof(CAR *));
    *obstacles = malloc(*obstacles_number * sizeof(OBSTACLE *));

    initCars(*playwin, *cars, *cars_number, "const.txt", *max_car_speed);
    InitObstacles(*playwin, *obstacles, *obstacles_number);
}

void Ranking(char name[], float ran_pts, TIMER *t) {
    ran_pts=ran_pts-(t->pass_time);
    ran_pts=(600.0+ran_pts);
    int ran=(int)ran_pts;
    
    FILE *file = fopen("game_ranking.txt", "r");

    char n[3][9]; //names
    int val[3]; //best values
    int c = 0; //count
    char line[4];

    while (c < 3 && fgets(n[c], sizeof(n[c]), file) != NULL) {
        for (int i = 0; n[c][i] != '\0'; i++) {
            if (n[c][i] == '\n') {
                n[c][i] = '\0';
                break;
            }
        }

        if (fgets(line, sizeof(line), file) != NULL) {
            val[c] = atoi(line); 
            c++;
        }
    }
    fclose(file);
if (ran < val[2] || c < 3) { 
    if (ran < val[1] || c < 2) {
        if (ran < val[0] || c < 1) {
            //run is the best
            for (int i = 0; i < 9; i++) {
                n[2][i] = n[1][i];
                n[1][i] = n[0][i];
                n[0][i] = name[i];
                if (name[i] == '\0') break;
            }
            val[2] = val[1];
            val[1] = val[0];
            val[0] = ran;
        } else {
            //run is second best
            for (int i = 0; i < 9; i++) {
                n[2][i] = n[1][i];
                n[1][i] = name[i];
                if (name[i] == '\0') break;
            }
            val[2] = val[1];
            val[1] = ran;
        }
    } else {
        //run is third best
        for (int i = 0; i < 9; i++) {
            n[2][i] = name[i];
            if (name[i] == '\0') break;
        }
        val[2] = ran;
    }
}
    file = fopen("game_ranking.txt", "w");
    if (file != NULL){
        for (int i = 0; i < c; i++) {
            fprintf(file, "%s\n%d\n", n[i], val[i]);
    }
    }
    fclose(file);
}


int main() {
    int lvl = 1; 
    char name[8];
    float ran_pts;
    int cars_n, obstacles_n, max_car_v, stork_v;
    WINDOW *mainwin = NULL;
    WIN *playwin = NULL, *statwin = NULL;
    TIMER *timer = NULL;
    FROG *frog = NULL;
    STORK *stork = NULL;
    CAR **cars = NULL;
    OBSTACLE **obstacles = NULL;

    InitGame(name, lvl, &cars_n, &obstacles_n, &max_car_v, &stork_v, &mainwin, &playwin, &statwin, &timer, &frog, &stork, &cars, &obstacles);

    while (1) {
        int r = RunGame(&ran_pts, name, &lvl, statwin, playwin, mainwin, timer, frog, stork, cars, cars_n, obstacles, obstacles_n, max_car_v);
        if (r == 0) { //quit or die
            break;
        }
        if(r==1){ //new level
            CleanupLevel(timer, &ran_pts, playwin, statwin, mainwin, &cars, cars_n, &obstacles, obstacles_n);
            timer = NULL;
            frog = NULL;
            stork = NULL;
            cars = NULL;
            obstacles = NULL;
            InitGame(name, lvl, &cars_n, &obstacles_n, &max_car_v, &stork_v, &mainwin, &playwin, &statwin, &timer, &frog, &stork, &cars, &obstacles);
        }
        if(r==3){ //game complete
            EndGame("You completed the game.", statwin);
            Ranking(name, ran_pts, timer);
            break;
        }
    }
    CleanupLevel(timer, &ran_pts,playwin, statwin, mainwin, &cars, cars_n, &obstacles, obstacles_n);
    delwin(playwin->window);
    delwin(statwin->window);
    delwin(mainwin);
    endwin();
    return 0;
}
