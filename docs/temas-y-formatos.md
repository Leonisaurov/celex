# Temas y formatos de celda

Dos capas de color. El **tema** dice cómo se pinta cada *parte* de la pantalla; el
**formato** dice cómo se pinta *una celda*, por encima del tema. La alineación es
una tercera cosa: mueve el texto, no lo pinta.

## Las partes que pueden llevar color

| Estilo | Qué es |
|---|---|
| `STYLE_NONE` | el vacío de la pantalla, y el hueco del cursor de texto |
| `STYLE_GRID` | las rayas de la rejilla |
| `STYLE_HEADER` | las letras de las columnas y los números de las filas |
| `STYLE_TEXT` | el fuente de una celda |
| `STYLE_VALUE` | el valor de una celda, cuando se muestran los valores |
| `STYLE_CURSOR` | la celda seleccionada |
| `STYLE_COPIED` | el borde punteado de la celda copiada |
| `STYLE_STATUS` | la barra de estado |
| `STYLE_MESSAGE` | lo que la barra dice a la derecha |
| `STYLE_ERROR` | lo que la barra dice cuando algo no salió |

Son once (`STYLE_COUNT`), y cada tema tiene una secuencia ANSI para cada uno. Toda
secuencia empieza apagando lo que hubiera (`\x1b[0m`), que es lo que hace que
cambiar de estilo cueste una sola secuencia.

## Los tres temas

Se pasa de uno a otro con `Ctrl+T`, en este orden y dando la vuelta:
`normal` → `oscuro` → `claro` → `normal`. La barra dice cual quedó puesto, porque
la barra no lo muestra siempre.

### `normal` — el que tenía el editor antes de existir los temas

Se apoya en los colores de la terminal. Solo dos partes llevan algo, y es el
inverso: la celda seleccionada y la barra de estado. Todo lo demás queda sin color,
así que una hoja en `normal` escribe casi nada de secuencias.

### `oscuro` — para una terminal oscura

| Parte | Secuencia | Lo que se ve |
|---|---|---|
| rejilla | `\x1b[0;90m` | gris, para que las rayas no pesen |
| cabeceras | `\x1b[0;1;36m` | cian en negrita |
| fuente de una celda | `\x1b[0;37m` | blanco |
| valor de una celda | `\x1b[0;33m` | amarillo, para distinguirlo del fuente |
| celda seleccionada | `\x1b[0;44;97m` | blanco sobre azul |
| celda copiada | `\x1b[0;1;35m` | magenta en negrita |
| barra de estado | `\x1b[0;104;97m` | blanco brillante sobre azul claro |
| mensaje | `\x1b[0;32m` | verde |
| error | `\x1b[0;1;31m` | rojo en negrita |

### `claro` — para una terminal clara

| Parte | Secuencia | Lo que se ve |
|---|---|---|
| rejilla | `\x1b[0;37m` | gris claro |
| cabeceras | `\x1b[0;1;34m` | azul en negrita |
| fuente de una celda | `\x1b[0;30m` | negro |
| valor de una celda | `\x1b[0;1;32m` | verde en negrita |
| celda seleccionada | `\x1b[0;46;30m` | negro sobre cian |
| celda copiada | `\x1b[0;1;35m` | magenta en negrita |
| barra de estado | `\x1b[0;43;30m` | negro sobre amarillo |
| mensaje | `\x1b[0;34m` | azul |
| error | `\x1b[0;1;31m` | rojo en negrita |

El vacío no está en ninguna tabla: en los tres temas es el apagado, y es el hueco
del cursor de texto dentro de la celda seleccionada, para que se vea dónde entra lo
que se escriba.

Cambiar de tema olvida el fotograma y vuelve a pintar todo: el fotograma guarda
*números* de estilo, no secuencias, así que si no se olvidara la comparación con el
fotograma anterior no vería diferencia y no se repintaría nada.

## El formato de una celda

Cinco campos, y `0` en todos significa «el tema» (`TUI/cell.h:32-38`):

| Campo | Valores | Qué hace |
|---|---|---|
| `align` | 0 por defecto, 1 izquierda, 2 centro, 3 derecha | dónde se pone el texto dentro de la celda |
| `bold` | 0 o 1 | negrita |
| `underline` | 0 o 1 | subrayado |
| `fg` | 0 (el del tema) o 1 a 16 | color del texto |
| `bg` | 0 (el del tema) o 1 a 16 | color del fondo |

