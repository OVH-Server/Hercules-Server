#include "common/hercules.h"
#include "common/memmgr.h"
#include "common/mmo.h"
#include "common/random.h"
#include "common/socket.h"
#include "common/strlib.h"
#include "common/sql.h"
#include "common/timer.h"
#include "login/login.h"
#include "login/lclif.p.h"
#include "map/clif.h"
#include "map/pc.h"
#include "map/script.h"
#include "map/mob.h"
#include "map/pet.h"
#include "map/mercenary.h"
#include "map/homunculus.h"
#include "map/itemdb.h"
#include "map/map.h"
#include "map/battle.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

HPExport struct hplugin_info pinfo = {
    "my_plugin",
    SERVER_TYPE_MAP,
    "0.2",
    HPM_VERSION,
};


// Syntax: getmobspawn(<mob_id>, <map_array>, <qty_array>)
// Returns: number map of monster spawn
static BUILDIN(getmobspawn) {
    int mob_id;
    struct script_data *map_data;
    struct script_data *qty_data;
    struct mob_db *mob_data;
    int count = 0;
    int i, j;

    mob_id = script_getnum(st, 2);
    map_data = script_getdata(st, 3);
    qty_data = script_getdata(st, 4);

    if (!data_isreference(map_data) || !data_isreference(qty_data)) {
        ShowError("getmobspawn: Les paramètres 2 et 3 doivent être des arrays!\n");
        script_pushint(st, 0);
        return false;
    }

    // check mob exist
    if (!mob->db_checkid(mob_id)) {
        ShowError("getmobspawn: Mob ID %d invalide!\n", mob_id);
        script_pushint(st, 0);
        return false;
    }

    // get mob structure
    mob_data = mob->db(mob_id);
    if (!mob_data) {
        ShowError("getmobspawn: Impossible de récupérer les données du mob %d!\n", mob_id);
        script_pushint(st, 0);
        return false;
    }

    // iter on mob spawn
    for (i = 0; i <  ARRAYLENGTH(mob_data->spawn) && mob_data->spawn[i].qty; i++) {
        j = map->mapindex2mapid(mob_data->spawn[i].mapindex);
        
        if (j < 0) {
            continue; // invalid map
        }

        // store map name in array
        script->set_reg(st, NULL, reference_uid(reference_getid(map_data), count), 
                       map->list[j].name, map_data, NULL);
        
        // store qqty in array
        script->set_reg(st, NULL, reference_uid(reference_getid(qty_data), count), 
                       (const void *)(intptr_t)mob_data->spawn[i].qty, qty_data, NULL);
        
        count++;
    }

    ShowInfo("getmobspawn: Mob %d (%s) spawn on %d map(s)\n", 
             mob_id, mob_data->name, count);

    script_pushint(st, count);
    return true;
}

// poring map @warp prt_fild08
int check_drop_protector(int char_id, int mob_id) {
    int protected_mob_id = 0;
    
    if (SQL_ERROR == SQL->Query(map->mysql_handle, 
        "SELECT `value` FROM `char_reg_num_db` WHERE `char_id` = '%d' AND `key` = 'drop_protector_monster_id' AND `index` = 0", 
        char_id)) {
        Sql_ShowDebug(map->mysql_handle);
        ShowError("drop_protector: Error on request get drop_protector_monster_id\n");
        return 0;
    }
    
    if (SQL_SUCCESS == SQL->NextRow(map->mysql_handle)) {
        char *data;
        SQL->GetData(map->mysql_handle, 0, &data, NULL);
        protected_mob_id = atoi(data);
    }
    
    SQL->FreeResult(map->mysql_handle);
    ShowInfo("drop_protector: Get mob id %d for char id %d, killed mob %d\n", protected_mob_id, char_id, mob_id);
    return (protected_mob_id == mob_id) ? 1 : 0;
}

int myplugin_custom_mob_dead_pre(struct mob_data **md, struct block_list **src, int *type) {
    struct mob_data *mob_data = *md;
    struct map_session_data *sd = NULL;
    struct block_list *src_data = NULL;

    if (!src || !*src) {
        return 0;
    }

    src_data = *src;

    /* Get player struct from block list */
    if (src_data->type == BL_PC) {
        sd = BL_CAST(BL_PC, src_data);
    } 
    else if (src_data->type == BL_PET) {
        sd = BL_UCAST(BL_PET, src_data)->msd;
    } 
    else if (src_data->type == BL_MER) {
        sd = BL_UCAST(BL_MER, src_data)->master;
    }
    else if (src_data->type == BL_HOM) {
        sd = BL_UCAST(BL_HOM, src_data)->master;
    }

    // if we had player struct + mob data apply logic
    if (sd && mob_data) {
        int mob_id = mob_data->db->mob_id;
        int char_id = sd->status.char_id;
        
        ShowInfo("drop_protector: Player %s (ID:%d) killed Mob ID %d\n", 
                 sd->status.name, char_id, mob_id);
        
        if (check_drop_protector(char_id, mob_id)) {
            ShowInfo("drop_protector: DROP PROTECTOR ACTIVATED! Player %s has protection for Mob ID %d\n", 
                     sd->status.name, mob_id);
            
            struct item it;
            memset(&it, 0, sizeof(it));
            it.nameid = 7836; // Dawn essence (ID 7836)
            it.identify = 1;
            it.amount = 1;
            if (pc->additem(sd, &it, 1, LOG_TYPE_PICKDROP_MONSTER) != 0) {
                // Si l'inventaire est plein, drop au sol
                map->addflooritem(&mob_data->bl, &it, 1, mob_data->bl.m, 
                                 mob_data->bl.x, mob_data->bl.y, 
                                 sd->status.char_id, 0, 0, 4, true);
                ShowInfo("drop_protector: Dawn Essence dropped on floor (inventory full)\n");
            } else {
                ShowInfo("drop_protector: Dawn Essence added to player inventory\n");
            }
        }
    }

    return 0;
}

int myplugin_custom_mob_dead_post(int retVal___, struct mob_data *md, struct block_list *src, int type) {
    ShowInfo("drop_protector: POST Mob Id [%d] RetVal: %d\n", md->db->mob_id, retVal___);
    return retVal___;
}

HPExport void plugin_init(void) {
    ShowInfo("==========================================\n");
    ShowInfo("   [My_Plugin drop_protector] Initialisation... \n");
    ShowInfo("==========================================\n");

    if (SERVER_TYPE == SERVER_TYPE_MAP) {
        ShowInfo("drop_protector: Adding hook function for mob_dead\n");
        addHookPre(mob, dead, myplugin_custom_mob_dead_pre);
        addHookPost(mob, dead, myplugin_custom_mob_dead_post);
    } else {
        ShowInfo("drop_protector: Unknown SERVERTYPE\n");
    }
}

HPExport void server_online(void) {
    ShowInfo("drop_protector: Server is online\n");
}

HPExport void server_ready(void) {
    ShowInfo("drop_protector: Server is ready\n");
}