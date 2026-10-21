#include "order.h"
#include "hashmap.h"
#include "scanner.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Una lista de textos.
typedef struct {
	char** items;
	size_t count;
	size_t capacity;
} Strings;

// Una sentencia del fuente.
typedef struct {
	// El texto de la sentencia, tal cual estaba en el fuente.
	char* text;
	// La celda que asigna, o NULL si la sentencia no es una asignacion de
	// celda.
	char* ref;
	// Las celdas que lee, dentro de la lista de referencias de todas las
	// sentencias.
	size_t depsStart;
	size_t depsCount;
} Statement;

typedef struct {
	Statement* items;
	size_t count;
	size_t capacity;
} Statements;

// Una cola de prioridad por indice: siempre sale el mas pequeno. Asi el
// orden final se parece lo mas posible al original.
typedef struct {
	size_t* items;
	size_t count;
	size_t capacity;
} Heap;

static void heap_push(Heap* heap, size_t value) {
	if (heap->count == heap->capacity) {
		size_t capacity = heap->capacity == 0 ? 8 : heap->capacity * 2;
		size_t* items = (size_t*) realloc(heap->items, capacity * sizeof(size_t));
		if (items == NULL) {
			return;
		}

		heap->items = items;
		heap->capacity = capacity;
	}

	size_t index = heap->count++;
	heap->items[index] = value;

	// Sube el valor hasta su lugar.
	while (index > 0) {
		size_t parent = (index - 1) / 2;

		if (heap->items[parent] <= heap->items[index]) {
			break;
		}

		size_t swap = heap->items[parent];
		heap->items[parent] = heap->items[index];
		heap->items[index] = swap;

		index = parent;
	}
}

static size_t heap_pop(Heap* heap) {
	if (heap->count == 0) {
		return 0;
	}

	size_t value = heap->items[0];

	heap->items[0] = heap->items[heap->count - 1];
	heap->count--;

	// Baja el valor hasta su lugar.
	size_t index = 0;
	for (;;) {
		size_t left = index * 2 + 1;
		size_t right = left + 1;
		size_t smallest = index;

		if (left < heap->count && heap->items[left] < heap->items[smallest]) {
			smallest = left;
		}

		if (right < heap->count && heap->items[right] < heap->items[smallest]) {
			smallest = right;
		}

		if (smallest == index) {
			break;
		}

		size_t swap = heap->items[smallest];
		heap->items[smallest] = heap->items[index];
		heap->items[index] = swap;

		index = smallest;
	}

	return value;
}

// Una lista de indices.
typedef struct {
	size_t* items;
	size_t count;
	size_t capacity;
} Indices;

static char* duplicate(const char* text, size_t length) {
	char* copy = (char*) malloc(length + 1);
	if (copy == NULL) {
		return NULL;
	}

	memcpy(copy, text, length);
	copy[length] = '\0';

	return copy;
}

static void push_string(Strings* list, const char* text, size_t length) {
	if (list->count == list->capacity) {
		size_t capacity = list->capacity == 0 ? 8 : list->capacity * 2;
		char** items = (char**) realloc(list->items, capacity * sizeof(char*));
		if (items == NULL) {
			return;
		}

		list->items = items;
		list->capacity = capacity;
	}

	char* copy = duplicate(text, length);
	if (copy == NULL) {
		return;
	}

	list->items[list->count++] = copy;
}

static void push_statement(Statements* list, Statement statement) {
	if (list->count == list->capacity) {
		size_t capacity = list->capacity == 0 ? 8 : list->capacity * 2;
		Statement* items = (Statement*) realloc(list->items, capacity * sizeof(Statement));
		if (items == NULL) {
			return;
		}

		list->items = items;
		list->capacity = capacity;
	}

	list->items[list->count++] = statement;
}

static void push_index(Indices* list, size_t index) {
	if (list->count == list->capacity) {
		size_t capacity = list->capacity == 0 ? 8 : list->capacity * 2;
		size_t* items = (size_t*) realloc(list->items, capacity * sizeof(size_t));
		if (items == NULL) {
			return;
		}

		list->items = items;
		list->capacity = capacity;
	}

	list->items[list->count++] = index;
}

