// Juego del dinosaurio para el T-Keyboard: lógica y dibujo en memoria.
// No depende de Arduino (salvo pgm_read_byte), así que se puede probar en el PC.
//
// Control con una sola tecla (la barra espaciadora):
//   - Un toque corto (< DINO_TAP_FRAMES) salta al soltar.
//   - Mantenerla en el suelo agacha al dinosaurio hasta soltarla.
//   - Mantenerla en el aire hace caer más rápido.
//
// Sistema de coordenadas del búfer: sx = 0..159 (izquierda a derecha),
// sy = 0..39 (arriba a abajo). main.cpp vuelca el búfer a la pantalla.

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t *) (addr))
#endif

#include "bitmaps.h"
#include "dino_font.h"

#define DINO_SCREEN_W 160
#define DINO_SCREEN_H 40
#define DINO_FLOOR_Y 34           // primera fila del suelo (el suelo ocupa 34..39)
#define DINO_X 8                  // posición horizontal fija del dinosaurio
#define DINO_STAND_W 20
#define DINO_STAND_H 19
#define DINO_STAND_Y (DINO_FLOOR_Y - DINO_STAND_H)   // 15
#define DINO_DUCK_W 22
#define DINO_DUCK_H 11
#define DINO_DUCK_Y (DINO_FLOOR_Y - DINO_DUCK_H)     // 23
#define DINO_GRAVITY 0.20f        // salto de unos 0,8 s; la ventana para saltar es de unos 20 px
#define DINO_FAST_FALL 3.0f       // multiplicador de gravedad al mantener la tecla en el aire
#define DINO_JUMP_V (-2.37f)      // altura de salto 14 px (v²/2g)
#define DINO_TAP_FRAMES 7         // menos de esto = toque (salto); más = agachar (≈230 ms)
#define DINO_BIRD_AFTER 300.0f    // distancia a partir de la cual aparecen pájaros
#define DINO_MAX_OBS 4
#define DINO_GAMEOVER_X 48
#define DINO_GAMEOVER_Y 0

#define GAME_BLACK 0x0000
#define GAME_WHITE 0xFFFF

// Colores RGB565 del modo color.
#define GAME_RGB565(r, g, b) ((uint16_t) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

enum DinoState { DINO_IDLE, DINO_PLAYING, DINO_DEAD };
enum ObstacleKind : uint8_t { OB_CACTUS_A, OB_CACTUS_B, OB_DOUBLE, OB_BIRD_LOW, OB_BIRD_HIGH };

// Colores de una escena. El modo blanco y negro usa todo negro sobre blanco.
struct Palette {
    uint16_t sky;        // fondo del cielo
    uint16_t floor_bg;   // fondo del suelo
    uint16_t floor_ink;  // líneas y motas del suelo
    uint16_t dino;
    uint16_t cactus;
    uint16_t bird;
    uint16_t text;       // puntuación, GAME OVER y textos
};

static const Palette PAL_BW = {
    GAME_WHITE, GAME_WHITE, GAME_BLACK,
    GAME_BLACK, GAME_BLACK, GAME_BLACK, GAME_BLACK,
};

static const Palette PAL_COLOR = {
    GAME_RGB565(135, 206, 235),   // cielo azul
    GAME_RGB565(139, 90, 43),     // suelo marrón
    GAME_RGB565(70, 40, 15),      // motas del suelo
    GAME_RGB565(0, 100, 0),       // dinosaurio verde oscuro
    GAME_RGB565(144, 238, 144),   // cactus verde claro
    GAME_RGB565(255, 255, 255),   // pájaros blancos
    GAME_RGB565(255, 255, 255),   // textos blancos
};

struct Obstacle {
    float x;
    uint8_t kind;
    bool active;
};

