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

### The editor

Inside `TUI/` the editor is in pieces, each one with its own job:

| File                 | What it does                                     |
|----------------------|--------------------------------------------------|
| `TUI/main.c`         | the loop: draw, wait for a key, handle it        |
| `TUI/keys.c`         | what each key does                               |
| `TUI/input.c`        | turns what the terminal sends into keys and pastes|
| `TUI/tui_platform.c` | the system: `termios` on POSIX, the console of Win32 on Windows |
| `TUI/cell_manager.c` | the sheet, the drawing and the history            |
| `TUI/clipboard.c`    | the clipboard of the editor, and the one of the system |
| `TUI/script.c`       | runs the sheet and fills the cells with their values |
| `TUI/file.c`         | reads and writes the `.cle` file                  |

The keys are handled in `TUI/keys.c` and not in `main.c` on purpose: the
tests press the same keys the program does, instead of calling the
functions that are underneath. That is the only way to be sure that a
change does what it looks like it does.

## Build

```sh
make build
```

Two binaries are built:

- `build/celex` — the editor.
- `build/celexs` — the interpreter, to run a `.cle` file without the editor.

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

- `build\celex.exe` — the editor.
- `build\celexs.exe` — the interpreter.

```bat
build\celex.exe sample.cle
build\celexs.exe sample.cle --values
```

The editor needs a console that understands ANSI escape sequences: Windows
Terminal and the console of Windows 10 and later do. On the older console the
colors and the alternate screen are lost, but the editor still works.

The editor puts what it copies in the clipboard of the system, so it needs
`user32.lib` on Windows:

```bat
cl /nologo /std:c11 /W3 /utf-8 /ITUI TUI\*.c Common\ref.c Sheet\sheet.c ^
   Sheet\order.c Lang\*.c /link user32.lib
```

On POSIX the copy to the clipboard of the system is best effort: it uses
`xclip`, `wl-copy` or `pbcopy`, whichever is there. If none of them is
installed, the clipboard of the editor still works.

The `build` directory is shared by both systems, so `make clean` and
`build.bat clean` remove the binaries of the two of them.

## Tests

The cases of the language and of the sheet are in `tests`:

```sh
make test          # on POSIX
build.bat test     # on Windows
```

Each case is a `.cle` file in `tests/cases` and what it should print in
`tests/expected`. The first line of the expected file is the exit code the
interpreter should return, and the rest is what it should print. The list
of cases is in `tests/cases.txt`.

The runner is written in C, so the same tests run on both systems.

The suites are: `tests/runner.c` runs the interpreter over the cases
of `tests/cases` and compares what it prints with `tests/expected`;
`tests/editor_test.c` checks the editing of the cells — writing in an empty
cell, inserting in the middle, deleting, undoing — without needing a
terminal; `tests/keys_test.c` presses the keys the same way the program
does, to check that copying a cell and pasting it works, that the copied
cell is marked and that the text of a cell being edited is copied as text;
and `tests/input_test.c` checks that what the terminal sends is understood as
keys or as pastes.

`tests/app_test.c` runs the editor itself, with the keys written in a file,
and checks the sheet that it saves and what it draws in the screen. That is
the test that says if the editor works as it should, from the outside.

`tests/console_test.c` runs the editor inside a real console — a pseudo
console in Windows, a terminal in POSIX — and sends it the keys. It is the
test that catches the editor getting stuck when it reads the console, which
is what happens when the terminal is a terminal and not a pipe.

The language has some debug code that prints the symbols, the chunks and
the tokens. It is not compiled in a normal build, and it is turned on
with:

```sh
make build DEBUG=1     # on POSIX
build.bat debug        # on Windows
```

### Looking at the editor

Two tools are built to look at the editor rather than to check it. They are not
part of `make test`:

```sh
make tools                              # on POSIX
build.bat tools                         # on Windows

build/screen build/celex keys sample.cle   # what the editor drew, as a screen
build/bench 140 45                         # what a frame costs
```

