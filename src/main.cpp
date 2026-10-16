// Juego del dinosaurio para el T-Keyboard (ESP32-C3, pantalla GC9D01N 40x160).
//
// - Inicio: el dinosaurio espera bajo el texto «GAME OVER». La barra espaciadora empieza la partida.
// - La barra espaciadora es la única tecla del juego:
//     toque corto = salto; mantenerla en el suelo = agachado (para pájaros altos);
//     mantenerla en el aire = caída rápida.
// - Pájaros altos (hay que agacharse) y bajos (hay que saltar) y cactus.
// - ALT cambia entre modo blanco y negro y modo color.
// - Al terminar se muestran la puntuación y la máxima guardada en flash.
// - Sin ninguna tecla durante 5 s se apaga la pantalla (y una partida en curso termina).
//   Cualquier tecla la enciende; esa pulsación no actúa en el juego.

#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include <esp_system.h>

// Pines del T-Keyboard: los mismos que el firmware del reloj, que ya funciona en la placa.
#define TFT_BL_PIN 8
#define TFT_DC 2
#define TFT_MOSI 10
#define TFT_SCLK 20
#define TFT_CS -1
#define TFT_MISO 2
#define TFT_RST -1
#define KB_BACKLIGHT_PIN 9

#include <TFT_GC9D01N.h>
static TFT_GC9D01N_Class TFT_099;

#include "dino_game.h"

#define SCREEN_BACKLIGHT 50
#define SCREEN_OFF_MS 5000UL
#define FRAME_MS 33UL          // unos 30 fotogramas por segundo
#define SCORE_SLOTS 5
#define DEAD_PAUSE_MS 700UL    // pausa tras morir antes de poder reiniciar

// Posición de los textos (fila superior y fila inferior, 16 px de alto cada una).
#define TEXT_TOP_Y 8
#define TEXT_BOTTOM_Y 24
#define TEXT_COL 36            // columna de los textos, a la derecha del dinosaurio

// ------------------------------------------------------------------ teclado

static const uint8_t rows[] = {0, 3, 18, 12, 11, 6, 7};
static const uint8_t cols[] = {1, 4, 5, 19, 13};
static const int rowCount = sizeof(rows) / sizeof(rows[0]);
static const int colCount = sizeof(cols) / sizeof(cols[0]);

#define KEY_SPACE_C 0
#define KEY_SPACE_R 5
#define KEY_ALT_C 0            // misma tecla ALT que en el firmware del reloj
#define KEY_ALT_R 4

static bool key_prev[5][7];
static bool key_now[5][7];
static bool key_edge[5][7];   // flanco de subida: tecla recién pulsada
static bool key_fall[5][7];   // flanco de bajada: tecla recién soltada

static void scan_matrix() {
    for (int c = 0; c < colCount; c++) {
        pinMode(cols[c], OUTPUT);
        digitalWrite(cols[c], LOW);
        for (int r = 0; r < rowCount; r++) {
            pinMode(rows[r], INPUT_PULLUP);
            delayMicroseconds(30);   // la línea necesita un instante para subir
            bool now = (digitalRead(rows[r]) == LOW);
            key_edge[c][r] = now && !key_prev[c][r];
            key_fall[c][r] = !now && key_prev[c][r];
            key_now[c][r] = now;
            key_prev[c][r] = now;
            pinMode(rows[r], INPUT);
        }
        pinMode(cols[c], INPUT);
    }
}

// ------------------------------------------------------------------ pantalla

static uint16_t fb[DINO_SCREEN_W * DINO_SCREEN_H];
static bool color_mode = false;   // false = blanco y negro (por defecto), true = color

static const Palette &palette() {
    return color_mode ? PAL_COLOR : PAL_BW;
}

// Vuelca el búfer completo a la pantalla de una vez (mismo orden de píxeles que el reloj).
static void flush_frame() {
    TFT_099.BlockWrite(0, 39, 0, 159);   // eje de 40 px y eje de 160 px
    DC_D;
    SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
    for (int sx = 0; sx < DINO_SCREEN_W; sx++) {
        for (int a = 0; a < 40; a++) {
            int sy = 39 - a;
            SPI.transfer16(fb[sy * DINO_SCREEN_W + sx]);
        }
    }
    SPI.endTransaction();
}

// ------------------------------------------------------------------ puntuaciones

static uint32_t top_scores[SCORE_SLOTS];

static void load_scores() {
    Preferences p;
    p.begin("dino", true);
    for (int i = 0; i < SCORE_SLOTS; i++) {
        char k[4];
        snprintf(k, sizeof k, "h%d", i);
        top_scores[i] = p.getUInt(k, 0);
    }
    p.end();
}

// Inserta la puntuación en la tabla y guarda si entra. Devuelve true si cambió.
static bool record_score(uint32_t score) {
    if (score == 0) return false;
    int pos = -1;
    for (int i = 0; i < SCORE_SLOTS; i++) {
        if (score > top_scores[i]) { pos = i; break; }
    }
    if (pos < 0) return false;
    for (int i = SCORE_SLOTS - 1; i > pos; i--) top_scores[i] = top_scores[i - 1];
    top_scores[pos] = score;

    Preferences p;
    p.begin("dino", false);
    for (int i = 0; i < SCORE_SLOTS; i++) {
        char k[4];
        snprintf(k, sizeof k, "h%d", i);
        p.putUInt(k, top_scores[i]);
    }
    p.end();
    return true;
}

