// DuckLab - a 3DS game that is meant to be cheated in.
//
// Every level hides its values in memory in a different way, so you can
// practise writing CTRPluginFramework (CTRPF) cheats on a game you own.
//
//   Level 1  Plain values      -> static addresses, a simple Write32
//   Level 2  Moving target     -> values live on the heap, find the pointer
//   Level 3  Locked vault      -> values are XOR-encoded and checksummed
//
// Values cheats are meant to touch are marked `volatile` so the compiler
// always re-reads them from memory, the same as a real game would.

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define VERSION        "0.1.0"
#define TITLE_ID_TEXT  "000400000DC1AB00"
#define SAVE_DIR       "sdmc:/3ds/DuckLab"
#define SAVE_FILE      SAVE_DIR "/progress.txt"

#define GOAL_COINS     9999
#define START_HP       100
#define MAX_HP         100
#define LEVEL_SECONDS  60
#define HEAL_COST      10
#define HEAL_AMOUNT    20

// ---------------------------------------------------------------------------
// Level 1: plain values at fixed (static) addresses
// ---------------------------------------------------------------------------
volatile u32 g_coins = 0;
volatile u32 g_hp    = START_HP;
volatile u32 g_timer = LEVEL_SECONDS;

// ---------------------------------------------------------------------------
// Level 2: a heap-allocated struct reached through a static pointer
// ---------------------------------------------------------------------------
#define DUCK_MAGIC 0x4B435544 // "DUCK" in memory

typedef struct {
    u32   magic;  // +0x00
    u32   hp;     // +0x04
    u32   maxHp;  // +0x08
    u32   coins;  // +0x0C
    float speed;  // +0x10  quack speed multiplier
    u32   timer;  // +0x14
} Duck;

volatile Duck *volatile g_duck = NULL;
static void *s_duckPadding = NULL; // random-size block so the heap address moves

// ---------------------------------------------------------------------------
// Level 3: XOR-encoded coins with a checksum, re-keyed every few seconds
// ---------------------------------------------------------------------------
#define VAULT_SALT    0xDECAFBAD
#define REKEY_FRAMES  180

typedef struct {
    u32 key;    // +0x00  changes every 3 seconds
    u32 enc;    // +0x04  coins ^ key
    u32 check;  // +0x08  enc ^ key ^ VAULT_SALT
    u32 hp;     // +0x0C  plain, as a warm-up
    u32 timer;  // +0x10
} Vault;

volatile Vault *volatile g_vault = NULL;
static void *s_vaultPadding = NULL;
static u32 s_caught = 0; // times the anti-cheat caught you

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
typedef enum { SCREEN_MENU, SCREEN_PLAY, SCREEN_WIN, SCREEN_LOSE } Screen;

static PrintConsole s_top, s_bottom;
static Screen s_screen = SCREEN_MENU;
static int  s_level = 1;          // 1..3
static int  s_menuCursor = 0;
static int  s_hintMode = 1;       // 0 off, 1 hints, 2 spoilers
static bool s_stars[4][2];        // [level][0 = goal, 1 = bonus]
static u32  s_frame = 0;
static u32  s_quackCooldown = 0;
static u32  s_nextAttack = 0;
static const char *s_loseReason = "";
static const char *s_message = "";
static u32  s_messageFrames = 0;

static void say(const char *msg) { s_message = msg; s_messageFrames = 120; }

// --- Value access, one way per level ---------------------------------------

static u32 vault_coins(void) { return g_vault->enc ^ g_vault->key; }

static void vault_store(u32 coins)
{
    g_vault->enc   = coins ^ g_vault->key;
    g_vault->check = g_vault->enc ^ g_vault->key ^ VAULT_SALT;
}

static u32 get_coins(void)
{
    if (s_level == 1) return g_coins;
    if (s_level == 2) return g_duck->coins;
    return vault_coins();
}

static void set_coins(u32 v)
{
    if (s_level == 1) g_coins = v;
    else if (s_level == 2) g_duck->coins = v;
    else vault_store(v);
}

static u32 get_hp(void)
{
    if (s_level == 1) return g_hp;
    if (s_level == 2) return g_duck->hp;
    return g_vault->hp;
}

static void set_hp(u32 v)
{
    if (s_level == 1) g_hp = v;
    else if (s_level == 2) g_duck->hp = v;
    else g_vault->hp = v;
}

