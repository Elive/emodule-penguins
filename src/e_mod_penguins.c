#include <e.h>
#include "config.h"
#include "e_mod_main.h"
#include "e_mod_config.h"
#include "e_mod_penguins.h"


#define CLIMBER_PROB 4 // 4 Means: one climber every 5 - 1 Means: all climber - !!Don't set to 0
#define FALLING_PROB 5
#define MAX_FALLER_HEIGHT 300
#define FLYER_PROB 1000 // every n animation cicle
#define CUSTOM_PROB 600 // every n animation cicle (def: 600)

// type of return for _is_inside_any_win()
#define RETURN_NONE_VALUE 0
#define RETURN_BOTTOM_VALUE 1
#define RETURN_TOP_VALUE 2
#define RETURN_LEFT_VALUE 3
#define RETURN_RIGHT_VALUE 4

// _RAND(prob) is true one time every prob
#define _RAND(prob) ( ( random() % prob ) == 0 )

static Penguins_Population* population = NULL;


/* module private routines */
static int        _is_inside_any_win(Penguins_Actor *tux, int x, int y, int ret_value);
static Eina_Bool  _cb_animator(void *data);
static void       _population_load(void);
static void       _population_free(void);
static void       _theme_load(void);
static void       _start_walking_at(Penguins_Actor *tux, int at_y);
static void       _start_climbing_at(Penguins_Actor *tux, int at_x);
static void       _start_falling_at(Penguins_Actor *tux, int at_x);
static void       _start_flying_at(Penguins_Actor *tux, int at_y);
static void       _start_splatting_at(Penguins_Actor *tux, int at_x, int at_y);
static void       _start_custom_at(Penguins_Actor *tux, int at_y);
static void       _reborn(Penguins_Actor *tux);
static void       _cb_custom_end(void *data, Evas_Object *o, const char *emi, const char *src);
static void       _cb_click_l (void *data, Evas_Object *o, const char *emi, const char *src);
static void       _cb_click_r (void *data, Evas_Object *o, const char *emi, const char *src);
static void       _cb_click_c (void *data, Evas_Object *o, const char *emi, const char *src);
static void       _start_bombing_at(Penguins_Actor *tux, int at_y);
static Eina_Bool  _delay_born(void *data);
static Eina_Bool  _cb_zone_changed(void *data, int type EINA_UNUSED, void *event EINA_UNUSED);


Penguins_Population *
penguins_init(E_Module *m)
{
   char buf[PATH_MAX];

   population = E_NEW(Penguins_Population, 1);
   if (!population) return NULL;

   // Init module persistent config
   population->module = m;
   population->conf_edd = E_CONFIG_DD_NEW("Penguins_Config", Penguins_Config);
#undef T
#undef D
#define T Penguins_Config
#define D population->conf_edd
   E_CONFIG_VAL(D, T, zoom, DOUBLE);
   E_CONFIG_VAL(D, T, penguins_count, INT);
   E_CONFIG_VAL(D, T, theme, STR);
   E_CONFIG_VAL(D, T, alpha, INT);

   population->conf = e_config_domain_load("module.penguins", population->conf_edd);
   if (!population->conf)
   {
      population->conf = E_NEW(Penguins_Config, 1);
      population->conf->zoom = 1;
      population->conf->penguins_count = 3;
      population->conf->alpha = 200;
      snprintf(buf, sizeof(buf), "%s/themes/default.edj", e_module_dir_get(m));
      population->conf->theme = eina_stringshare_add(buf);
   }

   // Search available themes
   printf("PENGUINS: Searching themes...\n");
   Eina_List *files;
   char *filename;
   char *name;

   snprintf(buf, sizeof(buf), "%s/themes", e_module_dir_get(m));
   files = ecore_file_ls(buf);
   EINA_LIST_FREE(files, filename)
   {
      if (eina_str_has_suffix(filename, ".edj"))
      {
         snprintf(buf, sizeof(buf), "%s/themes/%s", e_module_dir_get(m), filename);
         if ((name = edje_file_data_get(buf, "PopulationName")))
         {
            printf("PENGUINS:   Theme: %s (%s)\n", filename, name);
            population->themes = eina_list_append(population->themes, strdup(buf));
            free(name);
         }
      }
      free(filename);
   }

   // be notified when zones changes
   E_LIST_HANDLER_APPEND(population->handlers, E_EVENT_ZONE_ADD, _cb_zone_changed, NULL);
   E_LIST_HANDLER_APPEND(population->handlers, E_EVENT_ZONE_DEL, _cb_zone_changed, NULL);

   // bootstrap
   _theme_load();
   _population_load();
   population->animator = ecore_animator_add(_cb_animator, population);

   return population;
}

