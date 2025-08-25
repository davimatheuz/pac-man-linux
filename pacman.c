#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

#define ALTURA 17
#define LARGURA 28
#define PACMAN_CHAR 'C'
#define DELAY 250000
#define PILL_CHAR '*'
#define EMPTY_CHAR ' '

typedef struct {
    int x, y;
    int dx, dy;
} Pacman;

Pacman pacman;
int score = 0;

char pilulas[ALTURA][LARGURA];
const char *mapa[ALTURA] = {
    "+---------+-- --+---------+",
    "|         |     |         |",
    "| -- | -- | --- | -- | -- |",
    "|    |               |    |",
    "+-- -+ ---+ --- +--- +- --+",
    "|         |     |         |",
    "| -- | -- | +-+ | -- | -- |",
    "|    |      | |      |    |",
    "+-- -+ ---+ +-+ +--- +- --+",
    "|         |     |         |",
    "| -- | -- | --- | -- | -- |",
    "|    |               |    |",
    "+-- -+ ---+ --- +--- +- --+",
    "|         |     |         |",
    "| -- | -- | --- | -- | -- |",
    "|    |               |    |",
    "+------------ ------------+"
};

// Funções de manipulação do terminal
struct termios old_tio, new_tio;

void restaurar_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}

void configurar_terminal() {
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    atexit(restaurar_terminal);
}

// Função que monitora o teclado
int kbhit() {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

// Função que lê o caractere inserido
char getch() {
    char buf = 0;
    read(STDIN_FILENO, &buf, 1);
    return buf;
}

// Funções auxiliares
void processar_entrada() {
    if (kbhit()) {
        char c = getch();
        switch (c) {
            case 'w': pacman.dx = 0; pacman.dy = -1; break;
            case 'a': pacman.dx = -1; pacman.dy = 0; break;
            case 's': pacman.dx = 0; pacman.dy = 1; break;
            case 'd': pacman.dx = 1; pacman.dy = 0; break;
            case 'q': exit(0); break; // Saída limpa
        }
    }
}

void atualizar_logica() {
    int next_x = pacman.x + pacman.dx;
    int next_y = pacman.y + pacman.dy;

    if (mapa[next_y][next_x] == EMPTY_CHAR) {
        pacman.x = next_x;
        pacman.y = next_y;
    }

    if (pilulas[pacman.y][pacman.x] == PILL_CHAR) {
        score += 1;
        pilulas[pacman.y][pacman.x] = EMPTY_CHAR;
    }
}

void desenhar_tela() {
    printf("\033[2J\033[H");

    for (int y = 0; y < ALTURA; y++) {
        printf("%s\n", mapa[y]);
    }

    for (int y = 0; y < ALTURA; y++) {
        for (int x = 0; x < LARGURA; x++) {
            if (pilulas[y][x] == PILL_CHAR) {
                printf("\033[%d;%dH%c", y+1, x+1, PILL_CHAR);
            }
        }
    }

    printf("\033[%d;%dH%c", pacman.y + 1, pacman.x + 1, PACMAN_CHAR);
    
    printf("\033[%d;%dH", ALTURA + 2, 1);
    printf("Score: %d", score);


    fflush(stdout);
}

void mostrar_cursor() {
    printf("\033[?25h");
}

void inicializar_jogo() {
    setlocale(LC_ALL, "");
    configurar_terminal();
    printf("\033[?25l");
    atexit(mostrar_cursor);

    pacman.x = 13;
    pacman.y = 13;
    pacman.dx = 0;
    pacman.dy = 0;

    score = 0;

    for (int y = 0; y < ALTURA; y++) {
        for (int x = 0; x < LARGURA; x++) {
            if (mapa[y][x] == ' ') {
                pilulas[y][x] = PILL_CHAR;
            } else {
                pilulas[y][x] = EMPTY_CHAR;
            }
        }
    }
    pilulas[7][13] = EMPTY_CHAR;
    pilulas[0][13] = EMPTY_CHAR;
    pilulas[16][13] = EMPTY_CHAR;
}

int main() {
    inicializar_jogo();

    while (1) {
        processar_entrada();
        atualizar_logica();
        desenhar_tela();
        usleep(DELAY);
    }

    return 0;
}