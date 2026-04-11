#pragma once

// =====================================================
// audioTypes.h — All audio type definitions, constants,
// global instances, and type-init functions.
// =====================================================

#include "fl/audio/audio.h"
#include "fl/audio/fft/fft.h"
//#include "fl/audio/audio_processor.h"
#include "fl/math/math.h"

namespace myAudio {

    using namespace fl;

    //=====================================================================
    // Hybrid audio pipeline toggles
    //=====================================================================

    // When enabled, we process *all* audio buffers drained per render frame,
    // but publish only the newest buffer's snapshot (AudioFrame) each frame.
    // This dramatically reduces missed onsets when render FPS is low.
    //
    // Set to 0 for quick A/B comparison with the legacy "latest-only" drain.
    #ifndef MYAUDIO_HYBRID_AUDIO
    #define MYAUDIO_HYBRID_AUDIO 1
    #endif

    // Reference dt for interpreting legacy per-frame EMA alphas.
    // Most of this audio pipeline was tuned around ~20 FPS (~50ms).
    constexpr float kAlphaRefDtMs = 50.0f;

    // Convert a "reference-frame alpha" into a per-step alpha for arbitrary dt,
    // preserving the *time feel* across varying update cadences.
    //
    // alpha(dt) = 1 - (1 - alpha_ref)^(dt / ref_dt)
    inline float alphaFromRef(float alphaRef, float dtMs, float refDtMs = kAlphaRefDtMs) {
        if (alphaRef <= 0.0f) return 0.0f;
        if (alphaRef >= 1.0f) return 1.0f;
        if (dtMs <= 0.0f || refDtMs <= 0.0f) return 0.0f;
        const float base = 1.0f - alphaRef;
        const float exponent = dtMs / refDtMs;
        const float decay = fl::powf(base, exponent);
        return 1.0f - decay;
    }

    //=====================================================================
    // Constants
    //=====================================================================

    constexpr uint8_t MAX_FFT_BINS = 32;

    // FFT window length (in samples) used for spectral analysis.
    // Note: input DMA blocks are currently 512 samples; we build a 1024-sample
    // window from the most recent filtered audio for better low-frequency bin coverage.
    constexpr uint16_t FFT_WINDOW_SAMPLES = 1024;
    // FFT band range (log-spaced). Tuned for 44.1kHz + 1024-sample FFT window.
    // Targets musical coverage ~60Hz–5kHz while avoiding "empty" LOG_REBIN bins.
    constexpr float FFT_MIN_FREQ = 63.f;
    constexpr float FFT_MAX_FREQ = 5000.f;
    constexpr uint8_t NUM_BUSES = 3;

    // Scale factors: single user control → domain-specific internal values
    // Level (RMS) values are tiny (~0.001-0.02), FFT/dB values are larger (~0.1-0.8)
    constexpr float FLOOR_SCALE_LEVEL = 0.05f;
    constexpr float FLOOR_SCALE_FFT   = 0.3f;
    constexpr float GAIN_SCALE_LEVEL  = 12.0f;
    constexpr float GAIN_SCALE_FFT    = 8.0f;

    //=====================================================================
    // FFT bin configuration
    //=====================================================================

    struct binConfig {
        uint8_t NUM_FFT_BINS;
        bool busBased = false;
    };

    binConfig bin16;
    binConfig bin32;

    void setBinConfig() {
        bin16.NUM_FFT_BINS = 16;
        bin32.NUM_FFT_BINS = 32;
    }

    //=====================================================================
    // Bus — per-frequency-band beat detector + envelope follower
    //=====================================================================

    struct Bus {

        // INPUTS
        float threshold = 0.25f;
        float minBeatInterval = 75.f; //milliseconds
        float expDecayFactor = 0.95f; // for exponential decay
        float rampAttack = 0.0f;
        float rampDecay = 150.f;
        float peakBase = 1.0f;

