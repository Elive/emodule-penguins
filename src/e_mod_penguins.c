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

// animation ids
#define ID_WALKER 1
#define ID_FALLER 2
#define ID_CLIMBER 3
#define ID_FLOATER 4
#define ID_SPLATTER 5
#define ID_FLYER 6
#define ID_BOMBER 7
#define ID_ANGEL 8


// _RAND(prob) is true one time every prob
#define _RAND(prob) ( ( random() % prob ) == 0 )


/* module private routines */
static int        _is_inside_any_win(Penguins_Population *pop, int x, int y, int ret_value);
static Eina_Bool  _cb_animator(void *data);
static void       _population_load(Penguins_Population *pop);
static void       _population_free(Penguins_Population *pop);
static void       _theme_load(Penguins_Population *pop);
static void       _start_walking_at(Penguins_Actor *tux, int at_y);
static void       _start_climbing_at(Penguins_Actor *tux, int at_x);
static void       _start_falling_at(Penguins_Actor *tux, int at_x);
static void       _start_flying_at(Penguins_Actor *tux, int at_y);
static void       _start_splatting_at(Penguins_Actor *tux, int at_y);
static void       _start_custom_at(Penguins_Actor *tux, int at_y);
static void       _reborn(Penguins_Actor *tux);
static void       _cb_custom_end(void *data, Evas_Object *o, const char *emi, const char *src);
static void       _cb_click_l (void *data, Evas_Object *o, const char *emi, const char *src);
static void       _cb_click_r (void *data, Evas_Object *o, const char *emi, const char *src);
static void       _cb_click_c (void *data, Evas_Object *o, const char *emi, const char *src);
static void       _start_bombing_at(Penguins_Actor *tux, int at_y);
static Eina_Bool        _delay_born(void *data);



Penguins_Population *
penguins_init(E_Module *m)
{
   Penguins_Population *pop;
   Eina_List *managers, *l, *l2;
   char buf[PATH_MAX];

   pop = E_NEW(Penguins_Population, 1);
   if (!pop) return NULL;

   // Init module persistent config
   pop->module = m;
   pop->conf_edd = E_CONFIG_DD_NEW("Penguins_Config", Penguins_Config);
#undef T
#undef D
#define T Penguins_Config
#define D pop->conf_edd
   E_CONFIG_VAL(D, T, zoom, DOUBLE);
   E_CONFIG_VAL(D, T, penguins_count, INT);
   E_CONFIG_VAL(D, T, theme, STR);
   E_CONFIG_VAL(D, T, alpha, INT);

   pop->conf = e_config_domain_load("module.penguins", pop->conf_edd);
   if (!pop->conf)
   {
      pop->conf = E_NEW(Penguins_Config, 1);
      pop->conf->zoom = 1;
      pop->conf->penguins_count = 3;
      pop->conf->alpha = 200;
      snprintf(buf, sizeof(buf), "%s/themes/default.edj", e_module_dir_get(m));
      pop->conf->theme = eina_stringshare_add(buf);
   }

   // get canvas size (TODO: multiple zones)
   pop->canvas = e_manager_current_get()->comp->evas;
   evas_output_viewport_get(pop->canvas, NULL, NULL, &pop->width, &pop->height);

   // Search available themes
   printf("PENGUINS: Get themes list\n");
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
         if (name = edje_file_data_get(buf, "PopulationName"))
         {
            printf("PENGUINS: Theme found: %s (%s)\n", filename, name);
            pop->themes = eina_list_append(pop->themes, strdup(buf));
         }
      }
      free(filename);
   }

   // bootstrap
   _theme_load(pop);
   _population_load(pop);
   pop->animator = ecore_animator_add(_cb_animator, pop);

   return pop;
}

void
penguins_shutdown(Penguins_Population *pop)
{
   printf("PENGUINS: KILL 'EM ALL\n");

   _population_free(pop);

   E_FREE_FUNC(pop->animator, ecore_animator_del);
   E_FREE_LIST(pop->themes, free);

   E_FREE_FUNC(pop->conf->theme, eina_stringshare_del);
   E_FREE(pop->conf);
   E_CONFIG_DD_FREE(pop->conf_edd);

   E_FREE(pop);
}

void
penguins_reload(Penguins_Population *pop)
{
   _population_free(pop);
   _theme_load(pop);
   _population_load(pop);
}

/* module private routines */
Eina_Bool 
_action_free(const Eina_Hash *hash, const void *key, void *data, void *fdata)
{
   Penguins_Action *a = data;

   //printf("PENGUINS: Free Action '%s' :(\n", a->name);
   E_FREE(a->name);
   E_FREE(a);

   return EINA_TRUE;
}

