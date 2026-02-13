#ifndef __LOCATIONFUSIONRK_H
#define __LOCATIONFUSIONRK_H

#include "Particle.h"

// Repository: https://github.com/rickkas7/LocationFusionRK
// License: MIT


#ifndef SYSTEM_VERSION_v620
#error "The LocationFusionRK library requires Device OS 6.2.0 or later because it requires Variant and CloudEvent"
#endif

#include <vector>

/**
 * This class is a singleton; you do not create one as a global, on the stack, or with new.
 * 
 * From global application setup you must call:
 * LocationFusionRK::instance().setup();
 */
class LocationFusionRK {
public:

#if Wiring_WiFi
    /**
     * @brief Class for holding information about a single Wi-Fi access point
     * 
     */
    class WAPEntry {
    public:
        WAPEntry();

        WAPEntry(const WiFiAccessPoint *wap);

        /**
         * @brief 
         * 
         * @param wap 
         */
        void fromWiFiAccessPoint(const WiFiAccessPoint *wap);

        /**
         * @brief Convert this object to JSON
         * 
         * @param writer JSONWriter to write the data to
         * @param wrapInObject true to wrap the data with writer.beginObject() and writer.endObject(). Default = true.
         */
        void toJsonWriter(JSONWriter &writer, bool wrapInObject = true) const;

        /**
         * @brief Save this data in a Variant object. Requires Device OS 6.2.0 or later.
         * 
         * @param obj Variant object to add to
         */
        void toVariant(Variant &obj) const;

        /**
         * @brief Convert to a string in 00-00-00-00-00-00 hex format
         * 
         * @return String 
         */
        String bssidString() const;

        uint8_t bssid[6]; //!< BSSID (base station MAC address)
        uint8_t channel; //!< Wi-Fi channel number
        uint8_t reserved; //!< reserved for future use and for structure alignment 
        int rssi; //!< The signal strength (RSSI) 
    };
#endif // Wiring_WiFi

#if Wiring_WiFi 
    /**
     * @brief Container for a list of Wi-Fi access points, along with methods for scanning and converting to JSON or Variant
     */
    class WAPList {
    public:
        /**
         * @brief Scan for Wi-Fi access points.
         * 
         * The data is stored in this object. The method is blocking, but it's typically called from a worker thread.
         * 
         * Calling this clears the previous results.
         */
        void scan();

        /**
         * @brief Return the number of access points found
         * 
         * @return size_t 
         */
        size_t size() const { return wapArray.size(); };


        /**
         * @brief Convert this object to JSON
         * 
         * @param writer JSONWriter to write the data to
         * @param numToInclude Limit to this number of entries. 0 (default) is unlimited.
         */
        void toJsonWriter(JSONWriter &writer, int numToInclude = 0) const;

        /**
         * @brief Save this data in a Variant object. Requires Device OS 6.2.0 or later.
         * 
         * @param obj Variant object to add to
         * @param numToInclude Limit to this number of entries. 0 (default) is unlimited.
         */
        void toVariant(Variant &obj, int numToInclude = 0) const;

    protected:
        /**
         * @brief Used internally to add an entry to wapArray
         * 
         * @param entry 
         */
        void appendEntry(const WAPEntry &entry);

        /**
         * @brief Used internally to add an entry to wapArray from a WiFiAccessPoint structure
         * 
         * @param entry 
         */
        void appendEntry(const WiFiAccessPoint *wap);

        /**
         * @brief Eventually called to process entries from WiFi.scan().
         * 
         * @param wap 
         */
        void scanCallback(WiFiAccessPoint* wap);

        /**
         * @brief Passed to WiFi.scan()
         * 
         * @param wap 
         */
        static void scanCallbackStatic(WiFiAccessPoint* wap, void *context);

        /**
         * @brief Array of access points found by Wifi.scan()
         */
        std::vector<WAPEntry> wapArray;
    };
#endif // Wiring_WiFi

#if Wiring_Cellular
    class ServingTower {
    public:
        /**
         * @brief Get the current serving tower information.
         * 
         * @return int A system error code. SYSTEM_ERROR_NONE (0) is a success code, non-zero indicates an error. 
         * 
         * On;y available on cellular devices, and only works when connected to the cloud (breathing cyan).
         */
        int get();

