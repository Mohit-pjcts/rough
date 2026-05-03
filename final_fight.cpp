/*
 * final_fight.cpp
 * TermiCraft — Dragon Boss Fight  (ncurses renderer)
 *
 * Layout (adapts fully to terminal size):
 *
 *   row 0          ╔════════ top border ════════╗
 *   row 1          ║  TITLE  │  PHASE  │  SCORE  ║
 *   row 2          ║  DRAGON HP bar              ║
 *   row 3          ╠═════════════════════════════╣
 *   rows 4-15      ║   dragon art (12 rows)      ║
 *   rows 16..H-6   ║   combat zone               ║
 *   row  H-5       ║   [/\=====/\]  (player)     ║
 *   row  H-4       ║   PLAYER HP bar             ║
 *   row  H-3       ╠═════════════════════════════╣
 *   row  H-2       ║   controls hint             ║
 *   row  H-1       ╚═════════════════════════════╝
 *
 * ncurses is initialised inside runBossFight() and torn down before
 * returning, so the surrounding ANSI game is unaffected.
 *
 * No "using namespace" anywhere per project rules.
 */

#include "final_fight.h"
#include "score.h"
#include "fileio.h"
#include "colors.h"

// ncurses must come BEFORE any header that declares getch() with a different signature
#include <ncurses.h>
#include <algorithm>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <locale.h>

// ── Color pair IDs ────────────────────────────────────────────────────────
#define CP_BORDER   1
#define CP_TITLE    2
#define CP_HUD      3
#define CP_DRAG_P1  4
#define CP_DRAG_P2  5
#define CP_DRAG_P3  6
#define CP_PLAYER   7
#define CP_FIRE     8
#define CP_ARROW    9
#define CP_HP_G    10
#define CP_HP_Y    11
#define CP_HP_R    12
#define CP_DIM     13
#define CP_WARN    14

static void initFightColors() {
    start_color();
    use_default_colors();
    init_pair(CP_BORDER,  COLOR_CYAN,    -1);
    init_pair(CP_TITLE,   COLOR_WHITE,   -1);
    init_pair(CP_HUD,     COLOR_YELLOW,  -1);
    init_pair(CP_DRAG_P1, COLOR_GREEN,   -1);
    init_pair(CP_DRAG_P2, COLOR_YELLOW,  -1);
    init_pair(CP_DRAG_P3, COLOR_RED,     -1);
    init_pair(CP_PLAYER,  COLOR_CYAN,    -1);
    init_pair(CP_FIRE,    COLOR_RED,     -1);
    init_pair(CP_ARROW,   COLOR_WHITE,   -1);
    init_pair(CP_HP_G,    COLOR_GREEN,   -1);
    init_pair(CP_HP_Y,    COLOR_YELLOW,  -1);
    init_pair(CP_HP_R,    COLOR_RED,     -1);
    init_pair(CP_DIM,     COLOR_WHITE,   -1);
    init_pair(CP_WARN,    COLOR_RED,     -1);
}

// ── Dragon ASCII art — 3 phase variants ──────────────────────────────────
static const char* DRAGON_P1[FF_DRAGON_ROWS] = {
    "        ,     \\    /      ,        ",
    "       / \\    )\\__/(     / \\       ",
    "      /   \\  (_\\  /_)   /   \\      ",
    " ____/_____\\__\\@ @/___/_____\\____  ",
    "|             |\\../|              |",
    "|              \\VV/               |",
    "|        ----------------         |",
    "|_________________________________|",
    " |    /\\ /      \\\\       \\ /\\    |",
    " |  /   V        ))       V   \\  |",
    " |/     `       //        '     \\|",
    " `              V                `"
};
static const char* DRAGON_P2[FF_DRAGON_ROWS] = {
    "        ,     \\    /      ,        ",
    "       / \\    )\\__/(     / \\       ",
    "      /   \\  (_\\  /_)   /   \\      ",
    " ____/_____\\__\\x @/___/_____\\____  ",
    "|             |\\../|              |",
    "|              \\VV/               |",
    "|        ----------------         |",
    "|_________________________________|",
    " |    /\\ /      \\\\       \\ /\\    |",
    " |  /   V        ))       V   \\  |",
    " |/     `       //        '     \\|",
    " `              V                `"
};
static const char* DRAGON_P3[FF_DRAGON_ROWS] = {
    "        ,     \\    /      ,        ",
    "       / \\    )\\__/(     / \\       ",
    "      /   \\  (_\\  /_)   /   \\      ",
    " ____/_____\\__\\x x/___/_____\\____  ",
    "|             |\\../|              |",
    "|              \\XX/               |",
    "|        ----------------         |",
    "|_________________________________|",
    " |    /\\ /      \\\\       \\ /\\    |",
    " |  /   V        ))       V   \\  |",
    " |/     `       //        '     \\|",
    " `              V                `"
};
static const char** DRAGON_ART[3] = {
    (const char**)DRAGON_P1,
    (const char**)DRAGON_P2,
    (const char**)DRAGON_P3
};

// ── Layout globals (computed each fight from getmaxyx) ────────────────────
static int NC_ROWS, NC_COLS;
static int ROW_HUD, ROW_DRAG_HP, ROW_TOP_SEP;
static int ROW_DRAG_START, ROW_DRAG_END;
static int ROW_PLAYER, ROW_PLR_HP, ROW_BOT_SEP, ROW_CTRL;
static int DRAG_MIN_X, DRAG_MAX_X;