static void
_population_free(Penguins_Population *pop)
{
   Penguins_Actor *tux;
   Penguins_Custom_Action *act;

   //printf("PENGUINS: Free Population\n");

   EINA_LIST_FREE(pop->penguins, tux)
   {
      //printf("PENGUINS: Free TUX :)\n");
      E_FREE_FUNC(tux->obj, evas_object_del);
      E_FREE(tux);
   }

   EINA_LIST_FREE(pop->customs, act)
   {
      //printf("PENGUINS: Free Custom Action\n");
      E_FREE(act->name);
      E_FREE(act->left_program_name);
      E_FREE(act->right_program_name);
      E_FREE(act);
   }

   eina_hash_foreach(pop->actions, _action_free, NULL);
   eina_hash_free(pop->actions);
   pop->actions = NULL;
}

static Penguins_Action *
_load_action(Penguins_Population *pop, const char *filename, char *name, int id)
{
   Penguins_Action *act;
   int w, h, speed, ret;
   char *data;
 
   data = edje_file_data_get(filename, name);
   if (!data) return NULL;

   ret = sscanf(data, "%d %d %d", &w, &h, &speed);
   free(data);
   if (ret != 3) return NULL;
   
   act = E_NEW(Penguins_Action, 1);
   if (!act) return NULL;

   act->name = strdup(name);
   act->id = id;
   act->w = w * pop->conf->zoom;
   act->h = h * pop->conf->zoom;
   act->speed = speed * pop->conf->zoom;

   if (!pop->actions)
     pop->actions = eina_hash_string_small_new(NULL);
   eina_hash_add(pop->actions, name, act);

   return act;
}

static Penguins_Custom_Action *
_load_custom_action(Penguins_Population *pop, const char *filename, char *name)
{
   Penguins_Custom_Action *c;
   int w, h, h_speed, v_speed, r_min, r_max, ret;
   char *data;
   char buf[25];

   data = edje_file_data_get(filename, name);
   if (!data) return NULL;

   ret = sscanf(data, "%d %d %d %d %d %d", 
                &w, &h, &h_speed, &v_speed, &r_min, &r_max);
   free(data);
   if (ret != 6) return NULL;

   c = E_NEW(Penguins_Custom_Action, 1);
   if (!c) return NULL;

   c->name = strdup(name);
   c->w = w * pop->conf->zoom;
   c->h = h * pop->conf->zoom;
   c->h_speed = h_speed * pop->conf->zoom;
   c->v_speed = v_speed * pop->conf->zoom;
   c->r_min = r_min;
   c->r_max = r_max;

   pop->custom_num++;
   snprintf(buf, sizeof(buf), "start_custom_%d_left", pop->custom_num);
   c->left_program_name = strdup(buf);
   snprintf(buf, sizeof(buf), "start_custom_%d_right", pop->custom_num);
   c->right_program_name = strdup(buf);

   pop->customs = eina_list_append(pop->customs, c);

   return c;
}

static void
_theme_load(Penguins_Population *pop)
{
   char *name;
   char buf[15];
   int i;

   pop->actions = NULL;
   pop->customs = NULL;
   pop->custom_num = 0;

   name = edje_file_data_get(pop->conf->theme, "PopulationName");
   if (!name) return;

   //printf("PENGUINS: Load theme: %s (%s)\n", name, pop->conf->theme);
   free(name);

   // load standard actions
   _load_action(pop, pop->conf->theme, "Walker", ID_WALKER);
   _load_action(pop, pop->conf->theme, "Faller", ID_FALLER);
   _load_action(pop, pop->conf->theme, "Climber", ID_CLIMBER);
   _load_action(pop, pop->conf->theme, "Floater", ID_FLOATER);
   _load_action(pop, pop->conf->theme, "Bomber", ID_BOMBER);
   _load_action(pop, pop->conf->theme, "Splatter", ID_SPLATTER);
   _load_action(pop, pop->conf->theme, "Flyer", ID_FLYER);
   _load_action(pop, pop->conf->theme, "Angel", ID_ANGEL);

   // load custom actions
   i = 2;
   snprintf(buf, sizeof(buf), "Custom_1");
   while (_load_custom_action(pop, pop->conf->theme, buf))
      snprintf(buf, sizeof(buf), "Custom_%d", i++);
}