        /**
         * @brief Get the result from the last get() call.
         * 
         * @return int A system error code. SYSTEM_ERROR_NONE (0) is a success code, non-zero indicates an error.  -1 if get() has not been called.
         */
        int getLastResult() const { return (int)cellularResult; };

        /**
         * @brief Convert this object to JSON
         * 
         * @param writer JSONWriter to write the data to
         * @param wrapInObject true (default) to surround with beginObject() and endObject()
         */
        void toJsonWriter(JSONWriter &writer, bool wrapInObject = true) const;

        /**
         * @brief Save this data in a Variant object. Requires Device OS 6.2.0 or later.
         * 
         * @param obj Variant object to add to
         */
        void toVariant(Variant &obj) const;
        /**
         * @brief Return the current CellularGlobalIdentity. Only valid after get() is called.
         * 
         * @return const CellularGlobalIdentity& 
         */
        const CellularGlobalIdentity &getCellularGlobalIdentity() const { return cgi; };

    protected:
        CellularGlobalIdentity cgi = {0}; //!< Filled in by cellular_global_identity()
        cellular_result_t cellularResult = -1; //!< Result from cellular_global_identity()

    };
#endif // Wiring_Cellular
    /**
     * @brief How often to publish location 
     */
    enum class PublishFrequency {
        manual, //!< Only when requested
        once, //!< Once after connecting to the cloud
        periodic //!< Periodically (period is configurable)
    };

    /**
     * @brief Current status of this library
     * 
     * Note that the publishSuccess and publishFailed status codes are only useful when
     * using the handler callback. Almost immediately after that notification, the status
     * goes to idle, so it would be hard to catch this status by polling.
     * 
     * The locEnhancedSuccess and locEnhancedFail also require the callback to be useful.
     * Note the locEnhanced status events are only used if you have enabled a loc-enhanced
     * callback to receive the events on-device. If you do not do this, the loc-enhanced
     * will only exist in the cloud and not be sent back to the device.
     * 
     * If you are implementing sleep control, you can go go sleep if the status is idle.
     */
    enum class Status:int {
        idle = 0, //!< is idle (cloud connected or not)
        publishing = 1, //!< publishing a loc event (includes building the event)
        publishSuccess = 2, //!< publish succeeded
        publishFail = 3, //!< publish failed
        locEnhancedWait = 4, //!< waiting for a loc-enhanced reply
        locEnhancedSuccess = 5, //!< loc-enhanced reply received
        locEnhancedFail = 6 //!< loc-enhanced reply timed out        
    };
     

    /**
     * @brief Gets the singleton instance of this class, allocating it if necessary
     * 
     * Use LocationFusionRK::instance() to instantiate the singleton.
     */
    static LocationFusionRK &instance();

    /**
     * @brief Perform setup operations; call this from global application setup()
     * 
     * You typically use LocationFusionRK::instance().setup();
     * 
     * Typically you use all of the withXXX() functions before setup.
     */
    void setup();

    /**
     * @brief Set the publish frequency to manual. Call requestPublish() to do so. This is the default.
     * 
     * @return LocationFusionRK& 
     */
    LocationFusionRK &withPublishManual() { publishFrequency = PublishFrequency::manual; return *this; };

    /**
     * @brief Set the publish frequency to publish once after connecting to the cloud.
     * 
     * @return LocationFusionRK& 
     */
    LocationFusionRK &withPublishOnce() { publishFrequency = PublishFrequency::once; return *this; };

    /**
     * @brief Set the publish frequency to publish periodically when cloud connected.
     * 
     * @param ms 
     * @return LocationFusionRK& 
     * 
     * Even though the parameter is in milliseconds, you probably shouldn't publish more often than every few minutes. 
     * 
     * If you are using location fusion on a non-Tracker device it costs 50 data operations per fusion request, so doing location fusion
     * frequently on the free plan may cause your account to be paused due to running out of data operations.
     */
    LocationFusionRK &withPublishPeriodic(std::chrono::milliseconds ms) { publishFrequency = PublishFrequency::periodic; publishPeriod = ms; return *this; };

    /**
     * @brief Get the current publish frequency. Default is manual.
     * 
     * @return PublishFrequency 
     */
    PublishFrequency getPublishFrequency() const { return publishFrequency; };