static void computeLayout() {
    getmaxyx(stdscr, NC_ROWS, NC_COLS);
    ROW_HUD        = 1;
    ROW_DRAG_HP    = 2;
    ROW_TOP_SEP    = 3;
    ROW_DRAG_START = 4;
    ROW_DRAG_END   = ROW_DRAG_START + FF_DRAGON_ROWS - 1;
    ROW_CTRL       = NC_ROWS - 2;
    ROW_BOT_SEP    = NC_ROWS - 3;
    ROW_PLR_HP     = NC_ROWS - 4;
    ROW_PLAYER     = NC_ROWS - 5;
    DRAG_MIN_X     = 1;
    DRAG_MAX_X     = NC_COLS - FF_DRAGON_COLS - 3;
    if (DRAG_MAX_X < DRAG_MIN_X + 4) DRAG_MAX_X = DRAG_MIN_X + 4;
}

// ── Drawing primitives ────────────────────────────────────────────────────

static void drawFrame() {
    attron(COLOR_PAIR(CP_BORDER) | A_BOLD);
    box(stdscr, 0, 0);
    mvhline(ROW_TOP_SEP, 1, ACS_HLINE, NC_COLS - 2);
    mvaddch(ROW_TOP_SEP, 0,          ACS_LTEE);
    mvaddch(ROW_TOP_SEP, NC_COLS-1,  ACS_RTEE);
    mvhline(ROW_BOT_SEP, 1, ACS_HLINE, NC_COLS - 2);
    mvaddch(ROW_BOT_SEP, 0,          ACS_LTEE);
    mvaddch(ROW_BOT_SEP, NC_COLS-1,  ACS_RTEE);
    attroff(COLOR_PAIR(CP_BORDER) | A_BOLD);
}

static void drawBar(int row, int col, int width, int cur, int maxV) {
    if (maxV <= 0) maxV = 1;
    if (cur < 0)  cur  = 0;
    int filled = cur * width / maxV;
    int pct    = cur * 100 / maxV;
    int cpair  = (pct > 50) ? CP_HP_G : (pct > 20) ? CP_HP_Y : CP_HP_R;
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            attron(COLOR_PAIR(cpair) | A_BOLD | A_REVERSE);
            mvaddch(row, col + i, ' ');
            attroff(COLOR_PAIR(cpair) | A_BOLD | A_REVERSE);
        } else {
            attron(COLOR_PAIR(CP_DIM) | A_DIM);
            mvaddch(row, col + i, '-');
            attroff(COLOR_PAIR(CP_DIM) | A_DIM);
        }
    }
}

// ── HUD ───────────────────────────────────────────────────────────────────

static void renderHUD(int score, const Dragon& dragon) {
    // Left: title
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvprintw(ROW_HUD, 2, " TERMICRAFT: THE LAIR ");
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

    // Centre: phase
    const char* phaseStr;
    int phaseCp;
    if      (dragon.phase == FF_PHASE1) { phaseStr = "[ PHASE I ]";   phaseCp = CP_DRAG_P1; }
    else if (dragon.phase == FF_PHASE2) { phaseStr = "[ PHASE II ]";  phaseCp = CP_DRAG_P2; }
    else                                { phaseStr = "[ PHASE III ]"; phaseCp = CP_DRAG_P3; }
    int midCol = (NC_COLS - (int)strlen(phaseStr)) / 2;
    attron(COLOR_PAIR(phaseCp) | A_BOLD);
    mvprintw(ROW_HUD, midCol, "%s", phaseStr);
    attroff(COLOR_PAIR(phaseCp) | A_BOLD);

    // Right: score
    char scoreBuf[24];
    std::snprintf(scoreBuf, sizeof(scoreBuf), "SCORE: %06d ", score);
    attron(COLOR_PAIR(CP_HUD) | A_BOLD);
    mvprintw(ROW_HUD, NC_COLS - (int)strlen(scoreBuf) - 1, "%s", scoreBuf);
    attroff(COLOR_PAIR(CP_HUD) | A_BOLD);

    // Dragon HP bar
    int barW = std::max(8, NC_COLS / 2 - 16);
    attron(COLOR_PAIR(CP_HUD) | A_BOLD);
    mvprintw(ROW_DRAG_HP, 2, "DRAGON");
    attroff(COLOR_PAIR(CP_HUD) | A_BOLD);
    drawBar(ROW_DRAG_HP, 9, barW, dragon.hp, dragon.maxHp);
    int pct = (dragon.maxHp > 0) ? (dragon.hp * 100 / dragon.maxHp) : 0;
    attron(COLOR_PAIR(CP_HUD));
    mvprintw(ROW_DRAG_HP, 9 + barW + 1, " %3d%%  HP: %d/%d", pct, dragon.hp, dragon.maxHp);
    attroff(COLOR_PAIR(CP_HUD));
}

// ── Dragon ────────────────────────────────────────────────────────────────

