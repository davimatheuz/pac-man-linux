#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>

#define ALTURA 17
#define LARGURA 28

#define PACMAN_CHAR         'C'
#define PACMAN_CLOSING_CHAR 'c'
#define PACMAN_DYING_CHAR_1 '('
#define PACMAN_DYING_CHAR_2 '.'

#define GHOST_CHAR         '@'
#define FLEEING_GHOST_CHAR 'w'
#define EATEN_GHOST_CHAR   'e'
#define NUM_GHOSTS          2

#define EMPTY_CHAR          ' '
#define PILL_CHAR           '.'
#define POWER_PILL_CHAR     '*'
#define PILLS_PER_LEVEL     238
#define POWER_PILL_DURATION 40

#define DELAY 250000
#define VIDAS_INICIAIS 4
#define SOUND_DIR "sounds/"

#define FRUIT_CHAR     'F'
#define FRUIT_DURATION 36
#define FRUIT_X        13
#define FRUIT_Y        9

#define MENU_CHAR_COUNT 5
#define MENU_DELAY      150000

#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[38;2;33;32;255m"
#define COLOR_YELLOW  "\033[38;2;255;255;0m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_RED     "\033[38;2;255;0;0m"
#define COLOR_PINK    "\033[38;2;255;186;255m"
#define COLOR_CYAN    "\033[38;2;0;255;255m"
#define COLOR_ORANGE  "\033[38;2;255;186;82m"
#define COLOR_SALMON  "\033[38;2;255;186;173m"

#define ENTER_ALT_SCREEN "\033[?1049h"
#define EXIT_ALT_SCREEN  "\033[?1049l"

typedef struct {
    int x;
    int y;
    const char* icon;
    const char* color;
} MenuCharacter;

typedef enum {
    IN_BOX,
    CHASING,
    FLEEING,
    EATEN
} Ghoststate;

typedef struct {
    int x, y;
    int dx, dy;
    int next_dx, next_dy;
} Pacman;

typedef struct {
    int x, y;
    int dx, dy;
    const char* color;
    Ghoststate state;
    int state_timer;
} Ghost;

Pacman pacman;
Ghost ghosts[NUM_GHOSTS];
int score = 0;
int lives = 0;
int game_over = 0;
int pills_captured;
int waka_toggle = 0;
int pacman_mouth_toggle = 0;
int ghost_fleeing_toggle = 0;
int fruit_visible = 0;
int fruit_timer = 0;
int power_pill_toggle = 0;
pid_t ambient_sound_pid = 0;

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

