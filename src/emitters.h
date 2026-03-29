#pragma once

// ═══════════════════════════════════════════════════════════════════
//  EMITTERS — emitters.h
// ═══════════════════════════════════════════════════════════════════

#include "colorTrailsTypes.h"
#include "modulators.h"

namespace colorTrails {

    // ═══════════════════════════════════════════════════════════════════
    //  ORBITAL DOTS
    // ═══════════════════════════════════════════════════════════════════

    struct OrbitalDotsParams {
        uint8_t numDots    = 3;
        float orbitSpeed = 2.0f;
        ModConfig modOrbitSpeed = {0, 1.0f, 1.0f};       // modTimer, modRate, modLevel                 
        float dotDiam    = 1.5f;
        float orbitDiam  = MIN_DIMENSION * 0.3f; 
        ModConfig modOrbitDiam = {1, 1.0f, 1.0f};         // modTimer, modRate, modLevel
        uint8_t numActiveTimers = 2;
    };

    OrbitalDotsParams orbitalDots; 

    static void emitOrbitalDots(float t) {
        static float orbitAngle = 0.0f;
        
        static unsigned long lastOrbitMs = 0;
        const unsigned long now = fl::millis();
        if (lastOrbitMs == 0) {
            lastOrbitMs = now;
        }
        const float dt = (now - lastOrbitMs) * 0.001f;
        lastOrbitMs = now;
        
        const ModConfig& speedMod = orbitalDots.modOrbitSpeed;
        const ModConfig& diamMod  = orbitalDots.modOrbitDiam;

        // -----------------------------------------------------------------
        // 1) Plumbing: assign timer rates from the parameter configs
        // -----------------------------------------------------------------
        timings.ratio[speedMod.modTimer] = 0.00006f * speedMod.modRate;
        timings.ratio[diamMod.modTimer]  = 0.0005f  * diamMod.modRate;

        calculate_modulators(timings, orbitalDots.numActiveTimers);

        // -----------------------------------------------------------------
        // 2) Signal acquisition: get normalized modulation signals
        //    directional_noise is centered bipolar noise in roughly [-1, 1]
        // -----------------------------------------------------------------
        const float speedSignal = move.directional_noise[speedMod.modTimer];
        const float diamSignal  = move.directional_noise[diamMod.modTimer];

        // -----------------------------------------------------------------
        // 3) Artistic application: decide what those signals mean
        // -----------------------------------------------------------------

        // Speed modulation intentionally allows reversal:
        // modLevel = 0 -> base speed only
        // modLevel = 1 -> full bipolar modulation, including negative speed
        const float currentSpeed =
            orbitalDots.orbitSpeed *
            ((1.0f - speedMod.modLevel) + speedMod.modLevel * speedSignal);

        orbitAngle += currentSpeed * dt;

        // Diameter modulation: centered multiplicative breathing around base orbit radius
        float radiusScale = 1.0f + diamMod.modLevel * 0.85f * diamSignal;

        // Prevent collapse into center
        radiusScale = fmaxf(radiusScale, 0.35f);

        const float fNumDots = static_cast<float>(orbitalDots.numDots);
        const float ocx = WIDTH * 0.5f - 0.5f;
        const float ocy = HEIGHT * 0.5f - 0.5f;

        const float minOrbit = orbitalDots.dotDiam * 1.5f;
        const float orad = fmaxf(orbitalDots.orbitDiam * radiusScale, minOrbit);

        // -----------------------------------------------------------------
        // 4) Rendering
        // -----------------------------------------------------------------
        for (int i = 0; i < orbitalDots.numDots; i++) {
            const float a = orbitAngle + i * (2.0f * CT_PI / fNumDots);
            const float cx = ocx + fl::cosf(a) * orad;
            const float cy = ocy + fl::sinf(a) * orad;

            const ColorF c = rainbow(t, vizConfig.colorShift, i / fNumDots);
            drawDot(cx, cy, orbitalDots.dotDiam, c.r, c.g, c.b);
        }
    }


    // ═══════════════════════════════════════════════════════════════════
    //  SWARMING DOTS
    // ═══════════════════════════════════════════════════════════════════

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
    

    // ═══════════════════════════════════════════════════════════════════
    //  AUDIO DOTS
    // ═══════════════════════════════════════════════════════════════════

    const myAudio::AudioFrame* cFrame = nullptr;

    inline void getAudio(myAudio::binConfig& b) {
        b.busBased = true;
        cFrame = &myAudio::updateAudioFrame(b);
    }