static void renderDragon(const Dragon& dragon, int tick, bool hitFlash) {
    const char** art = DRAGON_ART[dragon.phase - 1];
    int cpair = (dragon.phase == FF_PHASE1) ? CP_DRAG_P1 :
                (dragon.phase == FF_PHASE2) ? CP_DRAG_P2 : CP_DRAG_P3;
    int attrs = A_BOLD;
    if (dragon.phase == FF_PHASE3 && (tick / 2) % 2 == 0) attrs |= A_BLINK;

    for (int r = 0; r < FF_DRAGON_ROWS; r++) {
        int srow = ROW_DRAG_START + r;
        if (srow >= ROW_PLAYER - 2) break;
        int scol = 1 + dragon.x;
        if (hitFlash) attron(A_REVERSE | A_BOLD);
        else          attron(COLOR_PAIR(cpair) | attrs);
        const char* line = art[r];
        for (int c = 0; line[c] && scol + c < NC_COLS - 1; c++)
            mvaddch(srow, scol + c, (unsigned char)line[c]);
        if (hitFlash) attroff(A_REVERSE | A_BOLD);
        else          attroff(COLOR_PAIR(cpair) | attrs);
    }
}

// ── Projectiles ───────────────────────────────────────────────────────────

static void renderProjectiles(const Fireball* fbs, const Arrow* arrows) {
    attron(COLOR_PAIR(CP_FIRE) | A_BOLD);
    for (int i = 0; i < FF_MAX_FIREBALLS; i++) {
        if (!fbs[i].active) continue;
        if (fbs[i].y <= ROW_TOP_SEP || fbs[i].y >= ROW_PLAYER) continue;
        if (fbs[i].x > 0 && fbs[i].x < NC_COLS - 2)
            mvaddch(fbs[i].y, 1 + fbs[i].x, '*');
    }
    attroff(COLOR_PAIR(CP_FIRE) | A_BOLD);

    attron(COLOR_PAIR(CP_ARROW) | A_BOLD);
    for (int i = 0; i < FF_MAX_ARROWS; i++) {
        if (!arrows[i].active) continue;
        if (arrows[i].y <= ROW_TOP_SEP || arrows[i].y >= ROW_PLAYER) continue;
        if (arrows[i].x > 0 && arrows[i].x < NC_COLS - 2)
            mvaddch(arrows[i].y, 1 + arrows[i].x, '^');
    }
    attroff(COLOR_PAIR(CP_ARROW) | A_BOLD);
}

// ── Player ────────────────────────────────────────────────────────────────

static const char* getArmorName(MaterialTier armor) {
    switch (armor) {
        case MATERIAL_STONE:   return "Stone";
        case MATERIAL_IRON:    return "Iron";
        case MATERIAL_GOLD:    return "Gold";
        case MATERIAL_DIAMOND: return "Diamond";
        default:               return "None";
    }
}

static void renderPlayer(int playerX, int playerHp, int playerMaxHp, MaterialTier armor) {
    const char* sprite = "[/\\=====/\\]";
    int slen = (int)strlen(sprite);
    int scol = 1 + playerX - slen / 2;
    if (scol < 1)             scol = 1;
    if (scol + slen > NC_COLS - 1) scol = NC_COLS - 1 - slen;

    int pct   = (playerMaxHp > 0) ? (playerHp * 100 / playerMaxHp) : 0;
    int cpair = (pct > 50) ? CP_PLAYER : (pct > 20) ? CP_HP_Y : CP_HP_R;
    attron(COLOR_PAIR(cpair) | A_BOLD);
    mvprintw(ROW_PLAYER, scol, "%s", sprite);
    attroff(COLOR_PAIR(cpair) | A_BOLD);

    int barW = std::max(8, NC_COLS / 3);
    attron(COLOR_PAIR(CP_HUD) | A_BOLD);
    mvprintw(ROW_PLR_HP, 2, "PLAYER");
    attroff(COLOR_PAIR(CP_HUD) | A_BOLD);
    drawBar(ROW_PLR_HP, 9, barW, playerHp, playerMaxHp);
    attron(COLOR_PAIR(CP_HUD));
    mvprintw(ROW_PLR_HP, 9 + barW + 1, " %3d%%  [%s armor]", pct, getArmorName(armor));
    attroff(COLOR_PAIR(CP_HUD));
}

// ── Controls ──────────────────────────────────────────────────────────────

static void renderControls() {
    attron(COLOR_PAIR(CP_DIM) | A_DIM);
    mvprintw(ROW_CTRL, 2,
        "[A] move left    [D] move right    [SPACE] shoot    [Q] flee");
    attroff(COLOR_PAIR(CP_DIM) | A_DIM);
}

// ── Full frame ────────────────────────────────────────────────────────────

static void renderBanners(const Dragon& dragon) {
    // Phase / enrage announcement — flash for announceTicks frames
    if (dragon.announceTicks > 0) {
        bool blink = (dragon.announceTicks / 5) % 2 == 0;
        if (blink) {
            const char* msg = dragon.enraged      ? "  !! DRAGON ENRAGED !!  " :
                              (dragon.phase == FF_PHASE3) ? "  -- PHASE III --  "  :
                                                            "  -- PHASE II --  ";
            int cpair = dragon.enraged ? CP_WARN : CP_HUD;
            int msgRow = ROW_TOP_SEP + 1;
            int msgCol = (NC_COLS - (int)strlen(msg)) / 2;
            attron(COLOR_PAIR(cpair) | A_BOLD | A_REVERSE);
            mvprintw(msgRow, msgCol, "%s", msg);
            attroff(COLOR_PAIR(cpair) | A_BOLD | A_REVERSE);
        }
    }
}

