# Dino Run para T-Keyboard

🇬🇧 [English](README.md) · 🇪🇸 Español

Un juego de dinosaurio al estilo Chrome para el **T-Keyboard** de LilyGO (ESP32-C3 con pantalla GC9D01N de 0,99", 40×160 px). Tiene modo blanco y negro y modo color. Solo se usan dos teclas: la **barra espaciadora** para jugar, y **ALT** para cambiar los colores.

> por **@mmoroca & Leo.ai** · 2026 · [Licencia MIT](LICENSE)

## Características

- Pantalla de espera: el dinosaurio está de pie bajo el texto **GAME OVER**. Pulsa la barra espaciadora para empezar.
- Cactus (dos tipos y un cactus doble) y pájaros: **pájaros altos** (hay que agacharse) y **pájaros bajos** (hay que saltar).
- Dos modos de pantalla, que se cambian con **ALT**:
  - **Blanco y negro** (por defecto): gráficos negros sobre fondo blanco.
  - **Color**: cielo azul, suelo marrón, dinosaurio verde oscuro, cactus verde claro, pájaros blancos y textos blancos.
- La velocidad aumenta poco a poco a medida que avanzas.
- Al terminar: tu **puntuación** y la **mejor puntuación**. El dispositivo guarda las 5 mejores en la memoria flash.
- La pantalla se apaga tras **5 s** sin pulsar ninguna tecla, para ahorrar batería. Cualquier tecla la vuelve a encender.

## Controles

| Tecla | Acción |
|---|---|
| **Espacio**, toque corto (menos de unos 230 ms) | Saltar |
| **Espacio**, mantenido en el suelo | Agacharse. Suéltalo para levantarte |
| **Espacio**, mantenido en el aire | Caer más rápido |
| **Espacio** en la pantalla de espera, o 0,7 s después de un game over | Empezar / reiniciar |
| **ALT** | Cambiar entre blanco y negro y color |
| **Cualquier tecla** con la pantalla apagada | Enciende la pantalla (esa pulsación no hace nada más) |

Agáchate bajo los pájaros altos. Salta sobre los cactus y los pájaros bajos.

Si la pantalla se apaga durante una partida (5 s sin pulsar ninguna tecla), la partida termina y la puntuación se guarda.

El modo de pantalla no se guarda: al encender, el dispositivo arranca en blanco y negro.

## Hardware

- **T-Keyboard** de LilyGO (ESP32-C3, pantalla GC9D01N de 0,99" 40×160). Página del producto: <https://lilygo.cc/products/t-keyboard>
- Cable USB-C.

## Compilar y subir (VSCode + PlatformIO)

1. Instala [Visual Studio Code](https://code.visualstudio.com/) y la extensión **PlatformIO IDE**.
2. Abre esta carpeta con **Archivo → Abrir carpeta**.
3. Espera a que PlatformIO instale la plataforma (solo la primera vez).
4. Pulsa **Build** (✓ en la barra inferior) y después **Upload** (→).
5. Si la subida falla, pon el T-Keyboard en modo descarga: mantén pulsado **BOOT**, pulsa **RESET** (o conecta el USB) y suelta **BOOT**. Consulta el [README del T-Keyboard](https://github.com/Xinyuan-LilyGO/T-keyboard).

Desde la terminal (con la CLI de PlatformIO instalada):

```bash
pio run -e T-Keyboard-dino                 # compilar
pio run -e T-Keyboard-dino -t upload       # subir
pio device monitor -b 115200               # monitor serie
```

## Estructura del proyecto

- `src/main.cpp`: conexión con el hardware (pantalla, teclado, modo color, puntuaciones guardadas, bucle principal).
- `src/dino_game.h`: lógica del juego, paletas y dibujo en un búfer de imagen. No depende de Arduino.
- `src/bitmaps.h`: sprites de 1 bit (dinosaurio, cactus, pájaros, suelo, título GAME OVER).
- `src/dino_font.h`: la fuente 8×16 del driver de la pantalla, copiada para dibujar texto en el búfer.
- `lib/TFT_GC9D01N-Patched`: driver de la pantalla (MIT, Xinyuan-LilyGO).

## Notas

- Las mejores puntuaciones se guardan en el espacio `dino` de `Preferences` (claves `h0`…`h4`).
- El juego no tiene sonido: el T-Keyboard no tiene un pin de zumbador documentado.
- Los textos de la pantalla no llevan acentos, porque la fuente de pantalla tiene un problema de acentos pendiente.

## Créditos y licencias

- **Autor:** @mmoroca & Leo.ai (2026).
- **Licencia:** [MIT](LICENSE).
- **Sprites:** MIT, según el autor del código de referencia del Dino.
- **Driver de pantalla:** `lib/TFT_GC9D01N-Patched` (MIT, Copyright (c) 2022 Xinyuan-LilyGO).
- **Inspiración:** el juego del dinosaurio de Chrome. Este proyecto no usa código ni gráficos de Google y no está afiliado a Google.