    /**
     * @brief Add Wi-Fi access points nearby to the loc event. Default is false.
     * 
     * @param enable 
     * @return LocationFusionRK& 
     * 
     * This can be called on devices without Wi-Fi (B-SoM, for example) and it will be ignored.
     */
    LocationFusionRK &withAddWiFi(bool enable = true) { addWiFi = enable; return *this; };

    /**
     * @brief Add serving cellular tower information to the loc event. Default is false.
     * 
     * @param enable 
     * @return LocationFusionRK& 
     * 
     * This can be called on devices without cellular (P2, for example) and it will be ignored.
     */
    LocationFusionRK &withAddTower(bool enable = true) { addTower = enable; return *this; };

    /**
     * @brief Add an "add to event" handler
     * 
     * @param handler 
     * @return LocationFusionRK& 
     * 
     * The "add to event" handler allows you to add data to the loc event. You might do this if you want to
     * support a cellular modem that has built-in GNSS, or use an external GNSS receiver.
     * 
     * The handler can be a C function or C++11 lambda and has the following prototype:
     * 
     * void handler(Variant &eventData, Variant &locVariant)
     * 
     * - eventData is the whole loc event
     * - locVariant is the Variant for the inner loc object 
     * 
     */
    LocationFusionRK &withAddToEventHandler(std::function<void(Variant &eventData, Variant &locVariant)> handler) { addToEventHandlers.push_back(handler); return *this; };
    

    /**
     * @brief Adds a handler when the Particle function "cmd" is received.
     * 
     * @param cmd 
     * @param handler 
     * @return LocationFusionRK& 
     * 
     * Your handler is called when the "cmd" field within the function JSON matches.
     * 
     * The handler can be a C function or C++11 lambda. The prototype is:
     */
    LocationFusionRK &withCmdHandler(const char *cmd, std::function<void(const Variant &data)> handler);


    /**
     * @brief Enables or disables the "cmd" function handler. Default is enabled.
     * 
     * @param enable 
     * @return LocationFusionRK& 
     * 
     * Must be called before setup()! 
     * 
     * If disabled, withCmdHandler() and withLocEnhancedHandler() will not function because they require
     * the cmd handler. 
     * 
     * If you need to additional custom cmd handlers instead of disabling support, use `withCmdHandler()` to
     * hook your code into this library instead of the other direction.
     */
    LocationFusionRK &withEnableCmdFunction(bool enable) { enableCmdFunction = enable; return *this; };

    /**
     * @brief Adds a handler when loc-enhanced data is calculated by the cloud.
     * 
     * @param handler 
     * @return LocationFusionRK& 
     * 
     * The handler prototype is:
     * 
     * void handler(const Variant &data);
     * 
     * Fields typically in data in the handler:
     * - h_acc horizontal accuracy (meters)
     * - lat latitude
     * - lon longitude
     * 
     * If you do not add a handler, the loc-enhanced data is not sent to the device. Handling loc-enhanced data locally on device adds one data operation.
     */
    LocationFusionRK &withLocEnhancedHandler(std::function<void(const Variant &data)> handler) { locEnhancedHandlers.push_back(handler); return *this; };

    /**
     * @brief Adds a handler when the status has changed. Added in version 0.0.3.
     * 
     * @param cmd 
     * @param handler 
     * @return LocationFusionRK& 
     * 
     * Your handler is called when the "cmd" field within the function JSON matches.
     * 
     * The handler can be a C function or C++11 lambda. The prototype is:
     * 
     * void handler(Status status)
     * 
     * - status The new status value
     * 
     * This method uses a callback to be notified when the status changes. It is called from the library's
     * worker thread so you should not do anything that will block in your callback.
     */
    LocationFusionRK &withStatusHandler(const char *cmd, std::function<void(Status)> handler) { statusHandlers.push_back(handler); return *this; };


    /**
     * @brief Sets the worker thread stack size. Must be set before setup()
     * 
     * @param size in bytes. The default if you do not call this is 6144
     * @return LocationFusionRK& 
     * 
     * Prior to version 0.0.4 the stack was 3072 bytes, but it was increased due to SOS+13 on b5som devices. You can further
     * customize the size by calling this method prior to setup().
     */
    LocationFusionRK &withThreadStackSize(size_t size) { threadStackSize = size; return *this; };

