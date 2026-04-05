//================================================================================================================
/*
CREDITS:
 - flowFields based on visualizer by Stefan Petrick first introduced here:
			https://www.reddit.com/r/FastLED/comments/1rny5j3/i_used_codex_for_the_first_time/
*/
//===============================================================================================================

#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_timer.h>
#endif

/*// ESP32-P4 has no on-chip BT controller (SOC_BT_SUPPORTED is undefined),
// so the Arduino core doesn't compile btStarted(). esp-nimble-cpp calls it
// when CONFIG_ENABLE_ARDUINO_DEPENDS is set (forced on by Arduino Kconfig).
// Provide a stub — on P4, BT is hosted on the C6 via esp-hosted VHCI.
#if defined(CONFIG_IDF_TARGET_ESP32P4) && !defined(SOC_BT_SUPPORTED)
extern "C" bool btStarted() { return false; }
#endif*/

//#define FASTLED_OVERCLOCK 1.2
#include <FastLED.h>

//#include <FS.h>
//#include "LittleFS.h"
//#define FORMAT_LITTLEFS_IF_FAILED true 

bool debug = true;
bool audioEnabled = false;
bool audioLatencyDiagnostics = false;

/*
#include "profiler.h"
#ifdef PROFILING_ENABLED
	FrameProfiler profiler;
#endif
*/

#define BIG_BOARD
//#undef BIG_BOARD

#define PIN0 2

//*********************************************

#ifdef BIG_BOARD 
		
	/*
	#include "reference/matrixMap_32x48_3pin.h" 
	#define PIN1 3
    #define PIN2 4
    #define HEIGHT 32 
    #define WIDTH 48
    #define NUM_STRIPS 3
    #define NUM_LEDS_PER_STRIP 512
	*/

	///*
	#include "reference/matrixMap_48x64_6pin.h" 
	#define PIN1 3
    #define PIN2 4
	#define PIN3 50
	#define PIN4 49
	#define PIN5 5
    #define HEIGHT 48 
    #define WIDTH 64
    #define NUM_STRIPS 6
    #define NUM_LEDS_PER_STRIP 512
	//*/
			
#else 
	
	#include "reference/matrixMap_22x22.h"
	#define HEIGHT 22 
    #define WIDTH 22
    #define NUM_STRIPS 1
    #define NUM_LEDS_PER_STRIP 484

#endif

//*********************************************

#define NUM_LEDS ( WIDTH * HEIGHT )
const uint16_t MIN_DIMENSION = FL_MIN(WIDTH, HEIGHT);
const uint16_t MAX_DIMENSION = FL_MAX(WIDTH, HEIGHT);

fl::CRGB leds[NUM_LEDS];
uint16_t ledNum = 0;
static bool sLoopEnteredLogged = false;
static bool sLoopProbeLogged = false;
static bool sBeforeFlowLogged = false;
static bool sAfterFlowLogged = false;
static bool sAfterShowLogged = false;
static bool sAdvRestartPending = false;
static uint32_t sAdvRestartAtMs = 0;
static uint32_t sFrameCount = 0;
static uint64_t sNextFrameAtUs = 0;
static uint64_t sLastStatusUs = 0;
static uint64_t sLastDiagUs = 0;
static uint8_t sDiagPrints = 0;
constexpr uint64_t kFrameIntervalUs = 20000;   // 50 FPS cap
constexpr uint32_t kAdvRestartDelayMs = 250;   // Small debounce before restarting adverts
constexpr bool kBypassFlowFieldsForStabilityTest = true;
constexpr bool kDisableBleForDisplayBringup = true;
constexpr bool kVerboseFrameTrace = true;
constexpr bool kDisablePixelOutputForDiag = false;
constexpr uint32_t kShowEveryNFrames = 25;     // 50 FPS loop -> 2 FPS show calls
constexpr uint8_t kDiagBrightness = 35;
constexpr bool kRunSetupOutputTest = true;

//bleControl variables ***********************************************************************
//elements that must be set before #include "bleControl.h" 

uint8_t EMITTER = 0;
uint8_t FLOW = 0;  // FLOW_NOISE; declared before enum is in scope
uint8_t BRIGHTNESS = 35;

uint8_t defaultMapping = 0;
bool mappingOverride = false;

//#include "audio/audioInput.h"
//#include "audio/audioProcessing.h"
#if defined(FLOWFIELDS_PARLIO_ONLY)
struct DummyAdvertising {
	void start() {}
};
static DummyAdvertising* pAdvertising = nullptr;
static bool displayOn = true;
static bool deviceConnected = false;
static bool wasConnected = false;
static uint8_t cMapping = 0;
static uint8_t cOverrideMapping = 0;
static inline void bleSetup() {}
#else
#include "bleControl.h"
#endif
#include "flowFields.hpp"

