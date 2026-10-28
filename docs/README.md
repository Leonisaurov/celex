# Documentación de celex

Hoja de cálculo que vive en la terminal, escrita en C, con un lenguaje de
scripting propio (archivos `.cle`). El editor escribe la hoja como un guion, el
lenguaje lo evalúa y devuelve el valor de cada celda.

Esta carpeta es la **referencia**: qué hay en cada sitio, qué hace cada tecla, qué
acepta cada fichero. El `README.md` de la raíz es la puerta de entrada (compilar,
probar, arrancar); `docs/ideas.md` es el cuaderno de lo que queda por hacer.

## Índice

| Documento | De qué habla |
|---|---|
| [organizacion.md](organizacion.md) | las capas, fichero por fichero, el build, el número de compilación, los commits |
| [pantalla.md](pantalla.md) | el layout, las barras, las ventanas flotantes, el fotograma y el deslizamiento de la vista |
| [teclas-y-raton.md](teclas-y-raton.md) | todas las teclas, todos los clics y la barra de herramientas |
| [hojas-historial-y-entrada.md](hojas-historial-y-entrada.md) | las hojas, el portapapeles, deshacer/rehacer y el autocompletado |
| [temas-y-formatos.md](temas-y-formatos.md) | los colores del tema, los formatos de celda y cómo se pintan |
| [archivos.md](archivos.md) | el `.cle` y el `.clex`: gramática, carga, guardado e ida y vuelta |
| [lenguaje.md](lenguaje.md) | celex script: todo lo que acepta y todo lo que no |
| [pruebas.md](pruebas.md) | el runner de casos, las pruebas del editor y las herramientas de mirar |
| [ideas.md](ideas.md) | el cuaderno de lo que queda por hacer y de los defectos conocidos |

## Nombres que se repiten en todos los documentos

- **hoja** — cada una de las páginas del archivo. El documento admite varias y van
  en su fila de pestañas.
- **celda** — un sitio de la rejilla, con tres cosas distintas: el **fuente** (lo
  que está escrito), el **valor** (lo que calculó el lenguaje) y el **formato**
  (cómo se pinta).
- **referencia** — el nombre de una celda: columna en letras mayúsculas y fila en
  números, `A1`, `AB12`. Adentro del editor son coordenadas que empiezan en cero;
  en el texto empiezan en uno.
- **guion** — el archivo `.cle`, el programa del lenguaje.
- **documento** — el archivo `.clex`, el estado del editor.
- **preferencias** — lo que se elige una vez y vale para todos los archivos (tema,
  tamaño de celda, barra de herramientas, valores o fuente). Viven en
  `$HOME/.config/celex/config`, no en el documento.
- **barra de herramientas**, **fila de pestañas**, **barra de estado** — las tres
  líneas que enmarcan la rejilla: la barra arriba, las otras dos abajo.
- **ventana flotante** — lo que cuelga de la barra o sale en medio de la rejilla: el
  desplegable de una sección, la lista de los temas, el campo de un nombre de archivo.

## Un detalle de idioma

Los comentarios del código, las pruebas y esta documentación van en español; los
mensajes que ve el usuario en la pantalla van en inglés (`saved`, `copied A1`,
`tools off`), porque son strings de la interfaz. Cuando un documento cita un
mensaje, lo cita tal cual, en inglés.

## Por donde empezar

- Acabar de arrancar, compilar y probar: el [README de la raiz](../README.md), que ademas enlaza la referencia de cada seccion.
- Usar el editor sin saberse las teclas: [teclas-y-raton.md](teclas-y-raton.md).
- Entender un archivo ajeno, o escribirlo a mano: [archivos.md](archivos.md) y [lenguaje.md](lenguaje.md).
- Arreglar o anadir algo al codigo: [organizacion.md](organizacion.md) para saber donde, y [pruebas.md](pruebas.md) para comprobarlo.

Cada documento termina diciendo donde encaja con los demas, y todos vuelven aqui.
