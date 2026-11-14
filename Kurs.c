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

// для замеров
int last_visited = 0;  // раскрыто клеток
int last_path_len = -1;  // длина пути, -1 если нет

int** path = NULL;

int dx[] = { 0, 1, 0, -1 };
int dy[] = { -1, 0, 1, 0 };

// время в мс
double now_ms() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

// подсчёт соседей
int check_neighbors(int x, int y) {
    int count = 0;
    for (int i = 0; i < 4; i++) {
        int nx = x + dx[i] * 2;
        int ny = y + dy[i] * 2;
        if (nx > 0 && nx < maze_width - 1 && ny > 0 && ny < maze_height - 1) {
            if (maze[ny * maze_width + nx] == 0) {
                count++;
            }
        }
    }
    return count;
}

// ===== генерация =====
void generate_maze_recursive(int x, int y) {
    maze[y * maze_width + x] = 1;

    int dirs[4] = { 0, 1, 2, 3 };
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = dirs[i];
        dirs[i] = dirs[j];
        dirs[j] = temp;
    }

    for (int i = 0; i < 4; i++) {
        int dir = dirs[i];
        int nx = x + dx[dir] * 2;
        int ny = y + dy[dir] * 2;

        if (nx > 0 && nx < maze_width - 1 && ny > 0 && ny < maze_height - 1) {
            if (maze[ny * maze_width + nx] == 0) {
                maze[(y + dy[dir]) * maze_width + (x + dx[dir])] = 1;
                generate_maze_recursive(nx, ny);
            }
        }
    }
}

void free_maze() {
    free(maze);
    maze = NULL;
    if (path != NULL) {
        for (int i = 0; i < maze_height; i++) {
            free(path[i]);
        }
        free(path);
        path = NULL;
    }
}

// новый лабиринт
void generate_maze() {
    const int n = maze_width * maze_height;
    maze = (unsigned char*)calloc(n, 1);

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

    generate_maze_recursive(start_x, start_y);

    player_x = 1;
    player_y = 1;
    exit_x = maze_width - 2;
    exit_y = maze_height - 2;
    maze[player_y * maze_width + player_x] = 1;
    maze[exit_y * maze_width + exit_x] = 1;
    has_won = 0;
}

// ===== A* =====
void find_shortest_path_astar() {
    if (path != NULL) {
        for (int i = 0; i < maze_height; i++) {
            free(path[i]);
        }
        free(path);
    }

    path = (int**)malloc(maze_height * sizeof(int*));
    for (int i = 0; i < maze_height; i++) {
        path[i] = (int*)calloc(maze_width, sizeof(int));
    }

    const int W = maze_width;
    const int n = W * maze_height;
    // [О1] соседи: вверх, вправо, вниз, влево
    const int off[4] = { -W, 1, W, -1 };
    const unsigned char* M = maze;
    int* g = (int*)malloc(n * sizeof(int));  // расстояние от игрока
    int* parent = (int*)malloc(n * sizeof(int));  // откуда пришли
    unsigned char* closed = (unsigned char*)calloc(n, 1);  // раскрыта
    // корзины f и f+2
    int* cur = (int*)malloc((4 * n + 1) * sizeof(int));
    int* next = (int*)malloc((4 * n + 1) * sizeof(int));
    int cur_size = 0, next_size = 0;

    for (int i = 0; i < n; i++) g[i] = -1;

    const int start = player_y * W + player_x;
    const int goal = exit_y * W + exit_x;

    last_visited = 0;
    last_path_len = -1;

    g[start] = 0;
    parent[start] = -1;
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
        if (closed[idx]) continue;  // старая запись
        closed[idx] = 1;
        last_visited++;

        if (idx == goal) break;  // дошли

        // [О1] один раз на все 4 соседа
        const int ng = g[idx] + 1;
        for (int i = 0; i < 4; i++) {
            // [О1] границы не проверяем, по краям стены
            const int nidx = idx + off[i];
            if (!M[nidx] || closed[nidx]) continue;
            if (g[nidx] == -1 || ng < g[nidx]) {
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

    if (closed[goal]) {
        last_path_len = g[goal];
        for (int idx = goal; idx != -1; idx = parent[idx]) {
            path[idx / W][idx % W] = 1;
        }
    }

    free(g);
    free(parent);
    free(closed);
    free(cur);
    free(next);
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

// сообщение о победе
void draw_win_message() {
    int window_width, window_height;
    glfwGetWindowSize(window, &window_width, &window_height);

    float panel_w = 380.0f;
    float panel_h = 80.0f;
    float panel_x = (window_width - panel_w) / 2.0f;
    float panel_y = (window_height - panel_h) / 2.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.85f);
    glBegin(GL_QUADS);
    glVertex2f(panel_x, panel_y);
    glVertex2f(panel_x + panel_w, panel_y);
    glVertex2f(panel_x + panel_w, panel_y + panel_h);
    glVertex2f(panel_x, panel_y + panel_h);
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

    for (int y = 0; y < maze_height; y++) {
        for (int x = 0; x < maze_width; x++) {
            if (maze[y * maze_width + x] == 0) {
                glColor3f(0.3f, 0.3f, 0.3f);
            }
            else {
                glColor3f(1.0f, 1.0f, 1.0f);
            }

            glBegin(GL_QUADS);
            glVertex2f(x * CELL_SIZE, y * CELL_SIZE);
            glVertex2f((x + 1) * CELL_SIZE, y * CELL_SIZE);
            glVertex2f((x + 1) * CELL_SIZE, (y + 1) * CELL_SIZE);
            glVertex2f(x * CELL_SIZE, (y + 1) * CELL_SIZE);
            glEnd();
        }
    }

    if (show_path && path != NULL) {
        glColor3f(1.0f, 0.0f, 0.0f);
        for (int y = 0; y < maze_height; y++) {
            for (int x = 0; x < maze_width; x++) {
                if (path[y][x] == 1 && maze[y * maze_width + x] == 1) {
                    glBegin(GL_QUADS);
                    glVertex2f(x * CELL_SIZE + 4, y * CELL_SIZE + 4);
                    glVertex2f((x + 1) * CELL_SIZE - 4, y * CELL_SIZE + 4);
                    glVertex2f((x + 1) * CELL_SIZE - 4, (y + 1) * CELL_SIZE - 4);
                    glVertex2f(x * CELL_SIZE + 4, (y + 1) * CELL_SIZE - 4);
                    glEnd();
                }
            }
        }
    }

    glColor3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(player_x * CELL_SIZE + 2, player_y * CELL_SIZE + 2);
    glVertex2f((player_x + 1) * CELL_SIZE - 2, player_y * CELL_SIZE + 2);
    glVertex2f((player_x + 1) * CELL_SIZE - 2, (player_y + 1) * CELL_SIZE - 2);
    glVertex2f(player_x * CELL_SIZE + 2, (player_y + 1) * CELL_SIZE - 2);
    glEnd();

    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(exit_x * CELL_SIZE + 4, exit_y * CELL_SIZE + 4);
    glVertex2f((exit_x + 1) * CELL_SIZE - 4, exit_y * CELL_SIZE + 4);
    glVertex2f((exit_x + 1) * CELL_SIZE - 4, (exit_y + 1) * CELL_SIZE - 4);
    glVertex2f(exit_x * CELL_SIZE + 4, (exit_y + 1) * CELL_SIZE - 4);
    glEnd();

    if (has_won) {
        draw_win_message();
    }

    glfwSwapBuffers(window);
}

// клавиши
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
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
        }
    }
}

// изменение размера окна
void resizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
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

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);
        display();
        glfwPollEvents();
    }

    free_maze();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
