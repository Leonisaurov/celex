# Organización

## Las capas

El código está partido en cuatro carpetas, y las dependencias solo van en un
sentido:

```
TUI  ->  Sheet  ->  Lang  ->  Common
```

- **`Common/`** — lo que no sabe de nada: leer un archivo, un mapa hash, y las
  referencias de celda (`A1` ←→ coordenadas).
- **`Lang/`** — el lenguaje. Escanea, analiza, compila y ejecuta. No sabe qué es
  una hoja de cálculo: para él una celda es una variable más con un nombre que
  parece una referencia.
- **`Sheet/`** — la capa que los junta. Recibe el fuente de una hoja y devuelve el
  valor de cada celda. Aquí está también el reordenado de asignaciones y el
  intérprete autónomo de línea de comandos.
- **`TUI/`** — el editor. Solo conoce la interfaz de `Sheet/`: nunca llama al
  compilador ni a la máquina virtual directamente.

El editor puede vivir sin compilar el lenguaje: `Sheet/sheet.h` es toda la
interfaz que usa. Y el lenguaje puede vivir sin editor: `Sheet/cli.c` es el
intérprete de comandos.

## Fichero por fichero

### `Common/`

| Fichero | Qué es |
|---|---|
| `ref.c` / `ref.h` | una referencia de celda ↔ dos números: `ref_column_name`, `ref_to_string`, `ref_parse` |
| `fileread.c` / `fileread.h` | leer un archivo entero a memoria (solo lo usan el intérprete y sus pruebas) |
| `hashmap.h` | el mapa hash, cabecera de terceros |
| `debug.h` | las impresiones de depuración del lenguaje, activas solo con `-DCELEX_DEBUG` |

### `Lang/`

| Fichero | Qué es |
|---|---|
| `scanner.c` / `scanner.h` | convierte el texto en tokens |
| `parser.c` / `parser.h` | convierte los tokens en bytecode |
| `compiler.c` / `compiler.h` | compila un trozo de fuente |
| `chunck.c` / `chunck.h` | el trozo de bytecode y sus constantes |
| `symtable.c` / `symtable.h` | la tabla de símbolos: variables, celdas y literales deduplicados |
| `vm.c` / `vm.h` | la máquina que ejecuta el bytecode, y la volcadura de las celdas calculadas |

### `Sheet/`

| Fichero | Qué es |
|---|---|
| `sheet.c` / `sheet.h` | la interfaz: `sheet_evaluate`, `sheet_evaluate_cells`, `sheet_set_output`, `sheet_result_free` |
| `order.c` / `order.h` | reordena las asignaciones de celda para que sean calculables, y avisa de los ciclos |
| `cli.c` | `celexs`, el intérprete de línea de comandos |

### `TUI/`

| Fichero | Qué es |
|---|---|
| `main.c` | el bucle: tamaño, pintar, esperar, manejar |
| `keys.c` / `keys.h` | qué hace cada tecla y cada clic |
| `input.c` / `input.h` | convierte lo que manda la terminal en teclas, pegados y clics |
| `tui.c` / `tui.h` | la cara visible de la terminal: escribir, mover el cursor, leer una tecla |
| `tui_platform.c` | la parte que depende del sistema: la consola de Win32, `termios` en POSIX |
| `platform.h` | las cuatro funciones que el resto del editor puede pedir a la terminal |
| `cell.c` / `cell.h` | una celda: su fuente, su valor y su formato; y el tamaño de celda |
| `cell_manager.c` / `cell_manager.h` | el documento entero: las hojas, la rejilla, el pintado por diferencias, las tres barras, el historial, la edición, el autocompletado |
| `clipboard.c` / `clipboard.h` | el portapapeles del editor, y el portapapeles del sistema |
| `complete.c` / `complete.h` | las palabras que ofrece el tabulador |
| `theme.c` / `theme.h` | los colores: qué parte de la pantalla se pinta con qué secuencia |
| `tools.c` / `tools.h` | la fila de las herramientas, y la tecla que hay detrás de cada botón |
| `document.c` / `document.h` | el `.clex`: la estructura del documento del editor |
| `file.c` / `file.h` | leer y escribir el `.cle`, y decidir qué es cada archivo |
| `script.c` / `script.h` | correr la hoja y rellenar los valores |
| `config.h` | la marca `DEBUG` |

