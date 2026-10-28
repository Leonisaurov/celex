# Los dos archivos

Un trabajo de celex vive en dos ficheros, y cada uno es de uno.

| | `.cle` | `.clex` |
|---|---|---|
| qué es | un programa de celex script | el documento del editor |
| para quién | para que lo lea y lo escriba una persona | para que lo lea el editor |
| contiene | una asignación por celda | hojas, formatos y dónde estaba el cursor |
| qué lo entiende | `celexs` (el intérprete) y `load_script` | `document_load` |
| a mano | sí, es limpio y simple | no: está delimitado por bloques, y no tiene comentarios |

## El `.cle`

Una línea por celda: la referencia, un igual, y el fuente.

```
A1 = 1
A2 = 2
B1 = A1 + A2
C1 = "hola"
```

Al **exportar** (`Ctrl+E`) las celdas van ordenadas **por columna y después por
fila**. No hace falta que ese orden respete las dependencias: al evaluar, la capa de
hoja reordena las asignaciones de celda en el orden que el lenguaje puede calcular.
Las celdas sin
contenido no escriben línea. Del editor solo salen celdas: las asignaciones a
variables y los `print` que estuvieran en el fuente no vuelven a salir, porque nunca
llegaron al editor — al importar se cuentan como ignoradas.

Al **importar** se queda con **una sola hoja**, que se llama como el archivo sin
extensión, y se olvida lo que hubiera en el editor antes. Las líneas que no tienen
igual, o cuya izquierda no es una referencia, se cuentan: `loaded (3 lines ignored)`.

Con un `.cle` abierto, la barra dice `imported: saves as <ruta>.clex`: guarda al
lado, en el formato del editor.

## El `.clex`

Escrito para que lo lea el editor. Una cosa por línea, y una palabra al principio de
cada línea que dice qué es.

```
celex-document 1
sheet "numeros"
  select E3
  scroll 15 0
  cell A1 "1"
  cell B1 "A1 + A2"
  cell C1 "\"hola\""
  format C1 align=center bold fg=3
end-sheet
sheet "gastos"
  select A1
  scroll 0 0
end-sheet
```

### La cabecera

`celex-document 1` tiene que ser la primera línea, y el número es la versión del
formato (hoy 1). **Es lo que decide qué es el archivo**: no la extensión. Si el
texto empieza con esa cabecera es un documento; si no, es un guion. Así un archivo
guardado con un nombre raro se sigue abriendo bien, y uno de una versión futura no
deja de abrirse porque traiga un campo nuevo.

Un documento **de una versión más nueva** que el editor se abre igual: se lee lo que
se entiende y las líneas sueltas se cuentan como ignoradas.

### Los campos que ya no están

Los campos que tenía el documento en la primera versión —`theme`, `cell-size` y
`tools`— ya no son de la hoja: pasaron a las preferencias del usuario, en
`$HOME/.config/celex/config` (ver [temas-y-formatos.md](temas-y-formatos.md) y el
README). Un archivo viejo que todavía los traiga se abre igual: esas líneas se leen
y se dejan estar, sin contarlas como ignoradas. El tema y el tamaño que valen son
los de la configuración, no los del archivo.

### Dentro de cada hoja

`sheet "nombre"` abre el bloque y `end-sheet` lo cierra. La primera hoja del
documento renombra la que ya había, en vez de añadir una: así un archivo con una
hoja no deja dos.

| Línea | Qué hace | Si viene mal |
|---|---|---|
| `select <REF>` | dónde estaba el cursor | sin referencia o fuera de una hoja: se ignora y se cuenta |
| `scroll <x> <y>` | cuánto deslizada llevaba la vista, en columnas y filas de pantalla | negativos: se ignora y se cuenta |
| `cell <REF> "<fuente>"` | el fuente de una celda | sin comillas, fuera de una hoja, o con referencia inválida: se ignora |
| `format <REF> <fichas>` | el formato de una celda | ver abajo |

