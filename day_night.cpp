/*
 * day_night.cpp
 *
 * Day/Night cycle system for TermiCraft.
 * Manages time progression and sky rendering with sun, moon, and stars.
 *
 * Author: Mohit
 */

#include "day_night.h"
#include <cstdio>
#include <cstring>

// ----- SHARED STATE -----
int tickCount = 0;

// ----- TIME OF DAY -----

TimeOfDay getTimeOfDay() {
    int t = tickCount % CYCLE_LENGTH;
    if (t < DUSK_START)  return TIME_DAY;
    if (t < NIGHT_START) return TIME_DUSK;
    if (t < DAWN_START)  return TIME_NIGHT;
    return TIME_DAWN;
}

const char* getTimeLabel() {
    switch (getTimeOfDay()) {
        case TIME_DAY:   return "DAY";
        case TIME_DUSK:  return "DUSK";
        case TIME_NIGHT: return "NIGHT";
        case TIME_DAWN:  return "DAWN";
    }
    return "?";
}

void tickDayCycle() {
    tickCount++;
}

// ----- SKY BACKGROUND -----

const char* getSkyBg() {
    switch (getTimeOfDay()) {
        case TIME_DAY:   return "\033[48;5;39m";   // bright blue
        case TIME_DUSK:  return "\033[48;5;130m";   // warm orange
        case TIME_NIGHT: return "\033[48;5;17m";    // dark navy
        case TIME_DAWN:  return "\033[48;5;95m";    // muted pink
    }
    return "";
}

// ----- SUN / MOON ART -----

static const char* SUN_ART[] = {
    " \\|/ ",
    "- O -",
    " /|\\ "
};
static const int SUN_H = 3, SUN_W = 5;

static const char* MOON_ART[] = {
    " _ ",
    "( )",
    " ~ "
};
static const int MOON_H = 3, MOON_W = 3;

static const char STAR_CHARS[] = {'.', '+', '*', '`'};

// ----- POSITION HELPERS -----

static int getSunCol(int vpWidth) {
    TimeOfDay tod = getTimeOfDay();
    int t = tickCount % CYCLE_LENGTH;
    int col;

    switch (tod) {
        case TIME_DAY: {
            float p = (float)(t - DAY_START) / (DUSK_START - DAY_START);
            col = (int)(vpWidth * 0.15f + p * vpWidth * 0.7f);
            break;
        }
        case TIME_DUSK: {
            float p = (float)(t - DUSK_START) / (NIGHT_START - DUSK_START);
            col = (int)(vpWidth * 0.85f + p * vpWidth * 0.15f);
            break;
        }
        case TIME_DAWN: {
            float p = (float)(t - DAWN_START) / (CYCLE_LENGTH - DAWN_START);
            col = (int)(p * vpWidth * 0.15f);
            break;
        }
        default:
            return -100; // off-screen during night
    }

    // Clamp so sun art (5 chars wide) never renders past the viewport edge
    if (col < 0)           col = 0;
    if (col > vpWidth - 5) col = vpWidth - 5;
    return col;
}

static int getMoonCol(int vpWidth) {
    TimeOfDay tod = getTimeOfDay();
    int t = tickCount % CYCLE_LENGTH;

    switch (tod) {
        case TIME_NIGHT: {
            float p = (float)(t - NIGHT_START) / (DAWN_START - NIGHT_START);
            return (int)(vpWidth * 0.15f + p * vpWidth * 0.7f);
        }
        case TIME_DAWN: {
            float p = (float)(t - DAWN_START) / (CYCLE_LENGTH - DAWN_START);
            return (int)(vpWidth * 0.85f + p * vpWidth * 0.15f);
        }
        case TIME_DUSK: {
            float p = (float)(t - DUSK_START) / (NIGHT_START - DUSK_START);
            return (int)(p * vpWidth * 0.15f);
        }
        default:
            return -100; // off-screen during day
    }
}

// ----- ART LOOKUP -----

static char getSunChar(int skyRow, int vx, int sunCx) {
    int lx = vx - (sunCx - SUN_W / 2);
    int ly = skyRow - 1; // sun centered at rows 1-3
    if (ly < 0 || ly >= SUN_H || lx < 0 || lx >= SUN_W) return 0;
    return SUN_ART[ly][lx];
}

static char getMoonChar(int skyRow, int vx, int moonCx) {
    int lx = vx - (moonCx - MOON_W / 2);
    int ly = skyRow - 1;
    if (ly < 0 || ly >= MOON_H || lx < 0 || lx >= MOON_W) return 0;
    return MOON_ART[ly][lx];
}

static bool isStarAt(int row, int col) {
    unsigned int h = (unsigned int)(row * 7919 + col * 6271 + 1031);
    h ^= h >> 13;
    h *= 0x5bd1e995;
    h ^= h >> 15;
    return (h % 12) == 0;
}

// ----- RENDER A SINGLE SKY CELL INTO BUFFER -----
// Returns bytes written to buf.

int renderSkyCell(int skyRow, int vx, int vpWidth, char* buf) {
    TimeOfDay tod = getTimeOfDay();
    const char* bg = getSkyBg();
    int sunCx  = getSunCol(vpWidth);
    int moonCx = getMoonCol(vpWidth);

    // Sun
    char sc = getSunChar(skyRow, vx, sunCx);
    if (sc && sc != ' ') {
        return sprintf(buf, "%s\033[38;5;226m%c\033[0m", bg, sc);
    }

    // Moon
    char mc = getMoonChar(skyRow, vx, moonCx);
    if (mc && mc != ' ') {
        return sprintf(buf, "%s\033[38;5;255m%c\033[0m", bg, mc);
    }

    // Stars (not during day)
    if (tod != TIME_DAY && isStarAt(skyRow, vx)) {
        unsigned int h = (unsigned int)(skyRow * 31 + vx * 17);
        char star = STAR_CHARS[h % 4];
        const char* col = (tod == TIME_NIGHT) ? "\033[38;5;255m" : "\033[38;5;244m";
        return sprintf(buf, "%s%s%c\033[0m", bg, col, star);
    }

    // Empty sky with background color
    return sprintf(buf, "%s \033[0m", bg);
}