struct DinoGame {
    DinoState state;
    float y;            // borde superior del dinosaurio de pie
    float vy;
    bool on_ground;
    bool holding;       // la tecla está pulsada ahora mismo
    bool ignore_release;// la tecla que inició la partida: su suelta no cuenta como salto
    bool ducking;       // agachado en el suelo
    bool fast_fall;     // cayendo con la tecla pulsada
    uint32_t hold_frames;
    float dist;         // píxeles recorridos
    float speed;        // píxeles por fotograma
    float spawn_left;   // píxeles hasta el siguiente obstáculo
    uint32_t score;
    Obstacle obs[DINO_MAX_OBS];
    uint32_t frame;
};

struct ObstacleInfo {
    int w, h, y;        // tamaño y fila superior del sprite
};

static inline ObstacleInfo obstacle_info(uint8_t kind) {
    switch (kind) {
        case OB_CACTUS_A:  return {6, 11, DINO_FLOOR_Y - 11};
        case OB_CACTUS_B:  return {6, 11, DINO_FLOOR_Y - 11};
        case OB_DOUBLE:    return {15, 11, DINO_FLOOR_Y - 11};
        case OB_BIRD_LOW:  return {16, 10, DINO_FLOOR_Y - 10};   // hay que saltar
        default:           return {16, 10, 11};                  // OB_BIRD_HIGH: hay que agacharse
    }
}

static void dino_idle(DinoGame &g) {
    memset(&g, 0, sizeof g);
    g.state = DINO_IDLE;
    g.y = DINO_STAND_Y;
    g.on_ground = true;
}

// Empieza una partida. La suelta de la tecla que la inicia no debe contar como salto.
static void dino_start(DinoGame &g) {
    dino_idle(g);
    g.state = DINO_PLAYING;
    g.speed = 2.0f;
    g.spawn_left = 120.0f;
    g.ignore_release = true;
}

static void dino_press(DinoGame &g) {
    if (g.state != DINO_PLAYING) return;
    g.holding = true;
    g.hold_frames = 0;
    g.ignore_release = false;
}

static void dino_release(DinoGame &g) {
    if (g.state != DINO_PLAYING) return;
    if (g.ignore_release) {
        g.ignore_release = false;
        g.holding = false;
        return;
    }
    if (g.holding && g.on_ground && g.hold_frames < DINO_TAP_FRAMES) {
        g.vy = DINO_JUMP_V;
        g.on_ground = false;
    }
    g.holding = false;
    g.ducking = false;
}

static bool rect_overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

