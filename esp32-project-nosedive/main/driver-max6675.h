#ifndef MAX6675_H
#define MAX6675_H
#include "nosedive.h"
bool max6675_initialize(void);
int16_t max6675_read_temperature(void);
#endif // MAX6675_H
