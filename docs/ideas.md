# Ideas y camino

Notas de lo que queda por hacer y de como se piensa atacarlo, para que no se
pierdan entre commits. Lo que hay hecho esta en el README; aqui vive lo que esta
en la cabeza.

## Formatos de celda

Lo hecho: alineacion, negrita y subrayado por tecla (`Ctrl+O`, `Ctrl+B`,
`Ctrl+U`), colores propios por numero de paleta, y todo ello en el `.clex` con
una linea `format REF tokens...` por celda. La alineacion de empezar es la de una
hoja de calculo: numero a la derecha, lo demas a la izquierda. El fuente de la
celda no se toca, y el `.cle` sale sin formatos.

Lo que se deja para despues, y por que:

- **Los colores por teclado o por botones.** Hoy un color propio solo se pone
  editando el `.clex`. Hace falta una fila de botones (abajo) o una tecla que
  recorra la paleta; lo resuelve la barra de herramientas, que lleva su propio
  capitulo. Una tecla `Ctrl+G` que pase el color de delante, y `Ctrl+I` el de
  detras, es lo minimo para no tener que tocar el archivo.
- **Formatos de numero** (`#,##0.00`, porcentajes, fechas). Hace falta decidir
  como se ven los numeros —el lenguaje solo tiene enteros— y eso es un cambio del
  lenguaje, no del editor. Mejor despues de los numeros decimales.
- **Bordes** entre celdas (la rejilla es un dibujo, no bordes por celda). Haria
  falta una segunda capa de dibujo por encima de la rejilla.
- **Copiar el formato con la celda** (`Ctrl+C` lleva ahora solo el fuente). Se
  puede juntar con pegar-formato (`Ctrl+Shift+V`) cuando haya seleccion de rango.
- **Deshacer un formato**: el historial guarda fuentes, no formatos, y
  formatear si marca la hoja como cambiada.

## Herramientas a la vista, y raton

Lo hecho: una sola lista (`Tool` con nombre y tecla) en `TUI/tools.c`, la fila
`tools: bold line align copy paste undo redo save hide...` encima de las
pestañas, y `Ctrl+K` para quitarla y devolverle la fila a la rejilla. La lista no
tiene acciones propias: cada boton *es* su tecla, asi que la barra y el teclado no
pueden contradecirse. Lo que no cabe en el ancho no se pinta, como las pestanas.
En el documento se guarda `tools off`, y sin esa linea la fila esta puesta.

Quitar la fila quita el boton de quitarla, asi que quedan dos tiradores para
volver: el chip `[tools]` al final de la fila de las pestanas, que solo se pinta
mientras la fila esta fuera, y el nombre del archivo al final de la barra de
estado, que esta siempre. El aviso que deja es `tools off`, corto a proposito: la
barra va justa y un aviso largo se tapa el nombre, que es justamente lo que hay
que poder ver y clica.

El raton tambien esta: la terminal manda `ESC[<boton;columna;fila M` (o `m` al
soltar) y eso es lo que lee `TUI/input.c`. En Windows, la consola da `MOUSE_EVENT`
en vez de secuencias, y `TUI/tui_platform.c` los convierte en la misma secuencia
para que solo haya un camino despues. El reporte se pide al entrar y se apaga al
salir, no sea que la terminal se quede mandando clics a lo que venga detras.

Un clic en una celda mueve el cursor, dos clics encima de la misma abren la celda
a escribir, el boton derecho copia, la rueda pasa de fila, una pestana cambia de
hoja y un boton manda su tecla. Escribiendo en una celda, un clic en otra termina
la escritura primero, como el Escape: no se pierde lo escrito por llevar la mano
al raton.

Lo que queda:

- **Seleccion de rango con el arrastre.** Pide pedir tambien los movimientos con
  el boton apretado (`ESC[?1002h`) y un principio y un final en la hoja. Va junto
  con la seleccion de rango por teclado, que es la pieza que desbloquea mas cosas.
- **Los colores por boton o por tecla.** Hoy un color propio solo se pone
  editando el `.clex`. Una paleta de 16 numeros no cabe en una fila de botones,
  pero un boton que los recorra si: `Ctrl+G` el de delante y `Ctrl+I` el de
  detras, volviendo al color del tema al dar la vuelta.
- **El tiempo del doble clic** esta fijado en 500 ms (`CLICK_DOUBLE_MS`). Si algun
  dia se quisiera configurable, seria una linea mas en el documento.
- **Donde no hay raton** (una tuberia, las pruebas) todo sigue siendo de teclado,
  y las pruebas mandan los clics como bytes en el archivo de teclas: asi se
  comprueban los dos caminos en la misma prueba.