Las celdas van ordenadas por columna y después por fila, igual que en el `.cle`. Una
celda sin fuente no escribe línea `cell`, pero sí puede escribir su línea `format`:
un sitio vacío se puede pintar antes de llenarlo.

### La línea de formato

Una ficha por campo, separadas por espacios, sin comillas — nunca llevan texto
libre:

| Ficha | Qué hace |
|---|---|
| `align=left`, `align=center`, `align=right` | la alineación (`align=default` también se acepta, y es como no ponerla) |
| `bold` | negrita |
| `underline` | subrayado |
| `fg=<1 a 16>` | color del texto, de la paleta |
| `bg=<1 a 16>` | color del fondo |

**Todo o nada**: si una ficha no se entiende, o un color está fuera de 1 a 16, se
descarta la línea entera y se cuenta como ignorada; no se queda medio formato
puesto. Una celda cuyo formato es el de empezar no escribe línea.

Al escribir, las fichas salen siempre en el mismo orden: `align=`, `bold`,
`underline`, `fg=`, `bg=`.

### Las comillas

Lo que va entre comillas escapa la comilla como `\"` y la barra como `\\`. El
contenido de una celda es siempre una línea: si en el fuente había un salto de línea
o un retorno de carro, al escribir el documento se convierte en un espacio. Al leer,
una barra hace literal el carácter de detrás.

Un fuente de celda se lee en un sitio de 1024 bytes: **lo que pase de 1023
caracteres se corta en silencio**.

## Abrir y guardar

- **Abrir** (`celex <archivo>`): si el texto tiene cabecera de documento, se carga
  como documento; si no, se importa como guion. Los mensajes posibles:
  `imported: saves as <ruta>` (era un guion), `file loaded` (era un documento),
  `loaded (N lines ignored)`, `cannot open file`, o `new sheet` si el archivo no
  existía.
- **`Ctrl+S`** escribe siempre un **documento** en `savePath`: si se abrió un `.cle`
  es el `.clex` de al lado; si se abrió un `.clex`, el mismo. Dice `saved`,
  `no file to save`, o `cannot write file`.
- **`Ctrl+E`** exporta la **hoja que se está viendo** como guion `.cle`, al lado del
  documento. Dice `exported <ruta>`, `nothing to export` (sin documento donde mirar),
  `the name does not fit`, o `cannot write the script`.
- **`Ctrl+L`** vuelve a leer el archivo desde el disco. Si hay cambios sin guardar
  pide confirmación: `unsaved changes: press Ctrl+L again to reload`.

Un punto del nombre no es una extensión si cae dentro de un directorio:
`carpeta.d/me` se guarda como `carpeta.d/me.clex`.

## Qué se pierde en el viaje

De `.clex` a `.cle` se quedan fuera todo lo que no sea el fuente de una celda:
**los formatos**, **las demás hojas** (solo se exporta la que se ve), dónde estaba
el cursor y cuánto deslizada la vista. El tema, el tamaño de las celdas y la barra
de herramientas no están en el documento: son preferencias del usuario y el `.cle`
tampoco las lleva.

De `.cle` a `.clex` se pierde todo lo que no sea una asignación de celda: las
variables, los `print` y las sentencias del programa no se importan (se cuentan como
ignoradas), así que tampoco vuelven a salir al exportar. El viaje de vuelta solo
devuelve celdas.

Por eso son dos archivos: `Ctrl+E` enseña el guion limpio para editarlo a mano o
para correrlo con `celexs`, y `Ctrl+S` guarda la hoja de trabajo entera.

## Donde encaja con lo demas

- Lo que es un `.cle` para el lenguaje: [lenguaje.md](lenguaje.md).
- Los campos del formato que escribe la linea `format`: [temas-y-formatos.md](temas-y-formatos.md).
- Que tecla escribe que fichero, y que dice la barra al hacerlo: [teclas-y-raton.md](teclas-y-raton.md).

Volver al indice: [docs/README.md](README.md).