        // INTERNAL
        uint8_t id = 0;
        bool isActive = false;
        float avgLevel = 0.001f;  // linear scale: fft_pre = bins_raw/32768; tuned for FFT_MAX_FREQ=5000 (was 0.001 at 5000/8000, 0.01 at 16000)
        float energyEMA = 0.0f;
        float normEMA = 0.0f;
        float relativeIncrease = 0.0f;
        uint32_t lastBeat = 0;
        float preNorm = 0.0f;
        float norm = 0.0f;
        float factor = 0.0f;

        // OUTPUTS
        bool newBeat = false;
        float avResponse = 0.0f;
        //float spinRate = 0.0f;
    };

    Bus busA{.id = 0};
    Bus busB{.id = 1};
    Bus busC{.id = 2};

    //=====================================================================
    // BusPreset — declarative per-mode parameter overrides
    //=====================================================================

    struct BusPreset {
        float threshold         = -1.f;   // -1 = don't override
        float minBeatInterval   = -1.f;
        float peakBase          = -1.f;
        float rampAttack        = -1.f;
        float rampDecay         = -1.f;
    };

    inline void applyPreset(Bus& bus, const BusPreset& p) {
        if (p.threshold  >= 0.f) bus.threshold = p.threshold;
        if (p.minBeatInterval  >= 0.f) bus.minBeatInterval = p.minBeatInterval;
        if (p.peakBase   >= 0.f) bus.peakBase = p.peakBase;
        if (p.rampAttack >= 0.f) bus.rampAttack = p.rampAttack;
        if (p.rampDecay  >= 0.f) bus.rampDecay = p.rampDecay;
    }

    void initBus(Bus& bus) {
        // Inputs
        bus.threshold = 0.40f;
        bus.minBeatInterval = 75.f; // At 200 BPM, a 16th note is 75 ms.
                                    // (60,000 ÷ 200 = 300 ms per quarter note, ÷ 4 = 75 ms)
        bus.expDecayFactor = 0.85f;
        bus.rampAttack = 0.f;
        bus.rampDecay = 150.f;
        bus.peakBase = 1.0f;

        // Output/Internal
        bus.newBeat = false;
        bus.isActive = false;
        bus.avgLevel = 0.001f;  // tuned for FFT_MAX_FREQ=5000 (was 0.001 at 5000/8000, 0.01 at 16000)
        bus.energyEMA = 0.0f;
        bus.normEMA = 0.0f;
        bus.lastBeat = 0;
        bus.preNorm = 0.0f;
        bus.norm = 0.0f;
        bus.factor = 0.0f;
        bus.avResponse = 0.0f;
        //bus.spinRate = 0.0f;
    }

    //=====================================================================
    // FFT bin → bus routing
    //=====================================================================

    struct Bin {
        Bus* bus = nullptr;
    };

    Bin bin[MAX_FFT_BINS];

    /* Frequency bin reference (16-bin, log spacing) ------
        f(n) = FFT_MIN_FREQ * (FFT_MAX_FREQ/FFT_MIN_FREQ)^(n/15)
        = 63 * (5000/63)^(n/15)   [1024-sample FFT @ 44100 Hz -> 43.1 Hz linear resolution]
        Bin  Center Hz  Range label
        0    63         sub-bass
        1    84         bass
        2    113        bass
        3    151        bass/upper-bass
        4    202        upper-bass
        5    271        low-mid
        6    362        low-mid
        7    485        mid
        8    649        mid
        9    869        upper-mid
        10   1163       upper-mid
        11   1557       presence
        12   2085       presence
        13   2791       high
        14   3735       high
        15   5000       high
     ---------------------------------------------------*/

