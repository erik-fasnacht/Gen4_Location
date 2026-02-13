#include "LocationFusionRK.h"

static Logger _locfLog("app.locf");

LocationFusionRK *LocationFusionRK::_instance;

// [static]
LocationFusionRK &LocationFusionRK::instance() {
    if (!_instance) {
        _instance = new LocationFusionRK();
    }
    return *_instance;
}

LocationFusionRK::LocationFusionRK() {
}

LocationFusionRK::~LocationFusionRK() {
}

LocationFusionRK &LocationFusionRK::withCmdHandler(const char *cmd, std::function<void(const Variant &data)> handler) {
    CmdHandler cmdHandler;
    cmdHandler.cmd = cmd;
    cmdHandler.handler = handler;

    commandHandlers.push_back(cmdHandler);
    return *this;
}


void LocationFusionRK::setup() {
    os_mutex_create(&mutex);

    thread = new Thread("LocationFusionRK", [this]() { return threadFunction(); }, OS_THREAD_PRIORITY_DEFAULT, threadStackSize);

    if (enableCmdFunction) {
        Particle.function("cmd", functionHandlerStatic);

        
        withCmdHandler("loc-enhanced", locEnhancedStatic);
    }
}

void LocationFusionRK::updateStatus(Status status) {
    if (this->status != status) {
        this->status = status;

        for(auto it = statusHandlers.begin(); it != statusHandlers.end(); it++) {
            auto handler = *it;

            handler(status);
        }
    }
}



os_thread_return_t LocationFusionRK::threadFunction(void) {
    while(true) {
        // Put your code to run in the worker thread here
        stateHandler(*this);
        delay(1);
    }
}

void LocationFusionRK::stateIdle() {
    updateStatus(Status::idle);

    if (Particle.connected()) {
        stateHandler = &LocationFusionRK::stateConnected;
        return;
    }
}

void LocationFusionRK::stateConnected() {
    updateStatus(Status::idle);

    if (!Particle.connected()) {
        stateHandler = &LocationFusionRK::stateIdle;
        return;
    }

    if (!manualPublishRequested) {
        switch(publishFrequency) {
            case PublishFrequency::manual:
                // If we get here, manual publish mode and publish not requested
                return;

            case PublishFrequency::once: 
                if (publishCount > 0) {
                    // Already published and not manually requested
                    return;
                }
                break;   
                
            case PublishFrequency::periodic:
                // nextPublishMs is a uint64_t, so it's safe to compare this way as it never wraps
                if (System.millis() < nextPublishMs) {
                    // Not time to publish
                    return;
                }
                break;
        }

    }

    // If we get here. it's time to publish a location event
    stateHandler = &LocationFusionRK::stateBuildPublish;
}

void LocationFusionRK::stateBuildPublish() {
    updateStatus(Status::publishing);
    eventData = Variant();
    locEnhancedReceived = false;

    eventData.set("cmd", Variant("loc"));
    if (Time.isValid()) {
        eventData.set("time", Time.now());
    }

    if (locEnhancedHandlers.size() > 0) {
        eventData.set("loc_cb", 1);
    }

    Variant locVariant;
    locVariant.set("lck", 0);

#if Wiring_WiFi 
    if (addWiFi) {
        LocationFusionRK::WAPList wapList;

        wapList.scan();
        if (wapList.size()) {
            Variant arrayVariant;

            wapList.toVariant(arrayVariant);
            
            eventData.set("wps", arrayVariant);
        }

    }
#endif // Wiring_WiFi 

#if Wiring_Cellular
    if (addTower) {
        LocationFusionRK::ServingTower servingTower;
        if (servingTower.get() == SYSTEM_ERROR_NONE) {
            Variant servingTowerVariant;
            servingTower.toVariant(servingTowerVariant);

            Variant arrayVariant;
            arrayVariant.append(servingTowerVariant);

            eventData.set("towers", arrayVariant);
        }
    }
#endif // Wiring_Cellular

    // Call handlers to add custom data (such as GNSS). GNSS gets added to an inner loc key.
    for(auto it = addToEventHandlers.begin(); it != addToEventHandlers.end(); it++) {
        auto handler = *it;

        handler(eventData, locVariant);
    }
    eventData.set("loc", locVariant);

    eventData.set("req_id", locRequestId++);

    Log.info("Publishing loc event...");
    event.name("loc");
    event.data(eventData);
    Particle.publish(event);

    stateHandler = &LocationFusionRK::statePublishWait;
}



void LocationFusionRK::statePublishWait() {
    if (event.isSent()) {
        updateStatus(Status::publishSuccess);
        _locfLog.info("publish succeeded");
        event.clear();

        if (locEnhancedHandlers.size()) {
            stateTime = millis();
            stateHandler = &LocationFusionRK::stateLocEnhancedWait;
        }
        else {
            stateHandler = &LocationFusionRK::stateConnected;
        }

        manualPublishRequested = false;
        publishCount++;

        nextPublishMs = System.millis() + publishPeriod.count();
    }
    else 
    if (!event.isOk()) {
        updateStatus(Status::publishFail);
        _locfLog.info("publish failed error=%d", event.error());
        event.clear();
        stateHandler = &LocationFusionRK::stateConnected;
        
        nextPublishMs = System.millis() + publishFailureRetry.count();
    }

}

void LocationFusionRK::stateLocEnhancedWait() {
    updateStatus(Status::locEnhancedWait);

    if (locEnhancedReceived) {
        updateStatus(Status::locEnhancedSuccess);
        stateHandler = &LocationFusionRK::stateConnected;
        return;
    }
    if (millis() - stateTime >= locEnhancedTimeout.count()) {
        updateStatus(Status::locEnhancedFail);
        stateHandler = &LocationFusionRK::stateConnected;
        return;
    }
}