// protótipo de funções
void desenhar_tela(int desenhar_pacman, int desenhar_fantasmas, const char* map_color);
void play_sfx(const char* sound_file);
void play_sfx_blocking(const char* sound_file);
void start_siren(const char* intro_file, const char* loop_file);
void stop_siren();

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
void restauração_final() {
    stop_siren();
    printf("%s", EXIT_ALT_SCREEN);
    desenhar_tela(0, 0, NULL);
    printf("\n");
    printf("\033[?25h");
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
        int modo_busca = 0;

        switch (ghosts[i].state) {
            case IN_BOX:
                alvo_x = 13;
                alvo_y = 6;
                modo_busca = 0;

                if (ghosts[i].x == alvo_x && ghosts[i].y == alvo_y) {
                    ghosts[i].state = CHASING;
                }
                break;
            
            case CHASING:
                alvo_x = pacman.x;
                alvo_y = pacman.y;
                modo_busca = 0;
                break;
            
            case FLEEING:
                alvo_x = pacman.x;
                alvo_y = pacman.y;
                modo_busca = 1;

                if (ghosts[i].state_timer > 0) {
                    ghosts[i].state_timer--;
                }
                if(ghosts[i].state_timer == 0) {
                    ghosts[i].state = CHASING;
                    int fleeing_ghosts_left = 0;
                    for (int k = 0; k < NUM_GHOSTS; k++) {
                        if (ghosts[k].state == FLEEING) {
                            fleeing_ghosts_left = 1;
                            break;
                        }
                    }
                    int eaten_ghosts_left = 0;
                    for (int k = 0; k < NUM_GHOSTS; k++) {
                        if (ghosts[k].state == EATEN) {
                            eaten_ghosts_left = 1;
                            break;
                        }
                    }
                    if (!fleeing_ghosts_left && !eaten_ghosts_left) {
                        start_siren("siren0_firstloop.wav", "siren0.wav");
                    }
                }
                break;
            
            case EATEN:
                alvo_x = 13;
                alvo_y = 6;
                modo_busca = 0;
                break;
        }

        if (ghosts[i].state == EATEN && ghosts[i].x == alvo_x && ghosts[i].y == alvo_y) {
            ghosts[i].x = 13; ghosts[i].y = 7;
            ghosts[i].state = IN_BOX;
            ghosts[i].dx = 0; ghosts[i].dy = -1;

            int fleeing_ghosts_left = 0;
            for (int k = 0; k < NUM_GHOSTS; k++) {
                if (ghosts[k].state == FLEEING) {
                    fleeing_ghosts_left = 1;
                    break;
                }
            }

            if (fleeing_ghosts_left) {
                start_siren("fright_firstloop.wav", "fright.wav");
            } else {
                start_siren("siren0_firstloop.wav", "siren0.wav");
            }
            continue;
        }

        if (ghosts[i].state == FLEEING && ghost_fleeing_toggle == 1) {
            continue;
        }

        int melhor_dx = 0, melhor_dy = 0;
        // Cima, esquerda, baixo, direita
        int direcoes[4][2] = {{0, -1}, {-1, 0}, {0, 1}, {1, 0}};
        if (i == 1) {
            direcoes[0][0] = -1; direcoes[0][1] = 0; // Esquerda
            direcoes[1][0] = 1;  direcoes[1][1] = 0; // Direita
            direcoes[2][0] = 0;  direcoes[2][1] = -1; // Cima
            direcoes[3][0] = 0;  direcoes[3][1] = 1;  // Baixo
        }

        int draw_directions[4][2];
        int draw_counter = 0;
        int melhor_distancia = (modo_busca == 0) ? 9999 : -1;

        for (int j = 0; j < 4; j++) {
            int dx_teste = direcoes[j][0];
            int dy_teste = direcoes[j][1];

            if (dx_teste == -ghosts[i].dx && dy_teste == -ghosts[i].dy) {
                continue;
            }

            int proximo_x = ghosts[i].x + dx_teste;
            int proximo_y = ghosts[i].y + dy_teste;

            int eh_porta = (proximo_x == 13 && proximo_y == 6);

            if ((ghosts[i].state == CHASING || ghosts[i].state == FLEEING) && (ghosts[i].x == 13 && ghosts[i].y == 6)) {
                continue;
            }

            if (mapa[proximo_y][proximo_x] == EMPTY_CHAR || (ghosts[i].state == IN_BOX && eh_porta) ||
                (ghosts[i].state == EATEN && eh_porta)) {
                
                int distancia = calcular_distancia_manhattan(proximo_x, proximo_y, alvo_x, alvo_y);

                int nova_melhor_encontrada = (modo_busca == 0) ? (distancia < melhor_distancia) : (distancia > melhor_distancia);

                if (nova_melhor_encontrada) {
                    melhor_distancia = distancia;
                    draw_counter = 1;
                    draw_directions[0][0] = dx_teste;
                    draw_directions[0][1] = dy_teste;
                } else if (distancia == melhor_distancia) {
                    draw_directions[draw_counter][0] = dx_teste;
                    draw_directions[draw_counter][1] = dy_teste;
                    draw_counter++;
                }
            }
        }

        if (draw_counter > 0) {
            int escolha = rand() % draw_counter;
            melhor_dx = draw_directions[escolha][0];
            melhor_dy = draw_directions[escolha][1];
        } else {
            melhor_dx = ghosts[i].dx;
            melhor_dy = ghosts[i].dy;
        }

        ghosts[i].dx = melhor_dx;
        ghosts[i].dy = melhor_dy;

        ghosts[i].x += ghosts[i].dx;
        ghosts[i].y += ghosts[i].dy;

        // Travessia do túnel superior
        if (ghosts[i].x == 13 && ghosts[i].y == 0 && ghosts[i].dy == -1) {
            ghosts[i].y = 16;
        }
        // Travessia do túnel inferior
        else if (ghosts[i].x == 13 && ghosts[i].y == 16 && ghosts[i].dy == 1) {
            ghosts[i].y = 0;
        }
    }
}

