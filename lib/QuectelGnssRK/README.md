# QuectelGnssRK

This library implements GNSS support on Particle devices that have a Quecetel cellular modem that has GNSS support.

This library is can only be built with device OS version 5.8.2 and greater. It is intended for use the the following devices:

| Device | Modem    | Platform |
| :----- | :------- | :------  |
| M404   | BG95-M5  | msom     |
| M524   | EG91-EX  | msom     |
| B504e  | EG91-NAX | b5som    |

Note: This library cannot be used with the B524/B523 as those devices contain a EG91-E cellular modem, which does not have GNSS support.

It also cannot be used with any device with a u-blox cellular modem (B402/B404/B404X or any Boron) as none of the modems support GNSS.

Some location parameters are not supported on some cellular modems.

| Feature             | BG95    | EG91    |
| :------------------ | :-----: | :-----: |
| horizontalAccuracy  |   ✅    |         |
| verticalAccuracy    |   ✅    |         |


- Repository: https://github.com/rickkas7/QuectelGnssRK
- License: Apache 2
- This library is a modified version of https://github.com/particle-iot/particle-som-gnss/ with a modified API and additional features.
- Full [browseable API documentation](https://rickkas7.github.io/QuectelGnssRK/)


## Using with LocationFusionRK

Example 4 works with the [LocationFusionRK](https://github.com/rickkas7/LocationFusionRK) library to combine cellular, Wi-Fi, and GNSS location. 

```cpp
LocationFusionRK::instance()
    .withAddTower(true)
    .withAddWiFi(true)
    .withPublishPeriodic(5min)
    .withLocEnhancedHandler(locEnhancedCallback)
    .withAddToEventHandler(QuectelGnssRK::addToEventHandler)
    .setup();
```

Use the `withAddToEventHandler()` method of LocationFusionRK to add the handler `QuectelGnssRK::addToEventHandler`. This uses this library to obtain GNSS information from the cellular modem, but allow fallback to using Wi-Fi or single cellular tower geolocation if there is no GNSS fix available.

### Revision History

#### 0.0.1 (2025-10-29)

- Initial fork from particle-som-gnss v1.0.0 (2024-08-09)
- Added support for b5som and EG91
- Added asynchronous API
- Added more examples
