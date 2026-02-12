#ifndef __QUECTELGNSSRK_H
#define __QUECTELGNSSRK_H

#include "Particle.h"

// Repository: https://github.com/rickkas7/QuectelGnssRK
// License: Apache 2.0
// This library is a modified version of https://github.com/particle-iot/particle-som-gnss/ with a modified API and additional features.

/**
 * @brief QuectelGnssRK class to aquire GNSS location
 *
 */
class QuectelGnssRK {
public:

    /**
     * @brief Type of location fix.
     *
     */
    enum LocationFix {
        LOCATION_FIX_NONE = 0,
        LOCATION_FIX_2D,
        LOCATION_FIX_3D,
    };

    /**
     * @brief Type of point coordinates of the given event.
     *
     */
    struct LocationPoint {
        unsigned int fix;               /**< Indication of GNSS locked status */
        time_t epochTime;               /**< Epoch time from device sources */
        time32_t systemTime;            /**< System epoch time */
        double latitude;                /**< Point latitude in degrees */
        double longitude;               /**< Point longitude in degrees */
        float altitude;                 /**< Point altitude in meters */
        float speed;                    /**< Point speed in meters per second */
        float heading;                  /**< Point heading in degrees */
        float horizontalAccuracy;       /**< Point horizontal accuracy in meters */
        float horizontalDop;            /**< Point horizontal dilution of precision */
        float verticalAccuracy;         /**< Point vertical accuracy in meters */
        float verticalDop;              /**< Point vertical dilution of precision */
        float timeToFirstFix;           /**< Time-to-first-fix in seconds */
        unsigned int satsInUse;         /**< Point satellites in use */

        /**
         * @brief Creates a simple readable string with common fields inclusing latitude, longitude, altitude, speed, heading, and time to first fix.
         * 
         * @return String 
         */
        String toStringSimple() const;

        /**
         * @brief Convert this object to JSON
         * 
         * @param writer JSONWriter to write the data to
         * @param wrapInObject true to wrap the data with writer.beginObject() and writer.endObject(). Default = true.
         */
        void toJsonWriter(JSONWriter &writer, bool wrapInObject = true) const;

#ifdef SYSTEM_VERSION_v620
        /**
         * @brief Save this data in a Variant object. Requires Device OS 6.2.0 or later.
         * 
         * @param obj Variant object to add to
         * @return QuectelGnssRK& 
         */
        void toVariant(Variant &obj) const;
#endif // SYSTEM_VERSION_v620
    };



    /**
     * @brief GNSS constellation types
     *
     * Even though these are a bitfield, you can't OR them together.
     */
    enum LocationConstellation {
        LOCATION_CONST_GPS_ONLY         = 0,         //<! GPS only
        LOCATION_CONST_GPS_GLONASS      = (1 << 0),  //<! GPS and Glosnass
        LOCATION_CONST_GPS_BEIDOU       = (1 << 1),  //<! GPS and Beidou
        LOCATION_CONST_GPS_GALILEO      = (1 << 2),  //<! GPS and Galileo
        LOCATION_CONST_GPS_QZSS         = (1 << 3),  //<! GPS and QZSS (not supported on EG91)
    };


    /**
     * @brief LocationConfiguration class to configure Location class options
     */
    class LocationConfiguration {
    public:
        /**
         * @brief Construct a new Location Configuration object
         *
         */
        LocationConfiguration() :
            _constellations(LOCATION_CONST_GPS_ONLY),
            _antennaPin(PIN_INVALID),
            _hdop(100),
            _hacc(50.0),
            _maxFixSeconds(90) {
        }

        /**
         * @brief Construct a new Location Configuration object
         *
         */
        LocationConfiguration(LocationConfiguration&&) = default;

        /**
         * @brief Set GNSS constellations.
         *
         * @param constellations Bitmap of supported GNSS constellations
         * @return LocationConfiguration&
         */
        LocationConfiguration& constellations(LocationConstellation constellations) {
            _constellations = constellations;
            return *this;
        }

