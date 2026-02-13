# LocationFusionRK

*Library for Particle devices to generate enhanced geolocation requests*

The Tracker One, Tracker SoM, and Monitor One already include built-in support for enhanced location services or [location fusion](https://docs.particle.io/reference/tracker/location-fusion/). Other devices, however, can take advantage of these services in some cases, and this library can simplify doing so.

This library includes a small subset of features from Tracker Edge, but designed to work on non-Tracker devices. The most obvious target is the M-SoM which can use Wi-Fi and cellular tower geolocation as an alternative to GNSS.

This library requires Device OS 6.2.0 or later for Variant and CloudEvent support.

- Repository: https://github.com/rickkas7/LocationFusionRK
- License: MIT
- Full [browseable API documentation](https://rickkas7.github.io/LocationFusionRK/)

## Features

- Configurable for how often to publish `loc` events.
- Wi-Fi geolocation on Wi-Fi devices including the M-SoM.
- Single tower geolocation on all cellular devices.
- Multi-tower geolocation on devices with Quectel cellular modems using the [QuectelTowerRK](https://github.com/rickkas7/QuectelTowerRK) library.
- GNSS location using the Quectel cellular modem on some devices such as the M404, M524, and B504e using the [QuectelGnssRK](https://github.com/rickkas7/QuectelGnssRK) library.
- Extensible for other data sources, such as external GNSS modules connected by serial, I2C, or SPI.
- `loc-enhanced`data can optionally be delivered back to user firmware so the device can know its latitude, longitude, and accuracy when determined by location fusion.


## With other data sources

This library can also be used with these libraries:

- [QuectelGnssRK](https://github.com/rickkas7/QuectelGnssRK) to use cellular modem GNSS on some devices to get GNSS location, and if not available, fall back to cell tower and/or Wi-Fi geolocation. See example 4-location-fusion in that library.
- [QuectelTowerRK](https://github.com/rickkas7/QuectelTowerRK) for devices with a Quectel cellular modem such as the M-SoM to provide better location using neighbor cells using Location Fusion.

These libraries include a static method that is compatible with `withAddToEventHandler()` in this library to make it easy to add additional data. For example, adding GNSS using Quectel cellular modem GNSS using the QuectelGnssRK library.

```cpp
LocationFusionRK::instance()
    .withAddTower(true)
    .withAddWiFi(true)
    .withPublishPeriodic(5min)
    .withLocEnhancedHandler(locEnhancedCallback)
    .withAddToEventHandler(QuectelGnssRK::addToEventHandler)
    .setup();
```

This method can also be used to connect to other data sources, like external hardware GNSS units.

## Enhanced location callback

If you want to use location fusion and get the loc-enhanced results delivered back to the device, see example 2. By adding an asynchronous handler 
using `withLocEnhancedHandler()` the device will receive the enhanced location.

Note that on non-Tracker devices, location fusion is only done if there is no GNSS lock (`lck` in the inner `loc` is 0). Thus is will never be used to
enhance the GNSS location. However, this also means that when you have a GNSS location, the extra data operations fee for doing location fusion is not charged.
The callback is still called, but only with the original `lat` and `lng`.

See example 2-enhanced-callback.

## Version history

### 0.0.4 (2026-02-13)

- Increased worker thread stack size to 6144. In 0.0.3 and earlier it was 3072. You can customize this using withThreadStackSize() before setup().

### 0.0.3 (2026-01-29)

- Added getStatus() and withStatusHandler() methods. These can be used to know if the publish completed, or if the library is idle for sleep control.

### 0.0.2 (2025-10-31)

- Documentation updates

### 0.0.1 (2025-10-30)

- Initial version