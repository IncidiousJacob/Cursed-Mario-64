#ifndef SM64AP_COINSANITY_H
#define SM64AP_COINSANITY_H

// Coinsanity locked coin item received flags.
// Add future locked coin bools here.

bool sm64_have_locked_coin_6000 = false;
bool sm64_have_locked_coin_6001 = false;
bool sm64_have_locked_coin_6002 = false;
bool sm64_have_locked_coin_6003 = false;
bool sm64_have_locked_coin_6004 = false;
bool sm64_have_locked_coin_6005 = false;
bool sm64_have_locked_coin_6006 = false;
bool sm64_have_locked_coin_6007 = false;
bool sm64_have_locked_coin_6008 = false;

static void SM64AP_ResetCoinsanityFlags(void) {
    sm64_have_locked_coin_6000 = false;
    sm64_have_locked_coin_6001 = false;
    sm64_have_locked_coin_6002 = false;
    sm64_have_locked_coin_6003 = false;
    sm64_have_locked_coin_6004 = false;
    sm64_have_locked_coin_6005 = false;
    sm64_have_locked_coin_6006 = false;
    sm64_have_locked_coin_6007 = false;
    sm64_have_locked_coin_6008 = false;
}

#endif