static void free_strings(Strings* list) {
	for (size_t i = 0; i < list->count; i++) {
		free(list->items[i]);
	}

	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static void free_statements(Statements* list) {
	for (size_t i = 0; i < list->count; i++) {
		free(list->items[i].text);
		free(list->items[i].ref);
	}

	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

// Guarda una sentencia. El texto va desde el primer token hasta donde
// empieza el token que la termina, para que las comillas de los strings
// formen parte de el.
static void add_statement(Statements* statements, Token first, const char* end, Token second, size_t depsStart, Strings* deps) {
	Statement statement = {0};

	statement.text = duplicate(first.start, (size_t) (end - first.start));
	statement.depsStart = depsStart;
	statement.depsCount = deps->count - depsStart;

	// Una asignacion de celda es una referencia seguida de un igual.
	if (first.type == TOKEN_REF && second.type == TOKEN_ASIGN) {
		statement.ref = duplicate(first.start, (size_t) first.len);
	}

	push_statement(statements, statement);
}

// Separa el fuente en sentencias de primer nivel. Las asignaciones de celda
// se guardan aparte, con las celdas que leen.
static bool scan(const char* source, Statements* statements, Strings* deps) {
	loadSource(source);

	const char* end = source + strlen(source);

	int depth = 0;
	bool open = false;
	int position = 0;

	Token first = { .type = TOKEN_EOF, .start = source, .len = 0, .line = 0 };
	Token second = first;

	size_t depsStart = 0;

	for (;;) {
		Token token = getToken();

		if (token.type == TOKEN_EOF) {
			break;
		}

		if (token.type == TOKEN_ERR) {
			// El fuente tiene un error: lo reportara el compilador.
			return false;
		}

		if (!open) {
			// Las sentencias vacias no cuentan.
			if (token.type == TOKEN_NEWLINE || token.type == TOKEN_SEMI) {
				continue;
			}

			open = true;
			// El siguiente token es el segundo de la sentencia.
			position = 1;
			first = token;
			second = (Token) { .type = TOKEN_EOF, .start = source, .len = 0, .line = 0 };
			depsStart = deps->count;

			continue;
		}

		if (token.type == TOKEN_LBRACE) {
			depth++;
		} else if (token.type == TOKEN_RBRACE) {
			if (depth > 0) {
				depth--;
			}
		}

		// Una sentencia termina en el fin de linea o en el punto y coma, pero
		// solo si no esta dentro de un bloque.
		if (depth == 0 && (token.type == TOKEN_NEWLINE || token.type == TOKEN_SEMI)) {
			add_statement(statements, first, token.start, second, depsStart, deps);

			open = false;

			continue;
		}

		if (position == 1) {
			second = token;
		}

		if (token.type == TOKEN_REF) {
			push_string(deps, token.start, (size_t) token.len);
		}

		position++;
	}

	if (open) {
		add_statement(statements, first, end, second, depsStart, deps);
	}

	return true;
}

// Ordena las asignaciones para que cada una venga despues de las que usa.
// El algoritmo es el de Kahn: primero las asignaciones que no dependen de
// ninguna otra, y despues las que dependen de ellas. Entre las que no
// dependen de nadie se respeta el orden original.
//
// Las asignaciones que no se pueden ordenar, porque se dependen entre si,
// se devuelven en cycle.
static size_t* sort_assignments(Statement* assignments, size_t count, Strings* deps, Strings* cycle) {
	size_t* order = (size_t*) malloc(count * sizeof(size_t));
	bool* emitted = (bool*) calloc(count, sizeof(bool));
	size_t* pending = (size_t*) calloc(count, sizeof(size_t));

	// De cada asignacion, las que dependen de ella.
	Indices* dependents = (Indices*) calloc(count, sizeof(Indices));

	// La celda que asigna cada asignacion.
	struct hashmap_s index;
	hashmap_create(count > 0 ? (hashmap_uint32_t) count : 1, &index);

	for (size_t i = 0; i < count; i++) {
		hashmap_put(&index, assignments[i].ref, (hashmap_uint32_t) strlen(assignments[i].ref), (void*) (uintptr_t) (i + 1));
	}

	for (size_t i = 0; i < count; i++) {
		Statement* assignment = &assignments[i];

		for (size_t d = 0; d < assignment->depsCount; d++) {
			const char* ref = deps->items[assignment->depsStart + d];

			void* found = hashmap_get(&index, ref, (hashmap_uint32_t) strlen(ref));
			if (found == NULL) {
				// La celda no la asigna ninguna sentencia de primer nivel.
				continue;
			}

			size_t j = (size_t) (uintptr_t) found - 1;

			// Una celda que se usa a si misma tambien es un ciclo.
			pending[i]++;
			push_index(&dependents[j], i);
		}
	}

	// Las asignaciones sin dependencias pendientes, en su orden original.
	Heap heap = {NULL, 0, 0};

	for (size_t i = 0; i < count; i++) {
		if (pending[i] == 0) {
			heap_push(&heap, i);
		}
	}

	size_t total = 0;

	while (heap.count > 0) {
		size_t i = heap_pop(&heap);

		order[total++] = i;
		emitted[i] = true;

		for (size_t k = 0; k < dependents[i].count; k++) {
			size_t j = dependents[i].items[k];

			if (pending[j] > 0) {
				pending[j]--;
			}

			if (pending[j] == 0) {
				heap_push(&heap, j);
			}
		}
	}

	// Las que faltan estan en un ciclo: se quedan donde estaban.
	for (size_t i = 0; i < count; i++) {
		if (!emitted[i]) {
			order[total++] = i;
			push_string(cycle, assignments[i].ref, strlen(assignments[i].ref));
		}
	}

	for (size_t i = 0; i < count; i++) {
		free(dependents[i].items);
	}

	free(dependents);
	free(heap.items);
	free(emitted);
	free(pending);
	hashmap_destroy(&index);

	return order;
}

// Prepara las asignaciones de celda del fuente: las separa del resto del
// programa y las deja en el orden en que se pueden calcular.
//
// Devuelve NULL si el fuente no se puede analizar, si no hay asignaciones de
// celda o si las celdas se dependen entre si. En el ultimo caso, el ciclo
// queda en cycle.
static Statement* prepare_assignments(const char* source, size_t* count, bool* hasOther, Strings* cycle) {
    Statements statements = {NULL, 0, 0};
    Strings deps = {NULL, 0, 0};

    *count = 0;
    *hasOther = false;

    if (!scan(source, &statements, &deps)) {
        free_statements(&statements);
        free_strings(&deps);

        return NULL;
    }

    // Las asignaciones de celda, en su orden original.
    size_t total = 0;
    for (size_t i = 0; i < statements.count; i++) {
        if (statements.items[i].ref != NULL) {
            total++;
        } else {
            *hasOther = true;
        }
    }

    if (total == 0) {
        free_statements(&statements);
        free_strings(&deps);

        return NULL;
    }

    Statement* assignments = (Statement*) malloc(total * sizeof(Statement));
    if (assignments == NULL) {
        free_statements(&statements);
        free_strings(&deps);

        return NULL;
    }

    size_t slot = 0;
    for (size_t i = 0; i < statements.count; i++) {
        if (statements.items[i].ref != NULL) {
            assignments[slot++] = statements.items[i];
        }
    }

    size_t* order = sort_assignments(assignments, total, &deps, cycle);

    // Las celdas que se dependen entre si no se pueden calcular.
    if (cycle->count > 0) {
        free(order);
        free(assignments);
        free_statements(&statements);
        free_strings(&deps);

        return NULL;
    }

    // Se dejan las asignaciones ya en su orden, con el texto copiado para
    // que no dependa de la lista de sentencias.
    Statement* sorted = (Statement*) malloc(total * sizeof(Statement));
    if (sorted == NULL) {
        free(order);
        free(assignments);
        free_statements(&statements);
        free_strings(&deps);

        return NULL;
    }

    for (size_t i = 0; i < total; i++) {
        Statement* assignment = &assignments[order[i]];

        sorted[i].text = duplicate(assignment->text, strlen(assignment->text));
        sorted[i].ref = duplicate(assignment->ref, strlen(assignment->ref));
        sorted[i].depsStart = 0;
        sorted[i].depsCount = 0;
    }

    free(order);
    free(assignments);
    free_statements(&statements);
    free_strings(&deps);

    *count = total;

    return sorted;
}

// Libera las asignaciones preparadas.
static void free_sorted(Statement* sorted, size_t count) {
    if (sorted == NULL) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        free(sorted[i].text);
        free(sorted[i].ref);
    }

    free(sorted);
}

// El mensaje de las celdas que se dependen entre si.
static void report_cycle(const Strings* cycle, char* error, size_t errorSize) {
    if (error == NULL || errorSize == 0) {
        return;
    }

    char cells[128];
    size_t length = 0;
    // Con muchas celdas en el ciclo solo se nombran algunas.
    size_t shown = cycle->count < 4 ? cycle->count : 4;

    cells[0] = '\0';

    for (size_t i = 0; i < shown; i++) {
        int written = snprintf(cells + length, sizeof(cells) - length, "%s%s", i > 0 ? ", " : "", cycle->items[i]);

        if (written < 0 || (size_t) written >= sizeof(cells) - length) {
            break;
        }

        length += (size_t) written;
    }

    if (cycle->count > shown) {
        snprintf(cells + length, sizeof(cells) - length, " y %zu m\u00e1s", cycle->count - shown);
    }

    snprintf(error, errorSize, "referencia circular entre celdas: %s", cells);
}

char* order_source(const char* source, char* error, size_t errorSize) {
    if (source == NULL) {
        return NULL;
    }

    if (error != NULL && errorSize > 0) {
        error[0] = '\0';
    }

    Statements statements = {NULL, 0, 0};
    Strings deps = {NULL, 0, 0};
    Strings cycle = {NULL, 0, 0};

    if (!scan(source, &statements, &deps)) {
        free_statements(&statements);
        free_strings(&deps);
        free_strings(&cycle);

        return NULL;
    }

    // Las asignaciones de celda, en su orden original.
    size_t count = 0;
    for (size_t i = 0; i < statements.count; i++) {
        if (statements.items[i].ref != NULL) {
            count++;
        }
    }

    if (count == 0) {
        free_statements(&statements);
        free_strings(&deps);
        free_strings(&cycle);

        return NULL;
    }

    Statement* assignments = (Statement*) malloc(count * sizeof(Statement));
    if (assignments == NULL) {
        free_statements(&statements);
        free_strings(&deps);
        free_strings(&cycle);

        return NULL;
    }

    size_t slot = 0;
    for (size_t i = 0; i < statements.count; i++) {
        if (statements.items[i].ref != NULL) {
            assignments[slot++] = statements.items[i];
        }
    }

    size_t* order = sort_assignments(assignments, count, &deps, &cycle);

    // Las celdas que se dependen entre si no se pueden calcular.
    if (cycle.count > 0) {
        report_cycle(&cycle, error, errorSize);

        free(order);
        free(assignments);
        free_statements(&statements);
        free_strings(&deps);
        free_strings(&cycle);

        return NULL;
    }

    // Se reconstruye el fuente con las asignaciones en su nuevo orden, en los
    // mismos lugares que tenian.
    size_t length = 1;
    for (size_t i = 0; i < statements.count; i++) {
        length += strlen(statements.items[i].text) + 2;
    }

    char* ordered = (char*) malloc(length);
    if (ordered == NULL) {
        free(order);
        free(assignments);
        free_statements(&statements);
        free_strings(&deps);
        free_strings(&cycle);

        return NULL;
    }

    ordered[0] = '\0';
    slot = 0;

    for (size_t i = 0; i < statements.count; i++) {
        const char* text = statements.items[i].text;

        if (statements.items[i].ref != NULL) {
            text = assignments[order[slot]].text;
            slot++;
        }

        strcat(ordered, text);
        strcat(ordered, "\n");
    }

    free(order);
    free(assignments);
    free_statements(&statements);
    free_strings(&deps);
    free_strings(&cycle);

    return ordered;
}

SheetAssignment* order_assignments(const char* source, size_t* count, bool* hasOther) {
    if (source == NULL) {
        return NULL;
    }

    Strings cycle = {NULL, 0, 0};

    size_t total = 0;
    Statement* sorted = prepare_assignments(source, &total, hasOther, &cycle);

    if (sorted == NULL) {
        free_strings(&cycle);

        return NULL;
    }

    SheetAssignment* result = (SheetAssignment*) malloc(total * sizeof(SheetAssignment));
    if (result == NULL) {
        free_sorted(sorted, total);
        free_strings(&cycle);

        return NULL;
    }

    for (size_t i = 0; i < total; i++) {
        result[i].ref = sorted[i].ref;
        result[i].text = sorted[i].text;
    }

    free(sorted);
    free_strings(&cycle);

    *count = total;

    return result;
}

void free_assignments(SheetAssignment* assignments, size_t count) {
    if (assignments == NULL) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        free(assignments[i].ref);
        free(assignments[i].text);
    }

    free(assignments);
}
