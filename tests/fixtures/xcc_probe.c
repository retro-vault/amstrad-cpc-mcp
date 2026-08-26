/*
 * C11 program built by xcc and executed inside the CPC during make test.
 *
 * GPL 3.0 License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <stdint.h>

void main(void)
{
    volatile uint8_t *const result = (volatile uint8_t *)0x9000;
    uint8_t sum = 0;

    for (uint8_t value = 1; value <= 10; ++value)
        sum = (uint8_t)(sum + value);

    *result = sum;
}
