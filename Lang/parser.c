#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunck.h"
#include "scanner.h"
#include "symtable.h"

static char errorMessage[256];

const char* parseError() {
	return errorMessage;
}

static void setError(const char* message, uint16_t line) {
	snprintf(errorMessage, sizeof(errorMessage), "linea %hu: %s", line, message);
}

typedef struct {
	Token current;
	bool hasCurrent;
} Parser;

static Parser parser;

static Token next() {
	if (parser.hasCurrent) {
		parser.hasCurrent = false;
		return parser.current;
	}

	return getToken();
}

static Token peek() {
	if (!parser.hasCurrent) {
		parser.current = getToken();
		parser.hasCurrent = true;
	}

	return parser.current;
}

static bool check(TokenType type) {
	return peek().type == type;
}

static bool match(TokenType type) {
	if (!check(type)) {
		return false;
	}

	next();

	return true;
}

static bool consume(TokenType type, const char* message) {
	if (match(type)) {
		return true;
	}

	setError(message, peek().line);

	return false;
}

static void skipEndings() {
	while (check(TOKEN_NEWLINE) || check(TOKEN_SEMI)) {
		next();
	}
}

static void expression();
static void statement();
static void block();

static void emitSymbol(op_code code, Token token) {
	uint32_t ref = addString(token.start, (uint32_t) token.len);
	addOpCode(code);
	addUInt(ref);
}

static void numberExpression() {
	Token token = next();
	uint32_t ref = addInt64(token.start, (uint32_t) token.len);
	addOpCode(OP_UPN);
	addUInt(ref);
}

static void stringExpression() {
	Token token = next();
	uint32_t ref = addString(token.start, (uint32_t) token.len);
	addOpCode(OP_UPS);
	addUInt(ref);
}

static void primary() {
	Token token = peek();

	switch (token.type) {
		case TOKEN_NUM:
			numberExpression();
			return;
		case TOKEN_STR:
			stringExpression();
			return;
		case TOKEN_TRUE:
			next();
			addOpCode(OP_UPB);
			addBytecode(1);
			return;
		case TOKEN_FALSE:
			next();
			addOpCode(OP_UPB);
			addBytecode(0);
			return;
		case TOKEN_IDENT:
			next();
			emitSymbol(OP_GETVAR, token);
			return;
		case TOKEN_REF:
			next();
			emitSymbol(OP_GETREF, token);
			return;
		case TOKEN_LPAREN: {
			next();
			expression();
			consume(TOKEN_RPAREN, "Se esperaba ')'");
			return;
		}
		default:
			setError("Se esperaba una expresion", token.line);
			return;
	}
}

static void unary() {
	if (match(TOKEN_DEC)) {
		addOpCode(OP_UPN);
		addUInt(addInt64("0", 1));
		unary();
		addOpCode(OP_SUB);
		return;
	}

	primary();
}

static void multiplicative() {
	unary();

	while (check(TOKEN_MULT) || check(TOKEN_DIV)) {
		TokenType type = next().type;
		unary();

		if (type == TOKEN_MULT) {
			addOpCode(OP_MULT);
		} else {
			addOpCode(OP_DIV);
		}
	}
}

static void additive() {
	multiplicative();

	while (check(TOKEN_ADD) || check(TOKEN_DEC)) {
		TokenType type = next().type;
		multiplicative();

		if (type == TOKEN_ADD) {
			addOpCode(OP_ADD);
		} else {
			addOpCode(OP_SUB);
		}
	}
}

static void comparison() {
	additive();

	if (check(TOKEN_EQ) || check(TOKEN_NE) || check(TOKEN_GT) || check(TOKEN_LT) || check(TOKEN_GE) || check(TOKEN_LE)) {
		TokenType type = next().type;
		additive();

		switch (type) {
			case TOKEN_EQ: addOpCode(OP_EQ); break;
			case TOKEN_NE: addOpCode(OP_NE); break;
			case TOKEN_GT: addOpCode(OP_GT); break;
			case TOKEN_LT: addOpCode(OP_LT); break;
			case TOKEN_GE: addOpCode(OP_GE); break;
			default: addOpCode(OP_LE); break;
		}
	}
}

static void expression() {
	comparison();
}

static void printStatement() {
	expression();
	addOpCode(OP_PRINT);
}

static void inputStatement() {
	Token token = peek();
	if (!consume(TOKEN_IDENT, "Se esperaba el nombre de una variable")) {
		return;
	}

	uint32_t ref = addString(token.start, (uint32_t) token.len);
	addOpCode(OP_INPUT);
	addUInt(ref);
}

