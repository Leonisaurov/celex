#ifndef CELEX_TUI_CELL_MANAGER
#define CELEX_TUI_CELL_MANAGER
#include <stdbool.h>
#include "clipboard.h"
#include "complete.h"
#include "hashmap.h"
#include "tui.h"
#include "cell.h"

#define MESSAGE_SIZE 128

// Lo que ofrece el tabulador mientras se escribe una palabra en una celda.
typedef struct {
    // Si hay una oferta delante. Al escribir o borrar se apaga, porque la
    // palabra que se esta escribiendo ya no es la que se ofrecio.
    bool active;
    // Donde empieza la palabra en el contenido de la celda, en bytes.
    size_t start;
    // La palabra que estaba escrita, para volver a ella al dar la vuelta a la
    // lista.
    char origin[COMPLETE_WORD];
    // Las palabras que empiezan igual.
    CompleteList list;
    // Cuantas se han ofrecido.
    int index;
} Completion;

// Un cambio en el contenido de una celda, para poder deshacerlo.
typedef struct {
    unsigned int x;
    unsigned int y;
    char* oldContent;
    char* newContent;
} HistoryEntry;

typedef struct {
    HistoryEntry* items;
    size_t count;
    size_t capacity;
} History;

// Lo que es propio de una hoja: sus celdas, donde esta el cursor, cuanto se
// deslizo la vista y lo que se puede deshacer. Una hoja no depende de las demas,
// y por eso cambia de una a otra sin perder nada.
typedef struct {
    struct hashmap_s map;
    TermSize cursor;
    // Cuantas columnas y filas de pantalla se han desplazado de la rejilla.
    // Va de caracter en caracter, no de celda en celda, para que al llegar al
    // borde la vista se deslice y queden celdas cortadas a los lados.
    int originX;
    int originY;
    // Si se esta editando el contenido de una celda.
    bool editing;
    // Donde esta el cursor de texto dentro del contenido, en bytes.
    size_t editOffset;
    // Si la hoja cambio desde la ultima vez que se guardo.
    bool dirty;
    History undo;
    History redo;
} SheetState;

// Lo que puede pesar el nombre de una hoja.
#define SHEET_NAME_SIZE 24

typedef struct {
    char name[SHEET_NAME_SIZE];
    SheetState state;
} Sheet;

typedef struct {
    // Las hojas del archivo, con la que se esta viendo en el numero active.
    Sheet *sheets;
    size_t sheetCount;
    size_t sheetCapacity;
    int active;

    // Lo que es de la pantalla, y no de una hoja.
    TermSize size;
    bool showValues;
    char message[MESSAGE_SIZE];
    // Si lo que dice la barra es un error, y no un aviso normal.
    bool messageIsError;
    // El portapapeles del editor: lo que se copia y lo que se pega. Es uno solo
    // para todas las hojas, como en una hoja de calculo de verdad.
    Clipboard clipboard;
    // Lo que ofrece el tabulador.
    Completion completion;
    // Si se esta escribiendo el nombre de la hoja que se ve, con el Ctrl+F. El
    // texto se escribe al final, y se fija con Enter o se deja con Escape.
    bool renaming;
    char renameText[SHEET_NAME_SIZE];
    // Si la fila de las herramientas esta puesta. Es de la pantalla, y no de una
    // hoja: quien trabaja a teclado la quita para todos los archivos, y quien
    // trabaja a clic la deja.
    bool showTools;
} CellManager;

// Las filas de abajo, de arriba a abajo: la rejilla, las herramientas si estan
// puestas, las pestañas de las hojas y la barra de estado. La rejilla acaba donde
// empieza lo demas.
int grid_bottom(const CellManager *manager, int height);
int tools_row(const CellManager *manager, int height);
int sheet_row(int height);
int status_row(int height);

// Si una columna de la barra de estado es del nombre del archivo, que es el
// tirador de la fila de las herramientas.
int status_path_at(int x);

// En que columna de la fila de las pestanas empieza el chip que devuelve la fila
// de las herramientas cuando esta quitada.
int tools_chip_column(int width);

// La hoja que se esta viendo, y su estado. Todo lo que es de una hoja se mira
// aqui: las celdas, el cursor, la vista y el historial.
Sheet *current_sheet(CellManager *manager);
SheetState *sheet_state(CellManager *manager);

// La hoja numero index, o NULL si el numero no es de ninguna.
Sheet *sheet_at(CellManager *manager, int index);

// Cuantas hojas hay, cual se esta viendo, y como se llama la numero index.
int sheet_count(CellManager *manager);
int sheet_active(CellManager *manager);
const char *sheet_name_at(CellManager *manager, int index);

// Pone a ver la hoja numero index. La que estaba se queda con sus cambios, su
// cursor y su historial de deshacer.
void sheet_select(CellManager *manager, int index);

// Mete una hoja nueva despues de la que se esta viendo y se pone a verla.
// Devuelve el numero de la hoja nueva, o -1 si no se pudo.
int sheet_add(CellManager *manager, const char *name);

// Quita la hoja numero index, y se pone a ver la vecina. La ultima hoja no se
// quita: una hoja de calculo sin hojas no es una hoja de calculo.
void sheet_remove(CellManager *manager, int index);