`build/screen` reads what the editor writes to the terminal and lays it out as
text, so a layout can be looked at: the keys are a file with the bytes that
would have been typed, the same way `app_test` gives them. With `-cuadros` it
also says how many bytes and how many cursor moves that took, and with `-hasta
N` it stops after N bytes, which is how a frame in the middle of a slide is
seen.

`build/bench` moves the cursor over a filled sheet and says how long a frame
takes and how much it writes. While the editor is drawing it is not looking at
the keyboard, so a slow frame is a key that answers late.

## Keys

| Key                | Action                                    |
|--------------------|-------------------------------------------|
| `h` `a` `←`       | move the cursor left                      |
| `l` `d` `→`       | move the cursor right                     |
| `k` `w` `↑`       | move the cursor up                        |
| `j` `s` `↓`       | move the cursor down                      |
| any printable key  | start editing the cell, replacing content |
| `i` or `Enter`     | start editing the cell, keeping content   |
| `←` `→`            | move the text cursor while editing        |
| `Home` `End`       | move the text cursor to one end or other  |
| `Backspace`        | delete the character before the cursor    |
| `Del`              | delete the character at the cursor        |
| `Esc`              | leave the cell                            |
| `x`                | clear the cell                            |
| `v`                | show the values or the source of the cells|
| `]` `[`            | wider or narrower cells                   |
| `}` `{`            | taller or shorter cells                   |
| `Ctrl+R`           | evaluate the sheet                        |
| `Ctrl+S`           | save the sheet into the opened file       |
| `Ctrl+Z`           | undo the last change                      |
| `Ctrl+Y`           | redo the change that was undone           |
| `Ctrl+C`           | copy the cell, or the text being edited    |
| `Ctrl+V`           | paste into the cell, or into the text      |
| `Ctrl+L`           | reload the file                           |
| `q`                | quit                                      |

While a cell is being edited the arrows move the text cursor inside it,
and the cell under the cursor is the one that shows where the text goes.
If the content does not fit in the cell, the part that is shown is the one
where the cursor is.

The sheet is evaluated again every time a change is committed, so the values
always match the content of the cells. While a cell is being edited, that
cell shows its source, since that is what is being written; the rest of the
cells keep showing their values.

`Ctrl+C` copies the cell under the cursor and `Ctrl+V` pastes it into the
cell under the cursor, replacing its content. What is copied is the formula
of the cell, and the sheet is calculated again after the paste, so the new
formula has its value right away.

The copied cell is marked with a dashed border, like the marching ants of
Excel, and the status bar says which cell is in the clipboard (`CP A1`). The
mark stays until something else is copied.

The clipboard is the one of the editor, so the `Ctrl+C` and the `Ctrl+V`
always work with the same thing, no matter what the terminal does with
those keys. While a cell is being edited, the `Ctrl+C` and the `Ctrl+V`
work with the text, like in any text editor: the `Ctrl+C` copies the text
of the cell and the `Ctrl+V` puts it where the text cursor is.

Some terminals use the `Ctrl+V` for themselves: the Windows Terminal, for
example, pastes the clipboard of the system. To make that work, the editor
also puts what it copies in the clipboard of the system, and it asks the
terminal to mark the pastes (`ESC[?2004h`), so a text that the terminal
pastes is known to be a paste and not a text that was typed. A paste from
the terminal is put in the clipboard of the editor too, so the two always
agree.

When the values are being shown, the cell under the cursor always shows its
source instead of its value, so the formula of that cell is always visible.

The sheet slides instead of jumping. The view moves by characters and not by
whole cells, so when the cursor reaches an edge the cells are cut in half at the
other side — the next column is already peeking in, and the one left behind is
still partly there. A move that pans the view is drawn in two frames in between,
a few milliseconds apart, so the grid is seen gliding over those cut cells. A
jump that is too big (many columns at once) is drawn in one frame, because
animating it would leave the editor behind the keys.

The size of the cells is not fixed: `]` and `[` make a cell wider or narrower,
`}` and `{` make it taller or shorter, and the bar says what they are with
`7x5`. A wider cell shows a formula that would otherwise wrap, a narrower one
puts more of the sheet on the screen. The two sizes have limits — from two
columns to forty, from one row to eight — so a cell can never be bigger than
the screen nor too small to read.

