/* 
 * app_model.h - Model for the GTK GUI
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

#ifndef APP_MODEL_H
#define APP_MODEL_H

#include "../amsdos.h"
#include <gtk/gtk.h>

typedef struct app_model app_model_type;

app_model_type *app_model_new();
void app_model_set_builder(app_model_type *model, GtkBuilder *builder);
GtkBuilder *app_model_get_builder(app_model_type *model);
void app_model_set_amsdos(app_model_type *model, amsdos_type *amsdos);
amsdos_type *app_model_get_amsdos(app_model_type *model);
void app_model_delete(app_model_type *model);
void app_model_reset(app_model_type *model);
void app_model_set_modified(app_model_type *model, bool modified);
bool app_model_get_modified(app_model_type *model);
gchar *app_model_get_filename(app_model_type *model);
void app_model_set_filename(app_model_type *model, gchar *filename);

#endif //APP_MODEL_H
