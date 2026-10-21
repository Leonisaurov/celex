#include "vm.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ref.h"

typedef struct {
	Variable* vars;
	unsigned int length;
	unsigned int size;
} Stack;

typedef struct {
	struct hashmap_s vars;
	struct hashmap_s cells;
} VMContext;

static Stack stack;
static VMContext context;
static FILE* output;

static const char* symbols;
static const char* instruccions;
static unsigned int pointerIns;
static unsigned int LenIns;
static unsigned int LenSyms;

static Variable makeNumber(int64_t number) {
	Variable var = {
		.type = VAR_NUM,
		.value.number = number,
	};

	return var;
}

static Variable makeString(const char* text) {
	Variable var = {
		.type = VAR_STR,
		.value.str = NULL,
	};

	size_t length = strlen(text);
	var.value.str = (char*) malloc(sizeof(char) * (length + 1));
	memcpy(var.value.str, text, length + 1);

	return var;
}

static Variable makeBool(bool value) {
	Variable var = {
		.type = VAR_BOOL,
		.value.number = value ? 1 : 0,
	};

	return var;
}

static Variable makeNil() {
	Variable var = {
		.type = VAR_NULO,
		.value.number = 0,
	};

	return var;
}

static Variable copyVariable(Variable var) {
	if (var.type == VAR_STR && var.value.str != NULL) {
		return makeString(var.value.str);
	}

	return var;
}

static void freeVariable(Variable var) {
	if (var.type == VAR_STR && var.value.str != NULL) {
		free(var.value.str);
	}
}

static bool isTruthy(Variable var) {
	switch (var.type) {
		case VAR_NUM: return var.value.number != 0;
		case VAR_BOOL: return var.value.number != 0;
		case VAR_STR: return var.value.str != NULL && var.value.str[0] != '\0';
		default: return false;
	}
}

static void initVM(const char* source) {
	LenSyms = *(unsigned int*)source;
	symbols = source + sizeof(unsigned int);

	LenIns = *(unsigned int*) (symbols + LenSyms);
	instruccions = symbols + LenSyms + sizeof(unsigned int);
	pointerIns = 0;
}

static void initStack() {
	stack.vars = (Variable*) malloc(sizeof(Variable) * UINT8_MAX);
	stack.length = 0;
	stack.size = UINT8_MAX;
}

static void bigStack() {
	Variable* vars = (Variable*) realloc(stack.vars, sizeof(Variable) * stack.size * 3 / 2);
	if (vars == NULL) {
		printf("Error al hacer el Stack más grande\n");
		return;
	}

	stack.vars = vars;
	stack.size *= 2;
}

static void freeStack() {
	for (uint32_t i = 0; i < stack.length; i++) {
		freeVariable(stack.vars[i]);
	}
	free(stack.vars);
	stack.vars = NULL;
	stack.length = 0;
	stack.size = 0;
}

static void Up(Variable var) {
	while (stack.length >= stack.size) {
		bigStack();
	}

	stack.vars[stack.length] = var;
	stack.length++;
}

static Variable Down() {
	if (stack.length == 0) {
		return makeNil();
	}

	stack.length--;

	return stack.vars[stack.length];
}

static _Bool reachEnd() {
	return pointerIns >= LenIns;
}

static uint32_t getRefChunk() {
	return *(uint32_t*) (instruccions + pointerIns);
}

static int32_t getJumpChunk() {
	return *(int32_t*) (instruccions + pointerIns);
}

static const char* getText(uint32_t ref, uint32_t* length) {
	*length = *(uint32_t*)(symbols + ref);

	return (const char*) (symbols + ref + sizeof(uint32_t));
}

static int64_t getNumber(uint32_t ref) {
	return *(int64_t*) (symbols + ref);
}

static Variable getStrSym() {
	uint32_t ref = getRefChunk();
	pointerIns += sizeof(uint32_t);

	uint32_t length = 0;
	const char* text = getText(ref, &length);
	char* copy = (char*) malloc(sizeof(char) * (length + 1));
	memcpy(copy, text, length);
	copy[length] = '\0';

	Variable val = {
		.type = VAR_STR,
		.value.str = copy,
	};

	return val;
}

