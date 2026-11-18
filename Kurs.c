#define _CRT_SECURE_NO_WARNINGS
#include <GL/glew.h>
#include <GL/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define CELL_SIZE 20

int maze_width = 20;
int maze_height = 15;

// [О1] лабиринт одним массивом, клетка = y*W + x
unsigned char* maze = NULL;

int player_x, player_y;
int exit_x, exit_y;
int show_path = 0;
int has_won = 0;
// зерно генератора
unsigned int maze_seed = 0;
GLFWwindow* window;

// размер окна
int win_w = WINDOW_WIDTH, win_h = WINDOW_HEIGHT;
// [О4] нужна ли перерисовка
int need_redraw = 1;

// для замеров
int last_visited = 0;  // раскрыто клеток
int last_path_len = -1;  // длина пути, -1 если нет

// [О3] путь списком клеток
int* path_cells = NULL;
int path_len = 0;

// [О3] буферы A*, выделяются один раз на лабиринт
int* a_g = NULL;  // расстояние от игрока
int* a_parent = NULL;  // откуда пришли
unsigned int* a_seen = NULL;  // посещена в этом поиске
unsigned int* a_closed = NULL;  // раскрыта в этом поиске
int* a_bucket0 = NULL;  // корзины
int* a_bucket1 = NULL;
unsigned int a_epoch = 0;  // номер поиска

int dx[] = { 0, 1, 0, -1 };
int dy[] = { -1, 0, 1, 0 };

// время в мс
double now_ms() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}


// ===== генерация =====
typedef struct {
    int x, y;
    unsigned char dirs[4];
    int next;
} GenFrame;

static void gen_enter(GenFrame* f, int x, int y) {
    maze[y * maze_width + x] = 1;
    f->x = x;
    f->y = y;
    f->next = 0;
    unsigned char d[4] = { 0, 1, 2, 3 };
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        unsigned char t = d[i];
        d[i] = d[j];
        d[j] = t;
    }
    memcpy(f->dirs, d, 4);
}

static void generate_maze_iterative(int start_x, int start_y) {
    const int W = maze_width, H = maze_height;
    // глубже числа узловых клеток не уйдём
    int cap = ((W - 1) / 2) * ((H - 1) / 2) + 1;
    GenFrame* stack = (GenFrame*)malloc(cap * sizeof(GenFrame));
    int top = 0;

    gen_enter(&stack[top++], start_x, start_y);
    while (top > 0) {
        GenFrame* f = &stack[top - 1];
        if (f->next == 4) {  // назад
            top--;
            continue;
        }
        int dir = f->dirs[f->next++];
        int nx = f->x + dx[dir] * 2;
        int ny = f->y + dy[dir] * 2;
        if (nx > 0 && nx < W - 1 && ny > 0 && ny < H - 1 && maze[ny * W + nx] == 0) {
            maze[(f->y + dy[dir]) * W + (f->x + dx[dir])] = 1;  // ломаем стену
            gen_enter(&stack[top++], nx, ny);  // дальше
        }
    }
    free(stack);
}

void free_maze() {
    free(maze);       maze = NULL;
    free(path_cells); path_cells = NULL;
    free(a_g);        a_g = NULL;
    free(a_parent);   a_parent = NULL;
    free(a_seen);     a_seen = NULL;
    free(a_closed);   a_closed = NULL;
    free(a_bucket0);  a_bucket0 = NULL;
    free(a_bucket1);  a_bucket1 = NULL;
    path_len = 0;
}

// новый лабиринт
void generate_maze() {
    const int n = maze_width * maze_height;
    maze = (unsigned char*)calloc(n, 1);

    // [О3] буферы для A*
    path_cells = (int*)malloc(n * sizeof(int));
    a_g = (int*)malloc(n * sizeof(int));
    a_parent = (int*)malloc(n * sizeof(int));
    a_seen = (unsigned int*)calloc(n, sizeof(unsigned int));
    a_closed = (unsigned int*)calloc(n, sizeof(unsigned int));
    // хватает n на корзину
    a_bucket0 = (int*)malloc(n * sizeof(int));
    a_bucket1 = (int*)malloc(n * sizeof(int));
    a_epoch = 0;
    path_len = 0;

    // новое зерно каждый раз
    maze_seed = (unsigned int)time(NULL) ^ ((unsigned int)clock() * 2654435761u) ^ (maze_seed * 31u + 1u);
    srand(maze_seed);
    printf("Maze %dx%d, seed %u\n", maze_width, maze_height, maze_seed);
    int start_x = 1;
    int start_y = 1;
    if (maze_width > 2 && maze_height > 2) {
        start_x = 1 + 2 * (rand() % ((maze_width - 1) / 2));
        start_y = 1 + 2 * (rand() % ((maze_height - 1) / 2));
    }

    generate_maze_iterative(start_x, start_y);

    player_x = 1;
    player_y = 1;
    exit_x = maze_width - 2;
    exit_y = maze_height - 2;
    has_won = 0;
    need_redraw = 1;
}

