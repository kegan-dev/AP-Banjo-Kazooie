#include <ultra64.h>
#include "functions.h"
#include "variables.h"

void func_80363500(Actor *this);

/* .data */
// Next To SM Bridge Bottles
// Gets removed when player learns all SM Moves
ActorInfo D_80373DC0 = {
    0x1EE, ACTOR_3BA_UNKNOWN, 0,
    0, NULL, 
    func_80363500, actor_update_func_80326224, func_80325340,
    0, 0, 0.0f, 0
}; 

/* .code */
void func_80363440(void){
    func_8034DEB4(func_8034C528(0x1F1), -5000.0f);
}

void func_80363470(void){
    func_8034DEB4(func_8034C528(0x1F2), -5000.0f);
    func_8034DEB4(func_8034C528(0x1F3), 0.0f);
    func_80363440();
}

void func_803634BC(void){
    func_8034DEB4(func_8034C528(0x1F3), -5000.0f);
    func_8034DEB4(func_8034C528(0x1F2), 0.0f);
}

void func_80363500(Actor *this){
#if BKBT
    /* Despawn the broken bridge. Ensure it follows the way the game fixes the
     * bridge. Essential to allow progress without the full list of spiral mountain
     * moves */
    if(!this->volatile_initialized){
        func_80363440();
        marker_despawn(this->marker);
        this->volatile_initialized = TRUE;
        return;
    }
#endif
    if(!this->volatile_initialized){
        if(!chmole_learnedAllSpiralMountainAbilities()){
            func_803634BC();
        }
        else{
            func_80363440();
            marker_despawn(this->marker);
        }
        this->volatile_initialized = TRUE;
    }//L8036355C

    if(chmole_learnedAllSpiralMountainAbilities()){
        func_80363470();
        marker_despawn(this->marker);
    }
}
