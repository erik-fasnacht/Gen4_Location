# Gen4 Location Tracking with GNSS, Location Fusion, and Cell Tower Lookup

**Author:** Erik Fasnacht

**Date:** February 10, 2026

**Device OS:** 6.3.0 or later

A location tracking application for Particle Gen 4 devices that intelligently combines GNSS, Wi-Fi, and cellular tower geolocation to provide robust positioning even in challenging environments. Features a non-blocking state machine architecture designed for easy expansion with future capabilities.

## Table of Contents
- [Hardware Compatibility](#hardware-compatibility)
- [Libraries Used](#libraries-used)
- [Key Features](#key-features)
- [Location Priority & Fallback Strategy](#location-priority--fallback-strategy)
- [Architecture & Expandability](#architecture--expandability)
- [Configuration](#configuration)
- [State Machine](#state-machine)
- [Getting Started](#getting-started)
- [Usage](#usage)
- [Future Expansion Ideas](#future-expansion-ideas)

---

## Hardware Compatibility

This application supports the following Particle Gen 4 devices with Quectel cellular modems:

| Device | Modem | Platform | GNSS Support | Accuracy Parameters | Testing Status |
|--------|-------|----------|--------------|---------------------|----------------|
| **M404** | BG95-M5 | M-SoM | ✅ Yes | ✅ Horizontal & Vertical | ✅ Tested on Muon |
| **M524** | EG91-EX | M-SoM | ✅ Yes | ❌ No accuracy params | ⚠️ Not tested |

### ⚠️ B504E Not Supported

The **B504E** (B5-SoM with EG91-NAX modem) is **not compatible** with this application due to RAM limitations:
- B504E has only 256KB RAM vs 3MB on M404/M524
- LocationFusionRK and QuectelGnssRK libraries require more RAM than available
- Stack overflow errors occur even with minimal logging
- **Recommendation:** Use M404 or M524 for location fusion applications

### Testing Platform

This application has been tested on:
- **M404** module with **BG95-M5** modem
- **Muon** carrier board
- **Device OS 6.3.4**

### Important Hardware Limitations

#### M404 with BG95 Modem
The **BG95 modem cannot run GNSS and cellular data simultaneously**. This is a hardware constraint where the modem must switch between GNSS mode and cellular mode. The application handles this automatically through the finite state machine and LocationFusionRK library, but it means:
- GNSS acquisition happens when cellular is temporarily suspended
- Cellular reconnects after GNSS timeout or fix is obtained
- This is transparent to the user but may cause brief connectivity interruptions

The M524 device with EG91 modem does not have this limitation.

#### Accuracy Parameter Support
Only the BG95 modem (M404) provides horizontal and vertical accuracy estimates in the GNSS data. The EG91 modem (M524) provides position but without accuracy metadata.


---

## Libraries Used

This application integrates two specialized libraries:

### 1. QuectelGnssRK (v0.0.1)
Provides GNSS support for Particle devices with Quectel cellular modems that include GNSS capability.

- **Repository:** https://github.com/rickkas7/QuectelGnssRK
- **Purpose:** Interfaces with the cellular modem's built-in GNSS receiver
- **Features:** Asynchronous API, configurable timeouts, accuracy parameters (BG95 only)

### 2. LocationFusionRK (v0.0.3)
Orchestrates location fusion using multiple sources and integrates with Particle's cloud-enhanced location services.

- **Repository:** https://github.com/rickkas7/LocationFusionRK
- **Purpose:** Combines GNSS, Wi-Fi, and cellular tower data for robust location
- **Features:** Automatic fallback, cloud-enhanced responses, extensible data sources

> **⚠️ Version Requirement:** This application requires v0.0.3 or later of LocationFusionRK. The state machine implementation uses the `getStatus()` API which is only available in v0.0.3+.

---

## Key Features

✅ **Intelligent Multi-Source Location**
Automatically tries GNSS first, falls back to Location Fusion (Wi-Fi and cell tower data) if unavailable

✅ **Configurable Publish Frequency**
Publishes location every 5 minutes by default (adjustable, see [Configuration](#configuration))

✅ **Cloud-Enhanced Location Fusion**
Receives `loc-enhanced` responses with accurate coordinates and precision estimates

✅ **Non-Blocking State Machine**
Monitors location lifecycle without blocking the main loop

✅ **Comprehensive Logging**
Detailed state transitions and status updates for debugging

✅ **Automatic Antenna Power Management**
Enables antenna power on M-SoM for improved GNSS performance

✅ **Variant-Based Data Structure**
Uses modern `Variant` type for flexible event handling

### Cloud-First Connection Strategy

**Critical Design Decision:** The application connects to Particle Cloud in `setup()` ([main.cpp:169](src/main.cpp#L169)) *before* starting any location acquisition.

**Why this matters:**
- Enhanced location fusion (Wi-Fi and cell tower geolocation) **requires** active cloud connectivity
- The device needs to send `loc` events and receive `loc-enhanced` responses from Particle's location services
- Without cloud connection, you would only get raw GNSS data (no fallback when GNSS is unavailable)
- This architecture ensures **maximum location availability** by enabling all three location methods

---

## Location Priority & Fallback Strategy

The application uses an intelligent priority system to obtain the best available location:

### Priority Order

1. **Primary: GNSS** (90-second timeout)
   - Most accurate when satellite visibility is good
   - Timeout configured at [main.cpp:140](src/main.cpp#L140)
   - On M404 (BG95), cellular is suspended during GNSS acquisition

2. **Secondary: Location Fusion**
   - Used when GNSS fails or times out
   - Combines Wi-Fi access point data and cell tower information
   - Particle Cloud processes both data sources together for enhanced accuracy
   - Typical accuracy: 10-50 meters in urban areas (Wi-Fi dominant), 100-1000+ meters in areas with limited Wi-Fi (cell tower dominant)

### How the Fallback Works

LocationFusionRK manages this priority automatically:
- Starts GNSS acquisition via QuectelGnssRK
- If GNSS succeeds: publishes high-accuracy location
- If GNSS times out: automatically includes Wi-Fi and cell tower data in the event
- Particle Cloud performs location fusion and returns enhanced coordinates

This strategy ensures you **always get a location** even in challenging environments like:
- Indoor locations (no GNSS, but Wi-Fi available)
- Urban canyons (weak GNSS, but good cellular)
- Remote areas (GNSS available, limited Wi-Fi/cellular)

---

## Architecture & Expandability

### Why the State Machine is Separate from `loop()`

The application uses a dedicated `LocationStateMachine` class that is called from `loop()` but doesn't block execution. This design choice provides significant advantages:

#### Non-Blocking Operation
- The state machine runs once per second ([main.cpp:210-212](src/main.cpp#L210-L212))
- Between updates, `loop()` returns immediately
- Other code in `loop()` can execute without waiting for location operations
- System threads and background tasks continue running smoothly

#### Clean Separation of Concerns
- **State Machine:** Monitors LocationFusionRK status and manages application lifecycle
- **`loop()`:** Remains available for additional application logic
- **LocationFusionRK:** Handles actual location acquisition in the background

#### Easy Expansion Without Modification

The minimal `loop()` function ([main.cpp:172-178](src/main.cpp#L172-L178)) is intentionally simple:

```cpp
void loop() {
    // update the state machine to monitor LocationFusionRK status
    updateStateMachine();

    // expand code base here for future features
}
```

**Adding new features doesn't require touching the location logic.** Simply add your code in `loop()`:

```cpp
void loop() {
    updateStateMachine();           // Location monitoring (unchanged)

    checkMotionDetection();         // NEW: Motion detection
    updateBatteryMonitoring();      // NEW: Battery management
    handleGeofenceAlerts();         // NEW: Geofencing
    processCustomSensors();         // NEW: Additional sensors
}
```

#### Modular State Machine Design

The `LocationStateMachine` class encapsulates all state management:
- Self-contained state transitions with logging
- Helper methods for timeout monitoring
- Easy to add new states for additional workflows
- Can create parallel state machines for other features

**Example:** Adding a sleep management state machine:

```cpp
SleepStateMachine sleepManager;

void loop() {
    updateStateMachine();      // Location tracking
    sleepManager.update();     // Sleep management (new feature)
}
```

### Future-Ready Architecture

The state machine includes placeholder comments for expansion ([main.cpp:38-39](src/main.cpp#L38-L39), [main.cpp:332](src/main.cpp#L332)):

```cpp
// Future states for expansion:
// MOTION_DETECTED, SLEEP_PENDING, GEOFENCE_TRIGGERED, etc.

// Future expansion: Add motion detection, battery monitoring, geofencing, etc.
```

This architecture makes it trivial to add:
- **Motion detection:** Wake on movement, sleep when stationary
- **Battery optimization:** Adjust publish frequency based on charge level
- **Geofencing:** Trigger alerts when entering/leaving areas
- **Sensor fusion:** Combine with external sensors (temperature, humidity, etc.)
- **Adaptive timing:** Publish more frequently during movement, less when idle

---

## Configuration

### Adjust Publish Frequency

Location events are published every **5 minutes** by default. To change this, modify [main.cpp:149](src/main.cpp#L149):

```cpp
LocationFusionRK::instance()
    .withPublishPeriodic(5min)      // Change to: 1min, 10min, 30min, etc.
```

Available time units: `min`, `s` (seconds), `h` (hours)

### GNSS Timeout Configuration

GNSS acquisition times out after **90 seconds**. To adjust this, modify [main.cpp:140](src/main.cpp#L140):

```cpp
config.maximumFixTime(90);          // Change to: 60, 120, 180, etc.
```

**Trade-off:**
- **Shorter timeout (e.g., 60s):** Faster fallback to location fusion but may miss GNSS fixes in challenging conditions
- **Longer timeout (e.g., 120s):** Better chance of GNSS fix but delays fallback

### Adding Custom Data to Location Events

You can add additional sensor data, battery levels, motion state, or any custom fields to the published `loc` event using the `.withAddToEventHandler()` method ([main.cpp:151-152](src/main.cpp#L151-L152)).

**Current configuration:**
```cpp
LocationFusionRK::instance()
    .withAddToEventHandler(QuectelGnssRK::addToEventHandler)
```

**Example - Adding battery and temperature data:**
```cpp
void addCustomData(const Variant &eventVariant) {
    // Add battery state of charge
    eventVariant.set("battery", System.batteryCharge());

    // Add custom sensor data
    eventVariant.set("temp", readTemperatureSensor());

    // Add motion state
    eventVariant.set("motion", motionDetected ? "active" : "idle");
}

LocationFusionRK::instance()
    .withAddToEventHandler(QuectelGnssRK::addToEventHandler)
    .withAddToEventHandler(addCustomData)  // Add your custom handler
```

Multiple handlers can be chained together, and each one adds its data to the same event.

### Cellular Preference for Connectivity

The application sets cellular as the preferred connection method ([main.cpp:162](src/main.cpp#L162)) while keeping Wi-Fi active for location fusion:

```cpp
Cellular.prefer();
```

**Why this configuration:**
- **Cellular:** Used for Particle Cloud connectivity (reliable, always available)
- **Wi-Fi:** Remains active for access point scanning (needed for location fusion)
- This ensures stable cloud connection while maintaining location fusion capability

---

## State Machine

The application uses a finite state machine to monitor the location publishing lifecycle without blocking execution.

### States

| State | Description |
|-------|-------------|
| `WAITING_FOR_CLOUD` | Initial state after boot, waiting for Particle Cloud connection |
| `WAITING_FIRST_PUBLISH` | Cloud connected, waiting for first location publish to start |
| `IDLE` | Normal operation between location publishes |
| `LOCATION_BUILDING` | LocationFusionRK is actively collecting data (GNSS/Wi-Fi/Cell) |
| `LOCATION_PUBLISHING` | Location data published successfully, waiting for confirmation |
| `LOCATION_WAITING` | Waiting for cloud-enhanced location response |
| `ERROR_RECOVERY` | Handling publish failures with 60-second retry delay |

### State Flow

```
WAITING_FOR_CLOUD
    ↓ (cloud connects)
WAITING_FIRST_PUBLISH
    ↓ (first publish starts)
LOCATION_BUILDING
    ↓ (publish succeeds)
LOCATION_PUBLISHING
    ↓ (waiting for enhanced)
LOCATION_WAITING
    ↓ (enhanced received)
IDLE ←→ LOCATION_BUILDING  (periodic cycle every 5 min)
```

### Monitoring States

The state machine logs transitions and provides periodic status updates every 30 seconds ([main.cpp:336-344](src/main.cpp#L336-L344)), making it easy to monitor device behavior.

---

## Getting Started

### Prerequisites

- Particle Gen 4 device: M404, M524, or B504e
- Particle account (sign up at https://www.particle.io)
- Device OS 6.2.0 or later
- Particle Workbench (VS Code) or Particle CLI

### Installation

1. **Clone this repository:**
   ```bash
   git clone <repository-url>
   cd Gen4_Location
   ```

2. **Install dependencies:**

   Dependencies are defined in [project.properties](project.properties) and will be automatically installed during compilation:
   - LocationFusionRK v0.0.3
   - QuectelGnssRK v0.0.1

3. **Compile and flash:**

   **Using Particle Workbench:**
   - Open folder in VS Code
   - Select your device type (M404, M524, or B504e)
   - Click "Particle: Cloud Flash" or "Particle: Local Flash"

   **Using Particle CLI:**
   ```bash
   particle compile msom --target 6.3.4
   particle flash <device-name> firmware.bin
   ```

4. **Monitor operation:**
   ```bash
   particle serial monitor --follow
   ```

### Initial Setup

The device will:
1. Boot and initialize the state machine
2. Connect to Particle Cloud (required for location fusion)
3. Start location acquisition using GNSS → Location Fusion fallback
4. Publish the first `loc` event
5. Receive `loc-enhanced` response with accurate coordinates
6. Continue publishing every 5 minutes

---

## Usage

### Viewing Location Events

**Particle Console:**
1. Log in to https://console.particle.io
2. Navigate to your device
3. Go to the "Events" tab
4. Look for `loc` events (published by device) and `loc-enhanced` events (from cloud)
5. Users can also monitor the device location using map view in the "Location" tab which tracks historical position of asset (must opt into location storage).

**Serial Monitor:**
```bash
particle serial monitor --follow
```
Or https://docs.particle.io/tools/developer-tools/usb-serial/

Look for log messages:
- State transitions: `State: IDLE -> LOCATION_BUILDING`
- LocationFusionRK status updates
- Enhanced location callback: `Enhanced Position: lat=37.422408, lon=-122.084066, accuracy=15.0m`

### Understanding the Log Output

```
State: WAITING_FOR_CLOUD -> WAITING_FIRST_PUBLISH
Cloud connected! Transitioning to wait for first publish
LocationFusionRK Status: 1
First location publish started
State: WAITING_FIRST_PUBLISH -> LOCATION_BUILDING
Collecting location data (GNSS/WiFi/Cell)...
Location published successfully
State: LOCATION_BUILDING -> LOCATION_PUBLISHING
Enhanced Position: lat=37.422408, lon=-122.084066, accuracy=15.0m
State: LOCATION_WAITING -> IDLE
```

### Accessing Enhanced Location Data

The enhanced location callback ([main.cpp:181-205](src/main.cpp#L181-L205)) receives the cloud-processed location:

```cpp
void locEnhancedCallback(const Variant &variant) {
    Variant locEnhanced = variant.get("loc-enhanced");

    if (locEnhanced.has("lat") && locEnhanced.has("lon")) {
        double lat = locEnhanced.get("lat").toDouble();
        double lon = locEnhanced.get("lon").toDouble();
        double h_acc = locEnhanced.get("h_acc").toDouble();

        Log.info("Enhanced Position: lat=%.6f, lon=%.6f, accuracy=%.1fm",
                 lat, lon, h_acc);

        // YOUR CODE HERE: Store to EEPROM, trigger geofence, etc.
    }
}
```

This callback is where you can:
- Store location history
- Check geofence boundaries
- Trigger alerts or notifications
- Update displays or indicators
- Log to SD card or external storage

---

## Future Expansion Ideas

The architecture is designed for easy extension. Here are concrete examples:

### 1. Motion Detection & Adaptive Publishing

```cpp
// In global scope
#include "AccelerometerDriver.h"
AccelerometerDriver accel;
bool motionDetected = false;

void loop() {
    updateStateMachine();

    // Check for motion
    if (accel.detectMotion()) {
        motionDetected = true;
        // Publish more frequently when moving
        LocationFusionRK::instance().publishNow();
    } else {
        motionDetected = false;
        // Use normal 5-minute interval when stationary
    }
}
```

### 2. Battery-Aware Operation

```cpp
void loop() {
    updateStateMachine();

    // Adjust behavior based on battery
    float battery = System.batteryCharge();
    if (battery < 20.0) {
        // Low battery: publish less frequently
        LocationFusionRK::instance().withPublishPeriodic(15min);
    } else {
        // Normal operation
        LocationFusionRK::instance().withPublishPeriodic(5min);
    }
}
```

### 3. Geofencing

```cpp
void locEnhancedCallback(const Variant &variant) {
    Variant locEnhanced = variant.get("loc-enhanced");
    double lat = locEnhanced.get("lat").toDouble();
    double lon = locEnhanced.get("lon").toDouble();

    // Check if inside geofence
    if (isInsideGeofence(lat, lon, FENCE_LAT, FENCE_LON, FENCE_RADIUS)) {
        Particle.publish("geofence-enter", "Device entered area");
    }
}
```

### 4. Sleep Mode Management

```cpp
// Create a second state machine for power management
class SleepStateMachine {
    // Monitor activity and enter sleep when idle
    void update() {
        if (idleTime > 1h && Particle.connected()) {
            System.sleep(ULTRA_LOW_POWER, 15min);
        }
    }
};

SleepStateMachine sleepManager;

void loop() {
    updateStateMachine();   // Location tracking
    sleepManager.update();  // Power management
}
```

### 5. Custom Sensor Integration

Add temperature, humidity, or other sensor data to location events:

```cpp
#include "TemperatureSensor.h"
TemperatureSensor tempSensor;

void addSensorData(const Variant &eventVariant) {
    eventVariant.set("temperature", tempSensor.read());
    eventVariant.set("humidity", humiditySensor.read());
}

// In setup()
LocationFusionRK::instance()
    .withAddToEventHandler(QuectelGnssRK::addToEventHandler)
    .withAddToEventHandler(addSensorData)  // Add sensors
    .setup();
```

---

## Additional Resources

- **Particle Documentation:** https://docs.particle.io
- **QuectelGnssRK Library:** https://github.com/rickkas7/QuectelGnssRK
- **LocationFusionRK Library:** https://github.com/rickkas7/LocationFusionRK
- **Location Fusion Guide:** https://docs.particle.io/reference/tracker/location-fusion/
- **Particle Community:** https://community.particle.io

## License

See individual library licenses:
- QuectelGnssRK: Apache 2.0
- LocationFusionRK: MIT

---

## Support

For issues or questions:
- **Particle Community:** https://community.particle.io
- **QuectelGnssRK Issues:** https://github.com/rickkas7/QuectelGnssRK/issues
- **LocationFusionRK Issues:** https://github.com/rickkas7/LocationFusionRK/issues