// ===== A* =====
void find_shortest_path_astar() {
    const int W = maze_width;
    // [О1] соседи: вверх, вправо, вниз, влево
    const int off[4] = { -W, 1, W, -1 };
    const unsigned char* M = maze;
    int* g = a_g;
    int* parent = a_parent;
    unsigned int* seen = a_seen;
    unsigned int* closed = a_closed;
    int* cur = a_bucket0;  // f
    int* next = a_bucket1;  // f + 2
    int cur_size = 0, next_size = 0;

    // [О3] новая эпоха вместо очистки массивов
    unsigned int ep = ++a_epoch;
    if (ep == 0) {
        memset(seen, 0, (size_t)W * maze_height * sizeof(unsigned int));
        memset(closed, 0, (size_t)W * maze_height * sizeof(unsigned int));
        ep = a_epoch = 1;
    }

    const int start = player_y * W + player_x;
    const int goal = exit_y * W + exit_x;

    last_visited = 0;
    last_path_len = -1;
    path_len = 0;

    g[start] = 0;
    parent[start] = -1;
    seen[start] = ep;
    cur[cur_size++] = start;

    for (;;) {
        if (cur_size == 0) {
            if (next_size == 0) break;  // пути нет
            // корзина пуста, берём следующую
            int* tmp = cur;
            cur = next;
            next = tmp;
            cur_size = next_size;
            next_size = 0;
        }

        const int idx = cur[--cur_size];
        if (closed[idx] == ep) continue;  // старая запись
        closed[idx] = ep;
        last_visited++;

        if (idx == goal) break;  // дошли

        // [О1] один раз на все 4 соседа
        const int ng = g[idx] + 1;
        for (int i = 0; i < 4; i++) {
            // [О1] границы не проверяем, по краям стены
            const int nidx = idx + off[i];
            if (!M[nidx] || closed[nidx] == ep) continue;
            if (seen[nidx] != ep || ng < g[nidx]) {
                seen[nidx] = ep;
                g[nidx] = ng;
                parent[nidx] = idx;
                if (i == 1 || i == 2) {
                    cur[cur_size++] = nidx;  // вправо/вниз
                }
                else {
                    next[next_size++] = nidx;  // вверх/влево
                }
            }
        }
    }

    if (closed[goal] == ep) {
        last_path_len = g[goal];
        for (int idx = goal; idx != -1; idx = parent[idx]) {
            path_cells[path_len++] = idx;
        }
    }
}

// замер: гоняем поиск >= 200 мс, возвращаем среднее
double benchmark_search(void (*search)(void), int* runs_out) {
    int runs = 0;
    double t0 = now_ms();
    double t;
    do {
        search();
        runs++;
        t = now_ms();
    } while (t - t0 < 200.0 || runs < 5);
    *runs_out = runs;
    return (t - t0) / runs;
}

// [О4] квадрат
static inline void quad(float x0, float y0, float x1, float y1) {
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
}

