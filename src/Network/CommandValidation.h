#ifndef OPENHW_COMMAND_VALIDATION_H
#define OPENHW_COMMAND_VALIDATION_H
#include <stddef.h>
int hwValidatePlayerCommand(const void *packet,size_t length,unsigned player);
#endif
