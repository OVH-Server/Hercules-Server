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

// poring map @warp prt_fild08

typedef struct drop_protector_data {
    int killer_char_id;
    int item_id;
    int mob_id;
    int is_card;
    struct map_session_data *sd;
} DropProtectorData ;

HPExport struct hplugin_info pinfo = {
    "drop_protector",
    SERVER_TYPE_MAP,
    "0.2",
    HPM_VERSION,
};

#define DP_ORANGE_COLOR 0xffa220

#define DAWN_ESSENCE_ID 7836

#define SEND_SC_MSG(_sd_, _color_, _fmt_, ...)\
    do {\
        char _s_buff_[256] = {};\
        snprintf(_s_buff_, sizeof(_s_buff_), (_fmt_), ##__VA_ARGS__);\
        clif->messagecolor_self((_sd_)->fd, (_color_), _s_buff_);\
    } while(0)\

static int get_db_data_from_char_id(struct map_session_data *sd, char *variable_to_read) {
    int64 var_index = script->add_str(variable_to_read);
    int int_data = pc->readregistry(sd, var_index);
    // ShowInfo("drop_protector: Get %s ==> %d for char id %d\n", variable_to_read, int_data, sd->status.char_id);
    return (int_data);
}

static void set_db_data_for_char_id(struct map_session_data *sd, char *variable_name, int value) {
    int64 var_index = script->add_str(variable_name);
    pc->setregistry(sd, var_index, value);
    // ShowInfo("drop_protector: Set %s ==> %d for char id %d\n", variable_name, value, sd->status.char_id);
}

static int check_drop_protector(int char_id, int mob_id, DropProtectorData *mydata) {
    return (mydata->mob_id == mob_id) ? 1 : 0;
}

static struct map_session_data *get_map_session_data_from_block_list(struct block_list *src_data) {
    if (src_data->type == BL_PC) {
        return (BL_CAST(BL_PC, src_data));
    } 
    else if (src_data->type == BL_PET) {
        return (BL_UCAST(BL_PET, src_data)->msd);
    } 
    else if (src_data->type == BL_MER) {
        return (BL_UCAST(BL_MER, src_data)->master);
    }
    else if (src_data->type == BL_HOM) {
        return (BL_UCAST(BL_HOM, src_data)->master);
    }
    return (NULL);
}

static int drop_protector_custom_mob_dead_pre(struct mob_data **md, struct block_list **src, int *type) {
    struct mob_data *mob_data = *md;
    struct map_session_data *sd = NULL;
    struct block_list *src_data = NULL;

    if (!src || !*src) {
        return 0;
    }

    src_data = *src;

    sd = get_map_session_data_from_block_list(src_data);

    // if we had player struct + mob data apply logic
    if (sd && mob_data) {
        int mob_id = mob_data->db->mob_id;
        int char_id = sd->status.char_id;
        
        // ShowInfo("drop_protector: Player %s (ID:%d) killed Mob ID %d\n", 
        //          sd->status.name, char_id, mob_id);


        DropProtectorData *mydata;
        CREATE(mydata,DropProtectorData,1);
        if (!mydata) {
            ShowWarning("drop_protector: Can't alloc DropProtectorData struct\n");
            return 0;
        }

        mydata->mob_id = get_db_data_from_char_id(sd, "drop_protector_monster_id");
        mydata->item_id = get_db_data_from_char_id(sd, "drop_protector_item_id");
        mydata->is_card = get_db_data_from_char_id(sd, "drop_protector_item_is_card");
        mydata->killer_char_id = sd->status.char_id;
        mydata->sd = sd;
        // script->set_reg

        if (check_drop_protector(char_id, mob_id, mydata)) {
            // ShowInfo("drop_protector: DROP PROTECTOR ACTIVATED! Player %s has protection for Mob ID %d\n", 
            //          sd->status.name, mob_id);
        
            // add custom data to mob_data for next function
            addToMSD(mob_data,mydata,0,true);
            // ShowInfo("drop_protector: Killer %s %d registered for Mob\n", sd->status.name, mydata->killer_char_id);

            struct item it;
            memset(&it, 0, sizeof(it));
            it.nameid = DAWN_ESSENCE_ID;
            it.identify = 1;
            it.amount = 1;
            if (pc->additem(sd, &it, 1, LOG_TYPE_PICKDROP_MONSTER) != 0) {
                // if inventory is full drop item on floor
                map->addflooritem(&mob_data->bl, &it, 1, mob_data->bl.m, 
                                 mob_data->bl.x, mob_data->bl.y, 
                                 sd->status.char_id, 0, 0, 4, true);
                // ShowInfo("drop_protector: Dawn Essence dropped on floor (inventory full)\n");
            } 
        }
    }

    return 0;
}


static int count_item_in_inventory(struct map_session_data *sd, int item_id) {
    int count = 0;
    int i;
    
    for (i = 0; i < sd->status.inventorySize; i++) {
        if (sd->status.inventory[i].nameid == item_id) {
            count += sd->status.inventory[i].amount;
        }
    }
    
    // ShowInfo("drop_protector: Player %s has %d of item ID %d\n", 
    //          sd->status.name, count, item_id);
    return count;
}

static int remove_item_amount(struct map_session_data *sd, int item_id, int amount_to_remove) {
    int i;

    int remaining = amount_to_remove;


    for (i = 0; i < sd->status.inventorySize && remaining > 0; i++) {
        if (sd->status.inventory[i].nameid == item_id) {
            int slot_amount = sd->status.inventory[i].amount;
            int to_delete = (slot_amount > remaining) ? remaining : slot_amount;
            
            if (pc->delitem(sd, i, to_delete, 0, DELITEM_NORMAL, LOG_TYPE_SCRIPT) == 0) {
                remaining -= to_delete;
                // ShowInfo("drop_protector: Removed %d of item ID %d from slot %d\n", 
                //          to_delete, item_id, i);
            }
        }
    }
    
    if (remaining == 0) {
        return 1;
    } 
    return 0;
}

static void drop_protector_custom_mob_item_drop_post(struct mob_data *md, struct item_drop_list *dlist, struct item_drop *ditem, int loot, int drop_rate, unsigned short flag) {
    DropProtectorData *mydata;
    struct item_drop *current;
    int count = 0;

    // get the custom data injected in the last function
    if (!(mydata = getFromMSD(md, 0))) {
        return; 
    }

    // ShowInfo("drop_protector: --- Full Drop List for Mob %d (Killer: %d) loot: %d\n", md->db->mob_id, mydata->killer_char_id, loot);
    
    if (mydata->mob_id != md->db->mob_id) {
        // ShowInfo("drop_protector: Not targeted mob id return\n");
        return ;
    }

    current = dlist->item;
    while (current != NULL) {
        
        if (current->item_data.nameid == mydata->item_id) {
            ShowInfo("drop_protector: Item is drop update var\n");
            set_db_data_for_char_id(mydata->sd, "drop_protector_item_is_dropped", 1); // maybe not mandatory
            set_db_data_for_char_id(mydata->sd, "drop_protector_item_is_card", 0);
            set_db_data_for_char_id(mydata->sd, "drop_protector_monster_id", -1);
            set_db_data_for_char_id(mydata->sd, "drop_protector_item_id", -1);

            mydata->mob_id = -1;

            int amount_to_remove = count_item_in_inventory(mydata->sd, DAWN_ESSENCE_ID);

            int essence_to_add_account = amount_to_remove / 10;
            int modulo_essence = amount_to_remove % 10;
            if (modulo_essence != 0) {
                essence_to_add_account += 1;
            }
            int current_account_balance = get_db_data_from_char_id(mydata->sd, "drop_protector_essence_account");
            int new_account_balance = current_account_balance + essence_to_add_account;
            set_db_data_for_char_id(mydata->sd, "drop_protector_essence_account", new_account_balance);



            // truncate item name finish with '_'
            char item_name[256] = {};
            safestrncpy(item_name, itemdb_name(mydata->item_id), strlen(itemdb_name(mydata->item_id)));
            int item_name_len = strlen(item_name);
            if (item_name_len > 0 && item_name[item_name_len] == '_') {
                item_name[item_name_len] = '\0';
            }


            SEND_SC_MSG(mydata->sd, DP_ORANGE_COLOR, "===================================================================");
            SEND_SC_MSG(mydata->sd, DP_ORANGE_COLOR, "[Drop Protector]: Protected Item Droped: [%s]", item_name);
            SEND_SC_MSG(mydata->sd, DP_ORANGE_COLOR, "[Drop Protector]: Remove %d Dawn essence from your inventory", amount_to_remove);
            SEND_SC_MSG(mydata->sd, DP_ORANGE_COLOR, "[Drop Protector]: Credit %d Dawn essence to your account", essence_to_add_account);
            SEND_SC_MSG(mydata->sd, DP_ORANGE_COLOR, "[Drop Protector]: New account balance %d Dawn essence", new_account_balance);
            SEND_SC_MSG(mydata->sd, DP_ORANGE_COLOR, "===================================================================");

            remove_item_amount(mydata->sd, DAWN_ESSENCE_ID, amount_to_remove);
            break ;
        }

        current = current->next;
        count++;
    }
}

HPExport void plugin_init(void) {
    ShowInfo("   [drop_protector] Initialisation... \n");

    if (SERVER_TYPE == SERVER_TYPE_MAP) {
        ShowInfo("drop_protector: Adding pre hook function for mob->dead and pos hook function for mob->item_drop\n");
        addHookPre(mob, dead, drop_protector_custom_mob_dead_pre);
        addHookPost(mob, item_drop, drop_protector_custom_mob_item_drop_post);
    } else {
        ShowWarning("drop_protector: Unknown SERVERTYPE\n");
    }
}

// HPExport void server_online(void) {
//     ShowInfo("drop_protector: Server is online\n");
// }

// HPExport void server_ready(void) {
//     ShowInfo("drop_protector: Server is ready\n");
// }


/* Debug to display loot for drop_protector_custom_mob_item_drop_post loop */
// struct item_data *id = itemdb->search(current->item_data.nameid);
// ShowInfo("drop_protector: Drop #%d | ID: %d | Name: %s | Amount: %d\n", 
//             count,
//             current->item_data.nameid, 
//             id ? id->name : "Unknown",
//             current->item_data.amount);