static void renderOpening(int playerX, int playerHp, int playerMaxHp,
                          int score, const Dragon& dragon,
                          MaterialTier armor, int openingTicks, int roarDmg) {
    erase();
    drawFrame();
    renderHUD(score, dragon);

    int frame    = 50 - openingTicks;  // 0..49
    int combatTop = ROW_TOP_SEP + 1;
    int combatBot = ROW_PLAYER - 1;
    int centreRow = (combatTop + combatBot) / 2;
    int centreCol = NC_COLS / 2;

    // Expanding shockwave ring
    int radius = frame / 5;
    int maxR   = (combatBot - combatTop) / 2;
    if (radius > maxR) radius = maxR;

    if (radius > 0) {
        attron(COLOR_PAIR(CP_FIRE) | A_BOLD);
        int rTop  = centreRow - radius, rBot  = centreRow + radius;
        int cLeft = centreCol - radius * 2, cRight = centreCol + radius * 2;
        for (int c = cLeft; c <= cRight; c++) {
            if (rTop > ROW_TOP_SEP && rTop < ROW_PLAYER && c > 0 && c < NC_COLS-1)
                mvaddch(rTop, c, '*');
            if (rBot > ROW_TOP_SEP && rBot < ROW_PLAYER && c > 0 && c < NC_COLS-1)
                mvaddch(rBot, c, '*');
        }
        for (int r = rTop+1; r < rBot; r++) {
            if (r > ROW_TOP_SEP && r < ROW_PLAYER) {
                if (cLeft  > 0 && cLeft  < NC_COLS-1) mvaddch(r, cLeft,  '*');
                if (cRight > 0 && cRight < NC_COLS-1) mvaddch(r, cRight, '*');
            }
        }
        // Dense central cluster for first 10 frames
        if (frame < 10) {
            for (int dr = -1; dr <= 1; dr++)
                for (int dc = -2; dc <= 2; dc++) {
                    int rr = centreRow + dr, cc = centreCol + dc;
                    if (rr > ROW_TOP_SEP && rr < ROW_PLAYER && cc > 0 && cc < NC_COLS-1)
                        mvaddch(rr, cc, frame % 2 == 0 ? 'W' : '*');
                }
        }
        attroff(COLOR_PAIR(CP_FIRE) | A_BOLD);
    }

    // ROOAARRR message — blinks every 5 frames
    if ((frame / 5) % 2 == 0) {
        char roarMsg[64];
        std::snprintf(roarMsg, sizeof(roarMsg),
            "  ROOAARRR!  Dragon breathes fire!  -%d HP  ", roarDmg);
        int msgLen = (int)strlen(roarMsg);
        int msgRow = centreRow;
        int msgCol = std::max(1, (NC_COLS - msgLen) / 2);
        attron(COLOR_PAIR(CP_WARN) | A_BOLD | A_REVERSE);
        mvprintw(msgRow, msgCol, "%s", roarMsg);
        attroff(COLOR_PAIR(CP_WARN) | A_BOLD | A_REVERSE);

        const char* sub = "  BRACE FOR IMPACT  ";
        int subCol = std::max(1, (NC_COLS - (int)strlen(sub)) / 2);
        attron(COLOR_PAIR(CP_FIRE) | A_BOLD | A_BLINK);
        mvprintw(msgRow + 1, subCol, "%s", sub);
        attroff(COLOR_PAIR(CP_FIRE) | A_BOLD | A_BLINK);
    }

    renderPlayer(playerX, playerHp, playerMaxHp, armor);
    renderControls();
    attron(COLOR_PAIR(CP_DIM) | A_DIM);
    mvprintw(ROW_CTRL, 2, "[Q] flee");
    attroff(COLOR_PAIR(CP_DIM) | A_DIM);
    refresh();
}

static void renderFrame(const Dragon& dragon, const Fireball* fbs, const Arrow* arrows,
                        int playerX, int playerHp, int playerMaxHp,
                        int score, MaterialTier armor, int tick, bool hitFlash) {
    erase();
    drawFrame();
    renderHUD(score, dragon);
    renderDragon(dragon, tick, hitFlash);
    renderProjectiles(fbs, arrows);
    renderPlayer(playerX, playerHp, playerMaxHp, armor);
    renderControls();
    renderBanners(dragon);
    refresh();
}

// ── Intro screen ──────────────────────────────────────────────────────────

