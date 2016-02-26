#ifndef APP_MODEL_H
#define APP_MODEL_H

#include "../amsdos.h"

typedef struct app_model app_model_type;

app_model_type *app_model_new();
void app_model_set_builder(app_model_type *model, GtkBuilder *builder);
GtkBuilder *app_model_get_builder(app_model_type *model);
void app_model_set_amsdos(app_model_type *model, amsdos_type *amsdos);
amsdos_type *app_model_get_amsdos(app_model_type *model);
void app_model_delete(app_model_type *model);

#endif //APP_MODEL_H