**Es una capa encima del fuente, no encima del valor**: el fuente de `A1` sigue
siendo `= 1` y el lenguaje no ve nada del formato. Por eso vive en el `.clex` y no
en el `.cle`, y por eso el `.cle` exportado sale sin formatos.

Por teclado: `Ctrl+B` negrita, `Ctrl+U` subrayado, `Ctrl+O` recorre la alineación.
Son pocas teclas a propósito: cada una hace una cosa y no hay modos. Los colores
solo se ponen en el documento, con `fg=` y `bg=` (está apuntado en
[ideas.md](ideas.md) darles un botón o una tecla).

Poner formato a un sitio vacío hace que la celda exista, y marca la hoja como
cambiada: si no, formatear y salir no guardaría nada.

## La alineación

Una línea de texto dentro de una celda de `cellWidth` columnas se corre así
(`align_pad`, `TUI/cell_manager.c:656-676`):

| Alineación | Cuanto se corre |
|---|---|
| izquierda | nada |
| derecha | todo lo que sobra, delante del texto |
| centro | la mitad de lo que sobra (el resto se queda detrás) |
| por defecto | se decide con el texto de la celda |

Lo que sobra es `cellWidth - cuantos caracteres caben en esa línea`; si no cabe
nada, no se corre nada. **Se calcula por línea**, porque el texto de una celda se
reparte en las filas de la celda, y cada línea se alinea sola. El hueco del cursor
de texto se rige por la línea en la que cae.

**La de por defecto es la de una hoja de cálculo**: un número a la derecha y lo
demás a la izquierda. Un texto es número cuando tiene al menos un dígito y no tiene
más que dígitos, `.` `,` `-` y `+` (`looks_numeric`,
`TUI/cell_manager.c:634-651`). Consecuencias:

- `123`, `12.500` y `-7` son números → a la derecha;
- `total` es texto → a la izquierda;
- `=A1 + A2` **no** es un número (el `=` no está en el conjunto) → a la izquierda,
  que es lo que se quiere: una fórmula se ve como un fuente, no como un resultado.

## De los campos a las secuencias

Un formato con algo puesto no cambia el estilo del tema: lo **extiende**, en este
orden (`format_style`, `TUI/cell_manager.c:610-630`):

```
<secuencia del tema>  \x1b[1m si hay negrita  \x1b[4m si hay subrayado  <fg>  <bg>
```

Si no hay negrita, subrayado ni colores, se devuelve el estilo del tema tal cual y
no se gasta nada. La alineación no entra aquí: no pinta, mueve.

Los números de la paleta se traducen así (`color_sequence`,
`TUI/cell_manager.c:592-605`):

| Número | Delante | Detrás |
|---|---|---|
| 1 a 8 (negro, rojo, verde, amarillo, azul, magenta, cian, blanco) | `30` + n − 1 | `40` + n − 1 |
| 9 a 16 (los mismos brillantes) | `90` + n − 9 | `100` + n − 9 |

Así el 3 es el verde (`\x1b[32m`) y el 12 el amarillo brillante de fondo
(`\x1b[103m`). Fuera de 1 a 16 el documento no lo acepta y no se pinta nada.

## Los sitios de estilo propios

El fotograma guarda un número de estilo por celda, así que los estilos que inventa
un formato tienen que tener un número que signifique lo mismo en el fotograma de
ahora y en el de antes. Esos números van detrás de los once del tema, en hasta
**64 sitios** guardados por secuencia (`CUSTOM_MAX`, `TUI/theme.c:68`).

Se reutilizan: dos celdas con el mismo formato ocupan el mismo sitio. Y se olvidan
al cambiar de tema, porque las secuencias del tema anterior no valen con el nuevo —
de ahí que cambiar de tema redibuje la pantalla entera. Si alguna vez se llenan los
64 sitios, las celdas de más se pintan con el estilo del texto: no se pierde nada,
pero ese formato deja de notarse. Es muy difícil de alcanzar: hacen falta 64
combinaciones distintas de negrita, subrayado y dos colores en la misma pantalla.

## Donde encaja con lo demas

- Donde se guarda cada formato, ficha a ficha: [archivos.md](archivos.md).
- Por que el fotograma guarda numeros de estilo y que eso obliga a redibujar: [pantalla.md](pantalla.md).
- Las teclas del formato y los botones de la fila: [teclas-y-raton.md](teclas-y-raton.md).

Volver al indice: [docs/README.md](README.md).
