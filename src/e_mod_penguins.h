#ifndef E_MOD_PENGUINS_H
#define E_MOD_PENGUINS_H


typedef struct _Penguins_Config
{
   double zoom;
   int penguins_count;
   const char *theme;
   int alpha;
} Penguins_Config;

typedef struct _Penguins_Population
{
   E_Module *module;
   Evas *canvas;
   Ecore_Animator *animator;
   Eina_List *penguins;
   Eina_Hash *actions;
   Eina_List *customs;
   int custom_num;
   Eina_List *themes;

   E_Config_DD *conf_edd;
   Penguins_Config *conf;
   Evas_Coord width, height;
   E_Config_Dialog *config_dialog;
} Penguins_Population;

typedef struct _Penguins_Action
{
   char *name;
   int id;
   Evas_Coord w,h;
   int speed;
} Penguins_Action;

typedef struct _Penguins_Custom_Action
{
   char *name;
   Evas_Coord w,h;
   int h_speed;
   int v_speed;
   int r_min;
   int r_max;
   char *left_program_name;
   char *right_program_name;
} Penguins_Custom_Action;

typedef struct _Penguins_Actor
{
   Evas_Object *obj;
   int reverse;
   double x, y;
   int faller_h;
   int r_count;
   Penguins_Action *action;
   Penguins_Custom_Action *custom;
   Penguins_Population *pop;
} Penguins_Actor;


Penguins_Population *penguins_init(E_Module *m);
void                 penguins_shutdown(Penguins_Population *pop);
void                 penguins_reload(Penguins_Population *pop);


#endif