static u32 get_max_hp(void) { return s_level == 2 ? g_duck->maxHp : MAX_HP; }

static volatile u32 *timer_ptr(void)
{
    if (s_level == 1) return &g_timer;
    if (s_level == 2) return &g_duck->timer;
    return &g_vault->timer;
}

// --- Save file (stars) -----------------------------------------------------

static void save_progress(void)
{
    mkdir("sdmc:/3ds", 0777);
    mkdir(SAVE_DIR, 0777);
    FILE *f = fopen(SAVE_FILE, "w");
    if (!f) return;
    for (int l = 1; l <= 3; l++)
        fprintf(f, "level%d=%d%d\n", l, s_stars[l][0], s_stars[l][1]);
    fclose(f);
}

static void load_progress(void)
{
    FILE *f = fopen(SAVE_FILE, "r");
    if (!f) return;
    int l, a, b;
    char line[64];
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "level%d=%1d%1d", &l, &a, &b) == 3 && l >= 1 && l <= 3) {
            s_stars[l][0] = a;
            s_stars[l][1] = b;
        }
    }
    fclose(f);
}

// --- Level setup -----------------------------------------------------------

static void free_level_memory(void)
{
    free((void *)g_duck);   g_duck = NULL;
    free(s_duckPadding);    s_duckPadding = NULL;
    free((void *)g_vault);  g_vault = NULL;
    free(s_vaultPadding);   s_vaultPadding = NULL;
}

static void start_level(int level)
{
    s_level = level;
    free_level_memory();

    if (level == 1) {
        g_coins = 0;
        g_hp = START_HP;
        g_timer = LEVEL_SECONDS;
    } else if (level == 2) {
        // Allocate a random amount first so the duck lands somewhere new.
        s_duckPadding = malloc(0x40 + (rand() % 0x4000));
        g_duck = calloc(1, sizeof(Duck));
        g_duck->magic = DUCK_MAGIC;
        g_duck->hp = START_HP;
        g_duck->maxHp = MAX_HP;
        g_duck->coins = 0;
        g_duck->speed = 1.0f;
        g_duck->timer = LEVEL_SECONDS;
    } else {
        s_vaultPadding = malloc(0x40 + (rand() % 0x4000));
        g_vault = calloc(1, sizeof(Vault));
        g_vault->key = (u32)rand() ^ ((u32)rand() << 16);
        vault_store(0);
        g_vault->hp = START_HP;
        g_vault->timer = LEVEL_SECONDS;
    }

    s_frame = 0;
    s_quackCooldown = 0;
    s_nextAttack = 90 + rand() % 90;
    s_message = "";
    s_messageFrames = 0;
    s_screen = SCREEN_PLAY;
    consoleSelect(&s_top);    consoleClear();
    consoleSelect(&s_bottom); consoleClear();
}

// --- Drawing ---------------------------------------------------------------

#define AT(row, col) "\x1b[" #row ";" #col "H"
#define CLR_LINE     "\x1b[K"

static const char *level_name(int l)
{
    static const char *names[] = { "", "Plain Values", "Moving Target", "Locked Vault" };
    return names[l];
}