static void showIntro(const GameState& state) {
    erase();
    int artStart = (NC_ROWS / 2) - 9;
    if (artStart < 1) artStart = 1;
    int artWidth = (int)strlen(DRAGON_P1[0]);
    int artCol   = (NC_COLS - artWidth) / 2;
    if (artCol < 1) artCol = 1;

    attron(COLOR_PAIR(CP_DRAG_P3) | A_BOLD);
    for (int r = 0; r < FF_DRAGON_ROWS && artStart + r < NC_ROWS - 6; r++)
        mvprintw(artStart + r, artCol, "%s", DRAGON_P1[r]);
    attroff(COLOR_PAIR(CP_DRAG_P3) | A_BOLD);

    const char* title = "~ THE DRAGON CAVE ~";
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvprintw(artStart - 2, (NC_COLS - (int)strlen(title)) / 2, "%s", title);
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

    int loreRow = artStart + FF_DRAGON_ROWS + 1;
    const char* lore = "The cavern trembles. Two burning eyes open in the darkness...";
    attron(COLOR_PAIR(CP_HUD));
    mvprintw(loreRow, (NC_COLS - (int)strlen(lore)) / 2, "%s", lore);
    attroff(COLOR_PAIR(CP_HUD));

    char statBuf[80];
    std::snprintf(statBuf, sizeof(statBuf),
        "Armor: %s    HP: %d/%d    Score: %d",
        getArmorName(state.player.equipment.armor),
        state.player.health, state.player.maxHealth, state.score);
    attron(COLOR_PAIR(CP_PLAYER) | A_BOLD);
    mvprintw(loreRow + 2, (NC_COLS - (int)strlen(statBuf)) / 2, "%s", statBuf);
    attroff(COLOR_PAIR(CP_PLAYER) | A_BOLD);

    if (state.player.equipment.armor < MATERIAL_DIAMOND) {
        const char* warn = "DIAMOND ARMOR RECOMMENDED. GOOD LUCK SURVIVING.";
        attron(COLOR_PAIR(CP_WARN) | A_BOLD | A_REVERSE);
        mvprintw(loreRow + 5, (NC_COLS - (int)strlen(warn)) / 2, "%s", warn);
        attroff(COLOR_PAIR(CP_WARN) | A_BOLD | A_REVERSE);
    }

    const char* prompt = "Press any key to begin...";
    attron(COLOR_PAIR(CP_DIM) | A_DIM);
    mvprintw(NC_ROWS - 2, (NC_COLS - (int)strlen(prompt)) / 2, "%s", prompt);
    attroff(COLOR_PAIR(CP_DIM) | A_DIM);

    refresh();
    nodelay(stdscr, FALSE);
    getch();
    nodelay(stdscr, TRUE);
}

// ── Score breakdown (post-fight, ANSI — ncurses already torn down) ────────

static void showScoreBreakdown(bool won, int miningSnap, int p1h, int p2h, int p3h,
                                bool killBonus, int total, float mult) {
    // art(7) + gap(1) + table(~10) + gap(1) + prompt(1) = ~20
    clearAndCenterV(20);

    // Art widths (display cols): YOU WIN ~63, GAME OVER ~80
    std::string Aw = hpad(63);
    std::string Ag = hpad(80);
    // Table: ╔55═╗ = 57 display cols
    std::string Tb = hpad(57);

    if (won) {
        std::cout << "\033[1;32m"
            << Aw << "   ██╗   ██╗ ██████╗ ██╗   ██╗    ██╗    ██╗██╗███╗   ██╗██╗\n"
            << Aw << "   ╚██╗ ██╔╝██╔═══██╗██║   ██║    ██║    ██║██║████╗  ██║██║\n"
            << Aw << "    ╚████╔╝ ██║   ██║██║   ██║    ██║ █╗ ██║██║██╔██╗ ██║██║\n"
            << Aw << "     ╚██╔╝  ██║   ██║██║   ██║    ██║███╗██║██║██║╚██╗██║██║\n"
            << Aw << "      ██║   ╚██████╔╝╚██████╔╝    ╚███╔███╔╝██║██║ ╚████║██║\n"
            << Aw << "      ╚═╝    ╚═════╝  ╚═════╝      ╚══╝╚══╝ ╚═╝╚═╝  ╚═══╝╚═╝\n"
            "\033[0m\n";
    } else {
        std::cout << "\033[1;31m"
            << Ag << "   ██████╗  █████╗ ███╗   ███╗███████╗     ██████╗ ██╗   ██╗███████╗██████╗\n"
            << Ag << "  ██╔════╝ ██╔══██╗████╗ ████║██╔════╝    ██╔═══██╗██║   ██║██╔════╝██╔══██╗\n"
            << Ag << "  ██║  ███╗███████║██╔████╔██║█████╗      ██║   ██║██║   ██║█████╗  ██████╔╝\n"
            << Ag << "  ██║   ██║██╔══██║██║╚██╔╝██║██╔══╝      ██║   ██║╚██╗ ██╔╝██╔══╝  ██╔══██╗\n"
            << Ag << "  ╚██████╔╝██║  ██║██║ ╚═╝ ██║███████╗    ╚██████╔╝ ╚████╔╝ ███████╗██║  ██║\n"
            << Ag << "   ╚═════╝ ╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝     ╚═════╝   ╚═══╝  ╚══════╝╚═╝  ╚═╝\n"
            "\033[0m\n";
    }

    const int IW = 54;
    std::string Tp = hpad(IW + 2);

    auto hbar = [&](const char* lc, const char* rc) {
        std::cout << "\033[1;36m" << Tp << lc;
        for (int i = 0; i < IW; i++) std::cout << "\xe2\x95\x90";
        std::cout << rc << "\033[0m\n";
    };
    auto centerRow = [&](const char* text, const char* color) {
        int tlen = (int)strlen(text);
        int l = (IW - tlen) / 2, r = IW - tlen - l;
        std::cout << color << Tp << "\xe2\x95\x91"
                  << std::string(l, ' ') << text << std::string(r, ' ')
                  << "\xe2\x95\x91\033[0m\n";
    };
    char buf[80];
    auto dataRow = [&](const char* label, int val, const char* color) {
        std::snprintf(buf, sizeof(buf), "   %-35s %12d   ", label, val);
        std::string s(buf);
        while ((int)s.size() < IW) s += ' ';
        if ((int)s.size() > IW) s = s.substr(0, IW);
        std::cout << color << Tp << "\xe2\x95\x91" << s << "\xe2\x95\x91\033[0m\n";
    };

    hbar("\xe2\x95\x94", "\xe2\x95\x97");
    centerRow("SCORE BREAKDOWN", "\033[1;36m");
    hbar("\xe2\x95\xa0", "\xe2\x95\xa3");

    dataRow("Mining score (carried in):", miningSnap, "\033[1;36m");
    if (p1h > 0) {
        char lbl[50];
        std::snprintf(lbl, sizeof(lbl), "Phase I   hits: %3d x %2d =", p1h, FF_SCORE_HIT_P1);
        dataRow(lbl, (int)(p1h * FF_SCORE_HIT_P1 * mult), "\033[1;36m");
    }
    if (p2h > 0) {
        char lbl[50];
        std::snprintf(lbl, sizeof(lbl), "Phase II  hits: %3d x %2d =", p2h, FF_SCORE_HIT_P2);
        dataRow(lbl, (int)(p2h * FF_SCORE_HIT_P2 * mult), "\033[1;36m");
    }
    if (p3h > 0) {
        char lbl[50];
        std::snprintf(lbl, sizeof(lbl), "Phase III hits: %3d x %2d =", p3h, FF_SCORE_HIT_P3);
        dataRow(lbl, (int)(p3h * FF_SCORE_HIT_P3 * mult), "\033[1;36m");
    }
    if (killBonus)
        dataRow("Dragon kill bonus:", (int)(FF_SCORE_KILL * mult), "\033[1;36m");

    hbar("\xe2\x95\xa0", "\xe2\x95\xa3");
    dataRow("TOTAL SCORE:", total, "\033[1;33m");

    HighScore existing = getTopHighScore();
    if (total > existing.score)
        centerRow("** NEW HIGH SCORE! **", "\033[1;32m");

    hbar("\xe2\x95\x9a", "\xe2\x95\x9d");
    std::cout << "\n";

    {
        const char* prompt = "Press any key to continue...";
        std::cout << hpad((int)strlen(prompt)) << "\033[2m" << prompt << "\033[0m";
    }
    std::cout.flush();
    char dummy; read(STDIN_FILENO, &dummy, 1);
}

