#ifndef CELEX_TUI_DOCUMENT
#define CELEX_TUI_DOCUMENT

#include "cell_manager.h"

// El formato del archivo del editor: el ".clex". No es un programa de celex
// script, es la estructura del documento: como esta la pantalla y que hojas hay
// con sus celdas. Esta escrito para que lo lea el editor, no una persona; para
// editar a mano esta el ".cle".
//
//   celex-document 1
//   theme oscuro
//   cell-size 4 2
//   sheet "numeros"
//     select A1
//     scroll 0 0
//     cell A1 "1"
//     cell B1 "A1 + A2"
//     format B1 align=right bold
//   end-sheet
//   sheet "gastos"
//     select B2
//     scroll 10 6
//   end-sheet
//
// - Lo que va entre comillas lleva el texto escapado: la comilla se escribe \"
//   y la barra \". El contenido de una celda es siempre una linea, sin saltos.
// - Una linea, una cosa. No hay dos campos en la misma linea.
// - El formato va en su linea, token por token, y solo con palabras: la
//   alineacion (`align=left`, `center`, `right`), `bold`, `underline` y los
//   colores de la paleta (`fg=3`, `bg=12`). Lo que no se escribe es el tema.
// - Lo que no se reconoce se ignora y se cuenta, para que un archivo de una
//   version futura no deje de abrirse por tener un campo nuevo.
// - El numero que sigue a celex-document es la version del formato.
#define DOCUMENT_VERSION 1
#define DOCUMENT_MAGIC "celex-document"

// El texto del documento, listo para escribir en un archivo. Quien lo pide lo
// suelta.
char *document_source(CellManager *manager);

// Deja el gestor como cuenta el texto: una sola hoja si no trae ninguna.
// Devuelve 1 si era un documento, 0 si la cabecera no es la de un .clex. Las
// lineas que no se entendieron se cuentan en ignored, si se pasa donde.
int document_load(const char *text, CellManager *manager, int *ignored);

// Si un camino termina en ".clex", que es cuando el editor abre un documento
// suyo y no un guion del lenguaje.
int document_is_path(const char *path);

// El camino de al lado, pero con la extension cambiada: de .cle a .clex para
// guardar, y de .clex a .cle paraexportar el guion. Devuelve 0 si no cupo.
int document_change_extension(const char *path, const char *extension, char *into, size_t size);

#endif