        /**
         * @brief Get GNSS constellations.
         *
         * @return LocationConstellation Bitmap of supported GNSS constellations
         */
        LocationConstellation constellations() const {
            return _constellations;
        }

        /**
         * @brief Set the pin assignment for GNSS antenna power
         *
         * @param pin Pin number to enable GNSS antenna power
         * @return LocationConfiguration&
         */
        LocationConfiguration& enableAntennaPower(pin_t pin) {
            _antennaPin = pin;
            return *this;
        }

        /**
         * @brief Get the pin assignment for GNSS antenna power
         *
         * @return pin_t Powered at initialization
         */
        pin_t enableAntennaPower() const {
            return _antennaPin;
        }

        /**
         * @brief Set the HDOP threshold for a stable position fix
         *
         * @param hdop Value to check for position stability, 0 to 100
         * @return LocationConfiguration&
         */
        LocationConfiguration& hdopThreshold(int hdop) {
            if (0 > hdop)
                hdop = 0;
            else if (100 < hdop)
                hdop = 100;
            _hdop = hdop;
            return *this;
        }

        /**
         * @brief Get the HDOP threshold for a stable position fix
         *
         * @return int Value to check for position stability
         */
        int hdopThreshold() const {
            return _hdop;
        }

        /**
         * @brief Set the horizontal accuracy threshold, in meters, for a stable position fix (if supported)
         *
         * @param hacc Value to check for position stability, 1.0 to 1000.0 meters
         * @return LocationConfiguration&
         */
        LocationConfiguration& haccThreshold(float hacc) {
            _hacc = hacc;
            return *this;
        }

        /**
         * @brief Get the horizontal accuracy threshold, in meters, for a stable position fix (if supported)
         *
         * @return float Value to check for position stability
         */
        float haccThreshold() const {
            return _hacc;
        }

        /**
         * @brief Set the maximum amount of time to wait for a position fix
         *
         * @param fixSeconds Number of seconds to allow for a position fix
         * @return TrackerConfiguration&
         */
        LocationConfiguration& maximumFixTime(unsigned int fixSeconds) {
            _maxFixSeconds = fixSeconds;
            return *this;
        }

        /**
         * @brief Get the maximum amount of time to wait for a position fix
         *
         * @return Number of seconds to allow for a position fix
         */
        unsigned int maximumFixTime() const {
            return _maxFixSeconds;
        }

        /**
         * @brief Copy an existing configuration object
         * 
         * @param rhs 
         * @return LocationConfiguration& 
         */
        LocationConfiguration& operator=(const LocationConfiguration& rhs) {
            if (this == &rhs) {
                return *this;
            }
            this->_constellations = rhs._constellations;
            this->_antennaPin = rhs._antennaPin;
            this->_hdop = rhs._hdop;
            this->_maxFixSeconds = rhs._maxFixSeconds;

            return *this;
        }

    private:
        LocationConstellation _constellations;
        pin_t _antennaPin;
        int _hdop;
        float _hacc;
        unsigned int _maxFixSeconds;
    };


    /**
     * @brief Command request codes made from user thread to worker thread
     * 
     */
    enum class LocationCommand {
        None,                   /**< Do nothing */
        Acquire,                /**< Perform GNSS acquisition */
        Exit,                   /**< Exit from thread */
    };

    /**
     * @brief QuectelGnssRK result response enumerations
     *
     */
    enum class LocationResults {
        Unavailable,            /**< GNSS is not available, typically this indicates the modem is off */
        Unsupported,            /**< GNSS is not supported on this hardware */
        Idle,                   /**< No GNSS acquistions are pending or in progress */
        Acquiring,              /**< GNSS is aquiring a fix */
        Pending,                /**< A previous GNSS acquisition is in progress */
        Fixed,                  /**< GNSS position has been aquired and fixed */
        TimedOut,               /**< GNSS has not fix */
    };

    /**
     * @brief QuectelGnssRK class response callback prototype
     *
     */
    typedef std::function<void(LocationResults, const LocationPoint& point)> LocationDoneCallback;

