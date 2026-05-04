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

struct my_plugin_data {
    int killer_char_id;
    int item_id;
    int mob_id;
    int is_card;
};

HPExport struct hplugin_info pinfo = {
    "my_plugin",
    SERVER_TYPE_MAP,
    "0.2",
    HPM_VERSION,
};

int get_npc_variable_data(int char_id, char *variable_to_read) {
    int int_data = 0;
    
    if (SQL_ERROR == SQL->Query(map->mysql_handle, 
        "SELECT `value` FROM `char_reg_num_db` WHERE `char_id` = '%d' AND `key` = '%s' AND `index` = 0", 
        char_id, variable_to_read)) {
        Sql_ShowDebug(map->mysql_handle);
        ShowError("drop_protector: Error on request get drop_protector_monster_id\n");
        return 0;
    }
    if (SQL_SUCCESS == SQL->NextRow(map->mysql_handle)) {
        char *data;
        SQL->GetData(map->mysql_handle, 0, &data, NULL);
        int_data = atoi(data);
    }
    SQL->FreeResult(map->mysql_handle);
    ShowInfo("drop_protector: Get %s ==> %d for char id %d\n", variable_to_read, int_data, char_id);
    return (int_data);
}

int check_drop_protector(int char_id, int mob_id, struct my_plugin_data *mydata) {
    return (mydata->mob_id == mob_id) ? 1 : 0;
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


        struct my_plugin_data *mydata;
        CREATE(mydata,struct my_plugin_data,1);
        mydata->mob_id = get_npc_variable_data(char_id, "drop_protector_monster_id");
        mydata->item_id = get_npc_variable_data(char_id, "drop_protector_item_id");
        mydata->is_card = get_npc_variable_data(char_id, "drop_protector_item_is_card");
        mydata->killer_char_id = sd->status.char_id;
        

        if (check_drop_protector(char_id, mob_id, mydata)) {
            ShowInfo("drop_protector: DROP PROTECTOR ACTIVATED! Player %s has protection for Mob ID %d\n", 
                     sd->status.name, mob_id);
        
            // add custom data to mob_data for next function
            addToMSD(mob_data,mydata,0,true);
            ShowInfo("drop_protector: Killer %s %d registered for Mob\n", sd->status.name, mydata->killer_char_id);

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

// poring map @warp prt_fild08

void myplugin_custom_mob_item_drop_post(struct mob_data *md, struct item_drop_list *dlist, struct item_drop *ditem, int loot, int drop_rate, unsigned short flag) {
    struct my_plugin_data *mydata;
    struct item_drop *current;
    int count = 0;

    // get the custom data injected in the last function
    if (!(mydata = getFromMSD(md, 0))) {
        return; 
    }

    ShowInfo("drop_protector: --- Full Drop List for Mob %d (Killer: %d) loot: %d\n", md->db->mob_id, mydata->killer_char_id, loot);
    
    current = dlist->item;
    while (current != NULL) {
        struct item_data *id = itemdb->search(current->item_data.nameid);
        
        // ShowInfo("drop_protector: Drop #%d | ID: %d | Name: %s | Amount: %d\n", 
        //             count,
        //             current->item_data.nameid, 
        //             id ? id->name : "Unknown",
        //             current->item_data.amount);
        
        if (current->item_data.nameid == mydata->item_id) {
            ShowInfo("drop_protector: ITEM DROPEEED\n");
        }

        current = current->next;
        count++;
    }
    ShowInfo("drop_protector: --- End of List (%d items) ---\n", count);
}

HPExport void plugin_init(void) {
    ShowInfo("==========================================\n");
    ShowInfo("   [My_Plugin drop_protector] Initialisation... \n");
    ShowInfo("==========================================\n");

    if (SERVER_TYPE == SERVER_TYPE_MAP) {
        ShowInfo("drop_protector: Adding hook function for mob_dead\n");
        addHookPre(mob, dead, myplugin_custom_mob_dead_pre);
        addHookPost(mob, item_drop, myplugin_custom_mob_item_drop_post);
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