static void draw_menu(void)
{
    consoleSelect(&s_top);
    printf(AT(2, 2) "\x1b[33;1mDuckLab\x1b[0m v" VERSION);
    printf(AT(3, 2) "The game you are SUPPOSED to cheat in.");
    printf(AT(5, 6) "   __");
    printf(AT(6, 6) " <(o )___");
    printf(AT(7, 6) "  ( ._> /");
    printf(AT(8, 6) "   `---'");

    for (int l = 1; l <= 3; l++) {
        bool sel = (s_menuCursor == l - 1);
        printf("\x1b[%d;2H" CLR_LINE, 11 + (l - 1) * 2);
        printf("%s Level %d: %-14s %s%s\x1b[0m",
               sel ? "\x1b[36;1m>" : " ", l, level_name(l),
               s_stars[l][0] ? "\x1b[33m*" : "\x1b[90m.",
               s_stars[l][1] ? "\x1b[33m*" : "\x1b[90m.");
    }

    printf(AT(19, 2) "\x1b[90mUp/Down choose   A play   START quit\x1b[0m");
    printf(AT(20, 2) "\x1b[90mY: Lab Panel hints (%s)\x1b[0m" CLR_LINE,
           s_hintMode == 0 ? "off" : s_hintMode == 1 ? "hints" : "spoilers");

    if (envIsHomebrew()) {
        printf(AT(23, 2) "\x1b[31;1mRunning as .3dsx!\x1b[0m");
        printf(AT(24, 2) "Plugins attach by title ID, so install");
        printf(AT(25, 2) "the .cia to use your CTRPF plugin.");
    }

    consoleSelect(&s_bottom);
    printf(AT(2, 2) "\x1b[33;1mHow to use DuckLab\x1b[0m");
    printf(AT(4, 2) "1. Pick a level.");
    printf(AT(5, 2) "2. Each goal is impossible to");
    printf(AT(6, 2) "   reach by playing normally.");
    printf(AT(7, 2) "3. Write a CTRPF plugin that edits");
    printf(AT(8, 2) "   the game's memory to win.");
    printf(AT(10, 2) "Plugin folder for this game:");
    printf(AT(11, 2) "\x1b[36m/luma/plugins/" TITLE_ID_TEXT "/\x1b[0m");
    printf(AT(13, 2) "Lessons: see the lessons/ folder");
    printf(AT(14, 2) "in the DuckLab repo.");
    printf(AT(28, 2) "\x1b[90mOnly cheat in offline games. Be cool.\x1b[0m");
}

static void draw_lab_panel(void)
{
    consoleSelect(&s_bottom);
    printf(AT(1, 1) "\x1b[33;1m== Lab Panel: Level %d ==\x1b[0m" CLR_LINE, s_level);

    if (s_hintMode == 0) {
        printf(AT(3, 2) "Hints are off. You're on your own!" CLR_LINE);
        return;
    }

    if (s_level == 1) {
        printf(AT(3, 2) "Values are stored as plain 32-bit");
        printf(AT(4, 2) "numbers at addresses that never");
        printf(AT(5, 2) "move. Search for your coin count,");
        printf(AT(6, 2) "quack, search again, repeat.");
        if (s_hintMode == 2) {
            printf(AT(8, 2) "\x1b[36mSpoilers:\x1b[0m");
            printf(AT(9, 2) "coins  @ 0x%08lX = %lu" CLR_LINE, (u32)&g_coins, g_coins);
            printf(AT(10, 2) "hp     @ 0x%08lX = %lu" CLR_LINE, (u32)&g_hp, g_hp);
            printf(AT(11, 2) "timer  @ 0x%08lX = %lu" CLR_LINE, (u32)&g_timer, g_timer);
        }
    } else if (s_level == 2) {
        printf(AT(3, 2) "The duck is rebuilt in a new spot");
        printf(AT(4, 2) "every time the level starts. Find");
        printf(AT(5, 2) "the pointer that never moves, then");
        printf(AT(6, 2) "add an offset.");
        printf(AT(7, 2) "Speed is a float, not an integer.");
        if (s_hintMode == 2) {
            printf(AT(9, 2) "\x1b[36mSpoilers:\x1b[0m");
            printf(AT(10, 2) "pointer @ 0x%08lX" CLR_LINE, (u32)&g_duck);
            printf(AT(11, 2) "  -> duck @ 0x%08lX" CLR_LINE, (u32)g_duck);
            printf(AT(12, 2) "hp +0x04  coins +0x0C" CLR_LINE);
            printf(AT(13, 2) "speed +0x10 (float)  timer +0x14" CLR_LINE);
        }
    } else {
        printf(AT(3, 2) "Coins are scrambled with a key");
        printf(AT(4, 2) "that changes every 3 seconds, and");
        printf(AT(5, 2) "a checksum catches lazy edits.");
        printf(AT(6, 2) "Times caught: %lu" CLR_LINE, s_caught);
        if (s_hintMode == 2) {
            printf(AT(8, 2) "\x1b[36mSpoilers:\x1b[0m");
            printf(AT(9, 2) "pointer @ 0x%08lX" CLR_LINE, (u32)&g_vault);
            printf(AT(10, 2) "  -> vault @ 0x%08lX" CLR_LINE, (u32)g_vault);
            printf(AT(11, 2) "key +0x00  enc +0x04  check +0x08" CLR_LINE);
            printf(AT(12, 2) "enc   = coins ^ key" CLR_LINE);
            printf(AT(13, 2) "check = enc ^ key ^ 0x%08X" CLR_LINE, VAULT_SALT);
        }
    }

    printf(AT(16, 2) "\x1b[33mGoal:\x1b[0m  %lu+ coins  %s" CLR_LINE,
           (u32)GOAL_COINS, s_stars[s_level][0] ? "\x1b[33m*\x1b[0m" : "");
    if (s_level == 1)
        printf(AT(17, 2) "\x1b[33mBonus:\x1b[0m 999+ HP (max is 100!) %s" CLR_LINE,
               s_stars[1][1] ? "\x1b[33m*\x1b[0m" : "");
    else if (s_level == 2)
        printf(AT(17, 2) "\x1b[33mBonus:\x1b[0m quack speed 5.0+ %s" CLR_LINE,
               s_stars[2][1] ? "\x1b[33m*\x1b[0m" : "");
    else
        printf(AT(17, 2) "\x1b[33mBonus:\x1b[0m win without getting caught %s" CLR_LINE,
               s_stars[3][1] ? "\x1b[33m*\x1b[0m" : "");

    printf(AT(28, 2) "\x1b[90mY: hints   SELECT: plugin menu\x1b[0m" CLR_LINE);
}

