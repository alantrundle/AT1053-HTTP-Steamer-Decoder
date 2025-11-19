#include "actions.h"
#include "ui.h"        // EEZ Studio export, contains ui_start, ui_* objects

extern bool player_next();
extern bool player_prev();
extern bool player_start();
extern bool player_stop();

bool isPaused = false;


// -----------------------------------------------------------------------------
// Helper: switch to START screen
// -----------------------------------------------------------------------------
static void goto_start_view()
{
    if (objects.start) {
        lv_scr_load_anim( objects.start, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);                       
    }
}

static void goto_stats_view()
{
    if (objects.stats) {
        lv_scr_load_anim( objects.stats, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);                 
    }
}

static void goto_player_view()
{
    if (objects.player) {
        lv_scr_load_anim( objects.player, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);                 
    }
}

// -----------------------------------------------------------------------------
// action_back: Assigned in EEZ Studio for “Back” buttons
// -----------------------------------------------------------------------------
void action_back(lv_event_t * e)
{
    LV_UNUSED(e);   // avoid compiler warnings
    goto_start_view();
}

void action_goto_stats_view(lv_event_t * e){
    LV_UNUSED(e);   // avoid compiler warnings
    goto_stats_view();
}

void action_goto_player_view(lv_event_t * e) {
    LV_UNUSED(e);   // avoid compiler warnings
    goto_player_view();
}

void action_player_play_pause(lv_event_t * e) {

    if(isPaused) {
        player_stop();
        lv_label_set_text(objects.player_lbl_btn_play_pause, "Play");
        isPaused = false;
    } else {
        player_start();
        lv_label_set_text(objects.player_lbl_btn_play_pause, "Pause");
        isPaused = true;
    }

}





void action_player_previous(lv_event_t * e) {
    player_prev();
}

void action_player_next(lv_event_t * e) {
    player_next();
}



