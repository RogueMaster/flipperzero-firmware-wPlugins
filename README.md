# San Morse — Flipper Zero

App de código Morse pensada para aprender mientras escribes: muestra el **árbol de
decisión** del Morse en pantalla como guía, en lugar de exigir que ya te sepas los
códigos. También convierte texto a Morse y lo reproduce.

## Modos

### Árbol de decisión
Se dibuja el árbol dicotómico completo (E/T → I/A/N/M → …) y el botón **OK
funciona como una llave telegráfica**: mientras lo mantienes presionado suena el
tono y se enciende el LED, y al soltarlo se marca el símbolo según la duración:

| Botón | Acción |
|---|---|
| OK soltado rápido (<250 ms) | punto (·) — baja a la rama izquierda |
| OK mantenido | raya (−) — baja a la rama derecha |
| *pausa de ~1.5 s* | confirma la letra del nodo actual (automático) |
| *pausa de ~4 s* | agrega un espacio (automático) |
| ◄ Izquierda | confirma la letra ya, sin esperar la pausa |
| ► Derecha | reproduce en Morse lo que llevas escrito |
| ▲ Arriba | deshace el último símbolo; en el inicio borra la última letra |
| ▼ Abajo | espacio manual |
| Atrás | cancela la letra en curso; otra vez para salir al menú |

El nodo actual se muestra invertido y sus dos hijos enmarcados, así ves en todo
momento qué letra te daría el siguiente punto o raya. Mientras mantienes OK, el
símbolo en la esquina superior derecha cambia de `·` a `−` al cruzar el umbral, y
una barra bajo el texto muestra la cuenta regresiva para confirmar la letra
(línea continua) o el espacio (línea punteada).

**Números y signos:** están en el nivel 5 del árbol (`2 = ··−−−`, `7 = −−···`…).
Al llegar al nivel 4 la vista hace zoom al subárbol y los muestra: los dígitos
0–9 y los signos `& + = / (` cuelgan de las posiciones del cuarto nivel
(incluidas las que no tienen letra latina, que se dibujan como puntos).

### Ajustes
Desde el menú, con valores persistentes en la SD (`apps_data/san_morse`):

| Ajuste | Valores |
|---|---|
| Sonido / Vibración / LED | sí / no |
| Volumen | 25–100 % |
| Tono | 440–800 Hz |
| Velocidad | 5–35 WPM |
| Umbral raya | 150–400 ms (cuánto mantener OK para que sea raya) |
| Confirma letra | NO (solo manual con ◄) o 0.8–3 s |
| Espacio auto | NO o 2–8 s |

### Texto a Morse
Escribes un texto con el teclado del Flipper y se reproduce con **sonido, LED y
vibración**, resaltando en pantalla el carácter y el símbolo actual.

Durante la reproducción:

| Botón | Acción |
|---|---|
| OK | pausa / continuar / repetir |
| ▲ / ▼ | velocidad (5–35 WPM) |
| ◄ | sonido sí/no |
| ► | vibración sí/no |
| Atrás | detener y volver |

## Compilar

```bash
pip install --user ufbt
cd san-morse-flipper
ufbt          # genera dist/san_morse.fap
```

Con el Flipper conectado por USB: `ufbt launch` (compila, instala y abre la app).

También puedes copiar `dist/san_morse.fap` a la tarjeta SD en `apps/Tools/`.

> Compilado contra el firmware oficial (release). Si usas otro firmware
> (Momentum, Unleashed…), apunta ufbt a su SDK, por ejemplo:
> `ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json`
