// =============================================
// File: src/game/api.c
// =============================================
#include "game/api.h"
#include <string.h>

int gameapi_validate(const GameAPI* api){
    if(!api) return -1;
    if(api->state_size == 0 || api->undo_size == 0) return -1;
    if(!api->side_to_move || !api->generate_legal || !api->make_move || !api->unmake_move) return -1;
    if(!api->hash || !api->is_terminal || !api->evaluate) return -1;
    return 0;
}

void gameapi_copy_state(const GameAPI* api, const game_state_t* src, game_state_t* dst){
    if(api->copy) { api->copy(src, dst); }
    else { memcpy(dst, src, api->state_size); }
}