void ready_screen_2() {
    stop_siren();
    desenhar_tela(1, 0, NULL);

    printf("\033[%d;%dH", ALTURA / 2 + 2, LARGURA / 2 - 3);
    printf("%sREADY!!", COLOR_YELLOW);
    fflush(stdout);
    usleep(2000000);

    start_siren("siren0_firstloop.wav", "siren0.wav");
}

void reset_positions() {
    pacman.x = 13;
    pacman.y = 13;
    pacman.dx = 0;
    pacman.dy = 0;
    pacman.next_dx = 0;
    pacman.next_dy = 0;
    waka_toggle = 0;
    fruit_visible = 0;
    fruit_timer = 0;
    power_pill_toggle = 0;
    pacman_mouth_toggle = 0;
    ghost_fleeing_toggle = 0;
    static int is_first_call = 1;

    if (pills_captured == PILLS_PER_LEVEL) {
        pills_captured = 0;
    }

    if (!is_first_call) {
        ready_screen_2();
    }
    is_first_call = 0;

    for (int i = 0; i < NUM_GHOSTS; i++) {
        ghosts[i].x = 13;
        ghosts[i].y = 7;
        ghosts[i].dx = 0;
        ghosts[i].dy = -1;
        ghosts[i].state = IN_BOX;
        ghosts[i].state_timer = 0;
    }
}

void preencher_pilulas() {
    for (int y = 0; y < ALTURA; y++) {
        for (int x = 0; x < LARGURA; x++) {
            if (mapa[y][x] == ' ') {
                pilulas[y][x] = PILL_CHAR;
            } else {
                pilulas[y][x] = EMPTY_CHAR;
            }
        }
    }

    pilulas[2][1] = POWER_PILL_CHAR;
    pilulas[2][25] = POWER_PILL_CHAR;
    pilulas[14][1] = POWER_PILL_CHAR;
    pilulas[14][25] = POWER_PILL_CHAR;

    pilulas[7][13] = EMPTY_CHAR;
    pilulas[0][13] = EMPTY_CHAR;
    pilulas[16][13] = EMPTY_CHAR;
    pilulas[13][13] = EMPTY_CHAR;
}

void death_sequence() {
    desenhar_tela(1, 0, NULL);
    usleep(1000000);

    printf("\033[%d;%dH", pacman.y + 1, pacman.x + 1);
    printf("%s%c", COLOR_YELLOW, PACMAN_DYING_CHAR_1);
    fflush(stdout);

    play_sfx_blocking("death_0.wav");

    printf("\033[%d;%dH", pacman.y + 1, pacman.x + 1);
    printf("%s%c", COLOR_YELLOW, PACMAN_DYING_CHAR_2);
    fflush(stdout);

    play_sfx_blocking("death_1.wav");

    usleep(1000000);

    if (lives <= 0) {
        game_over = 1;
    } else {
        reset_positions();
        start_siren("siren0_firstloop.wav", "siren0.wav");
    }
}

