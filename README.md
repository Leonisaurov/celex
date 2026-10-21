# celex

A spreadsheet that lives in the terminal, written in C, with a dedicated
scripting language (`.cle` files).

The editor writes the sheet as a script, the language evaluates it and gives
back the value of every cell.

## Layout

The editor and the language are separate, and a module layer joins them:

```
TUI/      the editor: grid, cells, keys and the .cle file
Sheet/    the module: turns a sheet into a script and a script into values
Lang/     the language: scanner, parser, compiler and virtual machine
Common/   shared code: cell references and file helpers
```

The dependencies only go one way:

```
TUI  ->  Sheet  ->  Lang  ->  Common
```

- `TUI/` knows nothing about the language. It builds the source of the sheet
  and hands it to `Sheet/`.
- `Sheet/sheet.h` is the whole interface between both: `sheet_evaluate()`
  takes the source of a sheet and returns the value of every cell.
- `Lang/` is the language itself, with no idea of a spreadsheet: it compiles
  and runs a program and keeps the cells it is told to assign.
- `Sheet/cli.c` is the standalone interpreter, built on the same module.

## Build

```sh
make build
```

Two binaries are built:

- `build/celex` â€” the editor.
- `build/celexs` â€” the interpreter, to run a `.cle` file without the editor.

```sh
make run         # opens the editor with sample.cle
make run-script  # runs sample.cle and prints the values of the cells
```

`make clean` removes the binaries.

### Build on Windows

The editor talks to the terminal, so it needs a small layer per system: on
POSIX it uses `termios` and on Windows the console of Win32. Both live in
`TUI/tui_platform.c` and the rest of the code is the same everywhere.

On Windows the build uses MSVC, from *Visual Studio Build Tools*:

```bat
build.bat           :: release
build.bat debug     :: with debug symbols
build.bat clean     :: removes the binaries
```

It produces the same two binaries, with the `.exe` extension:

- `build\celex.exe` â€” the editor.
- `build\celexs.exe` â€” the interpreter.

```bat
build\celex.exe sample.cle
build\celexs.exe sample.cle --values
```

The editor needs a console that understands ANSI escape sequences: Windows
Terminal and the console of Windows 10 and later do. On the older console the
colors and the alternate screen are lost, but the editor still works.

The `build` directory is shared by both systems, so `make clean` and
`build.bat clean` remove the binaries of the two of them.

## Keys

| Key                | Action                                    |
|--------------------|-------------------------------------------|
| `h` `a` `â†`       | move the cursor left                      |
| `l` `d` `â†’`       | move the cursor right                     |
| `k` `w` `â†‘`       | move the cursor up                        |
| `j` `s` `â†“`       | move the cursor down                      |
| any printable key  | start editing the cell, replacing content |
| `i` or `Enter`     | start editing the cell, keeping content   |
| `Backspace`        | delete the last character of the cell     |
| `Esc`              | leave the cell                            |
| `x` or `Del`       | clear the cell                            |
| `v`                | show the values or the source of the cells|
| `Ctrl+R`           | evaluate the sheet                        |
| `Ctrl+S`           | save the sheet into the opened file       |
| `q`                | quit                                      |

## The language

```
# Una hoja: cada linea asigna el valor de una celda
A1 = 1
A2 = 2
B1 = A1 + A2
C1 = "hola"

# Variables
total = 0
name = "celex"

# Print
print total
print name + "!"

# Condicionales
if total > 10 {
    print "grande"
} else {
    print "chico"
}

# Bucles
for i in 1:5 {
    total = total + i
}

while total < 100 {
    total = total + 10
}
```

- **Literals**: numbers (`42`), strings (`"hola"`), booleans (`true`, `false`).
- **Variables**: `name = expression`.
- **Cells**: `A1 = expression`, with the usual spreadsheet reference. The
  columns go from `A` to `ZZ` and the rows start at 1.
- **Operators**: `+` `-` `*` `/`, comparisons `==` `!=` `>` `<` `>=` `<=`, and
  the unary minus. `+` also joins strings.
- **Statements**: `print`, `input`, `if`/`else`, `while`, `for`, `return`.
- **Blocks**: `{ ... }`. A statement ends with a new line or a `;`.
- The numbers are integers for now.

## File format

A `.cle` file is Celex Script source. Every line assigns the source of a
cell to the cell it belongs to, addressed with the usual spreadsheet
reference (`A1`, `B2`, `AA10`):

```
A1 = 1
A2 = 2
B1 = A1 + A2
C1 = "hola"
```

- The reference is scanned as a cell reference (`TOKEN_REF`) by the scanner,
  `=` as an assignment and the right hand side as the expression of the
  cell, so the file is valid Celex Script.
- The right hand side is free form Celex Script; the editor stores it
  verbatim and never rewrites it.
- Cells are written back sorted by row and then by column.
  Lines that are not cell assignments are ignored when loading and the
  status bar warns about them.

## Evaluating the sheet

`Ctrl+R` takes the sheet, hands it to the module and fills every cell with
the value that the language calculates:

```
A1 = 1
A2 = 2
B1 = A1 + A2
```

becomes `A1 = 1`, `A2 = 2`, `B1 = 3` in the grid.

status bar always shows the source of the selected cell.

The interpreter can also be used on its own, which is what the editor does
under the hood:

```sh
./build/celexs sheet.cle --values
```

With `--values` the output of the program goes to stderr and stdout only
carries the resulting cells, one per line, with the same format the editor
reads.