// ------------------------------------------------------------------ estado

static DinoGame game;
static bool screen_on = true;
static bool need_static_draw = true;
static unsigned long last_activity_ms = 0;
static unsigned long next_frame_ms = 0;
static unsigned long dead_at_ms = 0;

// Pantalla de espera o de fin: dibuja la escena y los textos, y la envía.
static void show_static() {
    const Palette &pal = palette();
    dino_render(fb, game, pal);
    char best[24];
    snprintf(best, sizeof best, "MEJOR %lu", (unsigned long) top_scores[0]);
    if (game.state == DINO_DEAD) {
        char pts[24];
        snprintf(pts, sizeof pts, "PUNTOS %lu", (unsigned long) game.score);
        draw_text(fb, TEXT_COL, TEXT_TOP_Y, pts, pal.text, pal, true);
    }
    draw_text(fb, TEXT_COL, TEXT_BOTTOM_Y, best, pal.text, pal, true);
    flush_frame();
}

// Termina la partida (choque o apagado de pantalla) y muestra el resultado.
static void end_game(unsigned long now) {
    dead_at_ms = now;
    game.state = DINO_DEAD;
    game.holding = false;
    if (record_score(game.score)) Serial.println("Nueva puntuacion maxima");
    Serial.printf("Partida terminada: %lu puntos\n", (unsigned long) game.score);
    show_static();
}

// ------------------------------------------------------------------ bucle

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("Juego del dinosaurio iniciando");

    for (int r = 0; r < rowCount; r++) pinMode(rows[r], INPUT);
    for (int c = 0; c < colCount; c++) pinMode(cols[c], INPUT_PULLUP);
    pinMode(KB_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(KB_BACKLIGHT_PIN, HIGH);

    memset(key_prev, 0, sizeof key_prev);
    memset(key_now, 0, sizeof key_now);
    memset(key_edge, 0, sizeof key_edge);
    memset(key_fall, 0, sizeof key_fall);
    scan_matrix();
    memset(key_edge, 0, sizeof key_edge);
    memset(key_fall, 0, sizeof key_fall);

    SPI.end();
    TFT_099.begin();
    TFT_099.DispColor(0, 0, TFT_WIDTH, TFT_HEIGHT, BLACK);
    TFT_099.backlight(SCREEN_BACKLIGHT);

    srand((unsigned) esp_random());
    load_scores();
    dino_idle(game);
    last_activity_ms = millis();
    screen_on = true;
    need_static_draw = true;
}

void loop() {
    unsigned long now = millis();
    scan_matrix();

    bool any_press = false, any_down = false;
    bool space_press = false, space_release = false, alt_press = false;
    for (int c = 0; c < colCount; c++) {
        for (int r = 0; r < rowCount; r++) {
            bool is_space = (c == KEY_SPACE_C && r == KEY_SPACE_R);
            bool is_alt = (c == KEY_ALT_C && r == KEY_ALT_R);
            if (key_edge[c][r]) {
                any_press = true;
                if (is_space) space_press = true;
                if (is_alt) alt_press = true;
            }
            if (key_now[c][r]) any_down = true;
            if (key_fall[c][r] && is_space) space_release = true;
        }
    }

    // Actividad: cualquier tecla pulsada o mantenida retrasa el apagado.
    if (any_press || any_down) last_activity_ms = now;

    if (!screen_on) {
        if (any_press) {
            // Una tecla enciende la pantalla; esa pulsación no actúa en el juego.
            TFT_099.backlight(SCREEN_BACKLIGHT);
            screen_on = true;
            need_static_draw = true;
            next_frame_ms = now;
            last_activity_ms = now;
            Serial.println("Pantalla encendida");
        }
        delay(20);
        return;
    }

    if (now - last_activity_ms > SCREEN_OFF_MS) {
        if (game.state == DINO_PLAYING) end_game(now);   // no se juega a oscuras
        TFT_099.backlight(0);
        screen_on = false;
        Serial.println("Pantalla apagada por inactividad");
        delay(20);
        return;
    }

    if (alt_press) {
        color_mode = !color_mode;
        need_static_draw = true;
        Serial.println(color_mode ? "Modo color" : "Modo blanco y negro");
    }

    if (space_press) {
        if (game.state == DINO_IDLE) {
            dino_start(game);
            next_frame_ms = now;
        } else if (game.state == DINO_PLAYING) {
            dino_press(game);
        } else if (game.state == DINO_DEAD && now - dead_at_ms > DEAD_PAUSE_MS) {
            // Pausa tras morir para no reiniciar por error con un salto.
            dino_start(game);
            next_frame_ms = now;
        }
    }
    if (space_release && game.state == DINO_PLAYING) {
        dino_release(game);
    }

    if (game.state == DINO_PLAYING) {
        if (now >= next_frame_ms) {
            next_frame_ms = now + FRAME_MS;
            if (dino_update(game)) {
                end_game(now);
            } else {
                dino_render(fb, game, palette());
                flush_frame();
            }
        }
    } else if (need_static_draw) {
        need_static_draw = false;
        show_static();
    }

    delay(2);
}