// Avanza un fotograma. Devuelve true si el dinosaurio ha chocado en este fotograma.
static bool dino_update(DinoGame &g) {
    if (g.state != DINO_PLAYING) return false;
    g.frame++;

    if (g.holding) g.hold_frames++;
    g.ducking = g.holding && g.on_ground && g.hold_frames >= DINO_TAP_FRAMES;
    g.fast_fall = g.holding && !g.on_ground;

    if (!g.on_ground) {
        float grav = g.fast_fall ? DINO_GRAVITY * DINO_FAST_FALL : DINO_GRAVITY;
        g.vy += grav;
        g.y += g.vy;
        if (g.y >= DINO_STAND_Y) {
            g.y = DINO_STAND_Y;
            g.vy = 0;
            g.on_ground = true;
        }
    }

    // La velocidad sube lentamente con la distancia recorrida.
    g.dist += g.speed;
    g.score = (uint32_t) (g.dist / 6.0f);
    g.speed = fminf(6.0f, 2.0f + g.dist / 1200.0f);

    for (int i = 0; i < DINO_MAX_OBS; i++) {
        if (!g.obs[i].active) continue;
        g.obs[i].x -= g.speed;
        if (g.obs[i].x + obstacle_info(g.obs[i].kind).w < 0) g.obs[i].active = false;
    }

    g.spawn_left -= g.speed;
    if (g.spawn_left <= 0) {
        for (int i = 0; i < DINO_MAX_OBS; i++) {
            if (g.obs[i].active) continue;
            uint8_t kind;
            int r = rand() % 10;
            if (g.dist < DINO_BIRD_AFTER || r < 6) kind = (uint8_t) (rand() % 3);   // cactus
            else kind = (r < 8) ? OB_BIRD_LOW : OB_BIRD_HIGH;
            g.obs[i].active = true;
            g.obs[i].kind = kind;
            g.obs[i].x = DINO_SCREEN_W;
            break;
        }
        // Hueco mínimo proporcional a la velocidad, para que siempre se pueda pasar.
        g.spawn_left = 60.0f + (float) (rand() % 60) + g.speed * 12.0f;
    }

    // Caja del dinosaurio según la postura.
    int bx, by, bw, bh;
    if (g.ducking) {
        bx = DINO_X + 3; by = DINO_DUCK_Y + 2; bw = DINO_DUCK_W - 6; bh = DINO_DUCK_H - 2;
    } else {
        bx = DINO_X + 5; by = (int) roundf(g.y) + 3; bw = 10; bh = 12;
    }
    for (int i = 0; i < DINO_MAX_OBS; i++) {
        if (!g.obs[i].active) continue;
        ObstacleInfo info = obstacle_info(g.obs[i].kind);
        int ox = (int) roundf(g.obs[i].x), oy = info.y, ow = info.w, oh = info.h;
        bool bird = (g.obs[i].kind == OB_BIRD_LOW || g.obs[i].kind == OB_BIRD_HIGH);
        if (bird) { ox += 3; oy += 1; ow -= 6; oh -= 2; }
        else      { ox += 1; ow -= 2; }
        if (rect_overlap(ox, oy, ow, oh, bx, by, bw, bh)) {
            g.state = DINO_DEAD;
            g.holding = false;
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------------ dibujo

static inline void gpx(uint16_t *fb, int x, int y, uint16_t c) {
    if (x < 0 || y < 0 || x >= DINO_SCREEN_W || y >= DINO_SCREEN_H) return;
    fb[y * DINO_SCREEN_W + x] = c;
}

// Dibuja un bitmap de 1 bit (MSB a la izquierda). Solo los bits a 1 se pintan, en color `ink`.
static void draw_bits(uint16_t *fb, int x, int y, int w, int h, const uint8_t *bits, uint16_t ink) {
    int bpr = (w + 7) / 8;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint8_t b = pgm_read_byte(bits + row * bpr + col / 8);
            if ((b >> (7 - col % 8)) & 1) gpx(fb, x + col, y + row, ink);
        }
    }
}

// Índice de glifo como ToOrd() del driver: 0-9 -> 0..9, A-Z -> 10..35, espacio -> 72.
static int glyph_index(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') c = (char) (c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return 72;
}

// Texto con la fuente 8x16 de la pantalla. Si `opaque`, el fondo de cada celda se rellena
// con el color del escenario (cielo o suelo), así el texto se lee sobre cualquier zona.
static void draw_text(uint16_t *fb, int x0, int y0, const char *s, uint16_t ink, const Palette &pal, bool opaque) {
    for (int i = 0; s[i] != '\0'; i++) {
        const uint8_t *glyph = DINO_FONT_8X16 + glyph_index(s[i]) * 16;
        for (int r = 0; r < 16; r++) {
            int y = y0 + r;
            uint16_t bg = (y >= DINO_FLOOR_Y) ? pal.floor_bg : pal.sky;
            for (int c = 0; c < 8; c++) {
                int x = x0 + i * 8 + c;
                if ((glyph[r] >> (7 - c)) & 1) gpx(fb, x, y, ink);
                else if (opaque) gpx(fb, x, y, bg);
            }
        }
    }
}

// Dígitos 3x5 para la puntuación en partida.
static const uint8_t DIGIT_FONT[10][5] = {
    {0x7, 0x5, 0x5, 0x5, 0x7}, {0x2, 0x6, 0x2, 0x2, 0x7}, {0x7, 0x1, 0x7, 0x4, 0x7},
    {0x7, 0x1, 0x7, 0x1, 0x7}, {0x5, 0x5, 0x7, 0x1, 0x1}, {0x7, 0x4, 0x7, 0x1, 0x7},
    {0x7, 0x4, 0x7, 0x5, 0x7}, {0x7, 0x1, 0x1, 0x1, 0x1}, {0x7, 0x5, 0x7, 0x5, 0x7},
    {0x7, 0x5, 0x7, 0x1, 0x7},
};

static void draw_score(uint16_t *fb, uint32_t value, int x_right, int y, uint16_t ink) {
    char buf[12];
    snprintf(buf, sizeof buf, "%lu", (unsigned long) value);
    int n = strlen(buf);
    int x = x_right - (n * 4 - 1);
    for (int i = 0; i < n; i++) {
        const uint8_t *g = DIGIT_FONT[buf[i] - '0'];
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 3; c++)
                if (g[r] & (4 >> c)) gpx(fb, x + i * 4 + c, y + r, ink);
    }
}

