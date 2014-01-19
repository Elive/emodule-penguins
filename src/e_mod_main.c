#include <e.h>
#include "config.h"
#include "e_mod_main.h"
#include "e_mod_config.h"
#include "e_mod_penguins.h"



EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Penguins" };

EAPI E_Module *penguins_mod = NULL;


EAPI void *
e_modapi_init(E_Module *m)
{
   char buf[PATH_MAX];

   // Set up module's message catalogue
   snprintf(buf, sizeof(buf), "%s/locale", e_module_dir_get(m));
   bindtextdomain(PACKAGE, buf);
   bind_textdomain_codeset(PACKAGE, "UTF-8");

   // Add an item in the config panel
   snprintf(buf, sizeof(buf), "%s/e-module-penguins.edj", e_module_dir_get(m));
   e_configure_registry_category_add("appearance", 10, D_("Look"),
                                     NULL, "preferences-look");
   e_configure_registry_item_add("appearance/penguins", 150, D_("Penguins"),
                                 NULL, buf, e_int_config_penguins_module);

   penguins_mod = m;
   e_module_delayed_set(m, 1);

   return population_init(m);
}

EAPI int
e_modapi_shutdown(E_Module *m)
{
   Penguins_Population *pop;

   e_configure_registry_item_del("appearance/penguins");
   e_configure_registry_category_del("appearance");

   if (pop = m->data)
   {
      E_FREE_FUNC(pop->config_dialog, e_object_del);
      population_shutdown(pop);
   }
   return 1;
}

EAPI int
e_modapi_save(E_Module *m)
{
   Penguins_Population *pop;

   pop = m->data;
   if (!pop) return 1;
   e_config_domain_save("module.penguins", pop->conf_edd, pop->conf);
   return 1;
}

