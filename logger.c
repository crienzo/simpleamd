/* 
 * Copyright (c) 2014 Christopher M. Rienzo <chris@rienzo.com>
 *
 * See the file COPYING for copying permission.
 */
#include "simpleamd.h"
#include "samd_private.h"

#include <stdio.h>
#include <stdarg.h>

void _samd_log_printf(samd_log_fn log_handler, samd_log_level_t level, void *user_data, const char *file, int line, const char *format_string, ...)
{
	char message[256];
	va_list ap;
	va_start(ap, format_string);
	vsnprintf(message, sizeof(message) / sizeof(char), format_string, ap);
	log_handler(level, user_data, file, line, message);
	va_end(ap);
}
