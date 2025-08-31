#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <string.h>
#include <math.h>

#define ALTURA 17
#define LARGURA 28
#define PACMAN_CHAR 'C'
#define GHOST_CHAR '@'
#define DELAY 250000
#define PILL_CHAR '*'
#define EMPTY_CHAR ' '
#define NUM_GHOSTS 2

typedef struct {
    int x, y;
    int dx, dy;
    int next_dx, next_dy;
} Pacman;

typedef struct {
    int x, y;
    int dx, dy;
    int isinbox;
} Ghost;

Pacman pacman;
Ghost ghosts[NUM_GHOSTS];
int score = 0;
int game_over = 0;

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

// mostra o cursor quando o jogo acaba e configura a posição correta para printar a entrada
void mostrar_cursor() {
    printf("\033[?25h");
    printf("\033[%d;%dH", ALTURA + 2, 20);
    printf("\n");
}

int calcular_distancia_manhattan(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

void processar_entrada() {
    if (kbhit()) {
        char c = getch();
        switch (c) {
            case 'w': pacman.next_dx = 0; pacman.next_dy = -1; break;
            case 'a': pacman.next_dx = -1; pacman.next_dy = 0; break;
            case 's': pacman.next_dx = 0; pacman.next_dy = 1; break;
            case 'd': pacman.next_dx = 1; pacman.next_dy = 0; break;
            case 'q': exit(0); break;
        }
    }
}

void atualizar_logica_fantasmas() {
    for (int i = 0; i < NUM_GHOSTS; i++) {
        int alvo_x, alvo_y;

        // Verifica se o fantasma está dentro de sua caixa e atribui o alvo adequado
        if (ghosts[i].isinbox) {
            alvo_x = 13;
            alvo_y = 6;

            if (ghosts[i].x == alvo_x && ghosts[i].y == alvo_y) {
                ghosts[i].isinbox = 0;
            }
        }
        else {
            alvo_x = pacman.x;
            alvo_y = pacman.y;
        }

        int melhor_dx = 0, melhor_dy = 0;
        int menor_distancia = 9999;

        // Cima, esquerda, baixo, direita
        int direcoes[4][2] = {{0, -1}, {-1, 0}, {0, 1}, {1, 0}};

        if (i == 1) {
            direcoes[0][0] = -1; direcoes[0][1] = 0; // Esquerda
            direcoes[1][0] = 1;  direcoes[1][1] = 0; // Direita
            direcoes[2][0] = 0;  direcoes[2][1] = -1; // Cima
            direcoes[3][0] = 0;  direcoes[3][1] = 1;  // Baixo
        }

        // Travessia do túnel superior
        if (ghosts[i].x == 13 && ghosts[i].y == 0 && ghosts[i].dy == -1) {
            ghosts[i].y = 16;
        }
        // Travessia do túnel inferior
        else if (ghosts[i].x == 13 && ghosts[i].y == 16 && ghosts[i].dy == 1) {
            ghosts[i].y = 0;
        }
        // Verifica, para cada uma das direções, a menor distância até o pacman
        for (int j = 0; j < 4; j++) {
            int dx_teste = direcoes[j][0];
            int dy_teste = direcoes[j][1];

            // Impede o fantasma de mudar a direção 180º
            if (dx_teste == -ghosts[i].dx && dy_teste == -ghosts[i].dy) {
                continue;
            }

            int proximo_x = ghosts[i].x + dx_teste;
            int proximo_y = ghosts[i].y + dy_teste;

            if (!ghosts[i].isinbox && proximo_y == 7 && proximo_x == 13) {
                continue;
            }

            // Atualiza a menor distância até o pacman
            if (mapa[proximo_y][proximo_x] == EMPTY_CHAR) {
                int distancia = calcular_distancia_manhattan(proximo_x, proximo_y, alvo_x, alvo_y);
                if (distancia < menor_distancia) {
                    menor_distancia = distancia;
                    melhor_dx = dx_teste;
                    melhor_dy = dy_teste;
                }
            }
        }

        if (melhor_dx != 0 || melhor_dy != 0) {
            ghosts[i].dx = melhor_dx;
            ghosts[i].dy = melhor_dy;
        }

        ghosts[i].x += ghosts[i].dx;
        ghosts[i].y += ghosts[i].dy;
    }
}  

void atualizar_logica() {
    if (game_over) return;
    atualizar_logica_fantasmas();

    int intencao_x = pacman.x + pacman.next_dx;
    int intencao_y = pacman.y + pacman.next_dy;

    if ((pacman.next_dx != 0 || pacman.next_dy != 0) &&
        (intencao_y >= 0 && intencao_y < ALTURA && intencao_x >= 0 && intencao_x < LARGURA) &&
        mapa[intencao_y][intencao_x] == EMPTY_CHAR) {
        
        pacman.dx = pacman.next_dx;
        pacman.dy = pacman.next_dy;
    }

    int proximo_x = pacman.x + pacman.dx;
    int proximo_y = pacman.y + pacman.dy;

    if (pacman.x == 13 && pacman.y == 0 && pacman.dy == -1) {
        pacman.y = 16;
    } else if (pacman.x == 13 && pacman.y == 16 && pacman.dy == 1) {
        pacman.y = 0;
    } else if (mapa[proximo_y][proximo_x] == EMPTY_CHAR) {
        pacman.x = proximo_x;
        pacman.y = proximo_y;
    } else {
        pacman.dx = 0;
        pacman.dy = 0;
    }
    
    if (pilulas[pacman.y][pacman.x] == PILL_CHAR) {
        score += 1;
        pilulas[pacman.y][pacman.x] = EMPTY_CHAR;
    }

    for (int i = 0; i < NUM_GHOSTS; i++) {
        if (pacman.x == ghosts[i].x && pacman.y == ghosts[i].y) {
            game_over = 1;
        }
    }
}

void desenhar_tela() {
    char buffer_tela[(LARGURA + 1) * ALTURA + 1000];
    char *ptr = buffer_tela;
    ptr += sprintf(ptr, "\033[2J\033[H");

    // desenha o mapa, o pacman e as pílulas em um buffer, em seguida, imprime
    for (int y = 0; y < ALTURA; y++) {
        for (int x = 0; x < strlen(mapa[y]); x++) {
            int fantasma_aqui = 0;
            for (int i = 0; i < NUM_GHOSTS; i++) {
                if (ghosts[i].x == x && ghosts[i].y == y) {
                    fantasma_aqui = 1;
                    break;
                }
            }


            if (y == pacman.y && x == pacman.x) {
                ptr += sprintf(ptr, "%c", PACMAN_CHAR);
            } else if (fantasma_aqui) {
                ptr += sprintf(ptr, "%c", GHOST_CHAR);
            } else if (pilulas[y][x] == PILL_CHAR) {
                ptr += sprintf(ptr, "%c", PILL_CHAR);
            } else {
                ptr += sprintf(ptr, "%c", mapa[y][x]);
            }
        }
        ptr += sprintf(ptr, "\n");
    }

    ptr += sprintf(ptr, "\033[%d;%dH", ALTURA + 2, 1);
    ptr += sprintf(ptr, "Score: %d", score);

    if (game_over) {
        ptr += sprintf(ptr, "\033[%d;%dH", ALTURA / 2 + 1, LARGURA / 2 - 4);
        ptr += sprintf(ptr, "GAME OVER");
    }

    printf("%s", buffer_tela);
    fflush(stdout);
}

void inicializar_jogo() {
    configurar_terminal();
    printf("\033[?25l");
    atexit(mostrar_cursor);

    pacman.x = 13;
    pacman.y = 13;
    pacman.dx = 0;
    pacman.dy = 0;
    pacman.next_dx = 0;
    pacman.next_dy = 0;

    score = 0;
    game_over = 0;

    for (int i = 0; i < NUM_GHOSTS; i++) {
        ghosts[i].x = 13;
        ghosts[i].y = 7;
        ghosts[i].dx = 0;
        ghosts[i].dy = -1;
        ghosts[i].isinbox = 1;
    }

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
    pilulas[pacman.y][pacman.x] = EMPTY_CHAR;
}

int main() {
    inicializar_jogo();

    while (1) {
        processar_entrada();
        atualizar_logica();
        desenhar_tela();
        if (game_over) {
            usleep(3000000);
            break;
        }
        usleep(DELAY);
    }
    return 0;
}