    void initBins() {
        for (uint8_t i = 0; i < MAX_FFT_BINS; i++ ) {
            bin[i].bus = nullptr;
        }

        // target: kick drum
        bin[0].bus = &busA;
        bin[1].bus = &busA;
        bin[2].bus = &busA;
        bin[3].bus = &busA;
        //bin[4].bus = &busA;
        //bin[5].bus = &busB;

        // target: snare/mid percussive
        //bin[3].bus = &busB;
        //bin[4].bus = &busB;
        //bin[5].bus = &busB;
        bin[6].bus = &busB;
        bin[7].bus = &busB;
        bin[8].bus = &busB;
        bin[9].bus = &busB;
        bin[10].bus = &busB;

        // target: vocals/"lead instruments"
        //bin[5].bus = &busC;
        //bin[6].bus = &busC;
        //bin[7].bus = &busC;
        bin[8].bus = &busC;
        bin[9].bus = &busC;
        bin[10].bus = &busC;
        bin[11].bus = &busC;
        bin[12].bus = &busC;
        bin[13].bus = &busC;
        //bin[14].bus = &busC;
        //bin[15].bus = &busC;
    }

    //=====================================================================
    // AudioVizConfig — visualization normalization & auto-calibration
    //=====================================================================

    struct AudioVizConfig {
        // Internal values (derived from single user controls via scale factors)
        float audioFloorLevel = 0.0f;
        float audioFloorFft = 0.0f;
        float gainLevel = GAIN_SCALE_LEVEL;
        float gainFft = GAIN_SCALE_FFT;

        bool avLeveler = true;
        float avLevelerTarget = 0.5f;   // P90 ceiling → rms_norm target for loud signals
        bool autoFloor = true;
        float autoFloorAlpha = 0.01f;
        float autoFloorMin = 0.0f;
        float autoFloorMax = 0.5f;
    };

    AudioVizConfig vizConfig;
    float avLevelerValue = 1.0f;

    //=====================================================================
    // AudioFrame — per-frame snapshot of all audio data
    //=====================================================================

    struct AudioFrame {
        bool valid = false;
        uint32_t timestamp = 0;
        float rms_raw = 0.0f;
        float rms = 0.0f;
        float rms_norm = 0.0f;
        float rms_factor = 0.0f;
        float energy = 0.0f;
        float peak = 0.0f;
        float voxConf = 0.0f;
        float voxConfEMA = 0.0f;
        float smoothedVoxConf = 0.0f;
        float scaledVoxConf = 0.0f;
        float voxApprox = 0.0f;

        const fl::audio::fft::Bins* fft = nullptr;
        bool fft_norm_valid = false;
        float fft_pre[MAX_FFT_BINS] = {0};
        float fft_norm[MAX_FFT_BINS] = {0};
        fl::span<const int16_t> pcm;
        Bus busA;
        Bus busB;
        Bus busC;
    };

    //=====================================================================
    // Core audio objects
    //=====================================================================

    fl::audio::Sample currentSample;      // Raw sample from I2S (kept for diagnostics)
    fl::audio::Sample filteredSample;     // Spike-filtered sample for processing
    fl::audio::Processor audioProcessor;
    bool audioProcessingInitialized = false;

    // Buffer for filtered PCM data
    int16_t filteredPcmBuffer[512];

    //=====================================================================
    // Pipeline state variables
    //=====================================================================

    bool noiseGateOpen = false;
    uint16_t lastAudioBuffersDrained = 0;   // how many DMA buffers were drained last frame
    float lastBlockRms = 0.0f;
    float lastAutoGainCeil = 0.0f;
    float lastAutoGainDesired = 0.0f;
    uint16_t lastValidSamples = 0;
    uint16_t lastClampedSamples = 0;
    int16_t lastPcmMin = 0;
    int16_t lastPcmMax = 0;
    bool busBased = true;
    
    //=====================================================================
    // Vocal detection state (shared between pipeline and avHelpers)
    //=====================================================================

    /*float voxConf = 0.f;
    float voxConfEMA = 0.0f;
    float smoothedVoxConf = 0.0f;
    float scaledVoxConf = 0.0f;*/
    float voxApprox = 0.0f;
    //float voxApproxEMA = 0.0f;

} // namespace myAudio