int LocationFusionRK::functionHandler(const Variant &eventData) {
     _locfLog.trace("cmd function %s", eventData.toJSON().c_str());

     String cmd = eventData.get("cmd").toString();

    for(auto it = commandHandlers.begin(); it != commandHandlers.end(); it++) {
        CmdHandler cmdHandler = *it;
        
        if (cmd == cmdHandler.cmd) {
            cmdHandler.handler(eventData);
        }
    }
    return 0;
}


// [static]
int LocationFusionRK::functionHandlerStatic(String cmd) {

    Variant eventData = Variant::fromJSON(cmd.c_str());

    return instance().functionHandler(eventData);
}

void LocationFusionRK::locEnhanced(const Variant &eventData) {
    locEnhancedReceived = true;
    for(auto it = locEnhancedHandlers.begin(); it != locEnhancedHandlers.end(); it++) {
        (*it)(eventData);
    }
}


// [static] 
void LocationFusionRK::locEnhancedStatic(const Variant &eventData) {
    instance().locEnhanced(eventData);
}



#if Wiring_WiFi 

//
// WAPEntry
//
LocationFusionRK::WAPEntry::WAPEntry() {
}

LocationFusionRK::WAPEntry::WAPEntry(const WiFiAccessPoint *wap) {
    fromWiFiAccessPoint(wap);
}

void LocationFusionRK::WAPEntry::fromWiFiAccessPoint(const WiFiAccessPoint *wap) {
    memcpy(bssid, wap->bssid, sizeof(bssid));
    channel = wap->channel;
    rssi = wap->rssi;
}

void LocationFusionRK::WAPEntry::toJsonWriter(JSONWriter &writer, bool wrapInObject) const {
    if (wrapInObject) {
        writer.beginObject();
    }

    writer.name("bssid").value(bssidString().c_str());
    writer.name("ch").value((unsigned)channel);
    writer.name("str").value(rssi);

    if (wrapInObject) {
        writer.endObject();
    }
}

void LocationFusionRK::WAPEntry::toVariant(Variant &obj) const {
    obj.set("bssid", Variant(bssidString().c_str()));
    obj.set("ch", Variant((unsigned)channel));
    obj.set("str", Variant(rssi));
}

String LocationFusionRK::WAPEntry::bssidString() const {
    return String::format("%02x:%02x:%02x:%02x:%02x:%02x", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
}
#endif // Wiring_WiFi 





#if Wiring_WiFi 
//
// WAPList
// 

void LocationFusionRK::WAPList::scan() {
    wapArray.clear();

    _locfLog.trace("WAPList::scan called");

    int res = WiFi.scan(scanCallbackStatic, this);

    _locfLog.trace("WAPList::scan returned %d", res);

}

void LocationFusionRK::WAPList::appendEntry(const WAPEntry &entry) {
    wapArray.push_back(entry);
}

void LocationFusionRK::WAPList::appendEntry(const WiFiAccessPoint *wap) {
    LocationFusionRK::WAPEntry entry(wap);
    appendEntry(entry);
}


void LocationFusionRK::WAPList::toJsonWriter(JSONWriter &writer, int numToInclude) const {
    int numAdded = 0;

    writer.beginArray();

    for(auto it = wapArray.begin(); it != wapArray.end(); ++it) {
        if (numToInclude != 0 && numAdded >= numToInclude) {
            break;
        }
        (*it).toJsonWriter(writer, true);
        numAdded++;
    }

    writer.endArray();
}

void LocationFusionRK::WAPList::toVariant(Variant &obj, int numToInclude) const {
    int numAdded = 0;

    for(auto it = wapArray.begin(); it != wapArray.end(); ++it) {
        if (numToInclude != 0 && numAdded >= numToInclude) {
            break;
        }
        Variant obj2;
        (*it).toVariant(obj2);
        obj.append(obj2);
        numAdded++;
    }
}


void LocationFusionRK::WAPList::scanCallback(WiFiAccessPoint* wap) {
    appendEntry(wap);
}

// [static] 
void LocationFusionRK::WAPList::scanCallbackStatic(WiFiAccessPoint* wap, void *context) {
    ((LocationFusionRK::WAPList *)context)->scanCallback(wap);
}

#endif // Wiring_WiFi 

#if Wiring_Cellular

int LocationFusionRK::ServingTower::get() {
    memset(&cgi, 0, sizeof(CellularGlobalIdentity));
    cgi.size = sizeof(CellularGlobalIdentity);
    cgi.version = CGI_VERSION_LATEST;

    cellularResult = cellular_global_identity(&cgi, NULL);

    return (int)cellularResult;
}

void LocationFusionRK::ServingTower::toJsonWriter(JSONWriter &writer, bool wrapInObject) const {
    if (wrapInObject) {
        writer.beginObject();
    }

    writer.name("rat").value("lte");
    writer.name("mcc").value(cgi.mobile_country_code);
    writer.name("mnc").value(cgi.mobile_network_code);
    writer.name("lac").value(cgi.location_area_code);
    writer.name("cid").value(cgi.cell_id);

    // str (signal strength, rssi) could be included here 

    if (wrapInObject) {
        writer.endObject();
    }
    
}

void LocationFusionRK::ServingTower::toVariant(Variant &obj) const {
    obj.set("rat", Variant("lte"));
    obj.set("mcc", cgi.mobile_country_code);
    obj.set("mnc", cgi.mobile_network_code);
    obj.set("cid", cgi.cell_id);
    obj.set("lac", cgi.location_area_code);
}

#endif // Wiring_Cellular