// сообщение о победе
void draw_win_message() {
    float panel_w = 380.0f;
    float panel_h = 80.0f;
    float panel_x = (win_w - panel_w) / 2.0f;
    float panel_y = (win_h - panel_h) / 2.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.85f);
    glBegin(GL_QUADS);
    quad(panel_x, panel_y, panel_x + panel_w, panel_y + panel_h);
    glEnd();
    glDisable(GL_BLEND);

    glColor3f(1.0f, 0.84f, 0.0f);
    glLineWidth(2.5f);
    float x = panel_x + 30;
    float y = panel_y + 50;

    // C
    glBegin(GL_LINE_STRIP);
    glVertex2f(x + 20, y - 20); glVertex2f(x + 5, y - 20);
    glVertex2f(x + 5, y + 5); glVertex2f(x + 20, y + 5);
    glEnd();
    x += 25;

    // O
    glBegin(GL_LINE_LOOP);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 20, y - 20);
    glVertex2f(x + 20, y + 5); glVertex2f(x + 5, y + 5);
    glEnd();
    x += 25;

    // N
    glBegin(GL_LINES);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 5, y + 5);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 20, y + 5);
    glVertex2f(x + 20, y - 20); glVertex2f(x + 20, y + 5);
    glEnd();
    x += 25;

    // G
    glBegin(GL_LINE_STRIP);
    glVertex2f(x + 20, y - 20); glVertex2f(x + 5, y - 20);
    glVertex2f(x + 5, y + 5); glVertex2f(x + 20, y + 5);
    glVertex2f(x + 20, y - 5); glVertex2f(x + 12, y - 5);
    glEnd();
    x += 25;

    // R
    glBegin(GL_LINES);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 5, y + 5);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 20, y - 20);
    glVertex2f(x + 20, y - 20); glVertex2f(x + 20, y - 5);
    glVertex2f(x + 20, y - 5); glVertex2f(x + 5, y - 5);
    glVertex2f(x + 5, y - 5); glVertex2f(x + 20, y + 5);
    glEnd();
    x += 25;

    // A
    glBegin(GL_LINES);
    glVertex2f(x + 5, y + 5); glVertex2f(x + 12, y - 20);
    glVertex2f(x + 12, y - 20); glVertex2f(x + 20, y + 5);
    glVertex2f(x + 8, y - 5); glVertex2f(x + 17, y - 5);
    glEnd();
    x += 25;

    // T
    glBegin(GL_LINES);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 20, y - 20);
    glVertex2f(x + 12, y - 20); glVertex2f(x + 12, y + 5);
    glEnd();
    x += 25;

    // U
    glBegin(GL_LINE_STRIP);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 5, y + 5);
    glVertex2f(x + 20, y + 5); glVertex2f(x + 20, y - 20);
    glEnd();
    x += 25;

    // L
    glBegin(GL_LINES);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 5, y + 5);
    glVertex2f(x + 5, y + 5); glVertex2f(x + 20, y + 5);
    glEnd();
    x += 25;

    // A
    glBegin(GL_LINES);
    glVertex2f(x + 5, y + 5); glVertex2f(x + 12, y - 20);
    glVertex2f(x + 12, y - 20); glVertex2f(x + 20, y + 5);
    glVertex2f(x + 8, y - 5); glVertex2f(x + 17, y - 5);
    glEnd();
    x += 25;

    // T
    glBegin(GL_LINES);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 20, y - 20);
    glVertex2f(x + 12, y - 20); glVertex2f(x + 12, y + 5);
    glEnd();
    x += 25;

    // I
    glBegin(GL_LINES);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 20, y - 20);
    glVertex2f(x + 12, y - 20); glVertex2f(x + 12, y + 5);
    glVertex2f(x + 5, y + 5); glVertex2f(x + 20, y + 5);
    glEnd();
    x += 25;

    // O
    glBegin(GL_LINE_LOOP);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 20, y - 20);
    glVertex2f(x + 20, y + 5); glVertex2f(x + 5, y + 5);
    glEnd();
    x += 25;

    // N
    glBegin(GL_LINES);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 5, y + 5);
    glVertex2f(x + 5, y - 20); glVertex2f(x + 20, y + 5);
    glVertex2f(x + 20, y - 20); glVertex2f(x + 20, y + 5);
    glEnd();
    x += 25;

    // S
    glBegin(GL_LINE_STRIP);
    glVertex2f(x + 20, y - 20); glVertex2f(x + 5, y - 20);
    glVertex2f(x + 5, y - 5); glVertex2f(x + 20, y - 5);
    glVertex2f(x + 20, y + 5); glVertex2f(x + 5, y + 5);
    glEnd();
    x += 25;

    // !
    glBegin(GL_LINES);
    glVertex2f(x + 12, y - 20); glVertex2f(x + 12, y - 5);
    glEnd();
    glPointSize(3.0f);
    glBegin(GL_POINTS);
    glVertex2f(x + 12, y + 2);
    glEnd();
}