void verifica_colisao() {
    for (int i = 0; i < NUM_GHOSTS; i++) {
        if (ghosts[i].state == EATEN) continue;

        if (pacman.x == ghosts[i].x && pacman.y == ghosts[i].y) {
            if (ghosts[i].state == FLEEING) {
                score += 200;
                play_sfx_blocking("eat_ghost.wav");
                ghosts[i].state = EATEN;
                start_siren("eyes_firstloop.wav", "eyes.wav");
            } else {
                stop_siren();
                lives--;
                death_sequence();
            }
            break;
        }
    }
}

void animacao_vitoria() {
    stop_siren();
    desenhar_tela(1, 0, NULL);
    usleep(1000000);

    for (int i = 0; i < 8; i++) {
        const char* cor_do_flash = ((i % 2) != 0) ? COLOR_BLUE : COLOR_WHITE;
        desenhar_tela(1, 0, cor_do_flash);
        usleep(250000);
    }

    usleep(500000);
    reset_positions();
    preencher_pilulas();
    start_siren("siren0_firstloop.wav", "siren0.wav");
}

void atualizar_logica() {
    if (game_over) return;

    if (fruit_visible) {
        fruit_timer--;
        if (fruit_timer <= 0) {
            fruit_visible = 0;
        }
    }

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
    
    if (fruit_visible && pacman.x == FRUIT_X && pacman.y == FRUIT_Y) {
        score += 100;
        fruit_visible = 0;
        play_sfx("eat_fruit.wav");
    }

    if (pilulas[pacman.y][pacman.x] == PILL_CHAR) {
        score += 10;
        pills_captured++;
        pilulas[pacman.y][pacman.x] = EMPTY_CHAR;

        if (waka_toggle == 0) {
            play_sfx("wa_sound.wav");
            waka_toggle = 1;
        } else {
            play_sfx("ka_sound.wav");
            waka_toggle = 0;
        }

    } else if (pilulas[pacman.y][pacman.x] == POWER_PILL_CHAR) {
        score += 50;
        pills_captured++;
        pilulas[pacman.y][pacman.x] = EMPTY_CHAR;

        if (waka_toggle == 0) {
            play_sfx("wa_sound.wav");
            waka_toggle = 1;
        } else {
            play_sfx("ka_sound.wav");
            waka_toggle = 0;
        }

        start_siren("fright_firstloop.wav", "fright.wav");

        for (int i = 0; i < NUM_GHOSTS; i++) {
            if (ghosts[i].state != IN_BOX && ghosts[i].state != EATEN) {
                ghosts[i].state = FLEEING;
                ghosts[i].state_timer = POWER_PILL_DURATION;
            }
        }
    }

    if (pills_captured == 70 || pills_captured == 170) {
        fruit_visible = 1;
        fruit_timer = FRUIT_DURATION;
    }

    verifica_colisao();
    atualizar_logica_fantasmas();
    verifica_colisao();

    if (pills_captured == PILLS_PER_LEVEL) {
        animacao_vitoria();
    }

    if (pacman.dx != 0 || pacman.dy != 0) {
        pacman_mouth_toggle = !pacman_mouth_toggle;
    }

    power_pill_toggle = !power_pill_toggle;
    ghost_fleeing_toggle = !ghost_fleeing_toggle;
}