// Cambia el nombre de la hoja numero index.
void sheet_rename(CellManager *manager, int index, const char *name);

// Si alguna hoja tiene cambios sin guardar, y como marcarlas todas guardadas,
// que es lo que son despues de escribir el archivo entero.
bool sheet_any_dirty(CellManager *manager);
void sheet_mark_saved(CellManager *manager);

CellManager new_cell_manager(TermSize size);
void free_cell_manager(CellManager *manager);

MetaCell *get_cell(CellManager *manager, unsigned int x, unsigned int y);
MetaCell *get_or_create_cell(CellManager *manager, unsigned int x, unsigned int y);
void set_cell_value(CellManager *manager, unsigned int x, unsigned int y, const char *value);
void clear_cell_values(CellManager *manager);
void set_cell_content(CellManager *manager, unsigned int x, unsigned int y, const char *content);
void clear_cell(CellManager *manager, unsigned int x, unsigned int y);
void set_message(CellManager *manager, const char *message);

// Lo mismo, pero la barra lo pinta como un error.
void set_error_message(CellManager *manager, const char *message);

void print_cell_manager(CellManager *manager);
void print_status(CellManager *manager, const char *mode, const char *path);

// La fila de las pestañas de las hojas.
void print_sheets(CellManager *manager);

// La fila de las herramientas, encima de las pestañas.
void print_tools(CellManager *manager);

// Donde cae un punto de la pantalla, para poder llevar un clic al sitio que le
// corresponde. `sheet_at_column` devuelve la hoja de la columna de la fila de las
// pestanas, o -1 si no es de ninguna. `cell_at_screen` dice si la columna y la
// fila caen dentro de una celda de la rejilla, y cual es.
int sheet_at_column(CellManager *manager, int x);
int cell_at_screen(CellManager *manager, int x, int y, unsigned int *cellX, unsigned int *cellY);

// Que pestañas caben en una fila de `width` columnas, con `sizes` como lo que
// pesa el nombre de cada una de las `count` hojas y `active` como la que se esta
// viendo. Van de `first` hasta `last`, sin incluir `last`.
void sheet_bar_range(const int *sizes, int count, int active, int width, int *first, int *last);

// Cuantos fotogramas se pintan para recorrer un desplazamiento de la vista de
// dx columnas y dy filas. Cero es no deslizar: pintar de golpe.
int pan_frames(int dx, int dy);

// El tamano de las celdas, en columnas y filas de pantalla. Fuera de los
// limites no se sale.
void set_cell_size(int width, int height);

// El formato de la celda del cursor, para cambiarlo. Devuelve NULL si no hay
// sitio para la celda.
CellFormat *cursor_format(CellManager *manager);

// Cuanto se corre una linea de `lineLength` caracteres dentro de una celda, con
// la alineacion pedida y el texto de la celda entero por delante (es el texto el
// que decide cuando la alineacion es la de empezar).
int align_pad(int align, const char *text, int lineLength);

// Olvida lo que habia en la pantalla, para que lo siguiente se pinte entero.
void invalidate_screen(void);

// Vacia la hoja entera y olvida los cambios deshechos.
void clear_sheet(CellManager *manager);

// Deja el archivo con una sola hoja vacia, como al abrirlo.
void workbook_clear(CellManager *manager);

// Deshace el ultimo cambio y rehace el que se deshizo.
void undo_edit(CellManager *manager);
void redo_edit(CellManager *manager);
bool can_undo(CellManager *manager);
bool can_redo(CellManager *manager);

// Cambia el contenido de una celda y lo deja en el historial.
void edit_cell_content(CellManager *manager, unsigned int x, unsigned int y, const char *content);

// Copia el contenido de una celda al portapapeles del editor, y deja la
// celda marcada.
void copy_cell_content(CellManager *manager, unsigned int x, unsigned int y);

// Pega un texto en una celda, reemplazando lo que hay en ella.
void paste_cell_content(CellManager *manager, unsigned int x, unsigned int y, const char *text);

// Pega texto dentro de la celda que se esta editando: el texto entra donde
// esta el cursor de texto, en vez de reemplazar el contenido entero.
void paste_cell_text(CellManager *manager, unsigned int x, unsigned int y, const char *text);

// El cursor de texto, dentro del contenido de la celda que se edita.
void edit_reset(CellManager *manager);
void edit_move_cursor(CellManager *manager, int delta);
void edit_to_start(CellManager *manager);
void edit_to_end(CellManager *manager);
void edit_insert(CellManager *manager, const char *text, size_t length);
void edit_backspace(CellManager *manager);
void edit_delete(CellManager *manager);

// El tabulador, mientras se edita: completa la palabra que se esta escribiendo
// con las del lenguaje y las que ya hay en la hoja, y va pasando por las
// candidatas si hay mas de una.
void complete_tab(CellManager *manager);

// Lo que el tabulador esta ofreciendo, escrito en una linea para la barra de
// estado. Devuelve 0 si no hay nada que ofrecer.
int completion_text(CellManager *manager, char *into, size_t size);

#endif