static void
_population_load(Penguins_Population *pop)
{
   Evas_Object *o;
   Evas_Coord xx, yy, ww, hh;
   int i;
   Penguins_Actor *tux;

   evas_output_viewport_get(pop->canvas, &xx, &yy, &ww, &hh);

   printf("PENGUINS: Creating %d penguins\n", pop->conf->penguins_count);
   for (i = 0; i < pop->conf->penguins_count; i++)
   {
      tux = E_NEW(Penguins_Actor, 1);
      if (!tux) return;

      o = edje_object_add(pop->canvas);
      edje_object_file_set(o, pop->conf->theme, "anims");

      tux->action = eina_hash_find(pop->actions, "Faller");

      evas_object_color_set(o, pop->conf->alpha, pop->conf->alpha,
                            pop->conf->alpha, pop->conf->alpha);
      evas_object_pass_events_set(o, EINA_FALSE);

      edje_object_signal_callback_add(o,"click_l","penguins", _cb_click_l, tux);
      edje_object_signal_callback_add(o,"click_r","penguins", _cb_click_r, tux);
      edje_object_signal_callback_add(o,"click_c","penguins", _cb_click_c, tux);

      tux->obj = o;
      tux->pop = pop;

      //Randomly delay borns in the next 5 seconds
      ecore_timer_add(((double)(random() % 500)) / 100, _delay_born, tux);
   }
}