    /**
     * @brief State data passed to the worker thread for getLocation and getLocationAsync calls
     */
    struct LocationCommandContext {
        LocationCommand command;             /**< command request from user thread */ 
        bool sendResponse;                   /**< send response back to user thread */ 
        LocationDoneCallback doneCallback;   /**< call a callback function (if send response is not set) */ 
        bool publish;                        /**< publish a loc event if a fix is obtained */ 
        LocationPoint* point;                /**< where to store the location result if not null*/ 

        LocationCommandContext() {
            command = LocationCommand::None;
            sendResponse = false;
            doneCallback = nullptr;
            publish = false;
            point = nullptr;
        }
    };

    /**
     * @brief Error codes returned from Cellular.command as CME errors.
     */
    enum class CME_Error {
        NONE                  = 0,    /**< No error */
        FIX                   = 1,    /**< Fixed position */
        SESSION_IS_ONGOING    = 504,  /**< Session is ongoing */
        SESSION_NOT_ACTIVE    = 505,  /**< Session not active */
        OPERATION_TIMEOUT     = 506,  /**< Operational timeout */
        NO_FIX                = 516,  /**< No fix */
        GNSS_IS_WORKING       = 522,  /**< GNSS is working */
        UNKNOWN_ERROR         = 549,  /**< Unknown error */
        UNDEFINED             = 999,
    };


    /**
     * @brief Singleton class instance for QuectelGnssRK
     *
     * @return QuectelGnssRK&
     */
    static QuectelGnssRK& instance()
    {
        if(!_instance)
        {
            _instance = new QuectelGnssRK();
        }
        return *_instance;
    }

    /**
     * @brief Configure the QuectelGnssRK class
     *
     * @param configuration Structure containing configuration
     * @retval 0 Success
     */
    int begin(LocationConfiguration& configuration);

    /**
     * @brief Get GNSS position, synchronously
     *
     * @param point Location point with position
     * @param publish Publish location point after acquisition
     * @return LocationResults
     */
    LocationResults getLocation(LocationPoint& point, bool publish = false);

    /**
     * @brief Get GNSS position, asynchronously, with given callback
     *
     * @param callback Callback function to call after acquisition completion
     * @return LocationResults
     * 
     * The results are not automatically published when using a callback, but you can use publishLocationEvent from your callback.
     */
    LocationResults getLocationAsync(LocationDoneCallback callback);


    /**
     * @brief Get the current acquisition state
     *
     * @retval LocationResults::Idle No current acquisition
     * @retval LocationResults::Acquiring Acquisition in progress
     */
    LocationResults getStatus() const {
        return (_acquiring.load()) ? LocationResults::Acquiring : LocationResults::Idle;
    }

    /**
     * @brief Publish the a location, or the last location
     * 
     * @param point the location to publish (optional). If omitted or null, then the last location from getLocation() or getLocationAsync() is used.
     * @return true if the publish succeeded
     */
    bool publishLocationEvent(const LocationPoint *point = nullptr);

#ifdef SYSTEM_VERSION_v620
    /**
     * @brief Get a Variant for a location, or the last location
     * 
     * @param obj the Variant object to write to
     * @param point the location to publish (optional). If omitted or null, then the last location from getLocation() or getLocationAsync() is used.
     */
    void getLocationEventVariant(Variant &obj, const LocationPoint *point = nullptr);
#endif // SYSTEM_VERSION_v620

    /**
     * @brief Returns true if GNSS and cellular can really operate concurrently
     * 
     * @return true 
     * @return false 
     * 
     * This function returns false on BG95 and true on EG91.
     * 
     * The BG95 cellular modem only sort of supports concurrent GNSS and cellular. If you are trying to get a fix, it may block the
     * operation of the cellular modem for so long that it will fail to connect. This is because the modem shares radio components
     * between GNSS and cellular.
     * 
     * The EG91 works independently enough that you can run the GNSS and cellular operations at the same time.
     */
    bool concurrentGnssAndCellularSupported() const;

