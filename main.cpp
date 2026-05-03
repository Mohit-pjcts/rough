/*
 * main.cpp
 * TermiCraft — entry point and game loop
 */

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include "colors.h"
#include "day_night.h"
#include "fileio.h"
#include "final_fight.h"
#include "fog_of_war.h"
#include "menu.h"
#include "minesweeper.h"
#include "twentyfour.h"
#include "player.h"
#include "score.h"
#include "types.h"
#include "world_gen.h"

// wordle.cpp exports this free function
bool runWordle(int wordLength);
bool runSudoku(Difficulty diff);

// ----- GLOBALS -----

static GameState gameState;
static bool      gameRunning = true;
static struct termios originalTermios;
static volatile sig_atomic_t termResized = 0;

// ----- FORWARD DECLARATIONS -----

void setupTerminal();
void restoreTerminal();

// ----- DRAGON CAVE SEQUENCE -----
// Shows a clean centered popup and runs the boss fight.
// Returns true if game ended (win or lose), false if player declined.
static bool runDragonCaveSequence(GameState& state) {
    state.dragonCaveFound = true;
    restoreTerminal();

    // Box: dynamic width based on terminal size
    int termWdc, termHdc;
    getTermSize(termWdc, termHdc);
    int BW = std::max(50, std::min(termWdc - 6, 88));
    clearAndCenterV(16);
    std::string P = hpad(BW + 2);

    // Center content string within BW columns
    auto bline = [&](const std::string& s) {
        // Strip leading spaces so we can re-center cleanly
        std::string content = s;
        size_t first = content.find_first_not_of(' ');
        if (first != std::string::npos) content = content.substr(first);
        int len = (int)content.size();
        if (len >= BW) { content = content.substr(0, BW); len = BW; }
        int leftPad  = (BW - len) / 2;
        int rightPad = BW - len - leftPad;
        std::cout << P << "\xe2\x95\x91"
                  << std::string(leftPad, ' ') << content << std::string(rightPad, ' ')
                  << "\xe2\x95\x91\n";
    };
    auto sep = [&]() {
        std::cout << P << "\xe2\x95\xa0";
        for (int i = 0; i < BW; i++) std::cout << "\xe2\x95\x90";
        std::cout << "\xe2\x95\xa3\n";
    };

    // Top border  ╔══...══╗
    std::cout << P << "\xe2\x95\x94";
    for (int i = 0; i < BW; i++) std::cout << "\xe2\x95\x90";
    std::cout << "\xe2\x95\x97\n";

    // Title row: "~  D R A G O N ' S   L A I R  ~" = 31 display cols, centered in BW
    {
        int tl = (BW - 31) / 2;
        int tr = BW - 31 - tl;
        std::cout << P << "\xe2\x95\x91\033[1;31m"
                  << std::string(tl, ' ')
                  << "~  D R A G O N ' S   L A I R  ~"
                  << std::string(tr, ' ')
                  << "\033[0m\xe2\x95\x91\n";
    }

    sep();

    bline("");
    bline("  You broke through. The air burns. Something stirs.");
    bline("  Two eyes open in the dark. The ground trembles.");
    bline("");

    sep();

    // Stats line
    const char* modeName = (state.difficulty == DIFF_EASY)   ? "Easy"   :
                           (state.difficulty == DIFF_HARD)   ? "Hard"   : "Normal";
    char statBuf[80];
    std::snprintf(statBuf, sizeof(statBuf),
        "  HP: %d/%d    Armor: %-8s  Mode: %s",
        state.player.health, state.player.maxHealth,
        getMaterialName(state.player.equipment.armor).c_str(),
        modeName);
    bline(statBuf);

    sep();

    // Warning line — color and message vary by armor tier
    const char* warnColor;
    const char* warnMsg;
    switch (state.player.equipment.armor) {
        case MATERIAL_NONE:
        case MATERIAL_WOOD:
            warnColor = "\033[1;31m";
            warnMsg   = "NO ARMOR. YOU WILL NOT LAST LONG. GOOD LUCK SURVIVING.";  break;
        case MATERIAL_STONE:
            warnColor = "\033[1;31m";
            warnMsg   = "LIGHT ARMOR. EXPECT HEAVY DAMAGE. GOOD LUCK SURVIVING."; break;
        case MATERIAL_IRON:
            warnColor = "\033[38;5;208m";
            warnMsg   = "IRON ARMOR. RISKY BUT POSSIBLE. GOOD LUCK SURVIVING.";   break;
        case MATERIAL_GOLD:
            warnColor = "\033[1;33m";
            warnMsg   = "GOLD ARMOR. DECENT PROTECTION. GOOD LUCK SURVIVING.";    break;
        default:  // DIAMOND
            warnColor = "\033[1;32m";
            warnMsg   = "DIAMOND ARMOR. YOU ARE PREPARED. GOOD LUCK SURVIVING.";  break;
    }
    {
        int warnLen  = (int)std::strlen(warnMsg);
        int warnLeft = (BW - std::min(warnLen, BW)) / 2;
        int warnRight = BW - std::min(warnLen, BW) - warnLeft;
        std::cout << P << "\xe2\x95\x91"
                  << std::string(warnLeft, ' ')
                  << warnColor << warnMsg << "\033[0m"
                  << std::string(warnRight, ' ')
                  << "\xe2\x95\x91\n";
    }

    sep();

    bline("");
    bline("    Descend into the lair?    [ Y ] Yes   [ N ] No");
    bline("");

    // Bottom border  ╚══...══╝
    std::cout << P << "\xe2\x95\x9a";
    for (int i = 0; i < BW; i++) std::cout << "\xe2\x95\x90";
    std::cout << "\xe2\x95\x9d\n";
    std::cout.flush();

    char choice = '\0';
    while (choice != 'y' && choice != 'Y' && choice != 'n' && choice != 'N')
        read(STDIN_FILENO, &choice, 1);

    if (choice == 'n' || choice == 'N') {
        setupTerminal();
        return false;
    }

    bool bossWon = runBossFight(state);
    if (bossWon) {
        state.victory        = true;
        state.dragonDefeated = true;
        state.phase          = PHASE_VICTORY;
    } else {
        state.gameOver = true;
        state.phase    = PHASE_GAMEOVER;
    }
    return true;
}

