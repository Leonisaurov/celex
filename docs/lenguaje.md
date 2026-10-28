# celex script

El lenguaje de la hoja. Está en `Lang/` (escáner, analizador, compilador y máquina)
y lo que lo une a la hoja está en `Sheet/`. Corre de dos formas: dentro del editor
(al confirmar una celda y al recalcular con `Ctrl+R`) y solo, con `celexs`.

## Un programa es una hoja

Lo que se escribe son asignaciones de celda, una por línea, con el fuente después
del igual:

```
A1 = 1
A2 = 2
B1 = A1 + A2
C1 = "hola"
D1 = B1 > 2
```

Además de asignaciones, el fuente puede llevar lo demás del lenguaje: variables,
`print`, `if`, `while`, `for`, `input` y `return`. El editor no usa eso: al exportar
un `.cle` lo que sale son las celdas.

## Valores

Cuatro cosas, y ninguna más:

| Valor | Cómo se escribe | Cómo se imprime |
|---|---|---|
| número | entero, `42`, `-7`. Los decimales **se truncan**: `3.5` es `3` | `42` |
| cadena | entre comillas dobles: `"hola"` | `hola`, sin comillas |
| booleano | `true` y `false` | `true` / `false` |
| vacío | no se escribe; es lo que tiene una celda sin valor, o una variable que no existe | `nil` |

No hay listas ni tablas. **En las cadenas no hay secuencias de escape**: no se puede
meter una comilla dentro de una cadena, y una barra es una barra.

## Operadores

`+` `-` `*` `/` `==` `!=` `<` `<=` `>` `>=`, y el menos delante de un valor. De más
fuerte a más flojo: paréntesis, el menos unario, `* /`, `+ -`, las comparaciones.
Así `3 + 4 * 2` es 11, y `(3 + 4) * 2` es 14.

- **La división es entera y trunca hacia cero**: `7 / 2` es 3. Dividir por cero es un
  error de ejecución (`Error en la operacion`) y el programa para.
- **`+` con una cadena concatena**, y un número metido en la cadena se escribe como
  número: `"total " + 7` es `total 7`.
- En el resto de operaciones una cadena vale 0. `3 * "hola"` es 0, no un error.
- Comparar dos cadenas las compara de texto (`"a" < "b"` es cierto); comparar una
  cadena con un número compara 0 contra el número.
- **No existen**: `%`, potencias, `&` `|` `<<`, `&&` `||` `!` (el `!` solo forma
  `!=`), `+=`, ni el `not`. Para negar se compara contra 0.

## Variables

No se declaran: nacen con la primera asignación. `nombre = expresión`. Todas son del
programa entero, sin ámbitos: lo que se escribe dentro de un `while` sigue vivo al
salir.

Las variables y las celdas viven separadas y no se pisan: `A1 = 5` es la celda,
`a1 = 5` es una variable que se llama `a1`. **Una referencia de celda quiere las
letras mayúsculas**; en minúsculas es un nombre.

## Sentencias

| Sentencia | Cómo se escribe |
|---|---|
| imprimir | `print expresión` |
| leer | `input nombre` — una línea de la entrada, a esa variable |
| condición | `if expresión { ... }`, con `else if` y `else` |
| repetir | `while expresión { ... }` |
| recorrer | `for i in inicio:fin { ... }` — **incluye el final** |
| parar el programa | `return`, o `return expresión` |

Las llaves del bloque son opcionales: `if x { print 1 }` y `if x print 1` valen, y
un bloque puede llevar varias sentencias dentro. **Un bloque suelto, sin sentencia
delante, no es una sentencia**: `{ print 5 }` da `Se esperaba una sentencia`.

El `for` necesita un nombre, `in`, una expresión, `:` y otra expresión:
`for i in 1:5 { pasos = pasos + i }` suma 15, porque el 5 entra. Al terminar, la
variable se queda en `fin + 1`.

Las sentencias se separan con un salto de línea o con `;` — da igual, y se pueden
mezclar: `x = 1; y = 2; print x + y`.

**No hay `break` ni `continue` ni `switch`.** `return` no vuelve de nada: no hay
funciones, y lo que hace es terminar el programa entero.

## Funciones

No las hay. Ni se pueden escribir, ni hay ninguna hecha: no hay `len`, ni `str`, ni
`min`. Las tres sentencias de arriba (`print`, `input`, `return`) son sentencias de
lenguaje, no llamadas, y no se pueden usar como valores.

## Celdas y orden de cálculo

Una referencia es una o más letras mayúsculas y unos números: `A1`, `AB12`. `A` vale
1, la columna es base 26, y `AB` por tanto vale 28. La fila empieza en 1: no hay
fila 0, así que `A0 = 5` se traga en silencio — se asigna a una celda que no existe.
`A1B` tampoco es una referencia — sobra algo detrás —, y tampoco da error: se queda
como nombre de variable, y `A1B = 7` asigna una variable.

