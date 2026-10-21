# celex

A spreadsheet that lives in the terminal, written in C, with a dedicated
scripting language (`.cle` files).

The TUI in `TUI/` is the editor: it renders the sheet, lets you write the
source of every cell and stores that source back into a `.cle` file. The
`Celex-Script` repository holds the language implementation (scanner,
compiler and VM) that consumes those files.

## Build

```sh
make build
make run          # opens sample.cle
```

`make clean` removes the binary.

## Keys

| Key                | Action                                    |
|--------------------|-------------------------------------------|
| `h` `a` `←`       | move the cursor left                      |
| `l` `d` `→`       | move the cursor right                     |
| `k` `w` `↑`       | move the cursor up                        |
| `j` `s` `↓`       | move the cursor down                      |
| any printable key  | start editing the cell, replacing content |
| `i` or `Enter`     | start editing the cell, keeping content   |
| `Backspace`        | delete the last character of the cell     |
| `Esc`              | leave the cell                            |
| `x` or `Del`       | clear the cell                            |
| `Ctrl+S`           | save the sheet into the opened file       |
| `q`                | quit                                      |

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

- The reference is scanned as a cell reference (`TOKEN_REF`) by the
  Celex Script scanner, `=` as an assignment and the right hand side as the
  expression of the cell, so the file is valid Celex Script.
- The right hand side is free form Celex Script; the editor stores it
  verbatim and never rewrites it.
- Cells are written back sorted by row and then by column.
- The language has no comment syntax, so the editor never writes comments.
  Lines that are not cell assignments are ignored when loading and the
  status bar warns about them.

The editor keeps the whole cell source in memory, so a cell can hold more
text than the 4x2 characters shown on screen; the status bar always shows
the complete source of the selected cell.