void
penguins_shutdown(void)
{
   printf("PENGUINS: KILL 'EM ALL\n");

   _population_free();

   E_FREE_LIST(population->handlers, ecore_event_handler_del);
   E_FREE_FUNC(population->animator, ecore_animator_del);
   E_FREE_LIST(population->themes, free);

   E_FREE_FUNC(population->config_dialog, e_object_del);
   E_FREE_FUNC(population->conf->theme, eina_stringshare_del);
   E_FREE(population->conf);
   E_CONFIG_DD_FREE(population->conf_edd);

   E_FREE(population);
}

void
penguins_reload(void)
{
   _population_free();
   _theme_load();
   _population_load();
}

/* module private routines */
static void
_action_free(Penguins_Action *a)
{
   if (!a) return;
   // printf("PENGUINS: Free Action '%s'\n", a->name);
   E_FREE(a->name);
   E_FREE(a);
}

static void
_population_free(void)
{
   Penguins_Actor *tux;
   Penguins_Custom_Action *act;
   int i;

   // printf("PENGUINS: Free Population\n");

   EINA_LIST_FREE(population->penguins, tux)
   {
      // printf("PENGUINS: Free TUX :)\n");
      E_FREE_FUNC(tux->obj, evas_object_del);
      E_FREE(tux);
   }

   EINA_LIST_FREE(population->customs, act)
   {
      // printf("PENGUINS: Free Custom Action\n");
      E_FREE(act->name);
      E_FREE(act->left_program_name);
      E_FREE(act->right_program_name);
      E_FREE(act);
   }

   for (i = 0; i < AID_LAST; i++)
   {
      // printf("PENGUINS: Free Action\n");
      E_FREE_FUNC(population->actions[i], _action_free);
   }
}

static Penguins_Action *
_load_action(char *name, int id)
{
   Penguins_Action *act;
   int w, h, speed, ret;
   char *data;

   population->actions[id] = NULL;

   data = edje_file_data_get(population->conf->theme, name);
   if (!data) return NULL;
   
   ret = sscanf(data, "%d %d %d", &w, &h, &speed);
   free(data);
   if (ret != 3) return NULL;

   act = E_NEW(Penguins_Action, 1);
   if (!act) return NULL;

   act->name = strdup(name);
   act->id = id;
   act->w = w * population->conf->zoom;
   act->h = h * population->conf->zoom;
   act->speed = speed * population->conf->zoom;

   population->actions[id] = act;
   return act;
}

static Penguins_Custom_Action *
_load_custom_action(char *name)
{
   Penguins_Custom_Action *c;
   int w, h, h_speed, v_speed, r_min, r_max, ret;
   char *data;
   char buf[25];

   data = edje_file_data_get(population->conf->theme, name);
   if (!data) return NULL;

   ret = sscanf(data, "%d %d %d %d %d %d",
                &w, &h, &h_speed, &v_speed, &r_min, &r_max);
   free(data);
   if (ret != 6) return NULL;

   c = E_NEW(Penguins_Custom_Action, 1);
   if (!c) return NULL;

   c->name = strdup(name);
   c->w = w * population->conf->zoom;
   c->h = h * population->conf->zoom;
   c->h_speed = h_speed * population->conf->zoom;
   c->v_speed = v_speed * population->conf->zoom;
   c->r_min = r_min;
   c->r_max = r_max;

   population->customs = eina_list_append(population->customs, c);

   snprintf(buf, sizeof(buf), "start_custom_%d_left", eina_list_count(population->customs));
   c->left_program_name = strdup(buf);
   snprintf(buf, sizeof(buf), "start_custom_%d_right", eina_list_count(population->customs));
   c->right_program_name = strdup(buf);

   return c;
}

