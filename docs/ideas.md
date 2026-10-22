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

Lo que queda:

- El raton: hacer clic en una celda mueve el cursor alli; en una pestana cambia
  de hoja; en un boton ejecuta su comando. Con clic derecho se copia, y con doble
  clic se entra a editar, como en cualquier hoja de calcula.
- En Windows los eventos de raton llegan por la consola (`MOUSE_EVENT`) y por las
  secuencias de la terminal; en POSIX por `SGR` (`ESC[<b;x;yM`). El editor traduce
  los dos a un mismo evento: boton, accion, columna y fila. Hay que pedir el
  reporte al entrar (`ESC[?1000h ESC[?1006h`) y apagarlo al salir, no sea que la
  terminal se quede mandando clics a lo que venga detras.
- Donde no hay raton (una tuberia, las pruebas) la barra se puede seguir usando
  por teclado: los botones tambien tienen su tecla.
- Los colores de la celda por boton, cuando la barra tenga su hueco: una paleta
  de 16 numeros no cabe en una fila de botones, pero un boton que los recorra si.

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

Lo que hay hecho se comprueba en el ida y vuelta del documento
(`tests/editor_test`), en las teclas (`tests/keys_test`) y en el editor entero
corrido con un archivo de teclas (`tests/app_test`), que es donde se ve que el
`.cle` exportado sale limpio y el `.clex` guarda el formato.

Lo que queda tiene su prueba pensada: el raton, con los eventos escritos en el
archivo de teclas como hacen ya las pruebas del editor; la barra, con lo que sale
por la pantalla, que `tests/screen` deja mirar; y los numeros decimales, con un
caso nuevo en `tests/cases`.