static Variable getNumSym() {
	uint32_t ref = getRefChunk();
	pointerIns += sizeof(uint32_t);

	Variable val = {
		.type = VAR_NUM,
		.value.number = getNumber(ref),
	};

	return val;
}
static int freeItems(void *const context, void *const element) {
	(void) context;

	Variable* var = (Variable*) element;
	freeVariable(*var);
	free(var);

	// hashmap_iterate sigue mientras el callback devuelva un valor distinto de 0.
	return 1;
}

static void freeContext() {
	hashmap_iterate(&context.vars, freeItems, NULL);
	hashmap_iterate(&context.cells, freeItems, NULL);
	hashmap_destroy(&context.vars);
	hashmap_destroy(&context.cells);
}

static Variable* getEntry(struct hashmap_s* table, const char* key, uint32_t length) {
	Variable* var = (Variable*) hashmap_get(table, key, length);
	if (var != NULL) {
		return var;
	}

	var = (Variable*) malloc(sizeof(Variable));
	if (var == NULL) {
		return NULL;
	}

	*var = makeNil();
	hashmap_put(table, key, length, var);

	return var;
}

// La tabla toma posesion del valor.
static void setEntry(struct hashmap_s* table, const char* key, uint32_t length, Variable value) {
	Variable* var = getEntry(table, key, length);
	if (var == NULL) {
		freeVariable(value);
		return;
	}

	freeVariable(*var);
	*var = value;
}

static Variable arithmetic(op_code code, Variable left, Variable right, bool* error) {
	*error = false;

	if (code == OP_ADD && (left.type == VAR_STR || right.type == VAR_STR)) {
		char buffer[4096];

		if (left.type == VAR_STR && right.type == VAR_STR) {
			snprintf(buffer, sizeof(buffer), "%s%s", left.value.str, right.value.str);
		} else if (left.type == VAR_STR) {
			snprintf(buffer, sizeof(buffer), "%s%" PRId64, left.value.str, right.value.number);
		} else {
			snprintf(buffer, sizeof(buffer), "%" PRId64 "%s", left.value.number, right.value.str);
		}

		return makeString(buffer);
	}

	int64_t a = left.type == VAR_STR ? 0 : left.value.number;
	int64_t b = right.type == VAR_STR ? 0 : right.value.number;

	switch (code) {
		case OP_ADD: return makeNumber(a + b);
		case OP_SUB: return makeNumber(a - b);
		case OP_MULT: return makeNumber(a * b);
		case OP_DIV:
			if (b == 0) {
				*error = true;
				return makeNil();
			}
			return makeNumber(a / b);
		default:
			*error = true;
			return makeNil();
	}
}

static Variable comparison(op_code code, Variable left, Variable right) {
	int result = 0;

	if (left.type == VAR_STR && right.type == VAR_STR) {
		result = strcmp(left.value.str, right.value.str);
	} else {
		int64_t a = left.type == VAR_STR ? 0 : left.value.number;
		int64_t b = right.type == VAR_STR ? 0 : right.value.number;
		result = a < b ? -1 : (a > b ? 1 : 0);
	}

	switch (code) {
		case OP_EQ: return makeBool(result == 0);
		case OP_NE: return makeBool(result != 0);
		case OP_GT: return makeBool(result > 0);
		case OP_LT: return makeBool(result < 0);
		case OP_GE: return makeBool(result >= 0);
		default: return makeBool(result <= 0);
	}
}

static void printValue(Variable var) {
	switch (var.type) {
		case VAR_STR:
			fprintf(output, "%s\n", var.value.str);
			break;
		case VAR_BOOL:
			fprintf(output, "%s\n", var.value.number ? "true" : "false");
			break;
		case VAR_NULO:
			fprintf(output, "nil\n");
			break;
		default:
			fprintf(output, "%" PRId64 "\n", var.value.number);
			break;
	}
}