static void
_theme_load(void)
{
   char *name;
   char buf[15];
   int i = 1;

   name = edje_file_data_get(population->conf->theme, "PopulationName");
   if (!name) return;

   //printf("PENGUINS: Load theme: %s (%s)\n", name, pop->conf->theme);
   free(name);

   // load standard actions
   _load_action("Walker", AID_WALKER);
   _load_action("Faller", AID_FALLER);
   _load_action("Climber", AID_CLIMBER);
   _load_action("Floater", AID_FLOATER);
   _load_action("Bomber", AID_BOMBER);
   _load_action("Splatter", AID_SPLATTER);
   _load_action("Flyer", AID_FLYER);
   _load_action("Angel", AID_ANGEL);

   // load custom actions
   do {
      snprintf(buf, sizeof(buf), "Custom_%d", i++);
   } while (_load_custom_action(buf));
}

static void
_population_load(void)
{
   Penguins_Actor *tux;
   E_Comp *comp;
   E_Zone *zone;
   Eina_List *zones = NULL;
   Eina_List *l, *l2;
   int i;

   // Build a temporary flat list of E_Zone*
   printf("PENGUINS: Hooking zones...\n");
   EINA_LIST_FOREACH((Eina_List*)e_comp_list(), l, comp)
   {
      EINA_LIST_FOREACH(comp->zones, l2, zone)
      {
         zones = eina_list_append(zones, zone);
         printf("PENGUINS:   Zone: %s - %s || %d,%d @ %dx%d\n", zone->comp->name, zone->name, zone->x, zone->y, zone->w, zone->h);
      }
   }

   // Create one object for each penguin
   printf("PENGUINS: Creating %d penguins\n", population->conf->penguins_count);
   for (i = 0; i < population->conf->penguins_count; i++)
   {
      tux = E_NEW(Penguins_Actor, 1);
      if (!tux) return;

      tux->zone = eina_list_nth(zones, i % eina_list_count(zones));
      tux->obj = edje_object_add(tux->zone->comp->evas);
      edje_object_file_set(tux->obj, population->conf->theme, "anims");
      evas_object_color_set(tux->obj, population->conf->alpha, population->conf->alpha,
                                      population->conf->alpha, population->conf->alpha);
      evas_object_pass_events_set(tux->obj, EINA_FALSE);
      evas_object_layer_set(tux->obj, E_LAYER_DESKTOP_TOP + 10);
      edje_object_signal_callback_add(tux->obj, "click_l", "penguins", _cb_click_l, tux);
      edje_object_signal_callback_add(tux->obj, "click_r", "penguins", _cb_click_r, tux);
      edje_object_signal_callback_add(tux->obj, "click_c", "penguins", _cb_click_c, tux);

      //Randomly delay borns in the next 5 seconds
      ecore_timer_add(((double)(random() % 500)) / 100, _delay_born, tux);
   }

   eina_list_free(zones);
}

static Eina_Bool
_delay_born(void *data)
{
   Penguins_Actor *tux = data;

   population->penguins = eina_list_append(population->penguins, tux);
   _reborn(tux);

   return ECORE_CALLBACK_CANCEL;
}

static void
_cb_click_l (void *data, Evas_Object *o, const char *emi, const char *src)
{
   Penguins_Actor *tux = data;
   //printf("Left-click on TUX !!!\n");
   _start_bombing_at(tux, tux->y + tux->action->h);
}

static void
_cb_click_r (void *data, Evas_Object *o, const char *emi, const char *src)
{
   //printf("Right-click on TUX !!!\n");
   e_int_config_penguins_module(NULL, NULL);
}

static void
_cb_click_c (void *data, Evas_Object *o, const char *emi, const char *src)
{
   //printf("Center-click on TUX !!!\n");
}

static Eina_Bool
_cb_zone_changed(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   // printf("PENGUINS: ZONES CHANGED\n");
   penguins_reload();
   return ECORE_CALLBACK_PASS_ON;
}

