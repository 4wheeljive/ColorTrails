#include <Arduino.h>
#include <FastLED.h>

#define PIN0 2
#define PIN1 3
#define PIN2 4
#define PIN3 50
#define PIN4 49
#define PIN5 5

#define HEIGHT 48
#define WIDTH 64
#define NUM_STRIPS 6
#define NUM_LEDS_PER_STRIP 512
#define NUM_LEDS (WIDTH * HEIGHT)

CRGB leds[NUM_LEDS];

static void addParlioStrips() {
    FastLED.addLeds<WS2812B, PIN0, GRB>(leds, 0, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<WS2812B, PIN1, GRB>(leds, NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<WS2812B, PIN2, GRB>(leds, NUM_LEDS_PER_STRIP * 2, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<WS2812B, PIN3, GRB>(leds, NUM_LEDS_PER_STRIP * 3, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<WS2812B, PIN4, GRB>(leds, NUM_LEDS_PER_STRIP * 4, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<WS2812B, PIN5, GRB>(leds, NUM_LEDS_PER_STRIP * 5, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    printf("[parlio-sanity] build=%s %s\n", __DATE__, __TIME__);
    printf("[parlio-sanity] setup start\n");

    FastLED.setExclusiveDriver("PARLIO");
    addParlioStrips();
    FastLED.setBrightness(64);

    printf("[parlio-sanity] controllers=%d first_size=%d brightness=%u\n",
           FastLED.count(), FastLED.size(), FastLED.getBrightness());

    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    printf("[parlio-sanity] RED ON (1.5s)\n");
    delay(1500);

    FastLED.clear();
    FastLED.show();
    printf("[parlio-sanity] OFF\n");
}

void loop() {
    static bool on = false;
    EVERY_N_MILLIS(1000) {
        on = !on;
        fill_solid(leds, NUM_LEDS, on ? CRGB::Red : CRGB::Black);
        FastLED.show();
        printf("[parlio-sanity] toggle=%u\n", on ? 1U : 0U);
    }
    vTaskDelay(1);
}