void desenhar_tela(int desenhar_pacman, int desenhar_fantasmas, const char* map_color) {
    char buffer_tela[10000];
    char *ptr = buffer_tela;
    ptr += sprintf(ptr, "\033[H");

    const char* cor_mapa_atual = (map_color != NULL) ? map_color : COLOR_BLUE;

    // desenha o mapa, o pacman e as pílulas em um buffer, em seguida, imprime
    for (int y = 0; y < ALTURA; y++) {
        for (int x = 0; x < strlen(mapa[y]); x++) {
            int ghost_index_here = -1;
            if (desenhar_fantasmas) {
                for (int i = 0; i < NUM_GHOSTS; i++) {
                    if (ghosts[i].x == x && ghosts[i].y == y) {
                        ghost_index_here = i;
                        break;
                    }
                }
            }

            if (y == pacman.y && x == pacman.x && desenhar_pacman) {
                char char_pacman_atual = (pacman_mouth_toggle == 0) ? PACMAN_CHAR : PACMAN_CLOSING_CHAR;
                ptr += sprintf(ptr, "%s%c", COLOR_YELLOW, char_pacman_atual);
            } else if (ghost_index_here != -1) {
                Ghost* fantasma = &ghosts[ghost_index_here];
                char char_fantasma_atual;
                const char* cor_fantasma_atual;

                switch (fantasma->state) {
                    case FLEEING:
                        char_fantasma_atual = FLEEING_GHOST_CHAR;
                        cor_fantasma_atual = (fantasma->state_timer < 10 && fantasma->state_timer % 2 == 0)
                                           ? COLOR_WHITE
                                           : COLOR_BLUE;
                        break;
                    
                    case EATEN:
                        char_fantasma_atual = EATEN_GHOST_CHAR;
                        cor_fantasma_atual = COLOR_WHITE;
                        break;
                    
                    default:
                        char_fantasma_atual = GHOST_CHAR;
                        cor_fantasma_atual = fantasma->color;
                        break;
                }
                ptr += sprintf(ptr, "%s%c", cor_fantasma_atual, char_fantasma_atual);
            } else if (fruit_visible && y == FRUIT_Y && x == FRUIT_X) {
                ptr += sprintf(ptr, "%s%c", COLOR_RED, FRUIT_CHAR);
            } else if (pilulas[y][x] == PILL_CHAR) {
                ptr += sprintf(ptr, "%s%c", COLOR_SALMON, pilulas[y][x]);
            } else if (pilulas[y][x] == POWER_PILL_CHAR) {
                if (power_pill_toggle == 0) {
                    ptr += sprintf(ptr, "%s%c", COLOR_SALMON, POWER_PILL_CHAR);
                } else {
                    ptr += sprintf(ptr, "%s%c", COLOR_SALMON, EMPTY_CHAR);
                }
            } else {
                ptr += sprintf(ptr, "%s%c", cor_mapa_atual, mapa[y][x]);
            }
        }
        ptr += sprintf(ptr, "\n");
    }

    ptr += sprintf(ptr, "%s", COLOR_RESET);
    ptr += sprintf(ptr, "\033[%d;%dH", ALTURA + 2, 1);
    ptr += sprintf(ptr, "Score: %-5d   Lives:", score);

    const char* limpeza = "          ";
    ptr += sprintf(ptr, "%s", limpeza);
    ptr += sprintf(ptr, "\033[%d;%dH", ALTURA + 2, 22);
    ptr += sprintf(ptr, "%s", COLOR_YELLOW);
    for (int i = 0; i < lives; i++) {
        ptr += sprintf(ptr, " %c", PACMAN_CHAR);
    }


    if (game_over) {
        ptr += sprintf(ptr, "\033[%d;%dH", ALTURA / 2 + 2, LARGURA / 2 - 4);
        ptr += sprintf(ptr, "%sGAME OVER", COLOR_RED);
        ptr += sprintf(ptr, "\033[%d;%dH", ALTURA + 2, 1);
    }

    printf("%s", buffer_tela);
    fflush(stdout);
}

void desenhar_menu(MenuCharacter characters[]) {
    char buffer_tela[5000];
    char *ptr = buffer_tela;

    ptr += sprintf(ptr, "\033[2J\033[H");

    for (int i = 0; i < MENU_CHAR_COUNT; i++) {
        if (characters[i].x >= 0 && characters[i].x < LARGURA) {
            ptr += sprintf(ptr, "\033[%d;%dH", characters[i].y + 1, characters[i].x + 1);
            ptr += sprintf(ptr, "%s%s", characters[i].color, characters[i].icon);
        }
    }

    const char* start_text = "PRESS ENTER TO START";
    int text_x = (LARGURA - strlen(start_text)) / 2;
    int text_y = ((ALTURA + 2) / 2) + 2;

    ptr += sprintf(ptr, "\033[%d;%dH", text_y + 1, text_x + 1);
    ptr += sprintf(ptr, "%s%s", COLOR_RESET, start_text);

    printf("%s", buffer_tela);
    fflush(stdout);
}