using namespace fl;

// MAPPINGS **********************************************************************************

extern const uint16_t progTopDown[NUM_LEDS] PROGMEM;
extern const uint16_t progBottomUp[NUM_LEDS] PROGMEM;
extern const uint16_t serpTopDown[NUM_LEDS] PROGMEM;
extern const uint16_t serpBottomUp[NUM_LEDS] PROGMEM;

enum Mapping {
	TopDownProgressive = 0,
	TopDownSerpentine,
	BottomUpProgressive,
	BottomUpSerpentine
};

uint16_t myXY(uint8_t x, uint8_t y) {
		if (x >= WIDTH || y >= HEIGHT) return 0;
		uint16_t i = ( y * WIDTH ) + x;
		switch(cMapping){
			case 0:	 ledNum = progTopDown[i]; break;
			case 1:	 ledNum = progBottomUp[i]; break;
			case 2:	 ledNum = serpTopDown[i]; break;
			case 3:	 ledNum = serpBottomUp[i]; break;
		}
		return ledNum;
}

//XYMap myXYmap = XYMap::constructWithLookUpTable(WIDTH, HEIGHT, progBottomUp);
//XYMap xyRect = XYMap::constructRectangularGrid(WIDTH, HEIGHT);

// **********************************************************************************

void setup() {
		
	Serial.begin(115200);
	Serial.setTxTimeoutMs(1);  // 1ms timeout — avoids unsigned underflow
	delay(1000);
	delay(2500);  // Give monitor time to attach after reset/upload.
	printf("[flowfields] fw-tag=FFDBG4 build=%s %s\n", __DATE__, __TIME__);
	fflush(stdout);
	printf("[flowfields] setup start\n");
	fflush(stdout);

	FastLED.setExclusiveDriver("PARLIO");

	FastLED.addLeds<WS2812B, PIN0, GRB>(leds, 0, NUM_LEDS_PER_STRIP)
		.setCorrection(TypicalLEDStrip);

	#ifdef PIN1
		FastLED.addLeds<WS2812B, PIN1, GRB>(leds, NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP)
			.setCorrection(TypicalLEDStrip);
	#endif
	
	#ifdef PIN2
		FastLED.addLeds<WS2812B, PIN2, GRB>(leds, NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP)
			.setCorrection(TypicalLEDStrip);
	#endif

	#ifdef PIN3
		FastLED.addLeds<WS2812B, PIN3, GRB>(leds, NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP)
			.setCorrection(TypicalLEDStrip);
	#endif

	#ifdef PIN4
		FastLED.addLeds<WS2812B, PIN4, GRB>(leds, NUM_LEDS_PER_STRIP * 4, NUM_LEDS_PER_STRIP)
			.setCorrection(TypicalLEDStrip);
	#endif

	#ifdef PIN5
		FastLED.addLeds<WS2812B, PIN5, GRB>(leds, NUM_LEDS_PER_STRIP * 5, NUM_LEDS_PER_STRIP)
			.setCorrection(TypicalLEDStrip);
	#endif
	
	#ifndef BIG_BOARD
		FastLED.setMaxPowerInVoltsAndMilliamps(5, 750);
	#endif
	
	FastLED.setBrightness(BRIGHTNESS);
	if (kBypassFlowFieldsForStabilityTest) {
		FastLED.setBrightness(kDiagBrightness);
	}
	printf("[flowfields] FastLED controllers=%d first_size=%d brightness=%u\n",
	       FastLED.count(), FastLED.size(), FastLED.getBrightness());
	fflush(stdout);

	if (kRunSetupOutputTest) {
		printf("[flowfields] setup output test: RED ON\n");
		for (uint16_t i = 0; i < NUM_LEDS; ++i) {
			leds[i] = CRGB(255, 0, 0);
		}
		FastLED.show();
		delay(1000);

		printf("[flowfields] setup output test: OFF\n");
		FastLED.clear();
		FastLED.show();
		delay(300);
		fflush(stdout);
	}

	FastLED.clear();
	if (kDisablePixelOutputForDiag) {
		printf("[flowfields] pixel output disabled (diag mode)\n");
	} else {
		FastLED.show();
	}

	printf("[flowfields] calling bleSetup\n");
	fflush(stdout);
	if (kDisableBleForDisplayBringup) {
		printf("[flowfields] bleSetup skipped (display bring-up mode)\n");
	} else {
		bleSetup();
		printf("[flowfields] bleSetup returned\n");
	}
	fflush(stdout);
	printf("[flowfields] setup complete\n");
	fflush(stdout);

	/*
	if (!LittleFS.begin(true)) {
		Serial.println("LittleFS mount failed!");
		return;
	}
	Serial.println("LittleFS mounted successfully.");
	*/

	/*if (audioEnabled){
		myAudio::initAudioInput();
		myAudio::initAudioProcessing();
	}*/

}

