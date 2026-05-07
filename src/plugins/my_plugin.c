
/// Sample Hercules Plugin

#include "common/hercules.h" /* Should always be the first Hercules file included! (if you don't make it first, you won't be able to use interfaces) */
#include "common/memmgr.h"
#include "common/mmo.h"
#include "common/random.h"
#include "common/socket.h"
#include "common/strlib.h"
#include "login/login.h"
#include "login/lclif.p.h"
#include "map/clif.h"
#include "map/pc.h"
#include "map/script.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h" /* should always be the last Hercules file included! (if you don't make it last, it'll intentionally break compile time) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

HPExport struct hplugin_info pinfo = {
    "my_plugin",
    SERVER_TYPE_MAP,
    "0.2",
    HPM_VERSION,
};

HPExport void plugin_init(void) {
    // Dans les plugins récents, ShowInfo est une macro qui utilise le pointeur 'showmsg'
    // Il faut s'assurer que l'interface est bien liée avant l'usage
    ShowInfo("==========================================\n");
    ShowInfo("   [My_Plugin KOALA] Initialisation en cours... \n");
    ShowInfo("==========================================\n");
}

HPExport void server_ready(void) {
    ShowStatus("   [My_Plugin KOALA] Serveur pret, plugin actif !\n");
}