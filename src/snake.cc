#include <exception>
#include <iostream>
#include <mutex>
#include <vector>
#include <random>
#include <chrono>
#include <locale>
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

Direction reflect(const Direction& direction) {
    if (direction < 2) {
        return static_cast<Direction>(1 - direction);
    }
    return static_cast<Direction>(5 - direction);
}

std::string direction_to_str(const Direction& direction) {
    switch (direction) {
        case Direction::up:
            return "up";

        case Direction::down:
            return "down";

        case Direction::left:
            return "left";

        case Direction::right:
            return "right";
    }
}

/* should be read from head to tail */
struct SnakeBody {
    std::int32_t count = 0;
    Direction direction = Direction::up;

    SnakeBody() = default;
    SnakeBody(std::int32_t count, Direction direction) : count(count), direction(direction) {}
};


class Game {
public:
    std::int32_t y = 0, x = 0;
    bool **cells; /* true if food, false otherwise */
    std::int32_t heady = 0, headx = 0;
    std::vector<SnakeBody> body = {SnakeBody(1, Direction::up)};

    Game(std::int32_t y, std::int32_t x) : y(y), x(x), cells(new bool*[y]), heady(y/2), headx(x/2) {
        for (std::int32_t j = 0; j < y; j++) {
            cells[j] = new bool[x];
            for (std::int32_t i = 0; i < x; i++) {
                cells[j][i] = false;
            }
        }
    }

    ~Game() {
        for (std::int32_t j = 0; j < y; j++) {
            delete[] cells[j];
        }
        delete[] cells;
    }

    std::vector<SnakeBody>::iterator head() { return body.begin(); }
    std::vector<SnakeBody>::iterator tail() { return body.end() - 1; }

    void grow(std::int32_t amt = 1) {
        tail()->count += amt;
    }

