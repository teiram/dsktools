#include <gtk/gtk.h>
#include "app_model.h"

int main(int argc, char **argv) {
	GError     *error = NULL;

	/* Init GTK+ */
	gtk_init(&argc, &argv);

	/* Create new GtkBuilder object */
	GtkBuilder *builder = gtk_builder_new();
	/* Load UI from file. If error occurs, report it and quit application.
	 * Replace "tut.glade" with your saved project. */
	if (!gtk_builder_add_from_file(builder, "interface.glade", &error)) {
		g_warning("%s", error->message);
		g_free(error);
		return 1;
	}

	/* Get main window pointer from UI */
	GtkWidget *window = 
		GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));

	GtkListStore *file_store = 
		GTK_LIST_STORE(gtk_builder_get_object(builder,
						      "liststore_files"));

	app_model_type *model = app_model_new();
	app_model_set_builder(model, builder);

	gtk_list_store_clear(file_store);
	GtkTreeIter iter;
	gtk_list_store_append(file_store, &iter);
	gtk_list_store_set(file_store, &iter,
			   0, "GALAXIAN",
			   1, "BAS",
			   2, 23433,
			   -1);
	  
	/* Connect signals */
	gtk_builder_connect_signals(builder, model);

	/* Destroy builder, since we don't need it anymore */
	//g_object_unref(G_OBJECT(builder));

	/* Show main window. Any other widget is automatically shown 
	   by GtkBuilder */
	gtk_widget_show(window);

	/* Start main loop */
	gtk_main();

	return 0;
}
