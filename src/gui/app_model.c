/* 
 * app_model.c - Model for the GTK GUI
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

#include <stdlib.h>
#include <gtk/gtk.h>
#include "app_model.h"
#include "../amsdos.h"


struct app_model {
	amsdos_type *amsdos;
	GtkBuilder *builder;
	bool modified;
	gchar *filename;
};

app_model_type *app_model_new() {
	app_model_type *model = calloc(1, sizeof(app_model_type));
	return model;
}

void app_model_reset(app_model_type *model) {
	amsdos_delete(model->amsdos);
	model->modified = false;
	if (model->filename) {
		g_free(model->filename);
	}
}

void app_model_delete(app_model_type *model) {
	app_model_reset(model);
	if (model->builder) {
		g_object_unref(G_OBJECT(model->builder));
	}
	free(model);
}

GtkBuilder *app_model_get_builder(app_model_type *model) {
	return model->builder;
}

void app_model_set_builder(app_model_type *model, GtkBuilder *builder) {
	model->builder = builder;
}

void app_model_set_amsdos(app_model_type *model, amsdos_type *amsdos) {
	model->amsdos = amsdos;
}

amsdos_type *app_model_get_amsdos(app_model_type *model) {
	return model->amsdos;
}

void app_model_set_modified(app_model_type *model, bool modified) {
	model->modified = modified;
}

bool app_model_get_modified(app_model_type *model) {
	return model->modified;
}

gchar *app_model_get_filename(app_model_type *model) {
	return model->filename;
}

void app_model_set_filename(app_model_type *model, gchar *filename) {
	if (model->filename != NULL) {
		g_free(model->filename);
	}
	model->filename = filename;
}