    /**
     * @brief Request a publish now
     * 
     * Works in all modes (manual, once, and periodic). Can be called when offline; it will only be calculated
     * when connected to the cloud (breathing cyan).
     */
    void requestPublish() { manualPublishRequested = true; };


    /**
     * @brief Get the current status of the library (idle, publishing, etc.)
     * 
     * @return Status 
     * 
     * This method is used for polling. The be called when the status changes, see withStatusHandler.
     */
    Status getStatus() const { return status; };

    /**
     * @brief Locks the mutex that protects shared resources
     * 
     * This is compatible with `WITH_LOCK(*this)`.
     * 
     * The mutex is not recursive so do not lock it within a locked section.
     */
    void lock() { os_mutex_lock(mutex); };

    /**
     * @brief Attempts to lock the mutex that protects shared resources
     * 
     * @return true if the mutex was locked or false if it was busy already.
     */
    bool tryLock() { return os_mutex_trylock(mutex); };

    /**
     * @brief Unlocks the mutex that protects shared resources
     */
    void unlock() { os_mutex_unlock(mutex); };

protected:

    /**
     * @brief The constructor is protected because the class is a singleton
     * 
     * Use LocationFusionRK::instance() to instantiate the singleton.
     */
    LocationFusionRK();

    /**
     * @brief The destructor is protected because the class is a singleton and cannot be deleted
     */
    virtual ~LocationFusionRK();

    /**
     * This class is a singleton and cannot be copied
     */
    LocationFusionRK(const LocationFusionRK&) = delete;

    /**
     * This class is a singleton and cannot be copied
     */
    LocationFusionRK& operator=(const LocationFusionRK&) = delete;

    /**
     * @brief Update the status and call the status handlers. Used internally. Added in 0.0.3.
     * 
     * @param status 
     * 
     * This only calls the status handlers if the status changes.
     */
    void updateStatus(Status status);

    /**
     * @brief Worker thread function
     * 
     * This method is called to perform operations in the worker thread.
     * 
     * You generally will not return from this method.
     */
    os_thread_return_t threadFunction(void);

    /**
     * @brief Internal state handler for idle and not connected to the cloud
     * 
     * Exit conditions: 
     * - Particle.connected() return true -> stateConnected
     */
    void stateIdle();

    /**
     * @brief Internal state handler for connected to the cloud
     * 
     * Exit conditions: 
     * - Particle.connected() return false -> stateIdle
     * - It's time to publish a location -> stateBuildPublish
     */
    void stateConnected();

    /**
     * @brief Internal state handler for building a publish
     * 
     * May be in this state for a while, but the state handlers are run from a worker thread so 
     * it won't affect operation of the rest of the system typically.
     * 
     * Exit conditions: 
     * - When publish begins -> statePublishWait
     */
    void stateBuildPublish();

    /**
     * @brief Internal state handler for waiting for the publish to complete
     * 
     * Exit conditions: 
     * - When publish completes -> stateConnected if not receiving loc-enhanced on-device
     *                          -> stateLocEnhancedWait if receiving loc-enhanced on-device
     * 
     * May set
     * - manualPublishRequested (set to false on success)
     * - publishCount (increments on success) 
     * - nextPublishMs increased by either publishPeriod or publishFailureRetry
     */
    void statePublishWait();

    /**
     * @brief Internal state handler for waiting for the loc-enhanced to be received
     * 
     * Exit conditions: 
     * - When loc-enhanced is received or times out  -> stateConnected
     */
    void stateLocEnhancedWait();


    /**
     * @brief Called from the Particle.function handler for "cmd"
     * 
     * @param eventData 
     * @return int 
     * 
     * Calls commandHandlers, added with withCmdHandler.
     */
    int functionHandler(const Variant &eventData);

    /**
     * @brief Called from the Particle.function handler for "cmd" 
     * 
     * @param cmd 
     * @return int 
     * 
     * Parses cmd as JSON and the calls the non-static functionHandler.
     */
    static int functionHandlerStatic(String cmd);

    /**
     * @brief Called when a "loc-enhanced" cmd function is received
     * 
     * @param eventData 
     * 
     * Calls locEnhancedHandlers.
     */
    void locEnhanced(const Variant &eventData);