// ── Game logic helpers ────────────────────────────────────────────────────

BossConfig initBossConfig(Difficulty diff) {
    BossConfig cfg = {};
    switch (diff) {
        case DIFF_EASY:
            cfg.dragonHp = 50;  cfg.fireballDmg = 4;
            cfg.fireRateTicks = 35; cfg.dragonSpeed = 1;
            cfg.hasEnrage = false; cfg.spread3 = FF_SPREAD_P3;
            break;
        case DIFF_NORMAL:
            cfg.dragonHp = 80;  cfg.fireballDmg = 9;
            cfg.fireRateTicks = 18; cfg.dragonSpeed = 2;
            cfg.hasEnrage = false; cfg.spread3 = FF_SPREAD_P3;
            break;
        default:  // HARD
            cfg.dragonHp = 140; cfg.fireballDmg = 15;
            cfg.fireRateTicks = 12; cfg.dragonSpeed = 3;
            cfg.hasEnrage = true; cfg.spread3 = 8;
            break;
    }
    return cfg;
}

int calcFightHP(Difficulty diff, MaterialTier armor) {
    int base = (diff == DIFF_EASY) ? FF_BASE_HP_EASY :
               (diff == DIFF_HARD) ? FF_BASE_HP_HARD : FF_BASE_HP_NORMAL;
    int bonus = 0;
    switch (armor) {
        case MATERIAL_STONE:   bonus = FF_HP_BONUS_STONE;   break;
        case MATERIAL_IRON:    bonus = FF_HP_BONUS_IRON;    break;
        case MATERIAL_GOLD:    bonus = FF_HP_BONUS_GOLD;    break;
        case MATERIAL_DIAMOND: bonus = FF_HP_BONUS_DIAMOND; break;
        default: break;
    }
    return base + bonus;
}

int calcArmorDamage(MaterialTier armor) {
    switch (armor) {
        case MATERIAL_IRON:    return FF_DMG_IRON;
        case MATERIAL_GOLD:    return FF_DMG_GOLD;
        case MATERIAL_DIAMOND: return FF_DMG_DIAMOND;
        default:               return FF_DMG_DEFAULT;
    }
}

static Dragon initDragon(const BossConfig& cfg) {
    Dragon d = {};
    d.x = FF_DRAGON_COLS;
    d.y = ROW_DRAG_START;
    d.hp = cfg.dragonHp; d.maxHp = cfg.dragonHp;
    d.speed = cfg.dragonSpeed; d.direction = 1;
    d.phase = FF_PHASE1;
    d.enraged = false;
    d.announceTicks = 0;
    return d;
}