static void returnStatement() {
	if (check(TOKEN_NEWLINE) || check(TOKEN_SEMI) || check(TOKEN_EOF) || check(TOKEN_RBRACE)) {
		addOpCode(OP_POP);
	} else {
		expression();
	}

	addOpCode(OP_RETURN);
}

static void assignmentStatement() {
	Token token = next();
	bool isCell = token.type == TOKEN_REF;

	if (!consume(TOKEN_ASIGN, "Se esperaba '='")) {
		return;
	}

	// Una asignacion puede quedar sin valor: la celda queda vacia.
	if (check(TOKEN_NEWLINE) || check(TOKEN_SEMI) || check(TOKEN_EOF)) {
		addOpCode(OP_UPNIL);
	} else {
		expression();
	}

	emitSymbol(isCell ? OP_SETREF : OP_SETVAR, token);
}

static void ifStatement() {
	expression();

	addOpCode(OP_JF);
	uint32_t jumpIfFalse = chunkLength();
	addInt32(0);

	block();

	skipEndings();

	if (match(TOKEN_ELSE)) {
		addOpCode(OP_JMP);
		uint32_t jumpEnd = chunkLength();
		addInt32(0);

		setInt32(jumpIfFalse, (int32_t) chunkLength());
		block();
		setInt32(jumpEnd, (int32_t) chunkLength());
		return;
	}

	setInt32(jumpIfFalse, (int32_t) chunkLength());
}

static void whileStatement() {
	uint32_t start = chunkLength();

	expression();

	addOpCode(OP_JF);
	uint32_t jumpIfFalse = chunkLength();
	addInt32(0);

	block();

	addOpCode(OP_JMP);
	addInt32((int32_t) start);

	setInt32(jumpIfFalse, (int32_t) chunkLength());
}

static void forStatement() {
	Token variable = peek();
	if (!consume(TOKEN_IDENT, "Se esperaba el nombre de una variable")) {
		return;
	}

	if (!consume(TOKEN_IN, "Se esperaba 'in'")) {
		return;
	}

	uint32_t name = addString(variable.start, (uint32_t) variable.len);

	expression();

	if (!consume(TOKEN_COLON, "Se esperaba ':'")) {
		return;
	}

	addOpCode(OP_SETVAR);
	addUInt(name);

	uint32_t start = chunkLength();

	addOpCode(OP_GETVAR);
	addUInt(name);

	expression();

	addOpCode(OP_LE);
	addOpCode(OP_JF);
	uint32_t jumpIfFalse = chunkLength();
	addInt32(0);

	block();

	addOpCode(OP_GETVAR);
	addUInt(name);
	addOpCode(OP_UPN);
	addUInt(addInt64("1", 1));
	addOpCode(OP_ADD);
	addOpCode(OP_SETVAR);
	addUInt(name);

	addOpCode(OP_JMP);
	addInt32((int32_t) start);

	setInt32(jumpIfFalse, (int32_t) chunkLength());
}

static void statement() {
	Token token = peek();

	switch (token.type) {
		case TOKEN_PRINT:
			next();
			printStatement();
			return;
		case TOKEN_INPUT:
			next();
			inputStatement();
			return;
		case TOKEN_RETURN:
			next();
			returnStatement();
			return;
		case TOKEN_IF:
			next();
			ifStatement();
			return;
		case TOKEN_WHILE:
			next();
			whileStatement();
			return;
		case TOKEN_FOR:
			next();
			forStatement();
			return;
		case TOKEN_IDENT:
		case TOKEN_REF:
			assignmentStatement();
			return;
		default:
			setError("Se esperaba una sentencia", token.line);
			return;
	}
}

static void block() {
	if (match(TOKEN_LBRACE)) {
		while (true) {
			skipEndings();

			if (match(TOKEN_RBRACE)) {
				return;
			}

			if (check(TOKEN_EOF)) {
				setError("Se esperaba '}'", peek().line);
				return;
			}

			statement();

			if (errorMessage[0] != '\0') {
				return;
			}
		}
	}

	statement();
}

int parse(const char* source) {
	errorMessage[0] = '\0';
	parser.hasCurrent = false;

	loadSource(source);

	while (true) {
		skipEndings();

		if (check(TOKEN_EOF)) {
			break;
		}

		statement();

		if (errorMessage[0] != '\0') {
			return 1;
		}
	}

	addOpCode(OP_HALT);

	return 0;
}
