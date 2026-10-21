#ifndef CELEX_DEBUG_H
#define CELEX_DEBUG_H

// El codigo de depuracion del lenguaje imprime los simbolos, los trozos de
// codigo y los tokens. Se activa con -DCELEX_DEBUG, y no se compila en una
// build normal.

#ifdef CELEX_DEBUG
#define debug
#endif

#endif
