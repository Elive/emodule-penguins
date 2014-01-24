#ifndef E_MOD_PENGUINS_H
#define E_MOD_PENGUINS_H


enum {
   AID_WALKER,
   AID_FALLER,
   AID_CLIMBER,
   AID_FLOATER,
   AID_SPLATTER,
   AID_FLYER,
   AID_BOMBER,
   AID_ANGEL,
   AID_LAST
};

typedef struct _Penguins_Config
{
   double zoom;
   int penguins_count;
   const char *theme;
   int alpha;
} Penguins_Config;


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

typedef struct _Penguins_Population
{
   E_Module *module;
   Ecore_Animator *animator;
   Eina_List *themes;
   Eina_List *penguins;
   Penguins_Action *actions[AID_LAST];
   Eina_List *customs;
   Eina_List *handlers;

   E_Config_DD *conf_edd;
   Penguins_Config *conf;
   E_Config_Dialog *config_dialog;
} Penguins_Population;

typedef struct _Penguins_Actor
{
   Evas_Object *obj;
   E_Zone *zone;
   double x, y;
   int reverse;
   int faller_h;
   int r_count;
   Penguins_Action *action;
   Penguins_Custom_Action *custom;
} Penguins_Actor;


Penguins_Population *penguins_init(E_Module *m);
void                 penguins_shutdown(void);
void                 penguins_reload(void);


#endif
