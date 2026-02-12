#include "Particle.h"

#include "LocationFusionRK.h"

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

SYSTEM_MODE(SEMI_AUTOMATIC);

#ifndef SYSTEM_VERSION_v620
SYSTEM_THREAD(ENABLED); // System thread defaults to on in 6.2.0 and later and this line is not required
#endif

void locEnhancedCallback(const Variant &variant);

void setup() {
    LocationFusionRK::instance()
        .withAddTower(true)
        .withAddWiFi(true)
        .withPublishPeriodic(5min)
        .withLocEnhancedHandler(locEnhancedCallback)
        .setup();

#if Wiring_WiFi 
    WiFi.on();
#endif // Wiring_WiFi

    Particle.connect();
}

void loop() {
}

void locEnhancedCallback(const Variant &variant) {
    if (!variant.has("loc-enhanced")) {
        return;
    }
    Variant locEnhanced = variant.get("loc-enhanced");
    
    Log.info("locEnhancedCallback %s", locEnhanced.toJSON().c_str());

    // Fields in locEnhanced:
    // - h_acc horizontal accuracy (meters)
    // - lat latitude
    // - lon longitude

    double lat = locEnhanced.get("lat").asDouble();
    double lon = locEnhanced.get("lon").asDouble();
    int h_acc = locEnhanced.get("h_acc").asInt();

    Log.info("decoded lat=%.8lf lon=%.8lf h_acc=%d", lat, lon, h_acc);

}
