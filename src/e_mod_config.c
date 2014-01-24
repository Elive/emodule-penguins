#include <e.h>
#include "e_mod_main.h"
#include "e_mod_config.h"
#include "e_mod_penguins.h"
#include "config.h"


struct _E_Config_Dialog_Data
{
   double zoom;
   int penguins_count;
   const char *theme;
   int alpha;
};

/* Protos */
static void *_create_data(E_Config_Dialog *cfd);
static void _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create_widgets(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static int _basic_apply_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static void _fill_data(Penguins_Population *pop, E_Config_Dialog_Data *cfdata);

E_Config_Dialog *
e_int_config_penguins_module(E_Comp *comp, const char *params)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;
   char buf[4096];
   Penguins_Population *pop;

   pop = penguins_mod->data;
   if (e_config_dialog_find("Penguins", "appearance/penguins")) return NULL;
   v = E_NEW(E_Config_Dialog_View, 1);
   
   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.apply_cfdata = _basic_apply_data;
   v->basic.create_widgets = _basic_create_widgets;

   snprintf(buf, sizeof(buf), "%s/e-module-penguins.edj", e_module_dir_get(pop->module));
   cfd = e_config_dialog_new(comp, D_("Population Settings"),
                             "Penguins", "appearance/penguins",
                             buf, 0, v, pop);
   pop->config_dialog = cfd;
   return cfd;
}

static void
_fill_data(Penguins_Population *pop, E_Config_Dialog_Data *cfdata)
{
   cfdata->penguins_count = pop->conf->penguins_count;
   cfdata->zoom = pop->conf->zoom;
   cfdata->alpha = pop->conf->alpha;
   cfdata->theme = eina_stringshare_add(pop->conf->theme);
}

static void *
_create_data(E_Config_Dialog *cfd)
{
   E_Config_Dialog_Data *cfdata;
   Penguins_Population *pop;

   pop = cfd->data;
   cfdata = E_NEW(E_Config_Dialog_Data, 1);

   _fill_data(pop, cfdata);
   return cfdata;
}

static void
_free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata)
{
   Penguins_Population *pop;

   pop = cfd->data;
   pop->config_dialog = NULL;
   eina_stringshare_del(cfdata->theme);
   E_FREE(cfdata);
}

static Evas_Object *
_basic_create_widgets(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *o, *ob, *ol;
   Penguins_Population *pop;
   Eina_List *l;
   int count = 0;
   char *theme;

   pop = cfd->data;
   o = e_widget_list_add(evas, 0, 0);

   ob = e_widget_label_add(evas, D_("Number of penguins:"));
   e_widget_list_object_append(o, ob, 1, 1, 0.5);
   ob = e_widget_slider_add(evas, 1, 0, D_("%1.0f"), 1, 200, 1, 0, NULL, &(cfdata->penguins_count), 200);
   e_widget_list_object_append(o, ob, 1, 1, 0.5);

   ob = e_widget_label_add(evas, D_("Zoom factor:"));
   e_widget_list_object_append(o, ob, 1, 1, 0.5);
   ob = e_widget_slider_add(evas, 1, 0, D_("%1.1f X"), 0.2, 3, 0.2, 0, &(cfdata->zoom), NULL, 200);
   e_widget_list_object_append(o, ob, 1, 1, 0.5);

   ob = e_widget_label_add(evas, D_("Transparency:"));
   e_widget_list_object_append(o, ob, 1, 1, 0.5);
   ob = e_widget_slider_add(evas, 1, 0, D_("%1.0f"), 50, 255, 1, 0, NULL, &(cfdata->alpha), 200);
   e_widget_list_object_append(o, ob, 1, 1, 0.5);

   ob = e_widget_label_add(evas, D_("Select population:"));
   e_widget_list_object_append(o, ob, 1, 1, 0.5);
   ol = e_widget_ilist_add(evas, 24, 24, &(cfdata->theme));

   EINA_LIST_FOREACH(pop->themes, l, theme)
   {
      char *name = edje_file_data_get(theme, "PopulationName");
      Evas_Object *oi;

      if (name)
      {
         oi = edje_object_add(evas);
         edje_object_file_set(oi, theme, "icon");
         e_widget_ilist_append(ol, oi, name, NULL, theme, theme);
         if (!strcmp(theme, cfdata->theme))
            e_widget_ilist_selected_set(ol, count);
         free(name);
         count++;
      }
   }
   e_widget_ilist_go(ol);
   e_widget_size_min_set(ol, 155, 250);
   e_widget_list_object_append(o, ol, 1, 1, 0.5);

   return o;
}

static int
_basic_apply_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata)
{
   Penguins_Population *pop;

   pop = cfd->data;
   
   pop->conf->penguins_count = cfdata->penguins_count;
   pop->conf->zoom = cfdata->zoom;
   pop->conf->alpha = cfdata->alpha;
   
   eina_stringshare_del(pop->conf->theme);
   pop->conf->theme = eina_stringshare_ref(cfdata->theme);

   e_config_save_queue();

   penguins_reload();
   return 1;
}
