/*
 * error.h - Error managing interface
 * Copyright (C)2016 Manuel Teira <manuel.teira@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef ERROR_H
#define ERROR_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_ERROR_STACK 8
#define ERROR_OUTPUT_SIZE 256
#define MAX_ERROR_SIZE 256
#define ERROR_SEPARATOR "\n"

#define DSK_ERROR -1
#define DSK_OK 0

typedef struct {
	char *error_messages[MAX_ERROR_STACK];
	int errnum;
	char *error_output;
	uint32_t current_output_size;
} error_stack_type;

void error_add_error(const char *fmt, ...);
bool error_has_error();
void error_reset();
const char *error_get_error_message();

#endif //ERROR_H
