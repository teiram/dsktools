/*
 * error.c - Error managing interface
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <stdarg.h>
#include "error.h"

static error_stack_type *app_error = 0;

static void error_destroy() {
	if (app_error) {
		error_reset();
		if (app_error->error_output) {
			free(app_error->error_output);
		}
		free(app_error);
		app_error = NULL;
	}
}

static void error_init() {
	if (!app_error) {
		app_error = calloc(1, sizeof(error_stack_type));
		app_error->error_output = calloc(ERROR_OUTPUT_SIZE, 
						 sizeof(char));
		app_error->current_output_size = ERROR_OUTPUT_SIZE;
		atexit(error_destroy);
	}
}

void error_reset() {
	if (app_error) {
		for (int i = 0; i < app_error->errnum; i++) {
			free(app_error->error_messages[i]);
		}
		app_error->errnum = 0;
	}
}

static void shift_messages() {
	free(app_error->error_messages[0]);
	for (int i = 0; i < app_error->errnum - 1; i++) {
		app_error->error_messages[i] = 
			app_error->error_messages[i + 1];
	}
	app_error->errnum--;
}

void error_add(const char *fmt, ... ) {
	if (!app_error) {
		error_init();
	}
	if (app_error->errnum == MAX_ERROR_STACK) {
		shift_messages();
	}
	va_list ap;
	va_start(ap, fmt);
	char message[MAX_ERROR_SIZE + 1];
	vsnprintf(message, MAX_ERROR_SIZE, fmt, ap);
	va_end(ap);
	app_error->error_messages[app_error->errnum++] = 
		strdup(message);
}

bool error_has_error() {
	if (app_error) {
		return app_error->errnum != 0;
	} else {
		return false;
	}
}

static void init_output_message() {
	if (!app_error) {
		error_init();
	}
	app_error->error_output[0] = 0;
}

const char *error_get() {
	init_output_message();
	int output_size = 0;
	for (int i = app_error->errnum - 1; i >= 0; i--) {
		output_size += strlen(app_error->error_messages[i]) 
			+ (i > 0) ? 1 : 0;
		int32_t needs = output_size - app_error->current_output_size;
		if (needs > 0) {
			app_error->current_output_size += 
				MAX(ERROR_OUTPUT_SIZE, needs);
			app_error->error_output = (char *) realloc(app_error->error_output, app_error->current_output_size);
		}
		strcat(app_error->error_output, 
		       app_error->error_messages[i]);
		if (i > 0) {
			strcat(app_error->error_output, ERROR_SEPARATOR);
		}
	}
	return app_error->error_output;
}