static void updatePhase(Dragon& dragon, int baseSpeed) {
    if (dragon.hp <= 0) return;
    int pct = dragon.hp * 100 / dragon.maxHp;
    int newPhase = (pct <= FF_PHASE3_PCT) ? FF_PHASE3 :
                   (pct <= FF_PHASE2_PCT) ? FF_PHASE2 : FF_PHASE1;
    if (newPhase > dragon.phase) {
        dragon.phase = newPhase;
        dragon.speed = baseSpeed + (dragon.phase - 1);
    }
}

static void spawnFireballs(Fireball* fbs, const Dragon& dragon, int spread3) {
    int cx     = dragon.x + FF_DRAGON_COLS / 2;
    int spawnY = ROW_DRAG_END + 1;
    int spawnX[3], spawnDX[3], cnt;

    if (dragon.phase == FF_PHASE1) {
        spawnX[0] = cx; spawnDX[0] = 0; cnt = 1;
    } else if (dragon.phase == FF_PHASE2) {
        spawnX[0] = cx - FF_SPREAD_P2; spawnDX[0] = -1;
        spawnX[1] = cx + FF_SPREAD_P2; spawnDX[1] =  1; cnt = 2;
    } else {
        spawnX[0] = cx - spread3; spawnDX[0] = -1;
        spawnX[1] = cx;           spawnDX[1] =  0;
        spawnX[2] = cx + spread3; spawnDX[2] =  1; cnt = 3;
    }
    int activated = 0;
    for (int i = 0; i < FF_MAX_FIREBALLS && activated < cnt; i++) {
        if (!fbs[i].active) {
            fbs[i] = {spawnX[activated], spawnY, true, spawnDX[activated]};
            activated++;
        }
    }
}

static void fireArrow(Arrow* arrows, int playerX) {
    for (int i = 0; i < FF_MAX_ARROWS; i++) {
        if (!arrows[i].active) {
            arrows[i] = {playerX, ROW_PLAYER - 1, true};
            return;
        }
    }
}

// ── runBossFight — main entry point ───────────────────────────────────────

