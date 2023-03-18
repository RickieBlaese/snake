#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <thread>

#include <cinttypes>

#include <ncurses.h>


WINDOW *init_ncurses() {
    WINDOW *win = initscr();
    cbreak();
    keypad(win, true);
    wclear(win);
    return win;
}

void deinit_ncurses(WINDOW *win) {
    wclear(win);
    endwin();
}


/* in nanoseconds */
std::uint64_t get_current_time() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}


enum Direction : std::uint32_t {
    up, down, left, right
};

/* should be read from head to tail */
struct SnakeBody {
    std::int32_t count = 0;
    Direction direction = Direction::up;
};


class Game {
public:
    std::int32_t y = 0, x = 0;
    bool **cells; /* true if food, false otherwise */
    std::int32_t heady = 0, headx = 0;
    std::vector<SnakeBody> body;

    Game(std::int32_t y, std::int32_t x) : y(y), x(x), cells(new bool*[x]) {
        for (std::int32_t i = 0; i < x; i++) {
            cells[i] = new bool[y];
        }
    }

    ~Game() {
        for (std::int32_t i = 0; i < x; i++) {
            delete[] cells[i];
        }
        delete[] cells;
    }

    void grow(std::int32_t amt = 1) {
        (body.end() - 1)->count += amt;
    }

    void advance() {
        for (SnakeBody& piece : body) {
            piece.count++;
        }
        switch (body.begin()->direction) {
            case Direction::up:
                heady--;
                break;

            case Direction::down:
                heady++;
                break;

            case Direction::left:
                headx--;
                break;

            case Direction::right:
                headx++;
                break;
        }

        if (cells[headx][heady]) {
            grow();
        }
        
    }

    void out(WINDOW *win) {
        for (std::int32_t i = 0; i < x; i++) {
            for (std::int32_t j = 0; j < y; j++) {
                mvwaddstr(win, j, i * 2, cells[i][j] ? "()" : "  ");
            }
        }

        wmove(win, heady, headx);
        for (const SnakeBody& piece : body) {
            for (std::int32_t i = 0; i < piece.count; i++) {
                std::int32_t cury = 0, curx = 0;
                getyx(win, cury, curx);
                switch (piece.direction) {
                    case Direction::up:
                        wmove(win, cury - 1, curx * 2);
                        break;

                    case Direction::down:
                        wmove(win, cury + 1, curx * 2);
                        break;

                    case Direction::left:
                        wmove(win, cury, (curx - 1) * 2);
                        break;

                    case Direction::right:
                        wmove(win, cury, (curx + 1) * 2);
                        break;

                }
                waddstr(win, "##");
            }
        }
    }

        
};

int main() {
    static constexpr std::int32_t max_input_size = 50;
    static constexpr std::uint64_t visual_wait_ns = 500'000'000ULL;

    WINDOW *win = init_ncurses();
    wmove(win, 0, 0);

    char in[max_input_size + 1];
    std::int32_t x = 0, y = 0;

    waddstr(win, "x size: ");
    wgetnstr(win, in, max_input_size);
    x = std::atoi(in);

    waddstr(win, "y size: ");
    wgetnstr(win, in, max_input_size);
    y = std::atoi(in);
    
    curs_set(0);
    noecho();


    Game game(y, x);

    std::random_device device{};
    std::default_random_engine engine(device());
    std::uniform_int_distribution<> ydist(0, y - 1);
    std::uniform_int_distribution<> xdist(0, x - 1);
    
    std::atomic<Direction> curdir = Direction::up;

    auto displayl = [&](std::stop_token stoken) {
        std::uint64_t last_time = get_current_time();
        while (!stoken.stop_requested()) {
            while (get_current_time() - last_time < visual_wait_ns) {;}
            /* do weird direction checking */
            game.out(win);
        }
    };

    std::jthread displayt(displayl);

    chtype chin = '\0';
    while ((chin = wgetch(win)) != 'q') {

        switch (chin) {
            case 'w':
            case KEY_UP:
                curdir = Direction::up;
                break;

            case 'a':
            case KEY_LEFT:
                curdir = Direction::left;
                break;

            case 's':
            case KEY_DOWN:
                curdir = Direction::down;
                break;

            case 'd':
            case KEY_RIGHT:
                curdir = Direction::right;
                break;

            default:
                game.advance();
                game.out(win);
                wrefresh(win);
                continue;
        }
    }

    deinit_ncurses(win);
    
    return 0;
}