    /**
     * @brief Called when a "loc-enhanced" cmd function is received
     * 
     * @param eventData 
     * 
     * This just calls the non-static method with the singleton instance.
     */
    static void locEnhancedStatic(const Variant &eventData);

    /**
     * @brief Mutex to protect shared resources
     * 
     * This is initialized in setup() so make sure you call the setup() method from the global application setup.
     */
    os_mutex_t mutex = 0;

    /**
     * @brief Worker thread instance class
     * 
     * This is initialized in setup() so make sure you call the setup() method from the global application setup.
     */
    Thread *thread = 0;

    /**
     * @brief Size of the work thread stack. Must set before setup()
     */
    size_t threadStackSize = 6144;

    /**
     * @brief How often to publish (manual, once, periodic)
     */
    PublishFrequency publishFrequency = PublishFrequency::manual;

    /**
     * @brief If publishing periodic, how often to publish.
     */
    std::chrono::milliseconds publishPeriod = 5min;

    /**
     * @brief If publish fails, how long to wait before trying again. The location will be built again.
     */
    std::chrono::milliseconds publishFailureRetry = 1min;

    /**
     * @brief Amount of time to wait for loc-enhanced
     */
    std::chrono::milliseconds locEnhancedTimeout = 1min;

    /**
     * @brief Used to handle loc-enhanced state changes
     */
    bool locEnhancedReceived = false;

    /**
     * @brief Used for calculating the locEnhancedTimeout
     */
    unsigned long stateTime = 0;

    /**
     * @brief State handler. Run from the worker thread.
     */
    std::function<void(LocationFusionRK &)> stateHandler = &LocationFusionRK::stateIdle;

    /**
     * @brief When building a location publish, add Wi-Fi access point information.
     * 
     * Default is false, set uskng withAddWiFi().
     */
    bool addWiFi = false;

    /**
     * @brief When building a location publish, add serving tower information
     * 
     * Default is false, set uskng withAddTower().
     */
    bool addTower = false;

    /**
     * @brief Vector of handlers to add more information to the location event.
     * 
     * Add using withAddToEventHandler(). You can add multiple handlers.
     */
    std::vector<std::function<void(Variant &eventData, Variant &locVariant)>> addToEventHandlers;

    /**
     * @brief Add a function handler for "cmd"
     * 
     * This is enabled by default and must be enabled for loc-enhanced on device to work.
     */
    bool enableCmdFunction = true;

    /**
     * @brief This class is used internally for registerCommand
     */
    struct CmdHandler {
        String cmd; //!< The code that matches the cmd field within the JSON body
        std::function<void(const Variant &data)> handler; //!< Function to call if cmd matches
    };

    /**
     * @brief Handler functions to call when a cmd Particle.function is received
     */
    std::vector<CmdHandler> commandHandlers;

    /**
     * @brief Handler functions to call when loc-enhanced is received on-device.
     * 
     */
    std::vector<std::function<void(const Variant &eventData)>> locEnhancedHandlers;

    /**
     * @brief true when a manual publish has been requested
     * 
     * This can be requested when offline as it will be handled when online. This can also
     * be used in once and periodic modes to publish npw, out of schedule.
     */
    bool manualPublishRequested = false;

    /**
     * @brief Number of successful publishes. THis is used to handle once mode.
     */
    int publishCount = 0;

    /**
     * @brief Event being built or sent
     */
    CloudEvent event;

    /**
     * @brief The data payload being build for the loc event.
     */
    Variant eventData;

    /**
     * @brief When to publish next in periodic mode. Compare to System.millis().
     * 
     * Since this is a uint64_t it does not roll over so comparisons are easier.
     */
    uint64_t nextPublishMs = 0;

    /**
     * @brief loc events contain a request ID, this is the next one to use
     */
    int locRequestId = 1;

    /**
     * @brief Current status pof the library
     */
    Status status = Status::idle;

    /**
     * @brief Handlers to call when the status changes
     */
    std::vector<std::function<void(Status)>> statusHandlers;
    

    /**
     * @brief Singleton instance of this class
     * 
     * The object pointer to this class is stored here. It's NULL at system boot.
     */
    static LocationFusionRK *_instance;

};
#endif  /* __LOCATIONFUSIONRK_H */