static void draw_play(void)
{
    consoleSelect(&s_top);
    printf(AT(1, 1) "\x1b[33;1mLevel %d: %s\x1b[0m" CLR_LINE, s_level, level_name(s_level));
    printf(AT(3, 2) "Coins: \x1b[33;1m%-10lu\x1b[0m HP: \x1b[32;1m%3lu/%-3lu\x1b[0m  Time: %2lus" CLR_LINE,
           get_coins(), get_hp(), get_max_hp(), *timer_ptr());
    if (s_level == 2)
        printf(AT(4, 2) "Quack speed: %.2fx" CLR_LINE, (double)g_duck->speed);

    bool quacking = s_quackCooldown > 0;
    printf(AT(7, 6) "   __");
    printf(AT(8, 6) " <(%c )___   %s", quacking ? '^' : 'o', quacking ? "\x1b[33mQUACK!\x1b[0m" : "      ");
    printf(AT(9, 6) "  ( ._> /");
    printf(AT(10, 6) "   `---'");

    bool angry = s_nextAttack < 20;
    printf(AT(7, 30) "\x1b[%dm  .-\"\"-.  ", angry ? 31 : 35);
    printf(AT(8, 30) " / %c  %c \\ ", angry ? '>' : 'O', angry ? '<' : 'O');
    printf(AT(9, 30) " \\  ~~  / ");
    printf(AT(10, 30) "  '----'\x1b[0m");
    printf(AT(11, 29) "\x1b[90mPond Monster\x1b[0m");

    printf(AT(14, 2) "A: quack (+1 coin)   B: heal (%d coins)", HEAL_COST);
    printf(AT(15, 2) "START: back to menu");
    printf(AT(18, 2) "%s" CLR_LINE, s_messageFrames ? s_message : "");

    draw_lab_panel();
}

static void draw_result(void)
{
    consoleSelect(&s_top);
    if (s_screen == SCREEN_WIN) {
        printf(AT(8, 4) "\x1b[33;1m*** LEVEL %d HACKED! ***\x1b[0m", s_level);
        printf(AT(10, 4) "You cheated your way to %lu coins.", get_coins());
        printf(AT(11, 4) "That's the whole point. Nice work!");
    } else {
        printf(AT(8, 4) "\x1b[31;1mLevel failed\x1b[0m");
        printf(AT(10, 4) "%s", s_loseReason);
        printf(AT(11, 4) "(Tip: a cheat can stop this...)");
    }
    printf(AT(14, 4) "A: retry   B: menu");
}

// --- Game logic ------------------------------------------------------------

static void finish(Screen result, const char *reason)
{
    s_screen = result;
    s_loseReason = reason;
    consoleSelect(&s_top); consoleClear();
}