bool runBossFight(GameState& state) {
    setlocale(LC_ALL, "");  // enable UTF-8 in ncurses
    // Init ncurses
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);
    initFightColors();
    computeLayout();

    // Minimum terminal size check
    if (NC_ROWS < 24 || NC_COLS < 60) {
        endwin();
        std::cout << "\n\033[1;31m  Terminal too small!\033[0m  Please resize to at least 60 cols x 24 rows.\n"
                  << "  Current: " << NC_COLS << " x " << NC_ROWS << "\n"
                  << "  Press any key...\n";
        std::cout.flush();
        char dummy; read(STDIN_FILENO, &dummy, 1);
        return false;
    }

    BossConfig config     = initBossConfig(state.difficulty);
    int        playerMaxHp = state.player.maxHealth;
    int        playerHp    = state.player.health;
    int        arrowDmg    = calcArmorDamage(state.player.equipment.armor);
    int        miningSnap  = state.score;
    int        p1h = 0, p2h = 0, p3h = 0;
    bool       killBonus   = false;
    bool       won         = false;

    // Armor damage reduction (%) for incoming fireballs
    int armorPct = 0;
    switch (state.player.equipment.armor) {
        case MATERIAL_STONE:   armorPct = 10; break;
        case MATERIAL_IRON:    armorPct = 20; break;
        case MATERIAL_GOLD:    armorPct = 30; break;
        case MATERIAL_DIAMOND: armorPct = 45; break;
        default: break;
    }

    Dragon   dragon = initDragon(config);
    Fireball fbs[FF_MAX_FIREBALLS];
    Arrow    arrows[FF_MAX_ARROWS];
    for (int i = 0; i < FF_MAX_FIREBALLS; i++) fbs[i]    = {0,0,false,0};
    for (int i = 0; i < FF_MAX_ARROWS;    i++) arrows[i] = {0,0,false};

    int playerX   = NC_COLS / 2;
    const int HALF    = 6;
    const int PLR_MIN = 1 + HALF + 1;
    int       PLR_MAX = NC_COLS - 2 - HALF - 1;

    // Entry roar — calculated here, applied visibly on tick 1
    int roarDmg = (state.difficulty == DIFF_EASY) ? 20 :
                  (state.difficulty == DIFF_HARD) ? 45 : 30;
    roarDmg = roarDmg * (100 - armorPct) / 100;
    if (roarDmg < 5) roarDmg = 5;

    int  fireballTick    = -20;  // 20-tick grace after opening before first volley
    int  currentFireRate = config.fireRateTicks;
    int  tick            = 0;
    int  flashTicks      = 0;
    int  openingTicks    = 50;   // locked dramatic entry sequence
    bool running         = true;

    showIntro(state);

    while (running) {
        tick++;

        // Apply roar damage on the very first tick (visible in opening sequence)
        if (tick == 1) {
            playerHp -= roarDmg;
            if (playerHp < 1) playerHp = 1;
        }

        // ── Opening sequence: 50 ticks of locked fire-blast animation ─────────
        if (openingTicks > 0) {
            openingTicks--;
            int ch;
            while ((ch = getch()) != ERR)
                if (ch == 'q' || ch == 'Q') { running = false; won = false; }
            renderOpening(playerX, playerHp, playerMaxHp, state.score, dragon,
                          state.player.equipment.armor, openingTicks, roarDmg);
            usleep(FF_TICK_US);
            continue;
        }

        // Drain the full key buffer so move + shoot can happen in the same frame
        {
            bool wantLeft = false, wantRight = false, wantShoot = false;
            int ch;
            while ((ch = getch()) != ERR) {
                if      (ch == 'a' || ch == 'A') wantLeft  = true;
                else if (ch == 'd' || ch == 'D') wantRight = true;
                else if (ch == ' ')              wantShoot = true;
                else if (ch == 'q' || ch == 'Q') { running = false; won = false; break; }
                else if (ch == KEY_RESIZE)       { computeLayout(); PLR_MAX = NC_COLS - 2 - HALF - 1; }
            }
            // Apply movement — allow both shoot + move in the same tick
            if (wantLeft)  playerX = std::max(PLR_MIN, playerX - 4);
            if (wantRight) playerX = std::min(PLR_MAX, playerX + 4);
            if (wantShoot) fireArrow(arrows, playerX);
        }

        // Move dragon
        dragon.x += dragon.speed * dragon.direction;
        if      (dragon.x <= DRAG_MIN_X) { dragon.x = DRAG_MIN_X; dragon.direction =  1; }
        else if (dragon.x >= DRAG_MAX_X) { dragon.x = DRAG_MAX_X; dragon.direction = -1; }

        // Spawn fireballs
        if (++fireballTick >= currentFireRate) {
            fireballTick = 0;
            spawnFireballs(fbs, dragon, config.spread3);
        }

        // Move fireballs
        for (int i = 0; i < FF_MAX_FIREBALLS; i++) {
            if (!fbs[i].active) continue;
            fbs[i].y++;
            fbs[i].x += fbs[i].dx;
            if (fbs[i].y > ROW_PLAYER || fbs[i].x <= 0 || fbs[i].x >= NC_COLS - 2)
                fbs[i].active = false;
        }

        // Move arrows
        for (int i = 0; i < FF_MAX_ARROWS; i++) {
            if (!arrows[i].active) continue;
            arrows[i].y--;
            if (arrows[i].y < ROW_TOP_SEP) arrows[i].active = false;
        }

        // Arrow-dragon collision
        bool hitFlash = (flashTicks > 0);
        if (flashTicks > 0) flashTicks--;
        for (int i = 0; i < FF_MAX_ARROWS; i++) {
            if (!arrows[i].active) continue;
            bool hitRow = (arrows[i].y >= ROW_DRAG_START && arrows[i].y <= ROW_DRAG_END);
            bool hitCol = (arrows[i].x >= dragon.x && arrows[i].x < dragon.x + FF_DRAGON_COLS);
            if (hitRow && hitCol) {
                arrows[i].active = false;
                dragon.hp -= arrowDmg;
                if (dragon.hp < 0) dragon.hp = 0;
                int pts = (dragon.phase == FF_PHASE1) ? FF_SCORE_HIT_P1 :
                          (dragon.phase == FF_PHASE2) ? FF_SCORE_HIT_P2 : FF_SCORE_HIT_P3;
                addScore(state, pts);
                if      (dragon.phase == FF_PHASE1) p1h++;
                else if (dragon.phase == FF_PHASE2) p2h++;
                else                                p3h++;
                flashTicks = 4;
                hitFlash   = true;
            }
        }

        // Fireball-player collision
        int pLeft = playerX - HALF, pRight = playerX + HALF;
        for (int i = 0; i < FF_MAX_FIREBALLS; i++) {
            if (!fbs[i].active) continue;
            if (fbs[i].y == ROW_PLAYER && fbs[i].x >= pLeft && fbs[i].x <= pRight) {
                fbs[i].active = false;
                int dmg = config.fireballDmg * (100 - armorPct) / 100;
                if (dmg < 1) dmg = 1;
                playerHp -= dmg;
                if (playerHp < 0) playerHp = 0;
            }
        }

        {
            int prevPhase = dragon.phase;
            updatePhase(dragon, config.dragonSpeed);
            // Phase change announcement
            if (dragon.phase > prevPhase)
                dragon.announceTicks = std::max(dragon.announceTicks, 25);
        }

        // Hard-mode enrage: triggers once at or below 50% HP
        if (config.hasEnrage && !dragon.enraged && dragon.hp > 0 &&
            dragon.hp * 100 / dragon.maxHp <= 50) {
            dragon.enraged    = true;
            dragon.speed     += 3;
            currentFireRate   = std::max(5, currentFireRate / 2);
            dragon.announceTicks = 40;  // 2 seconds of banner
        }
        if (dragon.announceTicks > 0) dragon.announceTicks--;

        if (dragon.hp <= 0) {
            addScore(state, FF_SCORE_KILL);
            killBonus = true; won = true; running = false;
        }
        if (playerHp <= 0 && running) { won = false; running = false; }

        renderFrame(dragon, fbs, arrows, playerX, playerHp, playerMaxHp,
                    state.score, state.player.equipment.armor, tick, hitFlash);

        usleep(FF_TICK_US);
    }

    endwin();  // restore terminal before ANSI output

    state.dragonDefeated = won;
    state.phase = won ? PHASE_VICTORY : PHASE_GAMEOVER;

    showScoreBreakdown(won, miningSnap, p1h, p2h, p3h,
                       killBonus, state.score, state.settings.scoreMultiplier);
    saveFinalScore(state, won);
    return won;
}