VM_RESULT execute(const char* source) {
	initVM(source);
	initStack();

	if (output == NULL) {
		output = stdout;
	}

	hashmap_create(35, &context.vars);
	hashmap_create(35, &context.cells);

	VM_RESULT result = VM_SUCCESS;

	while (!reachEnd()) {
		op_code code = instruccions[pointerIns];
		pointerIns++;

		switch (code) {
			case OP_UPS:
				Up(getStrSym());
				break;
			case OP_UPN:
				Up(getNumSym());
				break;
			case OP_UPB: {
				byte value = (byte) instruccions[pointerIns];
				pointerIns++;
				Up(makeBool(value == 1));
				break;
			}
			case OP_UPNIL:
				Up(makeNil());
				break;
			case OP_GETVAR:
			case OP_GETREF: {
				uint32_t ref = getRefChunk();
				pointerIns += sizeof(uint32_t);

				uint32_t length = 0;
				const char* text = getText(ref, &length);

				struct hashmap_s* table = code == OP_GETVAR ? &context.vars : &context.cells;
				Variable* var = (Variable*) hashmap_get(table, text, length);
				if (var == NULL) {
					Up(makeNil());
					break;
				}

				Up(copyVariable(*var));
				break;
			}
			case OP_SETVAR:
			case OP_SETREF: {
				uint32_t ref = getRefChunk();
				pointerIns += sizeof(uint32_t);

				uint32_t length = 0;
				const char* text = getText(ref, &length);

				struct hashmap_s* table = code == OP_SETVAR ? &context.vars : &context.cells;
				Variable value = Down();
				setEntry(table, text, length, value);
				break;
			}
			case OP_ADD:
			case OP_SUB:
			case OP_MULT:
			case OP_DIV: {
				Variable right = Down();
				Variable left = Down();

				bool error = false;
				Variable value = arithmetic(code, left, right, &error);
				if (error) {
					printf("Error en la operacion\n");
					result = VM_ERR;
					freeVariable(left);
					freeVariable(right);
					freeVariable(value);
					goto end;
				}

				freeVariable(left);
				freeVariable(right);
				Up(value);
				break;
			}
			case OP_EQ:
			case OP_NE:
			case OP_GT:
			case OP_LT:
			case OP_GE:
			case OP_LE: {
				Variable right = Down();
				Variable left = Down();
				Variable value = comparison(code, left, right);
				freeVariable(left);
				freeVariable(right);
				Up(value);
				break;
			}
			case OP_JMP: {
				int32_t target = getJumpChunk();
				pointerIns += sizeof(int32_t);
				pointerIns = (unsigned int) target;
				break;
			}
			case OP_JF: {
				int32_t target = getJumpChunk();
				pointerIns += sizeof(int32_t);

				Variable condition = Down();
				bool truthy = isTruthy(condition);
				freeVariable(condition);

				if (!truthy) {
					pointerIns = (unsigned int) target;
				}
				break;
			}
			case OP_PRINT: {
				Variable value = Down();
				printValue(value);
				freeVariable(value);
				break;
			}
			case OP_INPUT: {
				uint32_t ref = getRefChunk();
				pointerIns += sizeof(uint32_t);

				uint32_t length = 0;
				const char* name = getText(ref, &length);

				char buffer[1024];
				if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
					buffer[0] = '\0';
				}

				size_t size = strlen(buffer);
				if (size > 0 && buffer[size - 1] == '\n') {
					buffer[size - 1] = '\0';
				}

				setEntry(&context.vars, name, length, makeString(buffer));
				break;
			}
			case OP_POP: {
				Variable value = Down();
				freeVariable(value);
				break;
			}
			case OP_RETURN:
			case OP_HALT:
				goto end;
			default:
				printf("Instruccion no handled\n");
				result = VM_ERR;
				goto end;
		}
	}

end:
	freeStack();

	return result;
}

void setOutput(FILE* stream) {
	output = stream;
}

void freeVM() {
	freeContext();
}

