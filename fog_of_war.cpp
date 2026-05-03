/*
 * fog_of_war.cpp
 *
 * Fog of War visibility system and world rendering for TermiCraft.
 * Everything — world, HUD, status line — is written into one large buffer
 * and flushed with a single write() call so there is never any tearing.
 *
 * Author: Mohit
 */

#include "fog_of_war.h"
#include "day_night.h"
#include "colors.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <unistd.h>

// ----- VISIBILITY RADIUS BY DEPTH -----

int getVisibilityRadius(int worldY) {
    if (worldY < STONE_LEVEL) {
        return -1;  // surface + dirt: always visible
    } else if (worldY < DEEP_LEVEL) {
        return 3;   // underground / stone layer
    } else {
        return 2;   // deep layer
    }
}

// ----- UPDATE VISIBILITY -----

void updateWorldVisibility(GameState& state) {
    int px = state.player.pos.x;
    int py = state.player.pos.y;

    // Surface/dirt rows: always fully visible within viewport
    int camX = state.camera.x;
    int camY = state.camera.y;
    for (int vy = 0; vy < state.viewportHeight; vy++) {
        int wy = camY + vy;
        if (wy < 0 || wy >= state.worldHeight) continue;
        if (wy < STONE_LEVEL) {
            for (int vx = 0; vx < state.viewportWidth; vx++) {
                int wx = camX + vx;
                if (wx >= 0 && wx < state.worldWidth)
                    state.world[wy][wx].visible = true;
            }
        }
    }

    // Underground/deep: circular reveal around player, permanent
    int radius = getVisibilityRadius(py);
    if (radius > 0) {
        int r2 = radius * radius;
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx * dx + dy * dy > r2) continue;
                int wx = px + dx;
                int wy = py + dy;
                if (wx >= 0 && wx < state.worldWidth &&
                    wy >= 0 && wy < state.worldHeight) {
                    state.world[wy][wx].visible = true;
                }
            }
        }
    }
}

// ----- BUFFERED RENDERING -----
// World + HUD + status all go into one buffer, one write() — zero tearing.

// 100 cols * 50 rows * ~30 bytes/cell + generous HUD space
static char renderBuf[700000];

// Append the HUD and status message to renderBuf[pos..] and return new pos.
static int appendHUD(char* buf, int pos, const GameState& state,
                     const std::string& statusMsg) {

    // ── Line 1: HP bar + score + equipment ──────────────────────────────────
    int maxHp = state.player.maxHealth > 0 ? state.player.maxHealth : 1;
    int hpFilled = (state.player.health * 20) / maxHp;
    if (hpFilled < 0)  hpFilled = 0;
    if (hpFilled > 20) hpFilled = 20;

    pos += sprintf(buf + pos, "\033[1;37m HP:\033[0m [");
    for (int i = 0; i < 20; i++) {
        if (i < hpFilled) {
            const char* col = (hpFilled > 10) ? "\033[0;32m"
                            : (hpFilled >  5) ? "\033[0;33m"
                                              : "\033[0;31m";
            // U+2588 FULL BLOCK (█)
            pos += sprintf(buf + pos, "%s\xe2\x96\x88\033[0m", col);
        } else {
            // U+2591 LIGHT SHADE (░)
            pos += sprintf(buf + pos, "\033[2m\xe2\x96\x91\033[0m");
        }
    }
    pos += sprintf(buf + pos, "] %d/%d", state.player.health, state.player.maxHealth);

    // score
    pos += sprintf(buf + pos, "  \033[0;33mPTS:%d\033[0m", state.score);

    // ⛏ pickaxe  (U+26CF = \xe2\x9b\x8f)
    pos += sprintf(buf + pos, "  %s\xe2\x9b\x8f %s\033[0m",
                   getMaterialColor(state.player.equipment.pickaxe),
                   getMaterialName(state.player.equipment.pickaxe).c_str());

    // 🛡 armor  (U+1F6E1 = \xf0\x9f\x9b\xa1)
    pos += sprintf(buf + pos, "  %s\xf0\x9f\x9b\xa1  %s\033[0m\n",
                   getMaterialColor(state.player.equipment.armor),
                   getMaterialName(state.player.equipment.armor).c_str());

    // ── Line 2: Inventory + direction + depth ────────────────────────────────
    pos += sprintf(buf + pos,
        " \033[38;5;130mT:%d\033[0m"
        " \033[38;5;102m#:%d\033[0m"
        " \033[38;5;208mI:%d\033[0m"
        " \033[38;5;220mG:%d\033[0m"
        " \033[95mD:%d\033[0m",
        state.player.inventory.wood,
        state.player.inventory.stone,
        state.player.inventory.iron,
        state.player.inventory.gold,
        state.player.inventory.diamond);

    const char* dirArrow =
        (state.player.facingX ==  1) ? "\xe2\x86\x92" :  // →
        (state.player.facingX == -1) ? "\xe2\x86\x90" :  // ←
        (state.player.facingY == -1) ? "\xe2\x86\x91" :  // ↑
        (state.player.facingY ==  1) ? "\xe2\x86\x93" :  // ↓
                                       "?";
    pos += sprintf(buf + pos,
        "  \033[0;36mDIR:%s\033[0m"
        "  Depth:%d  (%d,%d)\n",
        dirArrow,
        state.player.pos.y - SURFACE_LEVEL,
        state.player.pos.x, state.player.pos.y);

    // ── Line 3 (conditional): fire zone alert ────────────────────────────────
    const RandomEvent& ev = state.activeEvent;
    if (ev.active && ev.type == EVENT_RED_ZONE && ev.alertTicks > 0) {
        bool blink = (ev.alertTicks / 8) % 2 == 0;
        if (blink) {
            pos += sprintf(buf + pos,
                "\033[1;31m \xe2\x96\x88\xe2\x96\x88"
                " FIRE ZONE ACTIVE \xe2\x96\x88\xe2\x96\x88"
                "  EVACUATE OR TAKE BURN DAMAGE  "
                "\xe2\x96\x88\xe2\x96\x88 FIRE ZONE \xe2\x96\x88\xe2\x96\x88"
                "\033[0m\n");
        } else {
            pos += sprintf(buf + pos,
                "\033[1;33m !! DANGER: FIRE ZONE -- move away from the burning area !!\033[0m\n");
        }
    }

    // ── Line 4: status message ───────────────────────────────────────────────
    if (!statusMsg.empty()) {
        bool isErr = (statusMsg.find("Failed") != std::string::npos ||
                      statusMsg.find("Need")   != std::string::npos ||
                      statusMsg.find("BURNING") != std::string::npos);
        pos += sprintf(buf + pos, "%s >> %s\033[0m\n",
                       isErr ? "\033[0;31m" : "\033[0;32m",
                       statusMsg.c_str());
    }

    return pos;
}

