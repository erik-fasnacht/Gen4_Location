/* 
 * Project: Location Example using Gen 4, M-SoM + Muon, hardware
 * Author: Erik Fasnacht
 * Date: 2/10/2026
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

#include "Particle.h"

// inlcude libraries
#include "LocationFusionRK.h"
#include "QuectelGnssRK.h"

// turn on serial logger
SerialLogHandler logHandler(LOG_LEVEL_TRACE);

//set to semi-automatic mode 
SYSTEM_MODE(SEMI_AUTOMATIC);

// #ifndef to remove warning from compile
#ifndef SYSTEM_VERSION_v620
    SYSTEM_THREAD(ENABLED); // System thread defaults to on in 6.2.0 and later and this line is not required
#endif

// STATE MACHINE SECTION, used to monitor location publishing lifecycle (non-blocking)
// Expandable: Easy to add new states for motion detection, battery monitoring
class LocationStateMachine {
public:
    enum class AppState {
        WAITING_FOR_CLOUD,      // Waiting for initial cloud connection
        WAITING_FIRST_PUBLISH,  // Cloud connected, waiting for first position publish
        IDLE,                   // Normal operation between publishes
        LOCATION_BUILDING,      // LocationFusionRK is collecting data (GNSS/WiFi/Cell)
        LOCATION_PUBLISHING,    // Data published, waiting for confirmation
        LOCATION_WAITING,       // Waiting for enhanced location response

        // Future states for expansion:

        ERROR_RECOVERY          // Handling failures with retry delay
    };

//private members to track state and timing
private:
    AppState currentState = AppState::WAITING_FOR_CLOUD;
    AppState previousState = AppState::WAITING_FOR_CLOUD;
    uint32_t stateEntryTime = 0;
    uint32_t bootTime = 0;
    uint32_t errorRetryDelay = 60000;  // 60 seconds
    bool firstPublishCompleted = false;

// public methods to manage state transitions and logging
public:

    // Initialize state machine and record boot time
    void begin() {
        bootTime = millis();
        setState(AppState::WAITING_FOR_CLOUD);
    }

    // Centralized state transition function with logging
    void setState(AppState newState) {
        if (currentState != newState) {
            previousState = currentState;
            currentState = newState;
            stateEntryTime = millis();
            logStateTransition();
        }
    }

    // Getter for current state
    AppState getState() const { return currentState; }

    // function for loggin the state transitions
    void logStateTransition() {
        Log.info("State: %s -> %s",
            stateToString(previousState),
            stateToString(currentState));
    }

    // helper function to convert state enum to string for logging
    const char* stateToString(AppState state) {
        switch(state) {
            case AppState::WAITING_FOR_CLOUD: return "WAITING_FOR_CLOUD";
            case AppState::WAITING_FIRST_PUBLISH: return "WAITING_FIRST_PUBLISH";
            case AppState::IDLE: return "IDLE";
            case AppState::LOCATION_BUILDING: return "LOCATION_BUILDING";
            case AppState::LOCATION_PUBLISHING: return "LOCATION_PUBLISHING";
            case AppState::LOCATION_WAITING: return "LOCATION_WAITING";
            case AppState::ERROR_RECOVERY: return "ERROR_RECOVERY";
            default: return "UNKNOWN";
        }
    }

    // Check if we should retry after an error, based on time elapsed
    bool shouldRetry() const {
        return (currentState == AppState::ERROR_RECOVERY) &&
               (millis() - stateEntryTime > errorRetryDelay);
    }

    // Get time since boot for timeout monitoring
    uint32_t getTimeSinceBoot() const {
        return millis() - bootTime;
    }

    // Mark that the first publish has completed (used to track lifecycle and for potential future logic)
    void markFirstPublishComplete() {
        firstPublishCompleted = true;
    }

    // Getter to check if first publish has completed (used for monitoring and potential future logic)
    bool isFirstPublishComplete() const {
        return firstPublishCompleted;
    }
};

// Instantiate the state machine
LocationStateMachine appStateMachine;

//forward function declarations
void locEnhancedCallback(const Variant &variant);       // function for receiving enhanced location data from the cloud
void updateStateMachine();                              // function for the FSM

// setup() runs once at startup, loop() runs continuously after
void setup() {

    // Initialize application state machine
    appStateMachine.begin();

    // Configure GNSS with explicit 90-second timeout
    QuectelGnssRK::LocationConfiguration config;

    // turn on antenna power if supported (like on M-SoM) - this can improve time to first fix and location accuracy, especially in challenging environments
    #ifdef GNSS_ANT_PWR
        // This is only used on M-SoM
        config.enableAntennaPower(GNSS_ANT_PWR);
    #endif

    // Explicit 90s GNSS timeout, default is 60s, before moving to other location methods
    config.maximumFixTime(90); 

    // Initialize Quectel GNSS RK with the specified configuration
    QuectelGnssRK::instance().begin(config);

    // Configure LocationFusionRK (using polling, not callbacks)
    LocationFusionRK::instance()
        .withAddTower(true)
        .withAddWiFi(true)
        .withPublishPeriodic(5min)      //sets the publish frequency
        .withLocEnhancedHandler(locEnhancedCallback)
        .withAddToEventHandler(QuectelGnssRK::addToEventHandler)
        // .withAddToEventHandler() can add other data sources to the same event, like cellular or motion data in the future
        .setup();

    // explicitly turn on Wi-Fi for scanning (LocationFusionRK will manage it from here)
    #if Wiring_WiFi
        WiFi.on();
    #endif // Wiring_WiFi

    // Prefer cellular over WiFi for primary connectivity
    // WiFi remains on for access point scanning (needed for location fusion)
    Cellular.prefer();

    // logging messages
    Log.info("State machine initialized - monitoring mode");

    // CRITICAL: Connect to cloud in setup()
    // LocationFusionRK will wait for connection, then publish immediately
    Particle.connect();
}

void loop() {

    // update the state machine to monitor LocationFusionRK status and manage app lifecycle
    updateStateMachine();

    //expand code base here for future features like motion detection, battery monitoring, geofencing, etc.
}

// using new event variant type to receive enhanced location data from the cloud
void locEnhancedCallback(const Variant &variant) {
    Variant locEnhanced = variant.get("loc-enhanced");

    // logging message
    Log.info("locEnhancedCallback %s", locEnhanced.toJSON().c_str());

    // Extract enhanced location data with error checking
    if (locEnhanced.has("lat") && locEnhanced.has("lon")) {
        double lat = locEnhanced.get("lat").toDouble();
        double lon = locEnhanced.get("lon").toDouble();
        double h_acc = locEnhanced.get("h_acc").toDouble();

        // logging message
        Log.info("Enhanced Position: lat=%.6f, lon=%.6f, accuracy=%.1fm",
                 lat, lon, h_acc);

        // Future expansion: Store in EEPROM, trigger geofence actions, etc.

    } 
    
    else {
        // logging message
        Log.error("Enhanced location missing lat/lon data");
    }
}

// function for the FSM
void updateStateMachine() {
    // Rate limit - only run once per second to avoid overhead
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate < 1000) {
        return;
    }

    // Update the last update time
    lastUpdate = millis();

    // Poll LocationFusionRK status (thread-safe from application loop)
    static LocationFusionRK::Status lastStatus = LocationFusionRK::Status::idle;
    LocationFusionRK::Status currentStatus = LocationFusionRK::instance().getStatus();

    // Log status changes for debugging
    if (currentStatus != lastStatus) {
        lastStatus = currentStatus;
        Log.info("LocationFusionRK Status: %d", (int)currentStatus);
    }

    // State machine logic - MONITORING mode (doesn't block operations)
    switch(appStateMachine.getState()) {

        // Initial state after boot - waiting for cloud connection
        case LocationStateMachine::AppState::WAITING_FOR_CLOUD:
            // Monitor cloud connection
            if (Particle.connected()) {
                Log.info("Cloud connected! Transitioning to wait for first publish");
                appStateMachine.setState(LocationStateMachine::AppState::WAITING_FIRST_PUBLISH);
            }
            // Log warning if taking too long (90s timeout)
            else if (appStateMachine.getTimeSinceBoot() > 90000) {
                static bool timeoutWarned = false;
                if (!timeoutWarned) {
                    Log.warn("Cloud connection taking longer than 90s");
                    timeoutWarned = true;
                }
            }
            break;

        // After cloud connection, waiting for the first location publish to complete
        case LocationStateMachine::AppState::WAITING_FIRST_PUBLISH:
            // Monitor first location publish attempt
            if (currentStatus == LocationFusionRK::Status::publishing) {
                Log.info("First location publish started");
                appStateMachine.markFirstPublishComplete();
                appStateMachine.setState(LocationStateMachine::AppState::LOCATION_BUILDING);
            }
            // Log warning if taking too long
            else if (appStateMachine.getTimeSinceBoot() > 120000) {
                static bool firstPublishWarned = false;
                if (!firstPublishWarned) {
                    Log.warn("First publish taking longer than expected (120s since boot)");
                    firstPublishWarned = true;
                }
            }
            break;

        // Normal operation - monitor for location publish cycles and handle transitions
        case LocationStateMachine::AppState::IDLE:
            // Normal operation - monitor for next location publish cycle
            if (currentStatus == LocationFusionRK::Status::publishing) {
                appStateMachine.setState(LocationStateMachine::AppState::LOCATION_BUILDING);
                Log.info("Collecting location data (GNSS/WiFi/Cell)...");
            }
            break;

        // LocationFusionRK is collecting data for a publish cycle
        case LocationStateMachine::AppState::LOCATION_BUILDING:
            // LocationFusionRK is collecting data
            if (currentStatus == LocationFusionRK::Status::publishSuccess) {
                appStateMachine.setState(LocationStateMachine::AppState::LOCATION_PUBLISHING);
                Log.info("Location published successfully");
            } else if (currentStatus == LocationFusionRK::Status::publishFail) {
                appStateMachine.setState(LocationStateMachine::AppState::ERROR_RECOVERY);
                Log.error("Location publish failed");
            } else if (currentStatus == LocationFusionRK::Status::locEnhancedWait) {
                // Status skipped ahead - publish succeeded but we missed it
                Log.info("Location published, waiting for enhanced (skipped publishSuccess)");
                appStateMachine.setState(LocationStateMachine::AppState::LOCATION_WAITING);
            } else if (currentStatus == LocationFusionRK::Status::locEnhancedSuccess) {
                // Status jumped all the way to success
                Log.info("Enhanced location received (skipped intermediate states)");
                appStateMachine.setState(LocationStateMachine::AppState::IDLE);
            } else if (currentStatus == LocationFusionRK::Status::idle) {
                // Publish completed and returned to idle - we missed the whole sequence!
                Log.info("Location publish completed (missed intermediate states)");
                appStateMachine.setState(LocationStateMachine::AppState::IDLE);
            }
            break;

        // After publishing, waiting for confirmation and enhanced location response
        case LocationStateMachine::AppState::LOCATION_PUBLISHING:
            // Waiting for cloud-enhanced response
            if (currentStatus == LocationFusionRK::Status::locEnhancedWait) {
                appStateMachine.setState(LocationStateMachine::AppState::LOCATION_WAITING);
                Log.info("Waiting for cloud-enhanced location...");
            } else if (currentStatus == LocationFusionRK::Status::idle) {
                // Published but no enhanced location expected
                appStateMachine.setState(LocationStateMachine::AppState::IDLE);
            }
            break;

        // Waiting for enhanced location response after publish
        case LocationStateMachine::AppState::LOCATION_WAITING:
            // Waiting for enhanced location callback
            if (currentStatus == LocationFusionRK::Status::locEnhancedSuccess) {
                Log.info("Enhanced location received successfully");
                appStateMachine.setState(LocationStateMachine::AppState::IDLE);
            } else if (currentStatus == LocationFusionRK::Status::locEnhancedFail) {
                Log.warn("Enhanced location failed, but base location was sent");
                appStateMachine.setState(LocationStateMachine::AppState::IDLE);
            }
            break;

        // Error recovery state - waits for a delay before allowing retries
        case LocationStateMachine::AppState::ERROR_RECOVERY:
            // Handle error recovery with retry delay
            if (appStateMachine.shouldRetry()) {
                Log.info("Retrying after error recovery delay");
                appStateMachine.setState(LocationStateMachine::AppState::IDLE);
            }
            break;

        // Future expansion: Add motion detection, battery monitoring, geofencing, etc.

    }

    // Periodic state logging (every 30 seconds)
    static uint32_t lastStateLog = 0;
    if (millis() - lastStateLog > 30000) {
        Log.info("Current State: %s, Cloud: %s, Time since boot: %lus",
                 appStateMachine.stateToString(appStateMachine.getState()),
                 Particle.connected() ? "connected" : "disconnected",
                 appStateMachine.getTimeSinceBoot() / 1000);
        lastStateLog = millis();
    }

}