// ----- TERMINAL SIZE -----

static void updateViewportSize(GameState& state) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        // Reserve 5 rows: 2 always-on HUD lines + 1 alert (conditional)
        // + 1 status line + 1 buffer so content never scrolls.
        state.viewportWidth  = (int)ws.ws_col;
        state.viewportHeight = (int)ws.ws_row - 5;
        // Cap width to world width so wide terminals don't show garbage columns.
        if (state.viewportWidth  > WORLD_WIDTH)  state.viewportWidth  = WORLD_WIDTH;
        if (state.viewportWidth  < 40) state.viewportWidth  = 40;
        if (state.viewportHeight < 10) state.viewportHeight = 10;
    }
}

static void sigwinchHandler(int) { termResized = 1; }

// ----- TERMINAL SETUP -----

void setupTerminal() {
    static bool originalSaved = false;
    if (!originalSaved) {
        tcgetattr(STDIN_FILENO, &originalTermios);
        originalSaved = true;
    }

    struct termios raw = originalTermios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    signal(SIGWINCH, sigwinchHandler);
    std::cout << CURSOR_HIDE;
    clearScreen();
}

void restoreTerminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios);
    std::cout << CURSOR_SHOW;
    std::cout << COLOR_RESET;
    clearScreen();
}

void signalHandler(int signal) {
    restoreTerminal();
    exit(signal);
}

// ----- ENEMY SYSTEM -----

static void updateEnemies(GameState& state) {
    if (state.enemies.empty()) return;

    int px = state.player.pos.x;
    int py = state.player.pos.y;

    for (Enemy& e : state.enemies) {
        if (!e.alive) continue;

        // Move one step toward player (Manhattan)
        int dx = 0, dy = 0;
        if (e.pos.x < px)      dx =  1;
        else if (e.pos.x > px) dx = -1;
        if (e.pos.y < py)      dy =  1;
        else if (e.pos.y > py) dy = -1;

        int nx = e.pos.x + dx;
        int ny = e.pos.y + dy;

        bool canMove = (nx >= 0 && nx < state.worldWidth &&
                        ny >= 0 && ny < state.worldHeight);
        if (canMove) {
            Block& b = state.world[ny][nx];
            if (!isSolidBlock(b.type) || b.mined) {
                e.pos.x = nx;
                e.pos.y = ny;
            }
        }

        // Attack player on contact
        if (e.pos.x == px && e.pos.y == py) {
            damagePlayer(state, e.damage);
            e.alive = false; // enemy dies on contact
        }
    }

    // Remove dead enemies
    state.enemies.erase(
        std::remove_if(state.enemies.begin(), state.enemies.end(),
                       [](const Enemy& e) { return !e.alive; }),
        state.enemies.end());
}