static void
_reborn(Penguins_Actor *tux)
{
   printf("PENGUINS: Reborn on zone: %s (%d,%d @ %dx%d)\n",
          tux->zone->name, tux->zone->x, tux->zone->y, tux->zone->w, tux->zone->h);
   tux->custom = NULL;
   tux->action = population->actions[AID_FALLER];
   tux->reverse = random() % (2);
   tux->x = tux->zone->x + (random() % (tux->zone->w - tux->action->w));
   tux->y = tux->zone->y - 100;
   
   evas_object_move(tux->obj, (int)tux->x, (int)tux->y);
   _start_falling_at(tux, tux->x);
   evas_object_resize(tux->obj, tux->action->w, tux->action->h);
   evas_object_show(tux->obj);
}

static Eina_Bool
_cb_animator(void *data)
{
   Penguins_Population *pop = data;
   Penguins_Actor *tux;
   Eina_List *l;

   EINA_LIST_FOREACH(pop->penguins, l, tux)
   {
      int touch;

      // ******  CUSTOM ACTIONS  ********
      if (tux->custom)
      {
         tux->x += ((double)tux->custom->h_speed * ecore_animator_frametime_get());
         tux->y += ((double)tux->custom->v_speed * ecore_animator_frametime_get());
         if ((!_is_inside_any_win(tux,
               (int)tux->x + (tux->action->w / 2),
               (int)tux->y + tux->action->h + 1,
               RETURN_NONE_VALUE))
            && ((int)tux->y + tux->action->h + 1 < tux->zone->y + tux->zone->h)
            )
         {
            edje_object_signal_callback_del(tux->obj,"custom_done","edje", _cb_custom_end);
            _start_falling_at(tux, (int)tux->x + (tux->action->w / 2));
            tux->custom = NULL;
         }
      }
      // ******  FALLER  ********
      else if (tux->action->id == AID_FALLER)
      {
         tux->y += ((double)tux->action->speed * ecore_animator_frametime_get());
         if ((touch = _is_inside_any_win(tux,
                        (int)tux->x + (tux->action->w / 2),
                        (int)tux->y + tux->action->h,
                        RETURN_TOP_VALUE)))
         {
            if (((int)tux->y - tux->faller_h) > MAX_FALLER_HEIGHT)
               _start_splatting_at(tux, (int)tux->x + (tux->action->w / 2), touch);
            else
               _start_walking_at(tux, touch);
         }
         else if (((int)tux->y + tux->action->h) > tux->zone->y + tux->zone->h)
         {
            if (((int)tux->y - tux->faller_h) > MAX_FALLER_HEIGHT)
               _start_splatting_at(tux, (int)tux->x + (tux->action->w / 2), tux->zone->y + tux->zone->h);
            else
               _start_walking_at(tux, tux->zone->y + tux->zone->h);
         }
      }
      // ******  FLOATER ********
      else if (tux->action->id == AID_FLOATER)
      {
         tux->y += ((double)tux->action->speed * ecore_animator_frametime_get());
         if ((touch = _is_inside_any_win(tux,
                        (int)tux->x + (tux->action->w / 2),
                        (int)tux->y + tux->action->h,
                        RETURN_TOP_VALUE)
            ))
            _start_walking_at(tux, touch);
         else if (((int)tux->y + tux->action->h) > tux->zone->y + tux->zone->h)
            _start_walking_at(tux, tux->zone->y + tux->zone->h);
      }
      // ******  WALKER  ********
      else if (tux->action->id == AID_WALKER)
      {
         // random flyer
         if (_RAND(FLYER_PROB)){
            _start_flying_at(tux, tux->y);
         }
         // random custom
         else if (_RAND(CUSTOM_PROB)){
            _start_custom_at(tux, tux->y + tux->action->h);
         }
         // left
         else if (tux->reverse)
         {
            tux->x -= ((double)tux->action->speed * ecore_animator_frametime_get());
            if ((touch = _is_inside_any_win(tux, (int)tux->x , (int)tux->y, RETURN_RIGHT_VALUE))
                || (tux->x < tux->zone->x))
            {
               if (_RAND(CLIMBER_PROB))
               {
                  if (touch)
                     _start_climbing_at(tux, touch);
                  else
                     _start_climbing_at(tux, tux->zone->x);
               }
               else
               {
                  edje_object_signal_emit(tux->obj, "start_walking_right", "epenguins");
                  tux->reverse = EINA_FALSE;
               }
            }
            if ((tux->y + tux->action->h) < tux->zone->y + tux->zone->h)
               if (!_is_inside_any_win(tux, (int)tux->x + tux->action->w / 2,
                                            (int)tux->y + tux->action->h + 1,
                                            RETURN_NONE_VALUE))
                  _start_falling_at(tux, (int)tux->x + (tux->action->w / 2));
         }
         // right
         else
         {
            tux->x += ((double)tux->action->speed * ecore_animator_frametime_get());
            if ((touch = _is_inside_any_win(tux, (int)tux->x + tux->action->w,
                                                 (int)tux->y, RETURN_LEFT_VALUE))
                || (tux->x + tux->action->w) > tux->zone->x + tux->zone->w)
            {
               if (_RAND(CLIMBER_PROB))
               {
                  if (touch)
                     _start_climbing_at(tux, touch);
                  else
                     _start_climbing_at(tux, tux->zone->x + tux->zone->w);
               }
               else
               {
                  edje_object_signal_emit(tux->obj, "start_walking_left", "epenguins");
                  tux->reverse = EINA_TRUE;
               }
            }
            if ((tux->y + tux->action->h) < tux->zone->y + tux->zone->h)
               if (!_is_inside_any_win(tux, (int)tux->x + tux->action->w / 2,
                                            (int)tux->y + tux->action->h + 1,
                                            RETURN_NONE_VALUE))
                  _start_falling_at(tux, (int)tux->x + tux->action->w / 2);
         }
      }
      // ******  FLYER  ********
      else if (tux->action->id == AID_FLYER)
      {
         tux->y -= ((double)tux->action->speed * ecore_animator_frametime_get());
         tux->x += (random() % 3) - 1;
         if (tux->y < tux->zone->y)
         {
            tux->reverse = !tux->reverse;
            _start_falling_at(tux, (int)tux->x);
         }
      }
      // ******  ANGEL  ********
      else if (tux->action->id == AID_ANGEL)
      {
         tux->y -= ((double)tux->action->speed * ecore_animator_frametime_get());
         tux->x += (random() % 3) - 1;
         if (tux->y < tux->zone->y - 100)
            _reborn(tux);
      }
      // ******  CLIMBER  ********
      else if (tux->action->id == AID_CLIMBER)
      {
         tux->y -= ((double)tux->action->speed * ecore_animator_frametime_get());
         // left
         if (tux->reverse)
         {
            if (!_is_inside_any_win(tux,
                  (int)tux->x - 1,
                  (int)tux->y + (tux->action->h / 2),
                  RETURN_NONE_VALUE))
            {
               if (tux->x > tux->zone->x)
               {
                  tux->x -= (tux->action->w / 2) + 1;
                  _start_walking_at(tux, (int)tux->y + (tux->action->h / 2));
               }
            }
         }
         // right
         else
         {
            if (!_is_inside_any_win(tux,
                  (int)tux->x + tux->action->w + 1,
                  (int)tux->y + (tux->action->h / 2),
                  RETURN_NONE_VALUE))
            {
               if ((tux->x + tux->action->w) != tux->zone->x + tux->zone->w)
               {
                  tux->x += (tux->action->w / 2) + 1;
                  _start_walking_at(tux, (int)tux->y + (tux->action->h / 2));
               }
            }
         }
         if (tux->y < 0){
            tux->reverse = !tux->reverse;
            _start_falling_at(tux, (int)tux->x);
         }
      }
      // printf("PENGUINS: Place tux at x:%d y:%d w:%d h:%d\n", tux->x, tux->y, tux->action->w, tux->action->h);
      evas_object_move(tux->obj, (int)tux->x, (int)tux->y);
  }
  return ECORE_CALLBACK_RENEW;
}

