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
   Ecore_Animator *animator;
   Eina_List *themes;   // list of str (full theme path)
   Eina_List *penguins; // list of Penguins_Actor*
   Eina_Hash *actions;  // key:action_name val:Penguins_Action*
   Eina_List *customs;  // list of Penguins_Custom_Action
   int custom_num; // TODO: REMOVEME

   E_Config_DD *conf_edd;
   Penguins_Config *conf;
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
   E_Zone *zone;
   int reverse;
   double x, y;
   int faller_h;
   int r_count;
   Penguins_Action *action;
   Penguins_Custom_Action *custom;
   Penguins_Population *pop; // TODO: REMOVEME
} Penguins_Actor;


Penguins_Population *penguins_init(E_Module *m);
void                 penguins_shutdown(Penguins_Population *pop);
void                 penguins_reload(Penguins_Population *pop);


#endif
