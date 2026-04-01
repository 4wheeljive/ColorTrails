#pragma once

// ═══════════════════════════════════════════════════════════════════
//  SWARMING DOTS - emitter_swarmingDots.h
// ═══════════════════════════════════════════════════════════════════

#include "flowFieldsTypes.h"
#include "modulators.h"

namespace flowFields {

    FL_FAST_MATH_BEGIN
    FL_OPTIMIZATION_LEVEL_O3_BEGIN

    struct SwarmingDotsParams {
        uint8_t numDots = 3;
        float swarmSpeed = 0.5f;
        float swarmSpread = 0.5f;
        ModConfig modSwarmSpread = {10, 1.0f, 1.0f};       // modTimer, modRate, modLevel 
        float dotDiam = 1.5f;
        uint8_t numActiveTimers = 11;
    };

    SwarmingDotsParams swarmingDots;

    // Variable number of dots moving in a loose shifting group.
    // Uses calculate_modulators() with 2 timers per dot (X and Y).
    // swarmSpread controls grouping (0 = clustered, 1 = independent, >1 = wide).
    // Max 5 dots (num_timers=10, 2 timers per dot).
    static void emitSwarmingDots(float t) {
        const uint8_t n = swarmingDots.numDots;
        const float fNumDots = static_cast<float>(n);

        const ModConfig& spreadMod = swarmingDots.modSwarmSpread;

        // -----------------------------------------------------------------
        // 1) Plumbing: configure timer channels
        // -----------------------------------------------------------------

        // Parameter-owned modulation timer
        timings.ratio[spreadMod.modTimer] = 0.00055f * spreadMod.modRate;

        // Structural per-dot motion timers:
        // 2 timers per dot: [d*2] = X, [d*2+1] = Y
        // Similar ratios keep dots moving at comparable speeds;
        // irrational relationships prevent exact repetition.
        static const float baseRatios[10] = {
            0.00173f, 0.00131f,   // dot 0: X, Y
            0.00197f, 0.00149f,   // dot 1: X, Y
            0.00211f, 0.00113f,   // dot 2: X, Y
            0.00157f, 0.00189f,   // dot 3: X, Y
            0.00223f, 0.00167f    // dot 4: X, Y
        };

        // Offsets: each dot's Y is phase-shifted from its X
        // to create elliptical paths instead of diagonal lines
        static const float baseOffsets[10] = {
            0.0f,  900.0f,    // dot 0
            600.0f, 1700.0f,    // dot 1
            1300.0f, 2400.0f,    // dot 2
            1900.0f, 3100.0f,    // dot 3
            2600.0f, 3800.0f     // dot 4
        };

        const uint8_t numMotionTimers = n * 2;
        for (uint8_t i = 0; i < numMotionTimers; i++) {
            timings.ratio[i]  = baseRatios[i] * swarmingDots.swarmSpeed;
            timings.offset[i] = baseOffsets[i];
        }

        calculate_modulators(timings, swarmingDots.numActiveTimers);

        // -----------------------------------------------------------------
        // 2) Signal acquisition: sample structural motion signals
        // -----------------------------------------------------------------
        float dotX[5], dotY[5];
        float cenX = 0.0f;
        float cenY = 0.0f;

        for (uint8_t d = 0; d < n; d++) {
            const uint8_t xTimer = d * 2;
            const uint8_t yTimer = d * 2 + 1;

            dotX[d] = move.directional_sine[xTimer];
            dotY[d] = move.directional_sine[yTimer];

            cenX += dotX[d];
            cenY += dotY[d];
        }

        // Group center
        cenX /= fNumDots;
        cenY /= fNumDots;

        // -----------------------------------------------------------------
        // 3) Artistic application: spread modulation
        // -----------------------------------------------------------------

        float modSpread = move.directional_noise_norm[spreadMod.modTimer];

        // Current behavior: spread modulation adds above the base value
        const float spread =
            swarmingDots.swarmSpread +
            (modSpread * spreadMod.modLevel);

        // -----------------------------------------------------------------
        // 4) Rendering
        // -----------------------------------------------------------------
        for (uint8_t d = 0; d < n; d++) {
            const float sx = cenX + spread * (dotX[d] - cenX);
            const float sy = cenY + spread * (dotY[d] - cenY);

            const float cx = (WIDTH  - 1) * 0.5f * (1.0f + sx);
            const float cy = (HEIGHT - 1) * 0.5f * (1.0f + sy);

            const ColorF c = rainbow(t, vizConfig.colorShift, d / fNumDots);
            drawDot(cx, cy, swarmingDots.dotDiam, c.r, c.g, c.b);
        }
    }

    FL_OPTIMIZATION_LEVEL_O3_END
    FL_FAST_MATH_END

}