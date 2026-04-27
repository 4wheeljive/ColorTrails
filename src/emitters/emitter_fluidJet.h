#pragma once

// ═══════════════════════════════════════════════════════════════════
//  FLUID JET EMITTER — emitter_fluidJet.h
// ═══════════════════════════════════════════════════════════════════
//
//  Injects dye (RGB) and momentum (u,v) at a fixed bottom-center
//  position via a 3-layered Gaussian splat.  Designed to be paired
//  with flow_fluid; injects momentum directly into the velocity field.
//
//  Ported from FluidApp.emit_stationary_source() in
//  colorTrailsOrig/navier_stokes_1.py.

#include "flowFieldsTypes.h"
#include "modulators.h"
#include "../flows/flow_fluid.h"   // u, v arrays + fluidAddVelocity

namespace flowFields {
    FL_FAST_MATH_BEGIN
    FL_OPTIMIZATION_LEVEL_O3_BEGIN

    struct FluidJetParams {
        float jetDensity   = 120.0f;     // dye magnitude (per layer-weighted)
        float jetForce     = 0.87f;      // velocity magnitude
        float jetRadius    = 4.0f;       // gaussian splat radius (cells)
        float jetSpread    = 0.0f;       // side-injection lateral velocity
        float jetAngle     = 0.0f;       // base direction (radians; 0 = straight up)
        float jetHueSpeed  = 0.69f;      // hue rotation rate (Hz)

        ModConfig modJetForce = {0, 0.5f, 0.0f};   // modTimer, modRate, modLevel
        ModConfig modAngle    = {1, 0.5f, 0.0f};   // modLevel: 0 = no movement, 2 = full ±90°
    };

    FluidJetParams fluidJet;

    // 2D Gaussian splat: writes dye to gR/gG/gB and momentum to u/v.
    // Uses fastpow as a stand-in for exp() — ~5% error is fine for visual falloff.
    static void fluidJetSplat(float cx, float cy, float radius,
                              float dyeR, float dyeG, float dyeB,
                              float velX, float velY) {
        const float r2  = radius * radius * 0.6f;
        const float invR2 = 1.0f / r2;
        int x0 = max(0,           (int)fl::floorf(cx - radius));
        int x1 = min(WIDTH  - 1,  (int)fl::ceilf (cx + radius));
        int y0 = max(0,           (int)fl::floorf(cy - radius));
        int y1 = min(HEIGHT - 1,  (int)fl::ceilf (cy + radius));

        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                float dx = (x + 0.5f) - cx;
                float dy = (y + 0.5f) - cy;
                float d2 = dx * dx + dy * dy;
                // exp(-d2/r2) approximated via fastpow(e, -d2/r2)
                float w = fastpow(2.71828183f, -d2 * invR2);
                if (w < 0.005f) continue;

                gR[y][x] += dyeR * w;
                gG[y][x] += dyeG * w;
                gB[y][x] += dyeB * w;
                u[y][x]  += velX * w;
                v[y][x]  += velY * w;
            }
        }
    }

    static void emitFluidJet() {
        const ModConfig& forceMod = fluidJet.modJetForce;
        const ModConfig& angleMod = fluidJet.modAngle;

        // ─── 1) Plumbing: configure timer channels ─────────────────
        timings.ratio[forceMod.modTimer] = 0.0004f  * forceMod.modRate;
        timings.ratio[angleMod.modTimer] = 0.00045f * angleMod.modRate;
        calculate_modulators(timings, 2);

        // ─── 2) Signal acquisition ─────────────────────────────────
        const float forceSignal = move.directional_noise[forceMod.modTimer];
        const float angleSignal = move.directional_noise[angleMod.modTimer];

        // ─── 3) Artistic application ───────────────────────────────
        // Force: orbitalDots-style bipolar modulation
        const float currentForce = fluidJet.jetForce *
            ((1.0f - forceMod.modLevel) + forceMod.modLevel * forceSignal);

        // Angle: noise-based offset around base direction.
        // Coefficient π/4 per modLevel unit → modLevel=2 reaches full ±π/2 (±90°).
        constexpr float ANGLE_SCALE = CT_2PI * 0.125f;   // π/4
        const float angleOffset = angleMod.modLevel * ANGLE_SCALE * angleSignal;

        // Wrap final angle to [0, 2π) for sincos_fast (UB for negative inputs).
        constexpr float INV_2PI = 1.0f / CT_2PI;
        float angle = fluidJet.jetAngle + angleOffset;
        angle -= fl::floorf(angle * INV_2PI) * CT_2PI;

        // Direction decomposition: angle 0 = straight up (negative y)
        SinCosResult sc = sincos_fast(angle);
        const float dirX =  sc.sin_val;
        const float dirY = -sc.cos_val;
        const float velX = dirX * currentForce;
        const float velY = dirY * currentForce;

        // Hue from time × hueSpeed
        ColorF c = rainbow(t, fluidJet.jetHueSpeed, 0.0f);
        const float dyeR = c.r * fluidJet.jetDensity * (1.0f / 255.0f);
        const float dyeG = c.g * fluidJet.jetDensity * (1.0f / 255.0f);
        const float dyeB = c.b * fluidJet.jetDensity * (1.0f / 255.0f);

        // Jet position: bottom-center
        const float jx = (float)WIDTH * 0.5f;
        const float jy = (float)HEIGHT - 3.0f;

        // ─── 4) 3-layered Gaussian splat ──────────────────────────
        // Each layer is shifted along the jet axis to extend the plume.
        const float r = fluidJet.jetRadius;
        // Core layer: 55% density, 100% velocity
        fluidJetSplat(jx, jy, r,
                      dyeR * 0.55f, dyeG * 0.55f, dyeB * 0.55f,
                      velX,         velY);
        // Middle layer: 30% density, 82% velocity, shifted 1 cell along jet
        fluidJetSplat(jx + dirX * 1.2f, jy + dirY * 1.2f, r,
                      dyeR * 0.30f, dyeG * 0.30f, dyeB * 0.30f,
                      velX * 0.82f, velY * 0.82f);
        // Outer layer: 15% density, 65% velocity, shifted further
        fluidJetSplat(jx + dirX * 2.2f, jy + dirY * 2.2f, r,
                      dyeR * 0.15f, dyeG * 0.15f, dyeB * 0.15f,
                      velX * 0.65f, velY * 0.65f);

        // ─── 5) Side injections (lateral push outward) ────────────
        if (fluidJet.jetSpread > 0.0f) {
            // Perpendicular to jet axis: rotate (dirX,dirY) by 90°: (-dirY, dirX)
            const float perpX = -dirY;
            const float perpY =  dirX;
            const float side = fluidJet.jetSpread;
            // Left side: push left (negative perp)
            fluidJetSplat(jx - perpX * 1.5f, jy - perpY * 1.5f, r * 0.7f,
                          dyeR * 0.15f, dyeG * 0.15f, dyeB * 0.15f,
                          -perpX * side * 0.35f, -perpY * side * 0.35f);
            // Right side: push right (positive perp)
            fluidJetSplat(jx + perpX * 1.5f, jy + perpY * 1.5f, r * 0.7f,
                          dyeR * 0.15f, dyeG * 0.15f, dyeB * 0.15f,
                           perpX * side * 0.35f,  perpY * side * 0.35f);
        }
    }

    FL_OPTIMIZATION_LEVEL_O3_END
    FL_FAST_MATH_END

} // namespace flowFields
