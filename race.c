#include "race.h"
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#define NUM_CARS 3
#define TRACK_LEN 40

typedef struct {
    int id;
    int progress;
    int speed_us;
    short color_pair;
} Car;

static Car cars[NUM_CARS];
static pthread_mutex_t race_lock = PTHREAD_MUTEX_INITIALIZER;
static int winner_id = -1;
static WINDOW *race_win = NULL;

static void draw_race_frame(void) {
    int rows, cols;
    getmaxyx(race_win, rows, cols);
    werase(race_win);
    mvwprintw(race_win, 0, 2, "Thread Race Demo");
    for (int i = 0; i < NUM_CARS; i++) {
        int y = 2 + i * 2;
        int bar_start_x = 10;
        int track_len = TRACK_LEN;
        mvwprintw(race_win, y, 2, "Car %d", cars[i].id);
        int pos = (cars[i].progress * track_len) / 100;
        if (pos < 0) pos = 0;
        if (pos > track_len) pos = track_len;
        if (has_colors()) {
            wattron(race_win, COLOR_PAIR(cars[i].color_pair));
        }
        //draw track: [=====>     ]
        mvwprintw(race_win, y, bar_start_x - 1, "[");
        for (int x = 0; x < track_len; x++) {
            if (x < pos) {
                //trail
                mvwaddch(race_win, y, bar_start_x + x, '=');
            } else if (x == pos) {
                //car
                mvwaddch(race_win, y, bar_start_x + x, '>');
            } else {
                //remaining track
                mvwaddch(race_win, y, bar_start_x + x, ' ');
            }
        }
        mvwaddch(race_win, y, bar_start_x + track_len, ']');
        if (has_colors()) {
            wattroff(race_win, COLOR_PAIR(cars[i].color_pair));
        }
        //speed meter on the right side
        double cells_per_sec = 0.0;
        if (cars[i].speed_us > 0) {
            cells_per_sec = 1000000.0 / cars[i].speed_us;
        }
        mvwprintw(
            race_win,
            y,
            bar_start_x + track_len + 4,
            "spd: %4.1f",
            cells_per_sec
        );
    }
    //finish line
    int finish_x = 10 + TRACK_LEN + 2;
    for (int y = 1; y < 2 + NUM_CARS * 2; y++) {
        mvwaddch(race_win, y, finish_x, '|');
    }
    wrefresh(race_win);
}

static void *car_thread(void *arg) {
    Car *c = (Car *)arg;
    while (c->progress < 100) {
        usleep(c->speed_us);
        pthread_mutex_lock(&race_lock);
        c->progress++;
        if (c->progress >= 100 && winner_id == -1) {
            winner_id = c->id;
        }
        draw_race_frame();
        pthread_mutex_unlock(&race_lock);
    }
    return NULL;
}

void race_command(WINDOW *output_win) {
    race_win = output_win;
    if (has_colors()) {
        start_color();
        //give colors to cars
        init_pair(5, COLOR_CYAN,   COLOR_BLACK);
        init_pair(6, COLOR_YELLOW, COLOR_BLACK);
        init_pair(7, COLOR_MAGENTA, COLOR_BLACK);
    }
    werase(race_win);
    wrefresh(race_win);
    srand((unsigned int)time(NULL));
    for (int i = 0; i < NUM_CARS; i++) {
        cars[i].id = i + 1;
        cars[i].progress = 0;
        //give cars random speed and delay
        cars[i].speed_us = 30000 + (rand() % 40001);
        if (has_colors()) {
            cars[i].color_pair = 5 + i;
        } else {
            cars[i].color_pair = 0;
        }
    }
    winner_id = -1;
    pthread_t threads[NUM_CARS];
    for (int i = 0; i < NUM_CARS; i++) {
        pthread_create(&threads[i], NULL, car_thread, &cars[i]);
    }
    draw_race_frame();
    for (int i = 0; i < NUM_CARS; i++) {
        pthread_join(threads[i], NULL);
    }
    werase(race_win);
    if (winner_id == -1) {
        mvwprintw(race_win, 1, 2, "Race finished, but no winner recorded (weird)!");
    } else {
        mvwprintw(race_win, 1, 2, "Race finished! Winner: Car %d", winner_id);
    }
    mvwprintw(race_win, 3, 2, "Press any key to return to the shell...");
    wrefresh(race_win);
    wgetch(race_win);
}