// отрисовка
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    const int W = maze_width;
    // [О4] только видимые клетки
    int vis_w = (win_w + CELL_SIZE - 1) / CELL_SIZE;
    int vis_h = (win_h + CELL_SIZE - 1) / CELL_SIZE;
    if (vis_w > maze_width) vis_w = maze_width;
    if (vis_h > maze_height) vis_h = maze_height;

    // [О4] один glBegin на весь кадр
    glBegin(GL_QUADS);

    // фон белым, сверху стены
    glColor3f(1.0f, 1.0f, 1.0f);
    quad(0.0f, 0.0f, (float)(vis_w * CELL_SIZE), (float)(vis_h * CELL_SIZE));

    glColor3f(0.3f, 0.3f, 0.3f);
    for (int y = 0; y < vis_h; y++) {
        const unsigned char* row = maze + y * W;  // [О4] строка
        const float y0 = (float)(y * CELL_SIZE);
        const float y1 = y0 + CELL_SIZE;
        float x0 = 0.0f;  // [О4] без умножения
        for (int x = 0; x < vis_w; x++, x0 += CELL_SIZE) {
            if (!row[x]) {
                quad(x0, y0, x0 + CELL_SIZE, y1);
            }
        }
    }

    // [О3] рисуем только клетки пути
    if (show_path && path_len > 0) {
        glColor3f(1.0f, 0.0f, 0.0f);
        for (int k = 0; k < path_len; k++) {
            int x = path_cells[k] % W;
            int y = path_cells[k] / W;
            if (x < vis_w && y < vis_h) {
                float x0 = (float)(x * CELL_SIZE), y0 = (float)(y * CELL_SIZE);
                quad(x0 + 4, y0 + 4, x0 + CELL_SIZE - 4, y0 + CELL_SIZE - 4);
            }
        }
    }

    glColor3f(0.0f, 0.0f, 1.0f);
    {
        float x0 = (float)(player_x * CELL_SIZE), y0 = (float)(player_y * CELL_SIZE);
        quad(x0 + 2, y0 + 2, x0 + CELL_SIZE - 2, y0 + CELL_SIZE - 2);
    }

    glColor3f(0.0f, 1.0f, 0.0f);
    {
        float x0 = (float)(exit_x * CELL_SIZE), y0 = (float)(exit_y * CELL_SIZE);
        quad(x0 + 4, y0 + 4, x0 + CELL_SIZE - 4, y0 + CELL_SIZE - 4);
    }

    glEnd();

    if (has_won) {
        draw_win_message();
    }

    glfwSwapBuffers(window);
}

// клавиши
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        need_redraw = 1;
        if (key == GLFW_KEY_N) {
            free_maze();
            generate_maze();
            show_path = 0;
            return;
        }
        if (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return;
        }

        if (has_won) return;

        // [О1] границы не проверяем, по краям стены
        const int W = maze_width;
        const int p = player_y * W + player_x;
        switch (key) {
        case GLFW_KEY_W:
            if (maze[p - W]) player_y--;
            break;
        case GLFW_KEY_S:
            if (maze[p + W]) player_y++;
            break;
        case GLFW_KEY_A:
            if (maze[p - 1]) player_x--;
            break;
        case GLFW_KEY_D:
            if (maze[p + 1]) player_x++;
            break;
        case GLFW_KEY_SPACE: {
            show_path = 1;
            double t0 = now_ms();
            find_shortest_path_astar();
            double t1 = now_ms();
            printf("A*: %.4f ms, visited %d cells, path %d steps\n",
                t1 - t0, last_visited, last_path_len);
            break;
        }
        case GLFW_KEY_T: {
            // замер A*
            int runs;
            double avg = benchmark_search(find_shortest_path_astar, &runs);
            printf("[benchmark] maze %dx%d, seed %u, player (%d,%d)\n",
                maze_width, maze_height, maze_seed, player_x, player_y);
            printf("  A*: avg %.4f ms over %d runs, visited %d cells, path %d steps\n",
                avg, runs, last_visited, last_path_len);
            break;
        }
        }

        if (player_x == exit_x && player_y == exit_y) {
            has_won = 1;
            printf("Congratulations! You escaped the maze!\n");
        }
    }
    else if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_SPACE) {
            show_path = 0;
            need_redraw = 1;
        }
    }
}

// изменение размера окна
void resizeCallback(GLFWwindow* window, int width, int height) {
    win_w = width;
    win_h = height;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    need_redraw = 1;
}

// перерисовка окна
void refreshCallback(GLFWwindow* window) {
    display();
    need_redraw = 0;
}

int main(int argc, char** argv) {
    printf("Enter maze size (width height): ");
    scanf("%d %d", &maze_width, &maze_height);

    if (maze_width < 5) maze_width = 5;
    if (maze_height < 5) maze_height = 5;
    if (maze_width % 2 == 0) maze_width++;
    if (maze_height % 2 == 0) maze_height++;

    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Maze", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetWindowSizeCallback(window, resizeCallback);
    glfwSetWindowRefreshCallback(window, refreshCallback);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW: %s\n", glewGetErrorString(err));
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    resizeCallback(window, width, height);

    generate_maze();

    printf("Controls:\n");
    printf("WASD - move player\n");
    printf("SPACE - show shortest path (press/hold)\n");
    printf("T - benchmark path search (average time)\n");
    printf("N - new maze (same size)\n");
    printf("Q or ESC - quit\n");

    // [О4] рисуем только когда что-то поменялось
    while (!glfwWindowShouldClose(window)) {
        if (need_redraw) {
            display();
            need_redraw = 0;
        }
        glfwWaitEvents();
    }

    free_maze();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