// ----- GAME INIT/CLEANUP -----

void initGame(GameState& state, Difficulty difficulty, bool isNewGame,
              const std::string& playerName = "") {
    state.difficulty    = difficulty;
    state.settings      = getDifficultySettings(difficulty);
    state.phase         = PHASE_PLAYING;
    state.gameOver      = false;
    state.victory       = false;
    state.score         = 0;
    state.oresMined     = 0;
    state.enemiesKilled = 0;
    state.dragonCaveFound  = false;
    state.dragonDefeated   = false;
    state.minigameActive   = false;
    state.miningPending    = false;
    state.minigameDamage   = state.settings.minigameDamage;
    state.enemies.clear();
    state.activeEvent      = RandomEvent();
    state.eventCooldown    = 300;  // first event can't fire for 300 frames
    state.oreSurgeActive   = false;

    updateViewportSize(state);

    if (isNewGame) {
        state.seed = static_cast<unsigned int>(time(nullptr));
        initWorld(state);
        generateWorld(state);
        initPlayer(state, playerName);
    }

    updateWorldVisibility(state);
}

void cleanupGame(GameState& state) {
    if (state.world != nullptr) {
        for (int y = 0; y < state.worldHeight; y++)
            delete[] state.world[y];
        delete[] state.world;
        state.world = nullptr;
    }
    state.enemies.clear();
}

// ----- PAUSE MENU -----

static void runPauseMenu() {
    int termW, termH;
    getTermSize(termW, termH);
    int BW = std::max(35, std::min(termW - 6, 68));
    clearAndCenterV(13);
    std::string P = hpad(BW + 2);

    auto pline = [&](const char* textColor, const char* s) {
        std::string c(s);
        if ((int)c.size() > BW) c = c.substr(0, BW);
        else c += std::string(BW - c.size(), ' ');
        std::cout << COLOR_CYAN << P << "\xe2\x95\x91" << textColor << c << COLOR_CYAN << "\xe2\x95\x91\n";
    };
    auto topBar = [&]() { std::cout << COLOR_BOLD_CYAN << P << "\xe2\x95\x94"; for (int i=0;i<BW;i++) std::cout << "\xe2\x95\x90"; std::cout << "\xe2\x95\x97\n"; };
    auto midBar = [&]() { std::cout << COLOR_CYAN     << P << "\xe2\x95\xa0"; for (int i=0;i<BW;i++) std::cout << "\xe2\x95\x90"; std::cout << "\xe2\x95\xa3\n"; };
    auto botBar = [&]() { std::cout << COLOR_CYAN     << P << "\xe2\x95\x9a"; for (int i=0;i<BW;i++) std::cout << "\xe2\x95\x90"; std::cout << "\xe2\x95\x9d\n" << COLOR_RESET; };

    // Title: "⏸  PAUSED" = 9 display cols (⏸ = U+23F8, display width 1)
    int titleLeft = (BW - 9) / 2;
    int titleRight = BW - 9 - titleLeft;

    topBar();
    std::cout << COLOR_BOLD_CYAN << P << "\xe2\x95\x91"
              << std::string(titleLeft, ' ') << "\xe2\x8f\xb8  PAUSED" << std::string(titleRight, ' ')
              << "\xe2\x95\x91\n";
    midBar();

    char hpBuf[96], depBuf[96];
    std::snprintf(hpBuf,  sizeof(hpBuf),  "  HP: %d/%d   Score: %d",
                  gameState.player.health, gameState.player.maxHealth, gameState.score);
    std::snprintf(depBuf, sizeof(depBuf), "  Depth: %d   Pos: (%d,%d)",
                  gameState.player.pos.y - SURFACE_LEVEL,
                  gameState.player.pos.x, gameState.player.pos.y);
    pline(COLOR_WHITE, hpBuf);
    pline(COLOR_WHITE, depBuf);
    midBar();
    pline(COLOR_WHITE, "  [S] Save Game");
    pline(COLOR_WHITE, "  [R] Resume");
    pline(COLOR_WHITE, "  [Q] Quit to Menu");
    midBar();
    pline(COLOR_DIM, "  WASD=move  SPACE=mine  C=craft");
    pline(COLOR_DIM, "  I=inventory  P=pause  Q=quit");
    botBar();

    char pauseInput;
    if (read(STDIN_FILENO, &pauseInput, 1) == 1) {
        if (pauseInput == 's' || pauseInput == 'S') {
            restoreTerminal();
            if (saveGame(gameState)) {
                std::cout << "\n    " << COLOR_SUCCESS << "Game saved!\n" << COLOR_RESET;
            } else {
                std::cout << "\n    " << COLOR_DANGER << "Save failed!\n" << COLOR_RESET;
            }
            waitForKeypress();
            setupTerminal();
        } else if (pauseInput == 'q' || pauseInput == 'Q') {
            gameRunning = false;
        }
        // 'R' or anything else: just resume
    }
}