    /* return: true if grew, false otherwise */
    bool advance() {
        head()->count++;
        tail()->count--;
        if (tail()->count <= 0) {
            body.erase(tail());
        }
        switch (reflect(head()->direction)) {
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

        /* prevent segfault from reading at this pos */
        if (headx >= 0 && headx < x && heady >= 0 && heady < y) {
            if (cells[heady][headx]) {
                grow();
                cells[heady][headx] = false; /* consumed */
                return true;
            }
        }
        return false;
    }

    bool body_intersects(std::int32_t y, std::int32_t x) {
        std::int32_t cx = headx, cy = heady;
        for (const SnakeBody& piece : body) {
            for (std::int32_t i = 0; i < piece.count; i++) {
                if (y == cy && x == cx) {
                    return true;
                }

                switch (piece.direction) {
                    case Direction::up:
                        cy--;
                        break;

                    case Direction::down:
                        cy++;
                        break;

                    case Direction::left:
                        cx--;
                        break;

                    case Direction::right:
                        cx++;
                        break;
                }
            }
        }
        return false;
    }

    bool is_out() {
        if (headx < 0 || headx >= x || heady < 0 || heady >= y) {
            return true;
        }

        /* this is not performant but a side effect of the structure
         * there is probably a better way to do this */
        std::int32_t cx = headx, cy = heady;
        for (std::int32_t pi = 0; pi < body.size(); pi++) {

            const SnakeBody& piece = body[pi];
            for (std::int32_t i = 0; i < piece.count; i++) {

                std::int32_t cx2 = headx, cy2 = heady;
                for (std::int32_t pi2 = 0; pi2 < body.size(); pi2++) {

                    const SnakeBody& piece2 = body[pi2];
                    for (std::int32_t j = 0; j < piece2.count; j++) {

                        /* same piece cannot intersect with itself, avoid self intersection */
                        if ((pi != pi2) && (cy2 == cy && cx2 == cx)) {
                            return true;
                        }

                        switch (piece2.direction) {
                            case Direction::up:
                                cy2--;
                                break;

                            case Direction::down:
                                cy2++;
                                break;

                            case Direction::left:
                                cx2--;
                                break;

                            case Direction::right:
                                cx2++;
                                break;
                        }
                    }
                }

                switch (piece.direction) {
                    case Direction::up:
                        cy--;
                        break;

                    case Direction::down:
                        cy++;
                        break;

                    case Direction::left:
                        cx--;
                        break;

                    case Direction::right:
                        cx++;
                        break;
                }
            }
        }

        return false;
    }

    void out(WINDOW *win, std::int32_t offy, std::int32_t offx) {
        for (std::int32_t i = 0; i < x; i++) {
            for (std::int32_t j = 0; j < y; j++) {
                mvwaddwstr(win, j + offy, i + offx, cells[j][i] ? L"*" : L" ");
            }
        }

        wmove(win, heady + offy, headx + offx);
        for (const SnakeBody& piece : body) {
            for (std::int32_t i = 0; i < piece.count; i++) {
                std::int32_t cury = 0, curx = 0;
                getyx(win, cury, curx);
                waddwstr(win, L"█");
                wmove(win, cury, curx);
                switch (piece.direction) {
                    case Direction::up:
                        wmove(win, cury - 1, curx);
                        break;

                    case Direction::down:
                        wmove(win, cury + 1, curx);
                        break;

                    case Direction::left:
                        wmove(win, cury, curx - 1);
                        break;

                    case Direction::right:
                        wmove(win, cury, curx + 1);
                        break;
                }
            }
        }
    }

};


int main() {
    static constexpr std::int32_t max_input_size = 50;
    static constexpr std::uint64_t visual_wait_ns = 100'000'000ULL;

    std::setlocale(LC_ALL, "");

    WINDOW *win = init_ncurses();
    wmove(win, 0, 0);

    char in[max_input_size + 1];
    std::int32_t x = 0, y = 0;

    try {
        waddstr(win, "x size: ");
        wgetnstr(win, in, max_input_size);
        x = std::stoi(in);
        if (x == 0) {
            throw std::exception();
        }

        waddstr(win, "y size: ");
        wgetnstr(win, in, max_input_size);
        y = std::stoi(in);
        if (y == 0) {
            throw std::exception();
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << '\n';
        deinit_ncurses(win);
        std::cerr << "error: bad size entered\n";
        return 1;
    }
    
    curs_set(0);
    noecho();

    Game game(y, x);
    if (std::max(y, x) == x) {
        game.head()->direction = Direction::left;
    }

    std::random_device device{};
    std::default_random_engine engine(device());
    std::uniform_int_distribution<> ydist(0, y - 1);
    std::uniform_int_distribution<> xdist(0, x - 1);

    game.cells[ydist(engine)][xdist(engine)] = true; /* init food */
    
    std::atomic<Direction> curdir = game.head()->direction;
    std::atomic_bool end_flag = false;

    auto displayl = [&](const std::stop_token& stoken) {
        bool first = true;
        while (!stoken.stop_requested()) {
            std::uint64_t last_time = get_current_time();
            if (!first) {
                while (get_current_time() - last_time < visual_wait_ns && !stoken.stop_requested()) {;}
            }
            first = false;
            Direction hdir = reflect(game.head()->direction);

            /* stop from going in opposite or same direction */
            if (curdir != hdir && curdir != reflect(hdir)) {
                game.body.insert(game.body.begin(), SnakeBody(0, reflect(curdir)));
            }

            if (game.advance()) {
                std::int32_t fy = ydist(engine), fx = xdist(engine);
                while (game.body_intersects(fy, fx) && !game.cells[fy][fx]) {
                    fy = ydist(engine);
                    fx = xdist(engine);
                }
                game.cells[fy][fx] = true;
            }

            if (game.is_out()) {
                end_flag = true;
                return;
            }

            for (std::int32_t j = 0; j < y + 2; j += y + 1) {
                for (std::int32_t i = 1; i < x + 1; i++) { 
                    mvwaddwstr(win, j, i, L"━");
                }
            }
            for (std::int32_t i = 0; i < x + 2; i += x + 1) { 
                for (std::int32_t j = 1; j < y + 1; j++) {
                    mvwaddwstr(win, j, i, L"┃");
                }
            }
            mvwaddwstr(win, 0, 0, L"┏");
            mvwaddwstr(win, 0, x + 1, L"┓");
            mvwaddwstr(win, y + 1, 0, L"┗");
            mvwaddwstr(win, y + 1, x + 1, L"┛");
            game.out(win, 1, 1);
        }
    };

    std::jthread displayt(displayl);

    nodelay(win, true);

    chtype chin = '\0';
    while (chin != 'q' && !end_flag) {
        chin = ERR;
        while (chin == ERR && !end_flag) {
            chin = wgetch(win);
        }
        if (end_flag) { break; }

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
                continue;
        }
    }

    displayt.request_stop();

    deinit_ncurses(win);

    for (const SnakeBody& piece : game.body) {
        std::cout << "count: " << piece.count << ", direction: " << direction_to_str(piece.direction) << '\n';
    }
    
    return 0;
}