typedef struct {
	char* ref;
	Variable value;
	unsigned int x;
	unsigned int y;
} SheetEntry;

typedef struct {
	SheetEntry* items;
	size_t count;
	size_t capacity;
} Sheet;

static int compareEntries(const void* a, const void* b) {
	const SheetEntry* left = (const SheetEntry*) a;
	const SheetEntry* right = (const SheetEntry*) b;

	// El mismo orden que usa el editor al escribir la hoja: columna y luego
	// fila.
	if (left->x != right->x) {
		return left->x < right->x ? -1 : 1;
	}

	if (left->y != right->y) {
		return left->y < right->y ? -1 : 1;
	}

	return 0;
}

static int collectEntry(void* const context, struct hashmap_element_s* const element) {
	Sheet* sheet = (Sheet*) context;
	Variable* var = (Variable*) element->data;

	uint32_t length = element->key_len;
	char* key = (char*) malloc(length + 1);
	memcpy(key, element->key, length);
	key[length] = '\0';

	unsigned int x = 0;
	unsigned int y = 0;
	if (!ref_parse(key, &x, &y)) {
		free(key);
		return 0;
	}

	if (sheet->count == sheet->capacity) {
		size_t capacity = sheet->capacity == 0 ? 16 : sheet->capacity * 2;
		SheetEntry* items = (SheetEntry*) realloc(sheet->items, capacity * sizeof(SheetEntry));
		if (items == NULL) {
			free(key);
			return 0;
		}

		sheet->items = items;
		sheet->capacity = capacity;
	}

	sheet->items[sheet->count].ref = key;
	sheet->items[sheet->count].value = *var;
	sheet->items[sheet->count].x = x;
	sheet->items[sheet->count].y = y;
	sheet->count++;

	return 0;
}

static char* formatValue(Variable value) {
	char buffer[4096];

	switch (value.type) {
		case VAR_STR:
			return makeString(value.value.str).value.str;
		case VAR_BOOL:
			snprintf(buffer, sizeof(buffer), "%s", value.value.number ? "true" : "false");
			break;
		case VAR_NULO:
			return NULL;
		default:
			snprintf(buffer, sizeof(buffer), "%" PRId64, value.value.number);
			break;
	}

	char *text = (char*) malloc(sizeof(char) * (strlen(buffer) + 1));
	memcpy(text, buffer, strlen(buffer) + 1);

	return text;
}

int sheetValues(CellValue** out, size_t* count) {
	Sheet sheet = {NULL, 0, 0};

	hashmap_iterate_pairs(&context.cells, collectEntry, &sheet);
	if (sheet.count == 0) {
		free(sheet.items);
		*out = NULL;
		*count = 0;

		return 0;
	}

	qsort(sheet.items, sheet.count, sizeof(SheetEntry), compareEntries);

	CellValue *values = (CellValue*) malloc(sizeof(CellValue) * sheet.count);
	if (values == NULL) {
		free(sheet.items);
		*out = NULL;
		*count = 0;

		return 0;
	}

	size_t total = 0;
	for (size_t i = 0; i < sheet.count; i++) {
		char *value = formatValue(sheet.items[i].value);
		if (value == NULL) {
			continue;
		}

		values[total].ref = sheet.items[i].ref;
		values[total].value = value;
		sheet.items[i].ref = NULL;
		total++;
	}

	for (size_t i = 0; i < sheet.count; i++) {
		free(sheet.items[i].ref);
	}

	free(sheet.items);

	*out = values;
	*count = total;

	return (int) total;
}

void freeCellValues(CellValue* values, size_t count) {
	if (values == NULL) {
		return;
	}

	for (size_t i = 0; i < count; i++) {
		free(values[i].ref);
		free(values[i].value);
	}

	free(values);
}

void printSheet() {
	CellValue *values = NULL;
	size_t count = 0;

	sheetValues(&values, &count);

	for (size_t i = 0; i < count; i++) {
		printf("%s = %s\n", values[i].ref, values[i].value);
	}

	freeCellValues(values, count);
}