// ----- GAME LOOP -----

void runGameLoop() {
    while (gameRunning && !gameState.gameOver && !gameState.victory) {

        // ── Minigame phase ────────────────────────────────────────────────────
        if (gameState.phase == PHASE_MINIGAME && gameState.miningPending) {
            restoreTerminal();

            g_minigameForfeited = false;
            bool won = false;
            if (gameState.currentMinigame == MINIGAME_WORDLE)
                won = runWordle(gameState.settings.wordleWordLength);
            else if (gameState.currentMinigame == MINIGAME_MINESWEEPER)
                won = runMinesweeper(gameState.settings.minesweeperSize);
            else if (gameState.currentMinigame == MINIGAME_TWENTYFOUR)
                won = runTwentyFour(gameState.settings.twentyFourAttempts,
                                    gameState.settings.twentyFourTimeLimit);
            else if (gameState.currentMinigame == MINIGAME_SUDOKU)
                won = runSudoku(gameState.difficulty);
            else 
                won = false;

            if (g_minigameForfeited) {
                // Player fled mid-challenge — double penalty, cancel any pending upgrade
                int penalty = gameState.settings.minigameDamage * 2;
                damagePlayer(gameState, penalty);
                gameState.lastMessage = "COWARD! Fled the challenge! Took "
                    + std::to_string(penalty) + " damage!";
                gameState.pendingUpgrade = MATERIAL_NONE;
                gameState.phase          = PHASE_PLAYING;
                gameState.miningPending  = false;
                gameState.minigameActive = false;
                setupTerminal();
                continue;
            }

            bool wasCraftingTrial = (gameState.pendingUpgrade != MATERIAL_NONE);

            if (wasCraftingTrial) {
                // Crafting "rite of passage" minigame
                if (won) {
                    confirmUpgrade(gameState);
                } else {
                    // Failed rite — strip the just-crafted equipment back one tier
                    MaterialTier stripped = static_cast<MaterialTier>(
                        static_cast<int>(gameState.pendingUpgrade) - 1);
                    gameState.player.equipment.pickaxe = stripped;
                    damagePlayer(gameState, gameState.settings.minigameDamage);
                    gameState.lastMessage = "Trial failed! Lost the " +
                        getMaterialName(gameState.pendingUpgrade) +
                        " equipment. Took " +
                        std::to_string(gameState.settings.minigameDamage) + " damage!";
                    gameState.pendingUpgrade = MATERIAL_NONE;
                }
            } else {
                // Normal mining minigame
                BlockType minedType = gameState.pendingMineType;
                Position  minedPos  = gameState.pendingMinePos;
                resolveMiningAttempt(gameState, won);
                if (won) {
                    trySpawnEnemy(gameState, minedPos);

                    // Dragon cave: only enters after successfully mining the cave block
                    if (minedType == BLOCK_DRAGON_CAVE && !gameState.dragonDefeated) {
                        gameState.phase          = PHASE_PLAYING;
                        gameState.miningPending  = false;
                        gameState.minigameActive = false;
                        setupTerminal();
                        bool gameEnded = runDragonCaveSequence(gameState);
                        if (gameEnded) break;   // go to victory/gameover handling
                        continue;
                    }
                }
            }

            gameState.phase          = PHASE_PLAYING;
            gameState.miningPending  = false;
            gameState.minigameActive = false;
            setupTerminal();
            continue;
        }

        if (gameState.gameOver || gameState.victory) break;

        // ── Terminal resize ───────────────────────────────────────────────────
        if (termResized) {
            termResized = 0;
            updateViewportSize(gameState);
            updateCamera(gameState);
            clearScreen();
        }

        // ── Random event tick ─────────────────────────────────────────────────
        {
            RandomEvent& ev = gameState.activeEvent;
            if (ev.active) {
                ev.ticksLeft--;
                if (ev.ticksLeft <= 0) {
                    ev.active = false;
                    gameState.oreSurgeActive = false;
                    gameState.eventCooldown  = 400;
                    gameState.lastMessage    = "Event over.";
                }
                // Red zone: slow-burn damage — only ticks every damagePeriod frames
                if (ev.type == EVENT_RED_ZONE && ev.active) {
                    if (ev.alertTicks > 0) ev.alertTicks--;
                    ev.damageTimer++;
                    if (ev.damageTimer >= ev.damagePeriod) {
                        ev.damageTimer = 0;
                        int dx = std::abs(gameState.player.pos.x - ev.x);
                        int dy = std::abs(gameState.player.pos.y - ev.y);
                        if (dx + dy <= ev.radius) {
                            damagePlayer(gameState, ev.damage);
                            gameState.lastMessage = "BURNING! -" + std::to_string(ev.damage) + " HP";
                        }
                    }
                }
            } else {
                if (gameState.eventCooldown > 0) {
                    gameState.eventCooldown--;
                } else {
                    // 2% chance per frame to spawn a new event
                    if ((rand() % 100) < 2) {
                        int kind = rand() % 2;
                        ev.active    = true;
                        ev.ticksLeft = 180 + rand() % 120;  // 180-300 frames (~6-10s)
                        // Centre the event near the player but offset so they can react
                        int offX = (rand() % 15) - 7;
                        int offY = (rand() % 7)  - 3;
                        ev.x = std::max(0, std::min(gameState.worldWidth  - 1, gameState.player.pos.x + offX));
                        ev.y = std::max(0, std::min(gameState.worldHeight - 1, gameState.player.pos.y + offY));
                        if (kind == 0) {
                            ev.type   = EVENT_RED_ZONE;
                            ev.radius = (gameState.difficulty == DIFF_EASY)   ? 4 + rand() % 3 :
                                        (gameState.difficulty == DIFF_HARD)   ? 8 + rand() % 5 :
                                                                                 6 + rand() % 4;
                            ev.damage = (gameState.difficulty == DIFF_EASY)   ? 3 :
                                        (gameState.difficulty == DIFF_HARD)   ? 8 : 5;
                            ev.damagePeriod = (gameState.difficulty == DIFF_EASY)  ? 60 :
                                              (gameState.difficulty == DIFF_HARD)  ? 20 : 40;
                            ev.damageTimer = 0;
                            ev.alertTicks  = 90;  // 3 seconds of big alert banner
                            ev.ticksLeft   = (gameState.difficulty == DIFF_EASY)  ? 300 :
                                             (gameState.difficulty == DIFF_HARD)  ? 600 : 450;
                            gameState.lastMessage = "!!! DANGER: FIRE ZONE — GET OUT NOW !!!";
                        } else {
                            ev.type   = EVENT_ORE_SURGE;
                            ev.radius = 8;
                            ev.damage = 0;
                            gameState.oreSurgeActive = true;
                            gameState.lastMessage = "ORE SURGE — Mining doubled!";
                        }
                        gameState.eventCooldown = 500;
                    }
                }
            }
        }

        if (gameState.gameOver) break;

        // ── Render ────────────────────────────────────────────────────────────
        tickDayCycle();
        updateWorldVisibility(gameState);
        // World + HUD + status are all written in one atomic write — no flicker.
        renderWorld(gameState, gameState.lastMessage);

        // ── Update enemies ────────────────────────────────────────────────────
        updateEnemies(gameState);

        if (gameState.gameOver) break;

        // ── Input ─────────────────────────────────────────────────────────────
        char input;
        if (read(STDIN_FILENO, &input, 1) != 1) continue;

        // Map arrow keys → WASD
        if (input == '\033') {
            char seq[2] = {0, 0};
            fd_set fds2; FD_ZERO(&fds2); FD_SET(STDIN_FILENO, &fds2);
            struct timeval tv2 = {0, 5000};
            if (select(STDIN_FILENO + 1, &fds2, nullptr, nullptr, &tv2) > 0 &&
                read(STDIN_FILENO, &seq[0], 1) == 1 && seq[0] == '[') {
                if (read(STDIN_FILENO, &seq[1], 1) == 1) {
                    switch (seq[1]) {
                    case 'A': input = 'w'; break;
                    case 'B': input = 's'; break;
                    case 'C': input = 'd'; break;
                    case 'D': input = 'a'; break;
                    default: continue;
                    }
                } else { continue; }
            } else { continue; }
        }

        switch (input) {
        case 'q':
        case 'Q':
            if (showConfirmation("Quit to menu?")) {
                if (showConfirmation("Save progress?")) {
                    restoreTerminal();
                    saveGame(gameState);
                    setupTerminal();
                }
                gameRunning = false;
            }
            break;
        case 'p':
        case 'P':
            runPauseMenu();
            break;
        default:
            handleInput(gameState, input);
            break;
        }
    }
}