static Eina_Bool
_delay_born(void *data)
{
   Penguins_Actor *tux = data;

   tux->pop->penguins = eina_list_append(tux->pop->penguins, tux);
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

static void
_reborn(Penguins_Actor *tux)
{
   //printf("PENGUINS: Reborn :)\n");
   tux->reverse = random() % (2);
   tux->x = random() % (tux->pop->width);
   tux->y = -100;
   tux->custom = NULL;
   evas_object_move(tux->obj, (int)tux->x, (int)tux->y);
   _start_falling_at(tux, tux->x);
   evas_object_resize(tux->obj, tux->action->w, tux->action->h);
   evas_object_show(tux->obj);
}

static Eina_Bool
_cb_animator(void *data)
{
   Penguins_Population *pop;
   Eina_List *l;

   pop = data;
   for (l = pop->penguins; l; l = l->next)
   {
      Penguins_Actor *tux;
      int touch;
      tux = l->data;

      // ******  CUSTOM ACTIONS  ********
      if (tux->custom)
      {
         tux->x += ((double)tux->custom->h_speed * ecore_animator_frametime_get());
         tux->y += ((double)tux->custom->v_speed * ecore_animator_frametime_get());
         if ((!_is_inside_any_win(pop,
               (int)tux->x + (tux->action->w / 2),
               (int)tux->y + tux->action->h + 1,
               RETURN_NONE_VALUE))
            && ((int)tux->y + tux->action->h + 1 < pop->height)
            )
         {
            edje_object_signal_callback_del(tux->obj,"custom_done","edje", _cb_custom_end);
            _start_falling_at(tux, (int)tux->x + (tux->action->w / 2));
            tux->custom = NULL;
         }
      }
      // ******  FALLER  ********
      else if (tux->action->id == ID_FALLER)
      {
         tux->y += ((double)tux->action->speed * ecore_animator_frametime_get());
         if ((touch = _is_inside_any_win(pop,
                        (int)tux->x + (tux->action->w / 2),
                        (int)tux->y + tux->action->h,
                        RETURN_TOP_VALUE)))
         {
            if (((int)tux->y - tux->faller_h) > MAX_FALLER_HEIGHT)
               _start_splatting_at(tux, touch);
            else
               _start_walking_at(tux, touch); 
         }
         else if (((int)tux->y + tux->action->h) > pop->height)
         {
            if (((int)tux->y - tux->faller_h) > MAX_FALLER_HEIGHT)
               _start_splatting_at(tux, pop->height);
            else
               _start_walking_at(tux, pop->height);
         }
      }
      // ******  FLOATER ********
      else if (tux->action->id == ID_FLOATER)
      {
         tux->y += ((double)tux->action->speed * ecore_animator_frametime_get());
         if ((touch = _is_inside_any_win(pop,
                        (int)tux->x + (tux->action->w / 2),
                        (int)tux->y + tux->action->h,
                        RETURN_TOP_VALUE)
            ))
            _start_walking_at(tux, touch);
         else if (((int)tux->y + tux->action->h) > pop->height)
            _start_walking_at(tux, pop->height);
      }
      // ******  WALKER  ********
      else if (tux->action->id == ID_WALKER)
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
            //~ printf("FT: %f\n", edje_frametime_get());
            //~ printf("FT: %f\n", pop->frame_speed);
            tux->x -= ((double)tux->action->speed * ecore_animator_frametime_get());
            if ((touch = _is_inside_any_win(pop, (int)tux->x , (int)tux->y, RETURN_RIGHT_VALUE)) ||
                tux->x < 0)
            {
               if (_RAND(CLIMBER_PROB))
                  _start_climbing_at(tux, touch);
               else
               {
                  edje_object_signal_emit(tux->obj, "start_walking_right", "epenguins");
                  tux->reverse = EINA_FALSE;
               }
            }
            if ((tux->y + tux->action->h) < pop->height)   
               if (!_is_inside_any_win(pop, (int)tux->x + (tux->action->w / 2) ,
                                            (int)tux->y + tux->action->h + 1,
                                            RETURN_NONE_VALUE))
                  _start_falling_at(tux, (int)tux->x + (tux->action->w / 2));
         }
         // right
         else
         {
            tux->x += ((double)tux->action->speed * ecore_animator_frametime_get());
            if ((touch = _is_inside_any_win(pop, (int)tux->x + tux->action->w,
                                                 (int)tux->y, RETURN_LEFT_VALUE))
                || (tux->x + tux->action->w) > pop->width)
            {
               if (_RAND(CLIMBER_PROB))
               {
                  if (touch)
                     _start_climbing_at(tux, touch);
                  else
                     _start_climbing_at(tux, pop->width);
               }
               else
               {
                  edje_object_signal_emit(tux->obj, "start_walking_left", "epenguins");
                  tux->reverse = EINA_TRUE;
               }
            }
            if ((tux->y + tux->action->h) < pop->height)   
               if (!_is_inside_any_win(pop, (int)tux->x + (tux->action->w / 2),
                                            (int)tux->y + tux->action->h + 1,
                                            RETURN_NONE_VALUE))
                  _start_falling_at(tux, (int)tux->x + (tux->action->w / 2));
         }
      }
      // ******  FLYER  ********
      else if (tux->action->id == ID_FLYER)
      {
         tux->y -= ((double)tux->action->speed * ecore_animator_frametime_get());
         tux->x += (random() % 3) - 1;
         if (tux->y < 0)
         {
            tux->reverse = !tux->reverse;
            _start_falling_at(tux, (int)tux->x);
         }
      }
      // ******  ANGEL  ********
      else if (tux->action->id == ID_ANGEL)
      {
         tux->y -= ((double)tux->action->speed * ecore_animator_frametime_get());
         tux->x += (random() % 3) - 1;
         if (tux->y < -100)
            _reborn(tux);
      }
      // ******  CLIMBER  ********
      else if (tux->action->id == ID_CLIMBER)
      {
         tux->y -= ((double)tux->action->speed * ecore_animator_frametime_get());
         // left
         if (tux->reverse)
         {
            if (!_is_inside_any_win(pop,
                  (int)tux->x - 1,
                  (int)tux->y + (tux->action->h / 2),
                  RETURN_NONE_VALUE))
            {
               if (tux->x > 0)
               {
                  tux->x -= (tux->action->w / 2) + 1;  
                  _start_walking_at(tux, (int)tux->y + (tux->action->h / 2));
               }
            }
         }
         // right
         else
         {
            if (!_is_inside_any_win(pop, 
                  (int)tux->x + tux->action->w + 1, 
                  (int)tux->y + (tux->action->h / 2), 
                  RETURN_NONE_VALUE))
            {
               if ((tux->x + tux->action->w) < pop->width)
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
_is_inside_any_win(Penguins_Population *pop, int x, int y, int ret_value)
{
   Eina_List *l;
   E_Client *ec;

   // EINA_LIST_FOREACH(e_manager_current_get()->comp->clients, l, ec)
   // {
      // printf("PENGUINS: COMP EC%s:  %p - '%s:%s' || %d,%d @ %dx%d\n", ec->focused ? "*" : "", ec, ec->icccm.name, ec->icccm.class, ec->x, ec->y, ec->w, ec->h);
   // }

   E_CLIENT_FOREACH(e_manager_current_get()->comp, ec)
   {
      // printf("PENGUINS: COMP EC%s:  %p - '%s:%s' || %d,%d @ %dx%d\n", ec->focused ? "*" : "", ec, ec->icccm.name, ec->icccm.class, ec->x, ec->y, ec->w, ec->h);
      if ((ec->w > 1) && (ec->h > 1))
      {
         if ( ((x > ec->x) && (x < (ec->x + ec->w))) &&
              ((y > ec->y) && (y < (ec->y + ec->h))) )
         {
            switch (ret_value)
            {
               case RETURN_NONE_VALUE:
                  return 1;
               case RETURN_RIGHT_VALUE:
                  return ec->x + ec->w;
               case RETURN_BOTTOM_VALUE:
                  return ec->y + ec->h;
               case RETURN_TOP_VALUE:
                  return ec->y;
               case RETURN_LEFT_VALUE:
                  return ec->x;
               default:
                  return 1;
            }
         }
      }
   }
   /*E_Container *con;

   con = e_container_current_get(e_manager_current_get());

   for (l = e_container_shape_list_get(con); l; l = l->next)
   {
      E_Container_Shape *es;
      int sx, sy, sw, sh;
      es = l->data;
      if (es->visible)
      {
         e_container_shape_geometry_get(es, &sx, &sy, &sw, &sh);
         //printf("PENGUINS: E_shape: [%d] x:%d y:%d w:%d h:%d\n", es->visible, sx, sy, sw, sh);
         if ( ((x > sx) && (x < (sx+sw))) &&
              ((y > sy) && (y < (sy+sh))) )
         {
            switch (ret_value)
            {
               case _RET_NONE_VALUE:
                  return 1;
                  break;
               case _RET_RIGHT_VALUE:
                  return sx+sw;
                  break;
               case _RET_BOTTOM_VALUE:
                  return sy+sh;
                  break;
               case _RET_TOP_VALUE:
                  return sy;
                  break;
               case _RET_LEFT_VALUE:
                  return sx;
                  break;
               default:
                  return 1;
            }
         }
      }
   }*/
   return 0;
}

static void 
_start_walking_at(Penguins_Actor *tux, int at_y)
{
   //printf("PENGUINS: Start walking...at %d\n", at_y);
   tux->action = eina_hash_find(tux->pop->actions, "Walker");
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
   tux->action = eina_hash_find(tux->pop->actions, "Climber");
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
      tux->action = eina_hash_find(tux->pop->actions, "Faller");
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
      tux->action = eina_hash_find(tux->pop->actions, "Floater");
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
   tux->action = eina_hash_find(tux->pop->actions, "Flyer");
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
   tux->x = tux->x + (tux->action->w / 2);
   tux->action = eina_hash_find(tux->pop->actions, "Angel");
   if (!tux->action)
   {
      _reborn(tux);
      return;
   }

   tux->x = tux->x - (tux->action->w / 2);
   tux->y = at_y - 10;

   tux->custom = NULL;
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
_start_splatting_at(Penguins_Actor *tux, int at_y)
{
  // printf("PENGUINS: Start splatting...\n");
   evas_object_hide(tux->obj);
   tux->action = eina_hash_find(tux->pop->actions, "Splatter");
   evas_object_resize(tux->obj, tux->action->w, tux->action->h);
   tux->y = at_y - tux->action->h;
   if (tux->reverse)
      edje_object_signal_emit(tux->obj, "start_splatting_left", "epenguins");
   else
      edje_object_signal_emit(tux->obj, "start_splatting_right", "epenguins");

   edje_object_signal_callback_add(tux->obj,"splatting_done","edje", _cb_splatter_end, tux);
   evas_object_resize(tux->obj, tux->action->w, tux->action->h);
   evas_object_move(tux->obj, (int)tux->x, (int)tux->y);
   evas_object_show(tux->obj);
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
         (tux->action->id == ID_ANGEL) ||
         (tux->action->id == ID_BOMBER) ||
         (tux->action->id == ID_SPLATTER))
      )
     return;

   if (tux->reverse)
      edje_object_signal_emit(tux->obj, "start_bombing_left", "epenguins");
   else
      edje_object_signal_emit(tux->obj, "start_bombing_right", "epenguins");

   tux->x = tux->x + (tux->action->w / 2);
   tux->action = eina_hash_find(tux->pop->actions, "Bomber");
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

   //printf("PENGUINS: Custom action end.\n");
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

   if (tux->pop->custom_num < 1)
      return;

   ran = random() % (tux->pop->custom_num);
   //printf("START CUSTOM NUM %d RAN %d\n",tux->pop->custom_num, ran);
   
   tux->custom = eina_list_nth(tux->pop->customs, ran);
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

   //printf("START Custom Action n %d (%s) repeat: %d\n", ran, tux->custom->left_program_name, tux->r_count);

   edje_object_signal_callback_add(tux->obj,"custom_done","edje", _cb_custom_end, tux);
   
}
