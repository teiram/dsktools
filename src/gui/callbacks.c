/* 
 * callbacks.c - GTK Callbacks
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

#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "app_model.h"
#include "../log.h"
#include "../amsdos.h"
#include "../error.h"

static void show_error_dialog(app_model_type *model, const char *message) {
	GtkWidget *dialog = 
		GTK_WIDGET(gtk_builder_get_object(app_model_get_builder(model),
						  "error_dialog"));
	gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dialog),
				      message);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
						 error_get_error_message());
	error_reset();
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_hide(dialog);
}

static bool show_confirm_dialog(app_model_type *model, const char *fmt, ...) {
	GtkWidget *dialog = 
		GTK_WIDGET(gtk_builder_get_object(app_model_get_builder(model),
						  "confirm_dialog"));
	char message[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(message, 255, fmt, ap);
	va_end(ap);
	gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dialog),
				      message);
	gint status = gtk_dialog_run(GTK_DIALOG(dialog));
	LOG(LOG_INFO, "Confirm dialog returns %d", status);
	gtk_widget_hide(dialog);
	return status == GTK_RESPONSE_YES;
}

static int open_amsdos(const char *filename, app_model_type *model) {
	amsdos_type *amsdos = amsdos_new(filename);
	if (amsdos != NULL) {
		app_model_set_amsdos(model, amsdos);
		return 0;
	} else {
		return -1;
	}
}

static void update_disk_info_field(app_model_type *model,
				   const char *item,
				   const char *fmt, ...) {
	GtkBuilder *builder = GTK_BUILDER(app_model_get_builder(model));
	if (builder) {
		char value[32];
		va_list ap;
		GtkEntry *entry = 
			GTK_ENTRY(gtk_builder_get_object(builder, item));
		if (entry) {
			va_start(ap, fmt);
			vsnprintf(value, 31, fmt, ap);
			va_end(ap);
			gtk_entry_set_text(entry, value);
		} else {
			LOG(LOG_ERROR, "Unable to find item %s", item);
		}
	} else {
		LOG(LOG_ERROR, "No builder in model");
	}
}

static void update_disk_info(app_model_type *model) {
	amsdos_type *amsdos = app_model_get_amsdos(model);
	if (amsdos) {
		amsdos_info_type info;
		amsdos_get_info(amsdos, &info);
		update_disk_info_field(model, "format_info", info.dsk_info.type == DSK ? "DSK" : "EDSK" );
		update_disk_info_field(model, "header_info", info.dsk_info.magic);
		update_disk_info_field(model, "creator_info", info.dsk_info.creator);
		update_disk_info_field(model, "type_info", AMSDOS_DISK_STR(info.type));
		update_disk_info_field(model, "tracks_info", "%d", info.dsk_info.tracks);
		update_disk_info_field(model, "sides_info", "%d", info.dsk_info.sides);
		update_disk_info_field(model, "sectors_info", "%d", info.dsk_info.sectors);
		update_disk_info_field(model, "first_sector_info", "%02X", info.dsk_info.first_sector_id);
		update_disk_info_field(model, "size_info", "%d", info.dsk_info.capacity);
		update_disk_info_field(model, "used_info", "%d", info.used);

	}
}

static uint8_t get_selected_user(app_model_type *model) {
	GtkSpinButton *spin = 
		GTK_SPIN_BUTTON(gtk_builder_get_object(app_model_get_builder(model),
						      "user_spinbutton"));
	return (uint8_t) gtk_spin_button_get_value_as_int(spin);
}

static void widget_set_sensitive(app_model_type *model, const char *name,
				 gboolean value) {
	GtkBuilder *builder = GTK_BUILDER(app_model_get_builder(model));
	if (builder) {
		GtkWidget *widget = 
			GTK_WIDGET(gtk_builder_get_object(builder, name));
		if (widget) {
			gtk_widget_set_sensitive(widget, value);
		} else {
			LOG(LOG_ERROR, "Unable to find widget %s", name);
		}
	} else {
		LOG(LOG_ERROR, "No builder in model");
	}
}

static void update_toolbar_status(app_model_type *model) {
	gboolean enable = app_model_get_amsdos(model) ? TRUE : FALSE;
	widget_set_sensitive(model, "write_button", enable);
	widget_set_sensitive(model, "add_button", enable);

	widget_set_sensitive(model, "save_button", 
			     app_model_get_modified(model) ? TRUE: FALSE);
}

static void update_directory_info(app_model_type *model) {
	amsdos_type *amsdos = app_model_get_amsdos(model);
	if (amsdos) {
		GtkListStore *file_store = 
			GTK_LIST_STORE(gtk_builder_get_object(app_model_get_builder(model),
							      "liststore_files"));

		uint8_t selected_user = get_selected_user(model);
		LOG(LOG_DEBUG, "Selected user : %u", selected_user);
		amsdos_dir_type dir_entries[AMSDOS_NUM_DIRENT];
		GtkTreeIter iter;
		gtk_list_store_clear(file_store);
		char name_buffer[9];
		char ext_buffer[4];
		for (int i = 0; i < AMSDOS_NUM_DIRENT; i++) {
			amsdos_get_dir(amsdos, &dir_entries[i], i);
		}
		for (int i = 0; i < AMSDOS_NUM_DIRENT; i++) {
			amsdos_dir_type *dir_entry = &dir_entries[i];
			if (!amsdos_is_dir_deleted(dir_entry) &&
			    dir_entry->extent_low == 0 &&
			    dir_entry->record_count > 0 &&
			    dir_entry->user == selected_user) {
				gtk_list_store_append(file_store, &iter);
				gtk_list_store_set(file_store, &iter,
						   0, amsdos_get_dir_basename(dir_entry, name_buffer),
						   1, amsdos_get_dir_extension(dir_entry, ext_buffer),
						   2, amsdos_get_dir_size(dir_entries, i),
						   -1);
			}
		}
	}
}

static bool is_regular_file(gchar *filename) {
	struct stat buf;
	if (stat(filename, &buf)) {
		LOG(LOG_ERROR, "Unable to stat file %s", filename);
		return false;
	} else {
		return S_ISREG(buf.st_mode);
	}
}


G_MODULE_EXPORT void
cb_selection_changed(GtkFileChooser *chooser, app_model_type *model) {
	LOG(LOG_DEBUG, "cb_selection_changed(model=%08x)", model);
	GtkWidget *button = 
		GTK_WIDGET(gtk_builder_get_object(app_model_get_builder(model),
						  "dsk_filechooser_open"));
	gboolean enable = FALSE;
	if (button) {
		gchar *filename = gtk_file_chooser_get_filename(chooser);
		if (filename && is_regular_file(filename)) {
			enable = TRUE;
		}
		g_free(filename);
		gtk_widget_set_sensitive(button, enable);
	} else {
		LOG(LOG_WARN, "Unable to get open button dialog");
	}
}

G_MODULE_EXPORT void
cb_change_user(GtkSpinButton *spinbutton, app_model_type *model) {
	LOG(LOG_DEBUG, "cb_dsk_open(model=%08x)", model);
	update_directory_info(model);
}			
		
G_MODULE_EXPORT void 
cb_dsk_open(GtkToolButton *button, app_model_type *model) {
	LOG(LOG_DEBUG, "cb_dsk_open(model=%08x)", model);
	if (app_model_get_amsdos(model) && app_model_get_modified(model)) {
		if (show_confirm_dialog(model, "Discard changes on %s?",
					app_model_get_filename(model))) {
			app_model_reset(model);
		} else {
			return;
		}
	}
	GtkWidget *dialog = 
		GTK_WIDGET(gtk_builder_get_object(app_model_get_builder(model),
						  "dsk_filechooser"));
	gint res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_hide(dialog);
	if (res == 1) {
		gchar *filename = 
			gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (filename) {
			LOG(LOG_DEBUG, "Chosen filename %s", filename);
			if (open_amsdos(filename, model)) {
				show_error_dialog(model, "Error opening AMSDOS file");
			} else {
				app_model_set_filename(model, filename);
				update_disk_info(model);
				update_directory_info(model);
				update_toolbar_status(model);
			}
		}
	}   
}

G_MODULE_EXPORT void
cb_dsk_save(GtkToolButton *button, app_model_type *model) {
	LOG(LOG_DEBUG, "cb_dsk_save(model=%08x)", model);
	amsdos_type *amsdos = app_model_get_amsdos(model);
	if (amsdos) {
		gchar *filename = app_model_get_filename(model);
		if (filename) {
			if (is_regular_file(filename) &&
			    show_confirm_dialog(model, "Confirm overwriting of %s file", filename)) {
				if (dsk_save_image(amsdos->dsk, filename) != DSK_OK) {
					show_error_dialog(model, "Error saving image");
				} else {
					app_model_set_modified(model, false);
					update_toolbar_status(model);
				}
			}
		} else {
			LOG(LOG_WARN, "No filename defined");
		}
	} else {
		LOG(LOG_ERROR, "No amsdos object created");
	}
}

G_MODULE_EXPORT void
cb_dsk_new(GtkToolButton *button, app_model_type *model) {
	LOG(LOG_DEBUG, "cb_dsk_new(model=%08x)", model);
	show_error_dialog(model, "Still not implemented");
}

G_MODULE_EXPORT void
cb_dsk_read(GtkToolButton *button, app_model_type *model) {
	LOG(LOG_DEBUG, "cb_dsk_read(model=%08x)", model);
	show_error_dialog(model, "Still not implemented");
}

G_MODULE_EXPORT void
cb_dsk_write(GtkToolButton *button, app_model_type *model) {
	LOG(LOG_DEBUG, "cb_dsk_write(model=%08x)", model);
	show_error_dialog(model, "Still not implemented");
}

G_MODULE_EXPORT void
cb_dsk_add_file(GtkToolButton *button, app_model_type *model) {
	LOG(LOG_DEBUG, "cb_dsk_add_file(model=%08x)", model);
	GtkWidget *dialog = 
		GTK_WIDGET(gtk_builder_get_object(app_model_get_builder(model),
						  "dsk_filechooser"));
	gint res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_hide(dialog);
	amsdos_type *amsdos = app_model_get_amsdos(model);
	if (res == 1 && amsdos) {
		gchar *filename = 
			gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (filename) {
			LOG(LOG_DEBUG, "Chosen filename %s", filename);
			if (amsdos_add_file(amsdos, filename, 
					    filename,
					    get_selected_user(model)) != DSK_OK) {
				show_error_dialog(model, "Adding file to volume");
			} else {
				app_model_set_modified(model, true);
				update_disk_info(model);
				update_directory_info(model);
				update_toolbar_status(model);
			}
			g_free(filename);
		}
	}   	
}

static void remove_selected_file(GtkTreeModel *tree_model,
				 GtkTreePath *path,
				 GtkTreeIter *iter,
				 app_model_type *model) {
	LOG(LOG_DEBUG, "remove_selected_file(model=%08x)", model);
	char amsdos_name[13];
	gchar *name;
	gchar *extension;
	gtk_tree_model_get(tree_model, iter, 0, &name, 1, &extension, -1);
	strcpy(amsdos_name, name);
	strcat(amsdos_name, ".");
	strcat(amsdos_name, extension);
	g_free(name);
	g_free(extension);

	uint8_t user = get_selected_user(model);
	LOG(LOG_DEBUG, "Removing file %s.%s, user %u", name, extension, user);
	amsdos_type *amsdos = app_model_get_amsdos(model);
	if (amsdos_remove_file(amsdos, amsdos_name, user) != DSK_OK) {
		show_error_dialog(model, "Removing file from volume");
	} else {
		app_model_set_modified(model, true);
	}
}

G_MODULE_EXPORT void
cb_dsk_remove_file(GtkToolButton *button, app_model_type *model) {
	LOG(LOG_DEBUG, "cb_dsk_remove_file(model=%08x)", model);

	GtkTreeView *view = 
		GTK_TREE_VIEW(gtk_builder_get_object(app_model_get_builder(model),
						  "file_treeview"));
	GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
	gtk_tree_selection_selected_foreach(selection, 
					    (GtkTreeSelectionForeachFunc) remove_selected_file, 
					    model);
	update_disk_info(model);
	update_directory_info(model);
	update_toolbar_status(model);
}

G_MODULE_EXPORT void
cb_tree_selection_changed(GtkTreeSelection *tree_selection, 
			  app_model_type *model) {
	widget_set_sensitive(model, "remove_button", 
			     gtk_tree_selection_count_selected_rows(tree_selection) > 0);
}

G_MODULE_EXPORT void
cb_quit(GtkWidget *widget, app_model_type *model) {
	if (app_model_get_amsdos(model) &&
	    app_model_get_modified(model)) {
		if (!show_confirm_dialog(model, 
					"Changes will be lost. Sure to quit?")) {
			return;

		}
	}
	gtk_main_quit();
}