static int
_is_inside_any_win(Penguins_Actor *tux, int x, int y, int ret_value)
{
   Evas_Object *o;

   o = evas_object_top_get(tux->zone->comp->evas);
   while (o)
   {
      int xx, yy, ww, hh;

      if (evas_object_data_get(o, "comp_object") && evas_object_visible_get(o))
      {
         evas_object_geometry_get(o, &xx, &yy, &ww, &hh);
         if ((ww > 1) && (hh > 1))
         {
            // printf("* LAYER: %d OBJ: %p - %s || %d,%d @ %dx%d\n",
            //        evas_object_layer_get(o), o, evas_object_name_get(o), xx, yy, ww, hh);

            if ((x > xx) && (x < xx + ww) && (y > yy) && (y < yy + hh))
            {
               switch (ret_value)
               {
                  case RETURN_NONE_VALUE:
                     return 1;
                  case RETURN_RIGHT_VALUE:
                     return xx + ww;
                  case RETURN_BOTTOM_VALUE:
                     return yy + hh;
                  case RETURN_TOP_VALUE:
                     return yy;
                  case RETURN_LEFT_VALUE:
                     return xx;
                  default:
                     return 1;
               }
            }
         }
      }

      o = evas_object_below_get(o);
   }

   return 0;
}

static void
_start_walking_at(Penguins_Actor *tux, int at_y)
{
   //printf("PENGUINS: Start walking...at %d\n", at_y);
   tux->action = population->actions[AID_WALKER];
   tux->custom = NULL;

   tux->y = at_y - tux->action->h;
   evas_object_resize(tux->obj, tux->action->w, tux->action->h);

   if (tux->reverse)
      edje_object_signal_emit(tux->obj, "start_walking_left", "epenguins");
   else
      edje_object_signal_emit(tux->obj, "start_walking_right", "epenguins");
}

