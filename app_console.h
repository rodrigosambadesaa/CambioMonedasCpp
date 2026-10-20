#ifndef PROGVORAZ_APP_CONSOLE_H
#define PROGVORAZ_APP_CONSOLE_H

#include "bigint.h"

// Independent interactive interface, corresponding to original app_console.c.
int app_console_run(void);

// Public formatter matching the original C module's interface.
void app_console_imprimir_resultado(const BigIntArray *denominations,
                                  const BigIntArray *solution,
                                  const BigIntArray *stock,
                                  int withStock);

#endif