    struct AudioDotsParams {
        uint8_t dotDiam = 3.0f;
        //uint8_t numActiveTimers = 0;
    };

    AudioDotsParams audioDots;

    static void emitAudioDots(float t) {

        if (audioEnabled){
            myAudio::binConfig& b = maxBins ? myAudio::bin32 : myAudio::bin16;
            getAudio(b);
        }

        // On each new beat, spawn a dot at a random grid position
        if (myAudio::busC.newBeat) {
            float cx = random8(0, WIDTH  - 1) + random8() / 255.0f;
            float cy = random8(0, HEIGHT - 1) + random8() / 255.0f;
            // Color from rainbow based on current time
            ColorF c = rainbow(t, vizConfig.colorShift, random8() / 255.0f);
            drawDot(cx, cy, audioDots.dotDiam, c.r, c.g, c.b);
        }
    }

    
    // ═══════════════════════════════════════════════════════════════════
    //  LISSAJOUS LINE
    // ═══════════════════════════════════════════════════════════════════

    struct LissajousParams {
        float lineSpeed = 0.35f;
        float lineAmp = (MIN_DIMENSION - 4) * 0.75f;
        //uint8_t numActiveTimers;
    };

    LissajousParams lissajous;

    static void emitLissajousLine(float t) {
        const float cx = (WIDTH  - 1) * 0.5f;
        const float cy = (HEIGHT - 1) * 0.5f;
        float s = lissajous.lineSpeed;
        const float amp = lissajous.lineAmp;

        float lx1 = cx + (amp + 1.5f) * fl::sinf(t * s * 1.13f + 0.20f);
        float ly1 = cy + (amp + 0.5f) * fl::sinf(t * s * 1.71f + 1.30f);
        float lx2 = cx + (amp + 2.0f) * fl::sinf(t * s * 1.89f + 2.20f);
        float ly2 = cy + (amp + 1.0f) * fl::sinf(t * s * 1.37f + 0.70f);

        drawAASubpixelLine(lx1, ly1, lx2, ly2, t, vizConfig.colorShift);
        ColorF ca = rainbow(t, vizConfig.colorShift, 0.0f);
        ColorF cb = rainbow(t, vizConfig.colorShift, 1.0f);
        drawAAEndpointDisc(lx1, ly1, ca.r, ca.g, ca.b, 0.85f);
        drawAAEndpointDisc(lx2, ly2, cb.r, cb.g, cb.b, 0.85f);
    }

    // ═══════════════════════════════════════════════════════════════════
    //  NOISE KALEIDO
    // ═══════════════════════════════════════════════════════════════════

    struct NoiseKaleidoParams {
        float driftSpeed = 0.35f;   // noise field drift speed
    };

    NoiseKaleidoParams noiseKaleido;

    // 6-octave fractal Brownian motion using our Perlin2D
    static float fbm2d(float x, float y) {
        float total = 0.0f;
        float amp = 1.0f;
        float freq = 1.0f;
        float norm = 0.0f;
        for (int i = 0; i < 6; i++) {
            total += noise2X.noise(x * freq, y * freq) * amp;
            norm += amp;
            amp *= 0.5f;
            freq *= 2.0f;
        }
        return total / norm;
    }

    // Circular hue interpolation on [0, 1)
    static float lerpHue(float h0, float h1, float u) {
        float d = fmodPos(h1 - h0 + 0.5f, 1.0f) - 0.5f;
        return fmodPos(h0 + d * u, 1.0f);
    }