**El lenguaje no sabe mirar hacia delante**: una celda que se usa antes de estar
asignada valdría 0. Eso se resuelve fuera del lenguaje, reordenando las asignaciones
de celda antes de calcular, por dependencias y respetando el orden original
(`Sheet/order.c`). `D1 = B1 + C2` escrito antes de `C2 = 10` se calcula bien: vale
13. Lo hace solo con asignaciones de celda de primer nivel; las demás sentencias se
quedan donde estaban.

**Si dos celdas se necesitan la una a la otra, se avisa y no se calcula**:
`referencia circular entre celdas: A1, B1`, con hasta cuatro nombres y un
` y N más` si hay más. La autorreferencia (`A1 = A1 + 1`) cuenta como ciclo.

Una celda vacía vale `nil`, y en una operación `nil` vale 0. Al exportar la hoja,
una celda que quedó `nil` no se escribe.

## La entrada

`input nombre` lee una línea de la entrada estándar y la guarda como cadena, sin
imprimir ninguna pregunta. Al final de la entrada guarda la cadena vacía y sigue: un
programa con `input` corrido sin teclado no se para, y por eso las pruebas lo ejecutan
con la entrada cerrada.

## Correrlo solo: `celexs`

```
celexs hoja.cle            corre el programa: los print salen a la consola
celexs hoja.cle --values   imprime ademas el valor de cada celda, en stdout
```

Con `--values` lo que imprime el programa se manda a la salida de error, para que la
salida estándar solo traiga las celdas, una por línea, en el mismo orden en que el
editor guarda la hoja: por columna y después por fila:

```
A1 = 1
A2 = 2
B1 = 3
C1 = hola
```

Sin `--values` no se lista ninguna celda.

**Los códigos de salida**: 0 si el programa terminó, y error (el runner lo lee como
255) si no se pudo leer el archivo, si el fuente no se pudo analizar, o si las celdas
se referencian en círculo. Un error **de ejecución** — dividir por cero, una
instrucción que no existe — imprime su aviso, para el programa ahí, y **deja salir
con 0**: las celdas que había calculado se listan igual. Está apuntado en
[ideas.md](ideas.md) como cosa que se ha de arreglar.

Sin argumentos se imprime el uso y se termina con error: `celexs` solo no hace nada.

## Cómo lo usa el editor

`Ctrl+R` evalúa la hoja, y también se hace al confirmar una celda, al borrarla, al
pegar y al deshacer. **Dentro del propio proceso**: no se lanza ningún binario, se
llama a la capa de hoja directamente, que calcula **celda a celda** y conserva los
valores ya calculados para que la siguiente los vea.

Lo que el programa imprima con `print` **no llega a la pantalla del editor**: se
tira a `NUL` (o `/dev/null`), porque rompería el dibujo.

Si una celda no se puede calcular, las demás se calculan igual y solo se avisa de
esa: `B1: Se esperaba una expresion`. El aviso sale cuando el fuente de la celda no
compila; si lo que falla es la operación en sí —dividir por cero—, su aviso se tira
como el de `print`, y la celda se queda sin valor sin más nombre que ese. Como
aviso, no como fallo, si alguna se pudo. Si no se pudo ninguna, es un error. El aviso
cabe en la barra: lo que pase de 127 caracteres se corta.

Tras `Ctrl+R` se ponen los valores en la pantalla (`showValues`), para ver el
resultado. El editor deja también el mensaje `script executed` cuando no hubo nada
que avisar.

## Lo que el lenguaje no tiene

Para no buscarlo:

- comentarios: no hay `#`, ni `//`, ni nada. Un `#` es un símbolo no reconocido.
- números decimales (se truncan), fechas, horas
- `break`, `continue`, `switch`, funciones, `not`, `&&`, `||`, `%`, potencias
- rangos de celdas: `A1:B3` lo reconoce el escáner, pero el analizador no lo entiende
  (`Se esperaba una expresion`). El `:` solo vale dentro del `for`
- escapados en cadenas, ni una comilla dentro de una cadena
- ámbitos, constantes, tipos declarados
- referencias adelantadas (las resuelve el reordenado de fuera, no el lenguaje)

## El recorrido completo

`tests/cases/lenguaje.cle` es un programa que pasa por todo lo anterior y su salida
esperada está en `tests/expected/lenguaje.txt`: aritmética y su prioridad, cadenas y
`+` con números, booleanos y comparaciones, `if`/`else if`/`else`, `while`, `for`
con rango, celdas en desorden que se resuelven, sentencias con `;`, `input` sin
teclado y `return` parando el programa. Es a la vez la prueba del intérprete y la
especie de manual escrito en el propio lenguaje.

## Donde encaja con lo demas

- Como se escribe ese programa en un archivo: [archivos.md](archivos.md).
- Los casos que recorren el lenguaje y como se comparan: [pruebas.md](pruebas.md).
- Lo que hace el editor con los valores que devuelve: [hojas-historial-y-entrada.md](hojas-historial-y-entrada.md).

Volver al indice: [docs/README.md](README.md).
