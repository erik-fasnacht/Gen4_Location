#include "Particle.h"

#include "LocationFusionRK.h"

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

SYSTEM_MODE(SEMI_AUTOMATIC);

#ifndef SYSTEM_VERSION_v620
SYSTEM_THREAD(ENABLED); // System thread defaults to on in 6.2.0 and later and this line is not required
#endif

void setup() {
    WiFi.on();

    Particle.connect();
}

void loop() {
    delay(20000);
    
    LocationFusionRK::WAPList aps;
    aps.scan();
    Variant apVariant;
    aps.toVariant(apVariant);

    Log.info("aps %s", apVariant.toJSON().c_str());

    LocationFusionRK::ServingTower tower;
    if (tower.get() == SYSTEM_ERROR_NONE) {
        Variant towerVariant;
        tower.toVariant(towerVariant);

        Log.info("tower %s", towerVariant.toJSON().c_str());
    }
}