void executar_menu() {
    MenuCharacter characters[MENU_CHAR_COUNT];

    characters[0] = (MenuCharacter){-2, (ALTURA + 2) / 2, "C", COLOR_YELLOW};
    characters[1] = (MenuCharacter){-5, (ALTURA + 2) / 2, "@", COLOR_RED};
    characters[2] = (MenuCharacter){-8, (ALTURA + 2) / 2, "@", COLOR_PINK};
    characters[3] = (MenuCharacter){-11, (ALTURA + 2) / 2, "@", COLOR_CYAN};
    characters[4] = (MenuCharacter){-14, (ALTURA + 2) / 2, "@", COLOR_ORANGE};

     while (1) {
        if (kbhit()) {
            char c = getch();
            if (c == '\n' || c == '\r') {
                break;
            } else if (c == 'q') {
                exit(0);
            }
        }

        for (int i = 0; i < MENU_CHAR_COUNT; i++) {
            characters[i].x++;
            if (characters[i].x > LARGURA) {
                characters[i].x = -13;
            }
        }

        desenhar_menu(characters);
        usleep(MENU_DELAY);
    }
}

void play_sfx(const char* sound_file) {
    char command[256];
    sprintf(command, "paplay " SOUND_DIR "%s > /dev/null 2>&1 &", sound_file);
    system(command);
}

void play_sfx_blocking(const char* sound_file) {
    char command[256];
    sprintf(command, "paplay " SOUND_DIR "%s > /dev/null 2>&1", sound_file);
    system(command);
}

void stop_siren() {
    if (ambient_sound_pid > 0) {
        kill(ambient_sound_pid, SIGTERM);
        ambient_sound_pid = 0;
    }
}

void start_siren(const char* intro_file, const char* loop_file) {
    stop_siren();

    pid_t pid = fork();

    if (pid == 0) {
        char intro_path[256], loop_path[256];
        sprintf(intro_path, SOUND_DIR "%s", intro_file);
        sprintf(loop_path, SOUND_DIR "%s", loop_file);

        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        execlp("play", "play", "-q", intro_path, loop_path, "repeat", "-", NULL);

        exit(1);
    } else if (pid > 0) {
        ambient_sound_pid = pid;
    }
}

void ready_screen() {
    desenhar_tela(1, 0, NULL);
    printf("\033[%d;%dH", ALTURA / 2 + 2, LARGURA / 2 - 3);
    printf("%sREADY!!", COLOR_YELLOW);
    fflush(stdout);

    play_sfx_blocking("intro_sound.wav");
    start_siren("siren0_firstloop.wav", "siren0.wav");
}

void configuração_inicial() {
    printf("%s", ENTER_ALT_SCREEN);
    configurar_terminal();
    printf("\033[?25l");
    atexit(restauração_final);
    srand(time(NULL));
}

void inicializar_jogo() {
    lives = VIDAS_INICIAIS;
    score = 0;
    pills_captured = 0;
    game_over = 0;
    waka_toggle = 0;
    pacman_mouth_toggle = 0;
    power_pill_toggle = 0;
    ghost_fleeing_toggle = 0;

    reset_positions();
    preencher_pilulas();

    ghosts[0].color = COLOR_RED;
    ghosts[1].color = COLOR_CYAN;
}

int main() {
    configuração_inicial();
    executar_menu();
    inicializar_jogo();
    ready_screen();
    while (1) {
        processar_entrada();
        atualizar_logica();
        if (game_over) {
            stop_siren();
            break;
        }
        desenhar_tela(1, 1, NULL);

        usleep(DELAY);
    }
    return 0;
}