static void update_play(u32 down)
{
    if (down & KEY_START) {
        free_level_memory();
        s_screen = SCREEN_MENU;
        consoleSelect(&s_top);    consoleClear();
        consoleSelect(&s_bottom); consoleClear();
        return;
    }

    s_frame++;
    if (s_messageFrames) s_messageFrames--;
    if (s_quackCooldown) s_quackCooldown--;

    // Level 3: anti-cheat check, then re-key.
    if (s_level == 3) {
        if (g_vault->check != (g_vault->enc ^ g_vault->key ^ VAULT_SALT)) {
            s_caught++;
            vault_store(0);
            say("\x1b[31;1mANTI-CHEAT: checksum wrong! Coins reset.\x1b[0m");
        }
        if (s_frame % REKEY_FRAMES == 0) {
            u32 coins = vault_coins();
            g_vault->key = (u32)rand() ^ ((u32)rand() << 16);
            vault_store(coins);
        }
    }

    // Quack for coins.
    if ((down & KEY_A) && s_quackCooldown == 0) {
        set_coins(get_coins() + 1);
        u32 cooldown = 12;
        if (s_level == 2) {
            float speed = g_duck->speed;
            if (speed < 0.1f) speed = 0.1f;
            cooldown = (u32)(24.0f / speed);
        }
        s_quackCooldown = cooldown;
    }

    // Buy a heal.
    if (down & KEY_B) {
        if (get_coins() >= HEAL_COST) {
            set_coins(get_coins() - HEAL_COST);
            u32 hp = get_hp() + HEAL_AMOUNT;
            if (hp > get_max_hp()) hp = get_max_hp();
            set_hp(hp);
            say("Healed! Tasty bread.");
        } else {
            say("Not enough coins for bread.");
        }
    }

    // Pond monster attacks.
    if (s_nextAttack > 0) s_nextAttack--;
    if (s_nextAttack == 0) {
        s32 hp = (s32)get_hp() - (3 + rand() % 10);
        set_hp(hp < 0 ? 0 : (u32)hp);
        s_nextAttack = 90 + rand() % 90;
        say("\x1b[31mThe Pond Monster splashed you!\x1b[0m");
    }

    // Timer.
    volatile u32 *timer = timer_ptr();
    if (s_frame % 60 == 0 && *timer > 0) (*timer)--;

    // Bonus stars can be earned mid-level.
    if (s_level == 1 && g_hp >= 999 && !s_stars[1][1]) {
        s_stars[1][1] = true; save_progress(); say("\x1b[33mBONUS: 999 HP! Unkillable duck.\x1b[0m");
    }
    if (s_level == 2 && g_duck->speed >= 5.0f && !s_stars[2][1]) {
        s_stars[2][1] = true; save_progress(); say("\x1b[33mBONUS: Turbo quack unlocked!\x1b[0m");
    }

    // Win / lose.
    if (get_coins() >= GOAL_COINS) {
        s_stars[s_level][0] = true;
        if (s_level == 3 && s_caught == 0) s_stars[3][1] = true;
        save_progress();
        finish(SCREEN_WIN, "");
    } else if (get_hp() == 0) {
        finish(SCREEN_LOSE, "The Pond Monster got you.");
    } else if (*timer == 0) {
        finish(SCREEN_LOSE, "Out of time.");
    }
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, &s_top);
    consoleInit(GFX_BOTTOM, &s_bottom);
    srand((unsigned)osGetTime());
    load_progress();

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();

        if (down & KEY_Y) {
            s_hintMode = (s_hintMode + 1) % 3;
            consoleSelect(&s_bottom); consoleClear();
        }

        if (s_screen == SCREEN_MENU) {
            if (down & KEY_START) break;
            if (down & KEY_DOWN) s_menuCursor = (s_menuCursor + 1) % 3;
            if (down & KEY_UP)   s_menuCursor = (s_menuCursor + 2) % 3;
            if (down & KEY_A) {
                s_caught = 0;
                start_level(s_menuCursor + 1);
            } else {
                draw_menu();
            }
        } else if (s_screen == SCREEN_PLAY) {
            update_play(down);
            if (s_screen == SCREEN_PLAY) draw_play();
        } else {
            if (down & KEY_A) {
                s_caught = 0;
                start_level(s_level);
            } else if (down & KEY_B) {
                free_level_memory();
                s_screen = SCREEN_MENU;
                consoleSelect(&s_top);    consoleClear();
                consoleSelect(&s_bottom); consoleClear();
            } else {
                draw_result();
            }
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    free_level_memory();
    gfxExit();
    return 0;
}
