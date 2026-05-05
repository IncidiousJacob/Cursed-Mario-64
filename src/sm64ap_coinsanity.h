#ifndef SM64AP_COINSANITY_H
#define SM64AP_COINSANITY_H

// Coinsanity locked coin item received flags.
// Add future locked coin bools here.
//
// IMPORTANT:
// This header defines real bool variables, so only include it from sm64ap.cpp.

bool sm64_have_locked_coin_6000 = false;
bool sm64_have_locked_coin_6001 = false;
bool sm64_have_locked_coin_6002 = false;
bool sm64_have_locked_coin_6003 = false;
bool sm64_have_locked_coin_6004 = false;
bool sm64_have_locked_coin_6005 = false;
bool sm64_have_locked_coin_6006 = false;
bool sm64_have_locked_coin_6007 = false;
bool sm64_have_locked_coin_6008 = false;
bool sm64_have_locked_coin_6009 = false;
bool sm64_have_locked_coin_6010 = false;
bool sm64_have_locked_coin_6011 = false;
bool sm64_have_locked_coin_6012 = false;
bool sm64_have_locked_coin_6013 = false;
bool sm64_have_locked_coin_6014 = false;
bool sm64_have_locked_coin_6015 = false;
bool sm64_have_locked_coin_6016 = false;

typedef struct {
    int itemId;
    bool *flag;
} SM64APCoinsanityReturnItemEntry;

// This is the return/received item table.
// When AP returns item 6000, it sets sm64_have_locked_coin_6000 = true.
// When AP returns item 6001, it sets sm64_have_locked_coin_6001 = true.
// etc.
static SM64APCoinsanityReturnItemEntry sCoinsanityReturnItemTable[] = {
    { 6000, &sm64_have_locked_coin_6000 },
    { 6001, &sm64_have_locked_coin_6001 },
    { 6002, &sm64_have_locked_coin_6002 },
    { 6003, &sm64_have_locked_coin_6003 },
    { 6004, &sm64_have_locked_coin_6004 },
    { 6005, &sm64_have_locked_coin_6005 },
    { 6006, &sm64_have_locked_coin_6006 },
    { 6007, &sm64_have_locked_coin_6007 },
    { 6008, &sm64_have_locked_coin_6008 },
    { 6009, &sm64_have_locked_coin_6009 },
    { 6010, &sm64_have_locked_coin_6010 },
    { 6011, &sm64_have_locked_coin_6011 },
    { 6012, &sm64_have_locked_coin_6012 },
    { 6013, &sm64_have_locked_coin_6013 },
    { 6014, &sm64_have_locked_coin_6014 },
    { 6015, &sm64_have_locked_coin_6015 },
    { 6016, &sm64_have_locked_coin_6016 },
};

static bool SM64AP_SetCoinsanityReturnItemFlag(int64_t idx) {
    for (int i = 0; i < (int)(sizeof(sCoinsanityReturnItemTable) / sizeof(sCoinsanityReturnItemTable[0])); i++) {
        if (idx == sCoinsanityReturnItemTable[i].itemId) {
            *sCoinsanityReturnItemTable[i].flag = true;
            return true;
        }
    }

    return false;
}

static bool SM64AP_HaveCoinsanityReturnItemFlag(int itemId) {
    for (int i = 0; i < (int)(sizeof(sCoinsanityReturnItemTable) / sizeof(sCoinsanityReturnItemTable[0])); i++) {
        if (itemId == sCoinsanityReturnItemTable[i].itemId) {
            return *sCoinsanityReturnItemTable[i].flag;
        }
    }

    return false;
}

static void SM64AP_ResetCoinsanityFlags(void) {
    for (int i = 0; i < (int)(sizeof(sCoinsanityReturnItemTable) / sizeof(sCoinsanityReturnItemTable[0])); i++) {
        *sCoinsanityReturnItemTable[i].flag = false;
    }
}

#endif
