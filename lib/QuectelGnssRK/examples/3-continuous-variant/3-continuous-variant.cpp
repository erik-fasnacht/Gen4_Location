/*
 * Copyright (c) 2024 Particle Industries, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Particle.h"
#include "QuectelGnssRK.h"

SYSTEM_MODE(SEMI_AUTOMATIC);

#ifndef SYSTEM_VERSION_v620
#error "this example requires Device OS 6.2.0 or later for Variant support (and default threading on)"
#endif

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

static std::chrono::milliseconds checkPeriod = 10s;

static bool publishLocation = true;
static std::chrono::milliseconds publishPeriod = 2min;
static CloudEvent event;

enum class State {
    START,
    ACQUIRING,
    RETRY,
    IDLE,
    PUBLISH_WAIT
};
static State state = State::START;

void setup() {
    waitFor(Serial.isConnected, 10000); // Comment this line out for release

    Cellular.on();

    QuectelGnssRK::LocationConfiguration config;
    
#ifdef GNSS_ANT_PWR 
    // This is only used on M-SoM
    config.enableAntennaPower(GNSS_ANT_PWR);
#endif

    QuectelGnssRK::instance().begin(config);
}

unsigned long stateTime = 0;
const std::chrono::milliseconds retryPeriod = 30s;


void getLocationCallback(QuectelGnssRK::LocationResults results, const QuectelGnssRK::LocationPoint& point) {
    
    Log.info("async callback returned %d %s", (int)results, point.toStringSimple().c_str());
    
    if (point.fix) {
        Particle.connect();
        state = State::IDLE;
    }
    else {
        Log.info("No fix yet, will wait and retry");
        state = State::RETRY;
        stateTime = millis();
    }
}

void loop() {
    if (state == State::START) {
        if (Cellular.isOn()) {
            Log.info("GNSS acquisition starting...");
            QuectelGnssRK::instance().getLocationAsync(getLocationCallback);
            state = State::ACQUIRING;
        }
    }
    else
    if (state == State::RETRY) {
        if (millis() - stateTime >= retryPeriod.count()) {
            state = State::START;
            Log.info("retrying acquisition");
        }
    }
    else 
    if (state == State::IDLE) {
        static unsigned long lastCheckMs = 0;
        static unsigned long lastPublishMs = 0;

        if (millis() - lastCheckMs >= checkPeriod.count()) {
            lastCheckMs = millis();

            QuectelGnssRK::instance().getLocationAsync([](QuectelGnssRK::LocationResults results, const QuectelGnssRK::LocationPoint& point) {

                if (point.fix) {
                    Log.info("%s", point.toStringSimple().c_str());
                    
                    if (publishLocation && Particle.connected()) {
                        if (millis() - lastPublishMs >= publishPeriod.count()) {
                            lastPublishMs = millis();
                            
                            Variant eventData;
                            QuectelGnssRK::instance().getLocationEventVariant(eventData);
                            
                            Log.info("Publishing loc event...");
                            event.name("loc");
                            event.data(eventData);
                            Particle.publish(event);

                            state = State::PUBLISH_WAIT;
                        }
                    }
                }
                else {
                    Log.info("lost fix");
                }
            });

        }
    }
    else
    if (state == State::PUBLISH_WAIT) {
        if (event.isSent()) {
            Log.info("publish succeeded");
            event.clear();
            state = State::IDLE;
        }
        else 
        if (!event.isOk()) {
            Log.info("publish failed error=%d", event.error());
            event.clear();
            state = State::IDLE;
        }
    }

}