void renderWorld(const GameState& state, const std::string& statusMsg) {
    int pos = 0;

    // Hide cursor + cursor home (overwrite in place, no scroll)
    const char* home = "\033[?25l\033[H";
    memcpy(renderBuf + pos, home, 9);
    pos += 9;

    int camX = state.camera.x;
    int camY = state.camera.y;
    int vpW  = state.viewportWidth;
    int vpH  = state.viewportHeight;

    for (int vy = 0; vy < vpH; vy++) {
        int wy = camY + vy;

        for (int vx = 0; vx < vpW; vx++) {
            int wx = camX + vx;

            // Player
            if (wx == state.player.pos.x && wy == state.player.pos.y) {
                pos += sprintf(renderBuf + pos, "\033[38;5;226m@\033[0m");
                continue;
            }

            // Enemies (bright red B)
            bool isEnemy = false;
            for (const Enemy& e : state.enemies) {
                if (e.alive && e.pos.x == wx && e.pos.y == wy) {
                    pos += sprintf(renderBuf + pos, "\033[1;31mB\033[0m");
                    isEnemy = true;
                    break;
                }
            }
            if (isEnemy) continue;

            // Out of bounds
            if (wx < 0 || wx >= state.worldWidth ||
                wy < 0 || wy >= state.worldHeight) {
                if (wy >= 0 && wy <= SURFACE_LEVEL + 1)
                    pos += renderSkyCell(wy, vx, vpW, renderBuf + pos);
                else
                    pos += sprintf(renderBuf + pos, "\033[40m \033[0m");
                continue;
            }

            const Block& b = state.world[wy][wx];

            // Sky cells — delegate to day_night module.
            // Check block type first (not row) so the light cave's internal
            // sky rows and any terrain-dip sky at row >= SURFACE_LEVEL all render.
            if (b.type == BLOCK_SKY || (wy <= SURFACE_LEVEL && b.type == BLOCK_AIR)) {
                pos += renderSkyCell(wy, vx, vpW, renderBuf + pos);
                continue;
            }

            // Fog — unrevealed blocks
            if (!b.visible) {
                pos += sprintf(renderBuf + pos, "\033[38;5;236m:\033[0m");
                continue;
            }

            // Red zone tint
            const RandomEvent& ev = state.activeEvent;
            if (ev.active && ev.type == EVENT_RED_ZONE) {
                int evdx = abs(wx - ev.x);
                int evdy = abs(wy - ev.y);
                if (evdx + evdy <= ev.radius) {
                    pos += sprintf(renderBuf + pos,
                        "\033[41m\033[1;37m%s\033[0m", getBlockChar(b.type));
                    continue;
                }
            }

            // Visible block with color
            pos += sprintf(renderBuf + pos,
                "%s%s\033[0m", getBlockColor(b.type), getBlockChar(b.type));
        }

        pos += sprintf(renderBuf + pos, "\033[K\n");
    }

    // Clear anything below the viewport from a previous larger frame,
    // then write HUD inline — cursor is already at the right position.
    pos += sprintf(renderBuf + pos, "\033[J");  // erase to end of screen

    // Append HUD + status as part of the same buffer
    pos = appendHUD(renderBuf, pos, state, statusMsg);

    renderBuf[pos] = '\0';
    write(STDOUT_FILENO, renderBuf, pos);
}