//*****************************************************************************************

void loop() {
	if (!sLoopEnteredLogged) {
		printf("[flowfields] loop entered\n");
		sLoopEnteredLogged = true;
	}
	if (!sLoopProbeLogged) {
		printf("[flowfields] loop probe A\n");
		sLoopProbeLogged = true;
	}

	const uint32_t nowMs = ::millis();
#if defined(ARDUINO_ARCH_ESP32)
	const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());
#else
	const uint64_t nowUs = static_cast<uint64_t>(micros());
#endif
	if ((nowUs - sLastStatusUs) >= 1000000ULL) {
		sLastStatusUs = nowUs;
		printf("[flowfields] alive us=%llu frames=%lu displayOn=%u ble=%u\n",
		       static_cast<unsigned long long>(nowUs),
		       (unsigned long)sFrameCount,
		       displayOn ? 1U : 0U,
		       deviceConnected ? 1U : 0U);
	}
	if (sDiagPrints < 5 && (nowUs - sLastDiagUs) >= 1000000ULL) {
		sLastDiagUs = nowUs;
		++sDiagPrints;
		printf("[flowfields] diag controllers=%d first_size=%d brightness=%u frame=%lu\n",
		       FastLED.count(), FastLED.size(), FastLED.getBrightness(), (unsigned long)sFrameCount);
		fflush(stdout);
	}

	if (sAdvRestartPending && pAdvertising != nullptr && (int32_t)(nowMs - sAdvRestartAtMs) >= 0) {
		pAdvertising->start();
		sAdvRestartPending = false;
		if (debug) { Serial.println("Start advertising"); }
	}

	if (nowUs < sNextFrameAtUs) {
		vTaskDelay(1);
		return;
	}
	sNextFrameAtUs = nowUs + kFrameIntervalUs;

	if (!displayOn){
		FastLED.clear();
	}
	
	else {

		mappingOverride ? cMapping = cOverrideMapping : cMapping = defaultMapping;
		defaultMapping = Mapping::TopDownProgressive;
		if (kBypassFlowFieldsForStabilityTest) {
			for (uint16_t i = 0; i < NUM_LEDS; ++i) {
				leds[i] = CRGB(255, 0, 0);
			}
			if (!sAfterFlowLogged) {
				printf("[flowfields] flow bypass active (stability test)\n");
				sAfterFlowLogged = true;
			}
		} else {
			if (!flowFields::flowFieldsInstance) {
				flowFields::initFlowFields(myXY);
			}
			if (!sBeforeFlowLogged) {
				printf("[flowfields] before runFlowFields\n");
				sBeforeFlowLogged = true;
			}
			flowFields::runFlowFields();
			if (!sAfterFlowLogged) {
				printf("[flowfields] after runFlowFields\n");
				sAfterFlowLogged = true;
			}
		}
	}

	++sFrameCount;
	if (kDisablePixelOutputForDiag) {
		if (kVerboseFrameTrace && sFrameCount <= 10) {
			printf("[flowfields] frame %lu show skipped\n", (unsigned long)sFrameCount);
		}
		if (!sAfterShowLogged) {
			printf("[flowfields] FastLED.show skipped (pixel output disabled)\n");
			sAfterShowLogged = true;
		}
	} else {
		const bool doShowNow = (kShowEveryNFrames > 0U) && ((sFrameCount % kShowEveryNFrames) == 0U);
		if (doShowNow) {
			if (kVerboseFrameTrace) {
				printf("[flowfields] frame %lu pre-show (throttled)\n", (unsigned long)sFrameCount);
			}
			FastLED.show();
			if (kVerboseFrameTrace) {
				printf("[flowfields] frame %lu post-show (throttled)\n", (unsigned long)sFrameCount);
			}
			if (!sAfterShowLogged) {
				printf("[flowfields] after first FastLED.show (throttled)\n");
				sAfterShowLogged = true;
			}
		}
	}

	if (!deviceConnected && wasConnected) {
		if (debug) {Serial.println("Device disconnected.");}
		sAdvRestartPending = true;
		sAdvRestartAtMs = ::millis() + kAdvRestartDelayMs;
		wasConnected = false;
	}

	vTaskDelay(1);
} // loop()
