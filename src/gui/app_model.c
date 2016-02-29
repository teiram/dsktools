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

void app_model_delete(app_model_type *model) {
	if (model->builder) {
		g_object_unref(G_OBJECT(model->builder));
	}
	if (model->filename) {
		g_free(model->filename);
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