static void
_start_climbing_at(Penguins_Actor *tux, int at_x)
{
   //printf("PENGUINS: Start climbing...at: %d\n", at_x);
   tux->action = population->actions[AID_CLIMBER];
   evas_object_resize(tux->obj, tux->action->w, tux->action->h);

   if (tux->reverse)
   {
      tux->x = at_x;
      edje_object_signal_emit(tux->obj, "start_climbing_left", "epenguins");
   }
   else
   {
      tux->x = at_x - tux->action->w;
      edje_object_signal_emit(tux->obj, "start_climbing_right", "epenguins");
   }
}

static void
_start_falling_at(Penguins_Actor *tux, int at_x)
{
   if (_RAND(FALLING_PROB))
   {
      //printf("PENGUINS: Start falling...\n");
      tux->action = population->actions[AID_FALLER];
      evas_object_resize(tux->obj, tux->action->w, tux->action->h);

      if (tux->reverse)
      {
         tux->x = (double)(at_x - tux->action->w);
         edje_object_signal_emit(tux->obj, "start_falling_left", "epenguins");
      }
      else
      {
         tux->x = (double)at_x;
         edje_object_signal_emit(tux->obj, "start_falling_right", "epenguins");
      }
   }
   else
   {
      //printf("Start floating...\n");
      tux->action = population->actions[AID_FLOATER];
      evas_object_resize(tux->obj, tux->action->w, tux->action->h);

      if (tux->reverse)
      {
         tux->x = (double)(at_x - tux->action->w);
         edje_object_signal_emit(tux->obj, "start_floating_left", "epenguins");
      }
      else
      {
         tux->x = (double)at_x;
         edje_object_signal_emit(tux->obj, "start_floating_right", "epenguins");
      }
   }
   tux->faller_h = (int)tux->y;
   tux->custom = NULL;
}

static void
_start_flying_at(Penguins_Actor *tux, int at_y)
{
   tux->action = population->actions[AID_FLYER];
   evas_object_resize(tux->obj, tux->action->w, tux->action->h);
   tux->y = at_y - tux->action->h;
   if (tux->reverse)
      edje_object_signal_emit(tux->obj, "start_flying_left", "epenguins");
   else
      edje_object_signal_emit(tux->obj, "start_flying_right", "epenguins");
}

static void
_start_angel_at(Penguins_Actor *tux, int at_y)
{
   if (!population->actions[AID_ANGEL])
   {
      _reborn(tux);
      return;
   }

   tux->action = population->actions[AID_ANGEL];
   tux->custom = NULL;
   tux->x = tux->x - (tux->action->w / 2);
   tux->y = at_y - 10;

   edje_object_signal_emit(tux->obj, "start_angel", "epenguins");
   evas_object_move(tux->obj,(int)tux->x,(int)tux->y);
   evas_object_resize(tux->obj, tux->action->w, tux->action->h);
}