### `tests/`

| Fichero | Qué es |
|---|---|
| `runner.c` | corre el intérprete sobre los casos y compara con lo esperado |
| `cases/*.cle`, `expected/*.txt`, `cases.txt` | los casos del lenguaje |
| `editor_test.c` | la edición y el documento, sin terminal |
| `keys_test.c` | las teclas y los clics, sin terminal |
| `input_test.c` | el analizador de la entrada |
| `app_test.c` | el editor entero, corrido con las teclas en un archivo |
| `console_test.c` | el editor dentro de una consola de verdad |
| `screen.c`, `bench.c` | herramientas de mirar, no de comprobar |

## Compilar

Dos sistemas, los mismos fuentes.

**Windows** — `build.bat`, con MSVC:

```
build.bat              la salida normal
build.bat debug        con simbolos y sin optimizar
build.bat test        compila y corre todas las pruebas
build.bat tools       compila las herramientas de mirar
build.bat clean       tira el directorio build
```

Produce `build\celex.exe` (el editor) y `build\celexs.exe` (el intérprete), y en
los modos `test` y `tools` los binarios de pruebas y las herramientas. Los
objetos intermedios van a `build\obj\`.

Los avisos del compilador están en `/W3` y la fuente se lee como UTF-8
(`/utf-8`), que es lo que hacen falta por los caracteres de la rejilla.

**POSIX** — `Makefile`, con `gcc`:

```
make            el editor y el interprete
make run        corre el editor sobre sample.cle
make run-script corre sample.cle en el interprete
make test       compila y corre todas las pruebas
make tools      las herramientas de mirar
make clean
make DEBUG=1    con las impresiones de depuracion del lenguaje
```

Lleva puesto `-fsanitize=address` por defecto; se quita con `SANITIZE=` en
máquinas donde el sanitizador no esté instalado.

## El número de compilación

`build/version.h` se genera en cada build y no se commitea. Dentro hay una sola
define, `CELEX_BUILD`, que es el `git rev-parse --short HEAD`, o `dev` si no hay
git, con un `+` detras si el árbol tiene cambios sin commitear. Lo lee la barra de
estado, al principio, para que se sepa siempre con qué binario se está trabajando:

```
 d07d1b6    A1  NORMAL  SRC  4x2
```

## Cómo se escribe el código

- **Los comentarios van en español**, y dicen por qué, no qué. Cada decisión que
  no es evidente tiene su comentario al lado: por qué el fotograma guarda números
  de estilo y no secuencias, por qué un paneo grande no se anima, por qué el
  aviso de la fila de herramientas es corto.
- **Los mensajes de la pantalla van en inglés**, y son literales: `saved`,
  `copied A1`, `the last sheet stays`, `tools off`.
- **Sin asignaciones en la declaración**, para poder seguir los `free` sin
  dependencias.
- **Los nombres de los eventos huyen de `windows.h`**: los tipos de entrada se
  llaman `INPUT_CLICK` y los botones `CLICK_LEFT` porque `INPUT_MOUSE`,
  `MOUSE_LEFT` y `MOUSE_RIGHT` ya existen con otros números, y al incluirse
  `windows.h` después de `input.h` sustituían los nuestros sin avisar.

## Los commits

Conventional Commits con alcance, y el título en español, diciendo el
comportamiento y no el fichero tocado:

```
feat(tui): el raton hace lo que las teclas
fix(tui): pegar la formula y pegar el resultado
feat(sheet): evaluar las celdas de una en una
test(lenguaje): un recorrido por todo el lenguaje, corrido por el interprete
docs: contar como habla el editor con la terminal
```

Alcances que se usan: `tui`, `sheet`, `lang`, `lenguaje`. Los `docs:` van sin
alcance. El cuerpo cuenta el porqué de un cambio que no se ve en el título, con
las medidas cuando las hay.

## Donde encaja con lo demas

- Las pruebas de lo que se compila: [pruebas.md](pruebas.md).
- Como esta repartida la pantalla y por que se pinta como se pinta: [pantalla.md](pantalla.md).

Volver al indice: [docs/README.md](README.md).