// Dibuja la escena completa en el búfer, con la paleta indicada.
// Los textos "MEJOR" y "PUNTOS" los dibuja main.cpp encima con draw_text().
static void dino_render(uint16_t *fb, const DinoGame &g, const Palette &pal) {
    for (int y = 0; y < DINO_SCREEN_H; y++) {
        uint16_t c = (y < DINO_FLOOR_Y) ? pal.sky : pal.floor_bg;
        for (int x = 0; x < DINO_SCREEN_W; x++) fb[y * DINO_SCREEN_W + x] = c;
    }

    // Suelo: dos tiras de 128 px que se desplazan y cubren toda la pantalla.
    int fx = (int) fmodf(g.dist, 128.0f);
    draw_bits(fb, -fx, DINO_FLOOR_Y, 128, 6, bmp_floor1, pal.floor_ink);
    draw_bits(fb, 128 - fx, DINO_FLOOR_Y, 128, 6, bmp_floor2, pal.floor_ink);

    if (g.state == DINO_PLAYING) {
        for (int i = 0; i < DINO_MAX_OBS; i++) {
            if (!g.obs[i].active) continue;
            const Obstacle &o = g.obs[i];
            int x = (int) roundf(o.x);
            ObstacleInfo info = obstacle_info(o.kind);
            switch (o.kind) {
                case OB_CACTUS_A: draw_bits(fb, x, info.y, 6, 11, bmp_obst2, pal.cactus); break;
                case OB_CACTUS_B: draw_bits(fb, x, info.y, 6, 11, bmp_obst3, pal.cactus); break;
                case OB_DOUBLE:
                    draw_bits(fb, x, info.y, 6, 11, bmp_obst2, pal.cactus);
                    draw_bits(fb, x + 9, info.y, 6, 11, bmp_obst3, pal.cactus);
                    break;
                default: {
                    bool up = (((uint32_t) (g.dist / 8.0f)) % 2) == 0;
                    draw_bits(fb, x, info.y, 16, 10, up ? bmp_bird1 : bmp_bird2, pal.bird);
                }
            }
        }
    }

    // Dinosaurio.
    if (g.ducking) {
        draw_bits(fb, DINO_X, DINO_DUCK_Y, DINO_DUCK_W, DINO_DUCK_H, bmp_dinoD, pal.dino);
    } else {
        int dy = (int) roundf(g.y);
        const uint8_t *sprite = bmp_dinoB;
        if (g.state == DINO_PLAYING && g.on_ground) {
            sprite = (((uint32_t) (g.dist / 5.0f)) % 2) == 0 ? bmp_dinoL : bmp_dinoR;
        }
        draw_bits(fb, DINO_X, dy, DINO_STAND_W, DINO_STAND_H, sprite, pal.dino);
    }

    if (g.state != DINO_PLAYING) {
        draw_bits(fb, DINO_GAMEOVER_X, DINO_GAMEOVER_Y, 63, 8, bmp_gameover, pal.text);
    } else {
        draw_score(fb, g.score, DINO_SCREEN_W - 2, 1, pal.text);
    }
}