static void
_cb_splatter_end(void *data, Evas_Object *o, const char *emi, const char *src)
{
   Penguins_Actor *tux = data;

   edje_object_signal_callback_del(o,"splatting_done","edje", _cb_splatter_end);
   _start_angel_at(tux, tux->y + tux->action->h + 10);
}

static void
_start_splatting_at(Penguins_Actor *tux, int at_x, int at_y)
{
   // printf("PENGUINS: Start splatting...\n");

   tux->action = population->actions[AID_SPLATTER];
   evas_object_resize(tux->obj, tux->action->w, tux->action->h);
   tux->y = at_y - tux->action->h;
   tux->x = at_x - tux->action->w / 2;
   if (tux->reverse)
      edje_object_signal_emit(tux->obj, "start_splatting_left", "epenguins");
   else
      edje_object_signal_emit(tux->obj, "start_splatting_right", "epenguins");

   edje_object_signal_callback_add(tux->obj,"splatting_done","edje", _cb_splatter_end, tux);
   evas_object_resize(tux->obj, tux->action->w, tux->action->h);
   evas_object_move(tux->obj, (int)tux->x, (int)tux->y);
}

static void
_cb_bomber_end(void *data, Evas_Object *o, const char *emi, const char *src)
{
   Penguins_Actor *tux = data;

   edje_object_signal_callback_del(o,"bombing_done","edje", _cb_bomber_end);
   _start_angel_at(tux, tux->y);
}

static void
_start_bombing_at(Penguins_Actor *tux, int at_y)
{
   //printf("PENGUINS: Start bombing at %d...\n", at_y);
   if (tux->action && (
         (tux->action->id == AID_ANGEL) ||
         (tux->action->id == AID_BOMBER) ||
         (tux->action->id == AID_SPLATTER))
      )
     return;

   if (tux->reverse)
      edje_object_signal_emit(tux->obj, "start_bombing_left", "epenguins");
   else
      edje_object_signal_emit(tux->obj, "start_bombing_right", "epenguins");

   tux->x = tux->x + (tux->action->w / 2);
   tux->action = population->actions[AID_BOMBER];
   tux->x = tux->x - (tux->action->w / 2);
   tux->y = at_y - tux->action->h;

   edje_object_signal_callback_add(tux->obj,"bombing_done","edje", _cb_bomber_end, tux);
   evas_object_resize(tux->obj, tux->action->w, tux->action->h);
   evas_object_move(tux->obj, (int)tux->x, (int)tux->y);
}

static void
_cb_custom_end(void *data, Evas_Object *o, const char *emi, const char *src)
{
   Penguins_Actor* tux = data;

   // printf("PENGUINS: Custom action end.\n");
   if (!tux->custom)
      return;

   if (tux->r_count > 0)
   {
      if (tux->reverse)
         edje_object_signal_emit(tux->obj, tux->custom->left_program_name, "epenguins");
      else
         edje_object_signal_emit(tux->obj, tux->custom->right_program_name, "epenguins");
      tux->r_count--;
   }
   else
   {
      edje_object_signal_callback_del(o,"custom_done","edje", _cb_custom_end);
      _start_walking_at(tux, tux->y + tux->custom->h);
      tux->custom = NULL;
   }
}

static void
_start_custom_at(Penguins_Actor *tux, int at_y)
{
   int ran;

   if (eina_list_count(population->customs) < 1)
      return;

   ran = random() % (eina_list_count(population->customs));
   tux->custom = eina_list_nth(population->customs, ran);
   if (!tux->custom) return;

   evas_object_resize(tux->obj, tux->custom->w, tux->custom->h);
   tux->y = at_y - tux->custom->h;

   if ( tux->custom->r_min == tux->custom->r_max)
      tux->r_count = tux->custom->r_min;
   else
      tux->r_count = tux->custom->r_min +
                     (random() % (tux->custom->r_max - tux->custom->r_min + 1));
   tux->r_count --;

   if (tux->reverse)
      edje_object_signal_emit(tux->obj, tux->custom->left_program_name, "epenguins");
   else
      edje_object_signal_emit(tux->obj, tux->custom->right_program_name, "epenguins");

   // printf("START Custom Action n %d (%s) repeat: %d\n", ran, tux->custom->left_program_name, tux->r_count);
   edje_object_signal_callback_add(tux->obj,"custom_done","edje", _cb_custom_end, tux);
}
