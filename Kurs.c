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
int** maze;
int player_x, player_y;
int exit_x, exit_y;
int** path;
int show_path = 0;
int has_won = 0;
GLFWwindow* window;

int dx[] = { 0, 1, 0, -1 };
int dy[] = { -1, 0, 1, 0 };

// подсчитывает непосещённых соседей
int check_neighbors(int x, int y) {
    int count = 0;
    for (int i = 0; i < 4; i++) {
        int nx = x + dx[i] * 2;
        int ny = y + dy[i] * 2;
        if (nx > 0 && nx < maze_width - 1 && ny > 0 && ny < maze_height - 1) {
            if (maze[ny][nx] == 0) {
                count++;
            }
        }
    }
    return count;
}

// рекурсивная генерация лабиринта
void generate_maze_recursive(int x, int y) {
    maze[y][x] = 1;

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
            if (maze[ny][nx] == 0) {
                maze[y + dy[dir]][x + dx[dir]] = 1;
                generate_maze_recursive(nx, ny);
            }
        }
    }
}

// создаёт новый лабиринт, задаёт вход и выход
void generate_maze() {
    maze = (int**)malloc(maze_height * sizeof(int*));
    for (int i = 0; i < maze_height; i++) {
        maze[i] = (int*)malloc(maze_width * sizeof(int));
        for (int j = 0; j < maze_width; j++) {
            maze[i][j] = 0;
        }
    }

    srand(time(NULL));
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
    maze[player_y][player_x] = 1;
    maze[exit_y][exit_x] = 1;
    has_won = 0;
}

// поиск кратчайшего пути алгоритмом BFS
void find_shortest_path() {
    if (path != NULL) {
        for (int i = 0; i < maze_height; i++) {
            free(path[i]);
        }
        free(path);
    }

    path = (int**)malloc(maze_height * sizeof(int*));
    for (int i = 0; i < maze_height; i++) {
        path[i] = (int*)malloc(maze_width * sizeof(int));
        for (int j = 0; j < maze_width; j++) {
            path[i][j] = 0;
        }
    }

    int queue_x[10000];
    int queue_y[10000];
    int front = 0, rear = 0;

    int** dist = (int**)malloc(maze_height * sizeof(int*));
    for (int i = 0; i < maze_height; i++) {
        dist[i] = (int*)malloc(maze_width * sizeof(int));
        for (int j = 0; j < maze_width; j++) {
            dist[i][j] = -1;
        }
    }

    queue_x[rear] = player_x;
    queue_y[rear] = player_y;
    rear++;
    dist[player_y][player_x] = 0;

    while (front < rear) {
        int x = queue_x[front];
        int y = queue_y[front];
        front++;

        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];

            if (nx >= 0 && nx < maze_width && ny >= 0 && ny < maze_height) {
                if (maze[ny][nx] == 1 && dist[ny][nx] == -1) {
                    dist[ny][nx] = dist[y][x] + 1;
                    queue_x[rear] = nx;
                    queue_y[rear] = ny;
                    rear++;
                }
            }
        }
    }

    if (dist[exit_y][exit_x] != -1) {
        int x = exit_x;
        int y = exit_y;

        while (x != player_x || y != player_y) {
            path[y][x] = 1;

            for (int i = 0; i < 4; i++) {
                int nx = x + dx[i];
                int ny = y + dy[i];

                if (nx >= 0 && nx < maze_width && ny >= 0 && ny < maze_height) {
                    if (dist[ny][nx] == dist[y][x] - 1) {
                        x = nx;
                        y = ny;
                        break;
                    }
                }
            }
        }
        path[player_y][player_x] = 1;
    }

    for (int i = 0; i < maze_height; i++) {
        free(dist[i]);
    }
    free(dist);
}

// рисует поздравление при победе
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

// отрисовывает лабиринт, игрока, выход и путь
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    for (int y = 0; y < maze_height; y++) {
        for (int x = 0; x < maze_width; x++) {
            if (maze[y][x] == 0) {
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
                if (path[y][x] == 1 && maze[y][x] == 1) {
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

// обрабатывает нажатия клавиш
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_N) {
            for (int i = 0; i < maze_height; i++) {
                free(maze[i]);
            }
            free(maze);
            generate_maze();
            if (path != NULL) {
                for (int i = 0; i < maze_height; i++) {
                    free(path[i]);
                }
                free(path);
                path = NULL;
            }
            show_path = 0;
            return;
        }
        if (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return;
        }

        if (has_won) return;

        switch (key) {
        case GLFW_KEY_W:
            if (player_y > 0 && maze[player_y - 1][player_x] == 1) {
                player_y--;
            }
            break;
        case GLFW_KEY_S:
            if (player_y < maze_height - 1 && maze[player_y + 1][player_x] == 1) {
                player_y++;
            }
            break;
        case GLFW_KEY_A:
            if (player_x > 0 && maze[player_y][player_x - 1] == 1) {
                player_x--;
            }
            break;
        case GLFW_KEY_D:
            if (player_x < maze_width - 1 && maze[player_y][player_x + 1] == 1) {
                player_x++;
            }
            break;
        case GLFW_KEY_SPACE:
            show_path = 1;
            find_shortest_path();
            break;
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

// настраивает проекцию при изменении размера окна
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
    printf("N - new maze (same size)\n");
    printf("Q or ESC - quit\n");

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);
        display();
        glfwPollEvents();
    }

    for (int i = 0; i < maze_height; i++) {
        free(maze[i]);
    }
    free(maze);

    if (path != NULL) {
        for (int i = 0; i < maze_height; i++) {
            free(path[i]);
        }
        free(path);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}