## Lo que falta alrededor

- **Seleccion de rango** (mayusculas + flechas, o arrastre). Sin ella no tiene
  sentido rellenar, borrar un bloque, ni copiar formatos en grupo. Es la pieza
  que desbloquea mas cosas.
- **Rellenar** (`Ctrl+D` hacia abajo desde la seleccion) y **autocompletar por
  serie** (1,2,3 → sigue la serie).
- **Numeros decimales y fechas** en el lenguaje, que es lo que abre los formatos
  de numero.
- **Barra de formulas**: un campo encima de la rejilla donde se ve y se edita el
  fuente de la celda, como en una hoja de calculo de verdad.
- **Busqueda y reemplazo** en la hoja.
- **Proteger una hoja** o una celda, para que el formato no se mueva.

## Como se comprueba

La referencia esta en [pruebas.md](pruebas.md). Lo que hay hecho se comprueba en el
ida y vuelta del documento (`tests/editor_test`), en las teclas y los clics
(`tests/keys_test`), en las secuencias que llegan de la terminal
(`tests/input_test`) y en el editor entero corrido con un archivo de teclas
(`tests/app_test`) y dentro de una consola de verdad (`tests/console_test`), que es
donde se ve que el `.cle` exportado sale limpio y el `.clex` guarda formatos, fila
de herramientas y clics.

Lo que queda tiene su prueba pensada: la seleccion de rango, con el borde pintado y
el borrado en bloque; rellenar, con el ida y vuelta del documento; y los numeros
decimales, con un caso nuevo en `tests/cases`.

## Lo que se vio al escribir la documentacion

Escribir `docs/` fue pasar fichero por fichero, y salieron cosas que estan metidas
y no hacen nada, o que no cuadran. Ninguna es un fallo por el lado de usar el
editor; es limpieza, y cada una merece su commit:

- **`run_external_script` no la llama nadie** (`TUI/script.c:145`). Era el camino de
  sacar `celexs` como proceso aparte; hoy `Ctrl+R` evalua dentro del proceso. O se
  conecta a algo, o se quita, con su temporal, su `CELEX_POPEN` y sus cuatro
  mensajes.
- **`current_sheet` y `copy_cell_content` tampoco las llama nadie**
  (`TUI/cell_manager.c:877`, `TUI/cell_manager.c:810`).
- **`clipboard_value` no se lee nunca**: el valor de la celda copiada se guarda en el
  portapapeles y no se pega jamas (`TUI/clipboard.c:96`). Si algun dia se quiere
  pegar solo el valor, ahi esta; si no, ese `value` es peso muerto.
- **Tokens y opcodes que no llegan a existir**: `TOKEN_TYPE`, `TOKEN_DEF` y
  `TOKEN_BOOL` los enumera el escaner pero no los produce, y `OP_GOTO`, `OP_GOTOB`,
  `OP_UPV`, `OP_IF`, `OP_DEF` y `OP_ASIGN` estan en el chunk sin que el analizador
  emita ninguno. Andamio de algo que no se construyo.
- **Un error de ejecucion sale con 0 y un error de analisis sale con error (255)**.
  `print 1 / 0` avisa, para el programa y termina bien; `A1 = 3 +` no. Hay que
  unificarlos: que la maquina avise y que el CLI devuelva error. Los casos de ciclo
  del runner no lo pillan porque esos fallan en el reordenado, que si devuelve
  error.
- **El aviso de una celda que no se pudo calcular se trunca al copiarlo**: se compone
  en un sitio de 512 (`Sheet/sheet.c:144`) y se guarda en uno de 128
  (`TUI/keys.c:19`, el mensaje de `recalculate_sheet`). Con varias celdas malas en un
  fuente largo se pierde el final del aviso. Es solo el aviso, no el calculo.
- **Una comprobacion escrita dos veces, pegada**: `if (content[0] == '\0') continue;`
  en `TUI/cell_manager.c:1659-1665`.
- **`sprintf` sin limite** en `ref_to_string` (`Common/ref.c:30`) y en los `print`
  del lenguaje: el sitio se calcula antes de escribir, asi que hoy cabe, pero no lo
  comprueba nadie.
- **Cuantos botones caben en la fila de herramientas** depende del ancho y no estaba
  dicho en ningun sitio: en 80 columnas son diez, y `value`, `sheet` y `theme`
  esperan pantalla mas ancha. Si la lista crece, merece la pena pensar en dos filas
  o en un boton de mas, en vez de dejar botones invisibles.

Volver al indice: [docs/README.md](README.md).