    static void emitNoiseKaleido(float t) {
        const float scale = 0.0375f;
        const float speed = noiseKaleido.driftSpeed;

        const int baseX = (WIDTH / 2) + 1;
        const int baseY = (HEIGHT / 2) + 1;
        const int mirrorLimitX = baseX - 2;
        const int mirrorLimitY = baseY - 2;

        // Two layers with different drifts and color phases
        struct LayerCfg { float offX, offY, phase, span; };
        const LayerCfg layers[2] = {
            {  t * 0.55f * speed, -t * 0.43f * speed,        0.00f,  0.16f },
            { -t * 0.34f * speed + 9.7f, t * 0.61f * speed + 4.1f, 0.48f, -0.18f },
        };

        for (int layer = 0; layer < 2; layer++) {
            const LayerCfg& L = layers[layer];
            float h0 = fmodPos(t * vizConfig.colorShift + L.phase, 1.0f);
            float h1 = fmodPos(h0 + L.span, 1.0f);

            for (int gy = 0; gy < baseY; gy++) {
                for (int gx = 0; gx < baseX; gx++) {
                    float sx = clampf(gx + 0.37f, 0.0f, (float)(baseX - 1) - 1e-6f);
                    float sy = clampf(gy + 0.29f, 0.0f, (float)(baseY - 1) - 1e-6f);

                    float n = fbm2d(sx * scale + L.offX, sy * scale + L.offY);

                    // Only draw in narrow noise band [0.0, 0.2] for sparse deposition
                    if (n < 0.0f || n > 0.2f) continue;

                    float u = n / 0.2f;

                    // Tent profile: peak at center of band, gamma-shaped
                    float tprof = 1.0f - fl::fabsf(u - 0.5f) * 2.0f;
                    tprof = fl::powf(tprof, 0.65f);
                    float gain = clampf(0.80f + 0.20f * tprof, 0.0f, 1.0f);

                    // Color from hue interpolation across noise range
                    float hm = lerpHue(h0, h1, u);
                    ColorF c = useRainbow ? hsvRainbow(hm) : hsvSpectrum(hm);
                    float cr = c.r * gain;
                    float cg = c.g * gain;
                    float cb = c.b * gain;

                    // Bilinear deposition with kaleidoscopic mirroring
                    int xi = (int)fl::floorf(sx);
                    int yi = (int)fl::floorf(sy);
                    float fx = sx - xi;
                    float fy = sy - yi;

                    float weights[4] = {
                        (1.0f - fx) * (1.0f - fy),
                        fx * (1.0f - fy),
                        (1.0f - fx) * fy,
                        fx * fy,
                    };
                    int tapX[4] = { xi, xi + 1, xi, xi + 1 };
                    int tapY[4] = { yi, yi, yi + 1, yi + 1 };

                    for (int tap = 0; tap < 4; tap++) {
                        if (weights[tap] <= 0.0f) continue;
                        int tx = tapX[tap];
                        int ty = tapY[tap];
                        float w = weights[tap];
                        int mx = WIDTH  - 1 - tx;
                        int my = HEIGHT - 1 - ty;

                        // Original quadrant
                        blendPixelWeighted(tx, ty, cr, cg, cb, w);
                        // Horizontal mirror
                        if (tx <= mirrorLimitX)
                            blendPixelWeighted(mx, ty, cr, cg, cb, w);
                        // Vertical mirror
                        if (ty <= mirrorLimitY)
                            blendPixelWeighted(tx, my, cr, cg, cb, w);
                        // Diagonal mirror
                        if (tx <= mirrorLimitX && ty <= mirrorLimitY)
                            blendPixelWeighted(mx, my, cr, cg, cb, w);
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  RAINBOW BORDER
    // ═══════════════════════════════════════════════════════════════════


    //struct BorderRectParams {};
    //BorderRectParams borderRect;

    static void emitRainbowBorder(float t) {
        const int total = 2 * (WIDTH + HEIGHT) - 4;
        int idx = 0;
        // Top edge: left to right
        for (int x = 0; x < WIDTH; x++) {
            ColorF c = rainbow(t, vizConfig.colorShift, (float)idx / total);
            gR[0][x] = c.r; gG[0][x] = c.g; gB[0][x] = c.b;
            idx++;
        }
        // Right edge: top+1 to bottom
        for (int y = 1; y < HEIGHT; y++) {
            ColorF c = rainbow(t, vizConfig.colorShift, (float)idx / total);
            gR[y][WIDTH-1] = c.r; gG[y][WIDTH-1] = c.g; gB[y][WIDTH-1] = c.b;
            idx++;
        }
        // Bottom edge: right-1 to left
        for (int x = WIDTH - 2; x >= 0; x--) {
            ColorF c = rainbow(t, vizConfig.colorShift, (float)idx / total);
            gR[HEIGHT-1][x] = c.r; gG[HEIGHT-1][x] = c.g; gB[HEIGHT-1][x] = c.b;
            idx++;
        }
        // Left edge: bottom-1 to top+1
        for (int y = HEIGHT - 2; y > 0; y--) {
            ColorF c = rainbow(t, vizConfig.colorShift, (float)idx / total);
            gR[y][0] = c.r; gG[y][0] = c.g; gB[y][0] = c.b;
            idx++;
        }
    }

} // namespace colorTrails