// ----- MAIN -----

int main() {
    signal(SIGINT,  signalHandler);
    signal(SIGTERM, signalHandler);

    srand(static_cast<unsigned int>(time(nullptr)));

    bool exitGame = false;

    while (!exitGame) {
        setupTerminal();

        HighScore topScore = getTopHighScore();
        int choice = showMainMenu(topScore);

        switch (choice) {

        case 1: { // New Game
            Difficulty diff = selectDifficulty();

            restoreTerminal();
            std::string name = getPlayerName("Enter your name");
            setupTerminal();

            initGame(gameState, diff, true, name);

            gameRunning = true;
            runGameLoop();

            if (gameState.gameOver || gameState.victory) {
                restoreTerminal();
                // Brief pause so death/win doesn't feel instant
                clearAndCenterV(7);
                std::string splashPad = hpad(30);
                if (gameState.victory) {
                    std::cout << "\033[1;32m"
                        << splashPad << "══════════════════════════════\n"
                        << splashPad << "      DRAGON SLAIN!  \xf0\x9f\x90\x89\n"
                        << splashPad << "══════════════════════════════\n"
                        << "\033[0m\n"
                        << "\033[38;5;220m" << splashPad << "Calculating score...\033[0m\n";
                } else {
                    std::cout << "\033[1;31m"
                        << splashPad << "══════════════════════════════\n"
                        << splashPad << "       YOU DIED  \xf0\x9f\x92\x80\n"
                        << splashPad << "══════════════════════════════\n"
                        << "\033[0m\n"
                        << "\033[38;5;240m" << hpad((int)gameState.lastMessage.size()) << gameState.lastMessage << "\033[0m\n";
                }
                std::cout.flush();
                usleep(1800000);  // 1.8 seconds
                // Save score during the splash pause — no separate "saving" screen
                saveFinalScore(gameState, gameState.victory);
                showGameOver(gameState, gameState.victory);
                std::vector<HighScore> scores = loadHighScores();
                showHighScores(scores);
                setupTerminal();
            }

            cleanupGame(gameState);
            break;
        }

        case 2: { // Load Game
            if (!saveFileExists()) {
                clearScreen();
                std::cout << "\n\n  " << COLOR_WARNING << "No save file found!\n" << COLOR_RESET;
                waitForKeypress();
            } else {
                if (loadGame(gameState)) {
                    // Re-apply any post-load setup
                    updateWorldVisibility(gameState);
                    gameState.phase          = PHASE_PLAYING;
                    gameState.gameOver       = false;
                    gameState.victory        = false;
                    gameState.miningPending  = false;
                    gameState.minigameActive = false;
                    gameState.currentMinigame = MINIGAME_NONE;
                    gameState.pendingUpgrade  = MATERIAL_NONE;

                    gameRunning = true;
                    runGameLoop();

                    if (gameState.gameOver || gameState.victory) {
                        restoreTerminal();
                        showGameOver(gameState, gameState.victory);
                        saveFinalScore(gameState, gameState.victory);
                        std::vector<HighScore> scores = loadHighScores();
                        showHighScores(scores);
                        setupTerminal();
                    }

                    cleanupGame(gameState);
                } else {
                    clearScreen();
                    std::cout << "\n\n  " << COLOR_DANGER << "Failed to load save file!\n" << COLOR_RESET;
                    waitForKeypress();
                }
            }
            break;
        }

        case 3: { // High Scores
            std::vector<HighScore> scores = loadHighScores();
            showHighScores(scores);
            break;
        }

        case 4: { // How to Play
            showHowToPlay();
            break;
        }

        case 5: { // Quit
            exitGame = true;
            break;
        }

        } // switch
    } // while !exitGame

    restoreTerminal();
    std::cout << "\n  Thanks for playing TermiCraft!\n\n";
    return 0;
}