    /**
     * @brief Get the location from the previous getLocation() or getLocationAsync() request
     * 
     * @return const LocationPoint& 
     * 
     * If the LocationPoint .fix member is true, then the location is valid. If any error occurred, then
     * the structure will be zeroed.
     */
    const LocationPoint &getLastLocationPoint() const { return lastLocation; };

    /**
     * @brief Get the result of the previous getLocation() or getLocationAsync() request
     * 
     * @return LocationResults 
     * 
     * See also getHasFix() if you only want to know that.
     */
    LocationResults getLastResults() const { return lastResults; };

    /**
     * @brief Returns true if the previous getLocation() or getLocationAsync() got a GNSS fix (has a valid location)
     * 
     * @return true 
     * @return false 
     */
    bool getHasFix() const { return lastResults == LocationResults::Fixed; };

    /**
     * @brief Handler function used with the LocationFusionRK library
     * 
     * @param eventData 
     * @param locVariant 
     */
    static void addToEventHandler(Variant &eventData, Variant &locVariant);

private:
    enum class _ModemType {
        Unavailable,                    /**< Modem type has not been read yet likely because the modem is off */
        Unsupported,                    /**< Modem type is not supported for this GNSS library */
        BG95_M5,                        /**< BG95-M5 modem type (also BG95-S5) */
        EG91,                           /**< EG91 modem type */
    };

    struct QlocContext {
        // QLOC parsed fields
        unsigned int tm_hour {};
        unsigned int tm_min {};
        unsigned int tm_sec {};
        unsigned int tm_day {};
        unsigned int tm_month {};
        unsigned int tm_year {};
        double latitude {};
        double longitude {};
        unsigned int fix {};
        float hdop {};
        float altitude {};
        unsigned int cogDegrees {};
        unsigned int cogMinutes {};
        float speedKmph {};
        float speedKnots {};
        unsigned int nsat {};

        // Time related
        std::tm timeinfo = {};
    };

    struct EpeContext {
        // EPE parsed fields
        float h_acc {};
        float v_acc {};
        float speed_acc {};
        float head_acc {};
    };

    QuectelGnssRK();

    bool modemNotDetected() const {
        return (_ModemType::Unavailable == _modemType);
    }

    bool detectModemType();
    void setAntennaPower();
    void clearAntennaPower();

    bool isModemOn() const {
        return Cellular.isOn();
    }

    bool isConnected() const {
        return Particle.connected();
    }

    int setConstellation(LocationConstellation flags);

    LocationCommandContext waitOnCommandEvent(system_tick_t timeout);
    LocationResults waitOnResponseEvent(system_tick_t timeout);
    static void stripLfCr(char* str);
    static int glocCallback(int type, const char* buf, int len, char* locBuffer);
    static int epeCallback(int type, const char* buf, int len, char* epeBuffer);
    CME_Error parseCmeError(const char* buf);
    int parseQloc(const char* buf, QlocContext& context, LocationPoint& point);
    CME_Error parseQlocResponse(const char* buf, QlocContext& context, LocationPoint& point);
    void parseEpeResponse(const char* buf, EpeContext& context, LocationPoint& point);
    void threadLoop();
    size_t buildPublish(char* buffer, size_t len, const LocationPoint& point, unsigned int seq);

    static QuectelGnssRK* _instance;
    os_queue_t _commandQueue;
    os_queue_t _responseQueue;
    Thread* _thread;
    std::atomic<bool> _acquiring{false};
    char _locBuffer[256];
    char _epeBuffer[256];
    QlocContext _qlocContext {};
    EpeContext _epeContext {};
    bool gnssStarted = false;
    uint32_t timeToFirstFixMs = 0;
    LocationPoint lastLocation = {0};
    LocationResults lastResults = LocationResults::Unavailable;

    LocationConfiguration _conf;
    pin_t _antennaPowerPin {PIN_INVALID};
    _ModemType _modemType {_ModemType::Unavailable};

    char _publishBuffer[particle::protocol::MAX_EVENT_DATA_LENGTH];
    unsigned int _reqid {1};
};



#endif  /* __QUECTELGNSSRK_H */
