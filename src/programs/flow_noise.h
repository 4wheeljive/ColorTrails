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

namespace colorTrails {

    struct NoiseFlowParams {
        float xSpeed     = -1.00f;   // Noise scroll speed  (column axis)
        float ySpeed     = -1.00f;   // Noise scroll speed  (row axis)
        float xAmp       =  1.00f;   // Noise amplitude     (column axis)
        float yAmp       =  1.00f;   // Noise amplitude     (row axis)
        float xFreq      =  0.33f;   // Noise spatial scale (column axis) (aka "xScale")
        float yFreq      =  0.32f;   // Noise spatial scale (row axis) (aka "yScale")
        float xShift     =  1.8f;    // Max horizontal shift per row  (pixels)
        float yShift     =  1.8f;    // Max vertical shift per column (pixels)
        //bool  use2DNoise =  true;    // false = 1D Perlin, true = 2D Perlin
    };
   
    NoiseFlowParams     noiseFlow;

    // --- Profile builders ---

    static void sampleProfile1D(const Perlin1D &n, float t, float speed,
                                float amp, float scale, int count, float *out) {
        const float freq  = 0.23f;
        const float phase = t * speed;
        for (int i = 0; i < count; i++) {
            float v = n.noise(i * freq * scale + phase);
            out[i]  = clampf(v * amp, -1.0f, 1.0f);
        }
    }

    static void sampleProfile2D(const Perlin2D &n, float t, float speed,
                                float amp, float scale, int count, float *out) {
        const float freq   = 0.23f;
        const float scrollY = t * speed;
        for (int i = 0; i < count; i++) {
            float v = n.noise(i * freq * scale, scrollY);
            out[i]  = clampf(v * amp, -1.0f, 1.0f);
        }
    }

    // --- Prepare: build noise profiles, apply modulator, apply flips ---

    static void noiseFlowPrepare(float t) {
        // Working copies of amplitude (modulator may alter these)
        float workXAmp = noiseFlow.xAmp;
        float workYAmp = noiseFlow.yAmp;

        if (vizConfig.useAmpMod) {
            applyAmpModulation(t, workXAmp, workYAmp);
        }

        // Build noise profiles
       // if (noiseFlow.use2DNoise) {
            sampleProfile2D(noise2X, t, noiseFlow.xSpeed, workXAmp,
                            noiseFlow.xFreq, WIDTH,  xProf);
            sampleProfile2D(noise2Y, t, noiseFlow.ySpeed, workYAmp,
                            noiseFlow.yFreq, HEIGHT, yProf);
        /*} else {
            sampleProfile1D(noiseX, t, noiseFlow.xSpeed, workXAmp,
                            noiseFlow.xFreq, WIDTH,  xProf);
            sampleProfile1D(noiseY, t, noiseFlow.ySpeed, workYAmp,
                            noiseFlow.yFreq, HEIGHT, yProf);
        }*/

        // Apply axis flip toggles
        if (vizConfig.flipY) {
            for (int i = 0; i < WIDTH / 2; i++) {
                float tmp = xProf[i];
                xProf[i] = xProf[WIDTH - 1 - i];
                xProf[WIDTH - 1 - i] = tmp;
            }
        }
        if (vizConfig.flipX) {
            for (int i = 0; i < HEIGHT / 2; i++) {
                float tmp = yProf[i];
                yProf[i] = yProf[HEIGHT - 1 - i];
                yProf[HEIGHT - 1 - i] = tmp;
            }
        }
    }

    // --- Advect: two-pass fractional advection (bilinear interpolation) + fade ---

    static void noiseFlowAdvect(float dt) {
        // The original Python applied fadeRate once per frame at 60 FPS.
        // Scale the exponent by actual dt so decay rate is frame-rate-independent.
        float fadePerSec = fl::powf(vizConfig.fadeRate, 60.0f);
        float fade = fl::powf(fadePerSec, dt);

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
