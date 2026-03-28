#pragma once

// ═══════════════════════════════════════════════════════════════════
//  NOISE FLOW FIELD — flow_noise.h
// ═══════════════════════════════════════════════════════════════════
//
//  Self-contained noise flow field implementation.
//  Includes colorTrailsTypes.h for shared types and instances.
//  cVar bridge helpers (pushFlowDefaultsToCVars / syncFlowFromCVars)
//  live in colorTrails_detail.hpp since they depend on bleControl.h.

#include "colorTrailsTypes.h"
#include "modulators.h"

namespace colorTrails {

    struct NoiseFlowParams {
        float xSpeed = -0.25f;   // Noise scroll speed  (column axis)
        float ySpeed = -0.25f;   // Noise scroll speed  (row axis)
        float xAmp = 1.00f;   // Noise amplitude     (column axis)
        float yAmp = 1.00f;   // Noise amplitude     (row axis)
        float xFreq = 0.33f;   // Noise spatial scale (column axis) (aka "xScale")
        float yFreq = 0.32f;   // Noise spatial scale (row axis) (aka "yScale")
        float xShift = 1.8f;    // Max horizontal shift per row  (pixels)
        float yShift = 1.8f;    // Max vertical shift per column (pixels)
        float noiseFreq = 0.23f;  // need to build out and enable BLE/UI control
            
        // Shared amplitude breathing control:
        // modTimer     -> X amplitude channel
        // modTimer + 1 -> Y amplitude channel
        ModConfig modAmp = {0, 1.0f, 0.0f};   // modTimer, modRate, modLevel 
        uint8_t numActiveTimers = 2;
    };
   
    NoiseFlowParams noiseFlow;

    static void sampleProfile2D(const Perlin2D &n, float t, float speed,
                                float amp, float scale, int count, float *out) {
        //const float freq   = 0.23f;
        const float scrollY = t * speed;
        for (int i = 0; i < count; i++) {
            float v = n.noise(i * noiseFlow.noiseFreq * scale, scrollY);
            out[i]  = clampf(v * amp, -1.0f, 1.0f);
        }
    }

    // --- Prepare: build noise profiles, apply modulator(s) ---

    static void noiseFlowPrepare(float t) {
        const ModConfig& ampMod = noiseFlow.modAmp;

        // Reserve a structural pair of timer channels:
        // X amplitude uses modTimer, Y amplitude uses modTimer + 1
        const uint8_t xAmpTimer = ampMod.modTimer;
        const uint8_t yAmpTimer = ampMod.modTimer + 1;

        // -----------------------------------------------------------------
        // 1) Plumbing: configure shared breathing channels
        //    Same user-controlled rate, slightly different internal timing
        //    and offset so X/Y don't inhale and exhale identically.
        // -----------------------------------------------------------------
        timings.ratio[xAmpTimer]  = 0.00043f * ampMod.modRate;
        timings.offset[xAmpTimer] = 0.0f;

        timings.ratio[yAmpTimer]  = 0.00049f * ampMod.modRate;
        timings.offset[yAmpTimer] = 1700.0f;

        calculate_modulators(timings, noiseFlow.numActiveTimers);

        // -----------------------------------------------------------------
        // 2) Signal acquisition: centered bipolar breathing signals [-1, 1]
        // -----------------------------------------------------------------
        const float xBreath = move.directional_noise[xAmpTimer];
        const float yBreath = move.directional_noise[yAmpTimer];

        // -----------------------------------------------------------------
        // 3) Artistic application: multiplicative breathing around base amp
        //
        // Centered around 1.0 so the base value remains the midpoint.
        // 0.85f gives a strong but still usable swing without going negative
        // in normal 0..1 modLevel use.
        // -----------------------------------------------------------------
        const float level = ampMod.modLevel;
        const float breathDepth = 0.85f;

        float workXAmp = noiseFlow.xAmp * (1.0f + level * breathDepth * xBreath);
        float workYAmp = noiseFlow.yAmp * (1.0f + level * breathDepth * yBreath);

        // Keep amplitudes non-negative
        workXAmp = fmaxf(0.0f, workXAmp);
        workYAmp = fmaxf(0.0f, workYAmp);

        sampleProfile2D(noise2X, t, noiseFlow.xSpeed, workXAmp,
                        noiseFlow.xFreq, WIDTH, xProf);

        sampleProfile2D(noise2Y, t, noiseFlow.ySpeed, workYAmp,
                        noiseFlow.yFreq, HEIGHT, yProf);
    }


    // --- Advect: two-pass fractional advection (bilinear interpolation) + fade ---

    static void noiseFlowAdvect(float dt) {
        // Frame-rate-independent fade: half-life = persistence seconds
        float fade = fl::powf(0.5f, dt / vizConfig.persistence);

        // Pass 1 — horizontal row shift  (Y-noise drives X movement)
        for (int y = 0; y < HEIGHT; y++) {
            float sh = yProf[y] * noiseFlow.xShift;
            for (int x = 0; x < WIDTH; x++) {
                float sx  = fmodPos((float)x - sh, (float)WIDTH);
                int   ix0 = (int)fl::floorf(sx) % WIDTH;
                int   ix1 = (ix0 + 1) % WIDTH;
                float f   = sx - fl::floorf(sx);
                float inv = 1.0f - f;
                tR[y][x] = gR[y][ix0] * inv + gR[y][ix1] * f;
                tG[y][x] = gG[y][ix0] * inv + gG[y][ix1] * f;
                tB[y][x] = gB[y][ix0] * inv + gB[y][ix1] * f;
            }
        }

        // Pass 2 — vertical column shift  (X-noise drives Y movement) + dim
        for (int x = 0; x < WIDTH; x++) {
            float sh = xProf[x] * noiseFlow.yShift;
            for (int y = 0; y < HEIGHT; y++) {
                float sy  = fmodPos((float)y - sh, (float)HEIGHT);
                int   iy0 = (int)fl::floorf(sy) % HEIGHT;
                int   iy1 = (iy0 + 1) % HEIGHT;
                float f   = sy - fl::floorf(sy);
                float inv = 1.0f - f;
                // truncate to integer — Python's Pygame surface stores uint8,
                // so int(value) kills sub-1.0 residuals every frame.
                gR[y][x] = fl::floorf((tR[iy0][x] * inv + tR[iy1][x] * f) * fade);
                gG[y][x] = fl::floorf((tG[iy0][x] * inv + tG[iy1][x] * f) * fade);
                gB[y][x] = fl::floorf((tB[iy0][x] * inv + tB[iy1][x] * f) * fade);
            }
        }
    }

} // namespace colorTrails