The status bar starts with the number of the build, so it is always clear
which binary is being used. It is the commit of the repository, with a `+`
when there are changes that were not committed.

If the sheet has changes that were not saved, `q` asks for a confirmation
before leaving.

The file is read when the editor starts. `Ctrl+L` reads it again, which is
useful when it changes from the outside. If there are changes that were not
saved, it asks for a confirmation first, because reloading throws them
away.

## The terminal

The editor talks to the terminal as a stream of bytes, and the same on every
system. `TUI/tui_platform.c` reads the bytes — from `termios` on POSIX
and from the events of the console of Win32 on Windows — and `TUI/input.c`
turns them into keys:

- A character is a key.
- The escape sequences of the arrows, `Home`, `End` and `Delete` are keys.
- A `Ctrl`+letter is the control code of the letter.

A paste is something else: it is a text that the terminal gives at once, and
not keys that a person pressed. There are two ways of telling them apart:

- The editor asks the terminal to mark the pastes (`ESC[?2004h`). Then the
  pasted text arrives between `ESC[200~` and `ESC[201~`.
- When the markers are not there — the console of Windows takes them out,
  and so do the terminals that do not know the mode — the editor sees a
  burst of characters that arrived together, and that is a paste too.

A burst is told apart from typing by the time: a person cannot press two
different keys in less than 10 ms, and the editor only calls a burst a paste
when the characters are not all the same, because a key that is held down
repeats the same character.

The console of Windows has its own way of sending the keys with `Ctrl`, the
*win32-input-mode* (`CSI Vk ; Sc ; Uc ; Kd ; Cs ; Rc _`). The editor
understands it, and it also understands the plain control codes, so it works
the same in the Windows Terminal, in the old console and in a terminal of
Linux.

### When it does not work

- **The editor does not answer**: it is waiting for a key that never comes.
  In the console of Windows this happens when the events of the console are
  not being read; `tests/console_test.c` is the test that catches it.
- **The `Ctrl+V` pastes something else**: the terminal has a keybinding of
  its own for it, and it pastes the clipboard of the system. That is why the
  editor puts what it copies there too.
- **The `Ctrl+C` does nothing**: there is text selected in the terminal. The
  Windows Terminal copies the selection and does not let the key reach the
  editor. With `Esc`, or with a click, the selection goes away and the
  `Ctrl+C` copies the cell.
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
- An assignment can be left without a value, `D1 =`, and then the cell is
  empty. The editor does not write those cells when it saves: a cell with
  no content does not need a line in the file.
- Cells are written back sorted by column and then by row. The language has
  no forward references, so the order matters: a cell is written after the
  cells it uses, and so it can be evaluated on its own.
- The language has no comment syntax, so the editor never writes comments.
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

The cells are evaluated one by one, in the order they can be calculated, and
each one sees the value of the cells that were calculated before it. If one
of them cannot be calculated, the others are anyway and the status bar
says which one failed:

```
 B1: Se esperaba una expresion
```

The interpreter runs the whole file at once, because a file can also be a
program with `print`, `for` or `if`. Both ways give the same values, since
they use the same order.

The language has no forward references: a cell can only use the value of the
cells that were calculated before it. The module takes care of that before
evaluating, and puts the cell assignments in the order they can be computed
in, no matter how they were written:

```
A1 = B1 + 1
B1 = 5
```

is evaluated as `B1 = 5` and then `A1 = B1 + 1`, so `A1` is `6`. Only the
assignments of cells are reordered, and only when they need it: the rest of
the program keeps its place.

Cells that depend on each other in a cycle cannot be calculated:

```
A1 = B1
B1 = A1
```

is reported as `referencia circular entre celdas: A1, B1`, and the sheet is
left as it was.

The `v` key switches between the values and the source of the cells. The
status bar says which one the grid is showing, with `SRC` or `VAL`, and it
always shows the source of the selected cell.

The interpreter can also be used on its own, which is what the editor does
under the hood:

```sh
./build/celexs sheet.cle --values
```

With `--values` the output of the program goes to stderr and stdout only
carries the resulting cells, one per line, with the same format the editor
reads.
