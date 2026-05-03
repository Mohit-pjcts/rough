/*
 * day_night.h
 *
 * Day/Night cycle system for TermiCraft.
 * Handles time progression, sky rendering (sun, moon, stars),
 * and sky background colors across four phases: day, dusk, night, dawn.
 *
 * Author: Mohit
 */

#ifndef DAY_NIGHT_H
#define DAY_NIGHT_H

#include "types.h"

// Cycle timing (ticks). At 20 ticks/sec, 2400 = 2 min full cycle.
// Adjust CYCLE_LENGTH to change how long a day lasts.
const int CYCLE_LENGTH = 6000;
const int DAY_START    = 0;
const int DUSK_START   = (int)(CYCLE_LENGTH * 0.40);
const int NIGHT_START  = (int)(CYCLE_LENGTH * 0.50);
const int DAWN_START   = (int)(CYCLE_LENGTH * 0.85);

enum TimeOfDay { TIME_DAY, TIME_DUSK, TIME_NIGHT, TIME_DAWN };

// Shared tick counter — declared here, defined in day_night.cpp
extern int tickCount;

// Get the current time of day based on tickCount
TimeOfDay getTimeOfDay();

// Get a label string for the HUD
const char* getTimeLabel();

// Get the ANSI background color escape for sky cells
const char* getSkyBg();

// Advance the cycle by one tick
void tickDayCycle();

// Write sky cell into buffer. Returns number of bytes written.
// skyRow = world y coordinate, vx = viewport x, buf = output buffer position
int renderSkyCell(int skyRow, int vx, int vpWidth, char* buf);

#endif
