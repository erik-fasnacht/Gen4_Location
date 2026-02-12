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

#ifndef SYSTEM_VERSION_v582
#error "This library must be built with device OS version >= 5.8.2"
#endif // SYSTEM_VERSION_v582

constexpr system_tick_t LOCATION_PERIOD_SUCCESS_MS {1 * 1000};
constexpr system_tick_t LOCATION_INACTIVE_PERIOD_SUCCESS_MS {120 * 1000};
constexpr system_tick_t LOCATION_PERIOD_ACQUIRE_MS {1 * 1000};
constexpr system_tick_t ANTENNA_POWER_SETTLING_MS {100};
constexpr int LOCATION_REQUIRED_SETTLING_COUNT {2};  // Number of consecutive fixes

Logger locationLog("loc");

QuectelGnssRK *QuectelGnssRK::_instance = nullptr;

QuectelGnssRK::QuectelGnssRK() {
    os_queue_create(&_commandQueue, sizeof(LocationCommandContext), 1, nullptr);
    os_queue_create(&_responseQueue, sizeof(LocationResults), 1, nullptr);
    _thread = new Thread("gnss_cellular", [this]() {QuectelGnssRK::threadLoop();}, OS_THREAD_PRIORITY_DEFAULT);
}

bool QuectelGnssRK::detectModemType() {
    bool detected = false;

    if (modemNotDetected() && isModemOn()) {
        CellularDevice celldev = {};
        cellular_device_info(&celldev, nullptr);
        locationLog.trace("Modem ID is %d", celldev.dev);
        switch (celldev.dev) {
          case 0:
            locationLog.trace("Modem not cached yet");
            // Do not set modem type
            break;

          case DEV_QUECTEL_BG95_M5:
          case DEV_QUECTEL_BG95_S5:
            _modemType = _ModemType::BG95_M5;
            locationLog.trace("BG95-M5 or -S5 detected");
            detected = true;
            break;

           case DEV_QUECTEL_EG91_EX:
           case DEV_QUECTEL_EG91_NAX:
             _modemType = _ModemType::EG91;
             locationLog.trace("EG91-EX or -NAX detected");
             detected = true;
             break;

          default:
            _modemType = _ModemType::Unsupported;
            locationLog.trace("Modem type %d not supported", celldev.dev);
            break;
        }
    }
    else if (!modemNotDetected() && (_ModemType::Unsupported != _modemType)) {
        detected = true;
    }

    return detected;
}

void QuectelGnssRK::setAntennaPower() {
    if (PIN_INVALID != _antennaPowerPin) {
        locationLog.trace("setAntennaPower pin %d", _antennaPowerPin);
        digitalWrite(_antennaPowerPin, HIGH);
        delay(ANTENNA_POWER_SETTLING_MS);
    }
}

void QuectelGnssRK::clearAntennaPower() {
    if (PIN_INVALID != _antennaPowerPin) {
        locationLog.trace("clearAntennaPower pin %d", _antennaPowerPin);
        digitalWrite(_antennaPowerPin, LOW);
    }
}

int QuectelGnssRK::setConstellation(LocationConstellation flags) {
    int configNumber = -1;


    if (_ModemType::BG95_M5 == _modemType) { // -M5 or -S5
        if ((flags & LOCATION_CONST_GPS_ONLY) ||
            (flags & LOCATION_CONST_GPS_GLONASS)) {

            configNumber = 1; // GPS + GLONASS
        }
        else if (flags & LOCATION_CONST_GPS_BEIDOU) {
            configNumber = 2; // GPS + BeiDou
        }
        else if (flags & LOCATION_CONST_GPS_GALILEO) {
            configNumber = 3; // GPS + Galileo
        }
        else if (flags & LOCATION_CONST_GPS_QZSS) {
            configNumber = 4; // GPS + QZSS
        }
    }
    else
    if (_ModemType::EG91 == _modemType) { // -EX or -NAX
        if (flags & LOCATION_CONST_GPS_ONLY) {
            configNumber = 0;
        }
        else if (flags & LOCATION_CONST_GPS_GLONASS) {
            configNumber = 4; // GPS + GLONASS
        }
        else if (flags & LOCATION_CONST_GPS_BEIDOU) {
            configNumber = 7; // GPS + BeiDou
        }
        else if (flags & LOCATION_CONST_GPS_GALILEO) {
            configNumber = 6; // GPS + Galileo
        }
        else if (flags & LOCATION_CONST_GPS_QZSS) {
            // Not supported by EG91?
        }
    }


    if (configNumber >= 0) {
        locationLog.trace("set constellations %d", configNumber);

        char command[64] = {};
        sprintf(command, "AT+QGPSCFG=\"gnssconfig\",%d", configNumber);
        Cellular.command(command);
    }
    return 0;
}

int QuectelGnssRK::begin(LocationConfiguration& configuration) {
    locationLog.info("Beginning location library");
    _conf = configuration;
    _antennaPowerPin = _conf.enableAntennaPower();
    if (PIN_INVALID != _antennaPowerPin) {
        locationLog.info("Configuring antenna pin");
        pinMode(_antennaPowerPin, OUTPUT);
    }

    if (isModemOn() && modemNotDetected()) {
        locationLog.info("Detecting modem type");
        detectModemType();

        setConstellation(_conf.constellations());
    }

    return 0;
}

QuectelGnssRK::LocationResults QuectelGnssRK::getLocation(LocationPoint& point, bool publish) {
    memset(&point, 0, sizeof(LocationPoint));
    
    if (!isModemOn()) {
        locationLog.trace("Modem is not on");
        lastResults = LocationResults::Unavailable;
        return lastResults;
    }
    if (modemNotDetected()) {
        auto detected = detectModemType();
        if (!detected) {
            locationLog.trace("Modem is not supported");
            lastResults = LocationResults::Unsupported;
            return lastResults;
        }
    }

    // Check if already running
    if (_acquiring.load()) {
        locationLog.trace("Acquisition is already underway");
        lastResults = LocationResults::Pending;
        return lastResults;
    }
    locationLog.trace("Starting synchronous acquisition");
    
    LocationCommandContext event;
    
    event.command = LocationCommand::Acquire;
    event.point = &point;
    event.sendResponse = true;
    event.doneCallback = nullptr;
    os_queue_put(_commandQueue, &event, 0, nullptr);
    auto result = waitOnResponseEvent((system_tick_t)_conf.maximumFixTime() * 1000 + LOCATION_PERIOD_ACQUIRE_MS);
    if (publish && (LocationResults::Fixed == result) && isConnected()) {
        locationLog.info("Publishing loc event");
        buildPublish(_publishBuffer, sizeof(_publishBuffer), point, _reqid);
        auto published = Particle.publish("loc", _publishBuffer);
        if (published) {
            _reqid++;
        }
    }
    return result;
}

QuectelGnssRK::LocationResults QuectelGnssRK::getLocationAsync(LocationDoneCallback callback) {
    if (!isModemOn()) {
        locationLog.trace("Modem is not on");
        lastResults = LocationResults::Unavailable;
        return lastResults;
    }
    if (modemNotDetected()) {
        auto detected = detectModemType();
        if (!detected) {
            locationLog.trace("Modem is not supported");
            lastResults = LocationResults::Unsupported;
            return lastResults;
        }
    }

    // Check if already running
    if (_acquiring.load()) {
        locationLog.trace("Acquisition is already underway");
        lastResults = LocationResults::Pending;
        return lastResults;
    }
    
    locationLog.trace("Starting asynchronous acquisition");
    LocationCommandContext event {};
    event.command = LocationCommand::Acquire;
    event.point = nullptr;
    event.sendResponse = false;
    event.doneCallback = callback;
    event.publish = false;
    os_queue_put(_commandQueue, &event, 0, nullptr);
    return LocationResults::Acquiring;
}

QuectelGnssRK::LocationCommandContext QuectelGnssRK::waitOnCommandEvent(system_tick_t timeout) {
    LocationCommandContext event = {};
    auto ret = os_queue_take(_commandQueue, &event, timeout, nullptr);
    if (ret) {
        event.command = LocationCommand::None;
        event.point = nullptr;
    }

    return event;
}

QuectelGnssRK::LocationResults QuectelGnssRK::waitOnResponseEvent(system_tick_t timeout) {
    LocationResults event = {LocationResults::Idle};
    auto ret = os_queue_take(_responseQueue, &event, timeout, nullptr);
    if (ret) {
        event = LocationResults::Idle;
    }

    return event;
}

void QuectelGnssRK::stripLfCr(char* str) {
    if (!str) {
        return;
    }

    auto read = str;
    auto write = str;

    while ('\0' != *read) {
        if (('\n' != *read) && ('\r' != *read)) {
            *write++ = *read;
        }
        ++read;
    }

    // Null-terminate the resulting string
    *write = '\0';
}

int QuectelGnssRK::glocCallback(int type, const char* buf, int len, char* locBuffer) {
    switch (type) {
        case TYPE_PLUS:
            // fallthrough
        case TYPE_ERROR:
            strlcpy(locBuffer, buf, min((size_t)len, sizeof(QuectelGnssRK::_locBuffer)));
            stripLfCr(locBuffer);
            locationLog.trace("glocCallback: (%06x) %s", type, locBuffer);
            break;
    }

    return WAIT;
}

int QuectelGnssRK::epeCallback(int type, const char* buf, int len, char* epeBuffer) {
    switch (type) {
        case TYPE_PLUS:
            // fallthrough
        case TYPE_ERROR:
            strlcpy(epeBuffer, buf, min((size_t)len, sizeof(QuectelGnssRK::_epeBuffer)));
            stripLfCr(epeBuffer);
            break;
    }

    return WAIT;
}

QuectelGnssRK::CME_Error QuectelGnssRK::parseCmeError(const char* buf) {
    unsigned int error_code = 0;
    auto nargs = sscanf(buf," +CME ERROR: %u", &error_code);

    if (0 == nargs) {
        return CME_Error::NONE;
    }

    auto ret = CME_Error::UNDEFINED;

    switch (error_code) {
        case 504:
            // fallthrough
        case 505:
            // fallthrough
        case 506:
            // fallthrough
        case 516:
            // fallthrough
        case 522:
            // fallthrough
        case 549:
            ret = static_cast<CME_Error>(error_code);
            break;
    }

    return ret;
}

int QuectelGnssRK::parseQloc(const char* buf, QlocContext& context, LocationPoint& point) {
    // The general form of the AT command response is as follows
    // <UTC HHMMSS.hh>,<latitude (-)dd.ddddd>,<longitude (-)ddd.ddddd>,<HDOP>,<altitude>,<fix>,<COG ddd.mm>,<spkm>,<spkn>,<date DDmmyy>,<nsat>
    auto nargs = sscanf(buf, " +QGPSLOC: %02u%02u%02u.%*03u,%lf,%lf,%f,%f,%u,%03u.%02u,%f,%f,%02u%02u%02u,%u",
                        &context.tm_hour, &context.tm_min, &context.tm_sec,
                        &context.latitude, &context.longitude, &context.hdop, &context.altitude,
                        &context.fix, &context.cogDegrees, &context.cogMinutes, &context.speedKmph, &context.speedKnots,
                        &context.tm_day, &context.tm_month, &context.tm_year,
                        &context.nsat);

    if (0 == nargs) {
        return -1;
    }

    // Although there are several QLOC output options, we are taking the format that gives us the appropriate number of
    // significant digits for the supported accuracy.
    // QLOC=0 would give us ddmm.mmmmN/S, dddmm.mmmmE/W resulting in 8 significant digits for latitude and 9 in longitude
    // QLOC=1 would give us ddmm.mmmmmm,N/S, dddmm.mmmmmm,E/W resulting in 10 significant digits for latitude and 11 in longitude
    // QLOC=2 would give us (-)dd.ddddd, (-)ddd.ddddd resulting in 7 significant digits for latitude and 8 in longitude

    // Convert tm structure to time_t (epoch time)
    context.timeinfo.tm_year = context.tm_year + 2000 - 1900;  // GPRMC year from 2000 and then the difference from 1900
    context.timeinfo.tm_mon = context.tm_month - 1;     // The number of months since January (0-11)
    context.timeinfo.tm_mday = context.tm_day;
    context.timeinfo.tm_hour = context.tm_hour;
    context.timeinfo.tm_min = context.tm_min;
    context.timeinfo.tm_sec = context.tm_sec;
    point.epochTime = std::mktime(&context.timeinfo);

    point.fix = context.fix;
    point.latitude = context.latitude;
    point.longitude = context.longitude;
    point.altitude = context.altitude;
    point.speed = context.speedKmph * 1000.0;
    point.heading = (float)context.cogDegrees + (float)context.cogMinutes / 60.0;
    point.horizontalDop = context.hdop;
    point.satsInUse = context.nsat;

    return 0;
}

QuectelGnssRK::CME_Error QuectelGnssRK::parseQlocResponse(const char* buf, QlocContext& context, LocationPoint& point) {
    // Only expect the following CME error codes if present
    //   CME_Error::SESSION_IS_ONGOING - if GNSS is not enabled or ready
    //   CME_Error::SESSION_NOT_ACTIVE - if GNSS is not enabled or ready
    //   CME_Error::NO_FIX - if GNSS acquiring and not fixed
    auto result = parseCmeError(buf);

    if (CME_Error::NO_FIX == result) {
        point.fix = 0;
        return result; // module explicitly reported GNSS no fix
    }
    if (CME_Error::NONE != result) {
        return CME_Error::NONE;  // module just may have not been initialized
    }

    int locResult = parseQloc(buf, context, point);
    return locResult == 0 ? CME_Error::FIX : CME_Error::UNKNOWN_ERROR;
}

void QuectelGnssRK::parseEpeResponse(const char* buf, EpeContext& context, LocationPoint& point) {
    // Only expect the following CME error codes
    //   CME_Error::SESSION_IS_ONGOING - if GNSS is not enabled or ready
    //   CME_Error::SESSION_NOT_ACTIVE - if GNSS is not enabled or ready
    //   CME_Error::NO_FIX - if GNSS acquiring and not fixed
    auto result = parseCmeError(buf);

    if (CME_Error::NONE != result) {
        return;  // module just may have not been initialized
    }

    auto nargs = sscanf(buf, " +QGPSCFG: \"estimation_error\",%f,%f,%f,%f",
                        &context.h_acc, &context.v_acc, &context.speed_acc, &context.head_acc);

    if (nargs) {
        point.horizontalAccuracy = context.h_acc;
        point.verticalAccuracy = context.v_acc;
    }

    return;
}

void QuectelGnssRK::threadLoop()
{
    auto loop = true;
    while (loop) {
        // Look for requests and provide a loop delay
        auto event = waitOnCommandEvent(LOCATION_PERIOD_SUCCESS_MS);

        switch (event.command) {
            case LocationCommand::None:
                // Do nothing
                break;

            case LocationCommand::Acquire: {
                _acquiring.store(true);
                SCOPE_GUARD({
                    _acquiring.store(false);
                });
                memset(&lastLocation, 0, sizeof(lastLocation));

                if (!gnssStarted) {
                    setAntennaPower();

                    locationLog.trace("Started acquisition");
                    Cellular.command(R"(AT+QGPS=1)");
                    if (_ModemType::BG95_M5 == _modemType) {
                        Cellular.command(R"(AT+QGPSCFG="nmea_epe",1)");
                    }
                    setConstellation(_conf.constellations());
                    gnssStarted = true;
                    timeToFirstFixMs = 0;
                }


                auto maxTime = (uint64_t)_conf.maximumFixTime() * 1000;
                int fixCount = {};
                LocationResults response = LocationResults::TimedOut;
                bool power = false;
                auto start = System.millis();
                while ((power = isModemOn())) {
                    auto now = System.millis();
                    if ((now - start) >= maxTime)
                        break;
                    Cellular.command(glocCallback, _locBuffer, 1000, R"(AT+QGPSLOC=2)");
                    auto ret = parseQlocResponse(_locBuffer, _qlocContext, lastLocation);
                    if (CME_Error::FIX == ret) {
                        fixCount++;
                        lastLocation.systemTime = Time.now();

                        if (0 == timeToFirstFixMs) {
                            timeToFirstFixMs = (uint32_t) (System.millis() - start);
                            locationLog.info("timeToFirstFix %lu ms", timeToFirstFixMs);
                        }
                        if (_ModemType::BG95_M5 == _modemType) {
                            // This is not supported on EG91, returns CME Error 501
                            Cellular.command(epeCallback, _epeBuffer, 1000, R"(AT+QGPSCFG="estimation_error")");
                            parseEpeResponse(_epeBuffer, _epeContext, lastLocation);
                        }
                        if ((LOCATION_REQUIRED_SETTLING_COUNT == fixCount) &&
                            (lastLocation.horizontalDop <= _conf.hdopThreshold()) &&
                            (lastLocation.horizontalAccuracy <= _conf.haccThreshold())) {

                            response = LocationResults::Fixed;
                            break;
                        }
                    }

                    delay(LOCATION_PERIOD_ACQUIRE_MS);
                }

                if (!concurrentGnssAndCellularSupported()) {
                    Cellular.command(R"(AT+QGPSEND)");

                    clearAntennaPower();
                    gnssStarted = false;
                }

                if (!power && (LocationResults::Fixed != response)) {
                    response = LocationResults::Unavailable;
                }

                if (timeToFirstFixMs) {
                    lastLocation.timeToFirstFix = (float)timeToFirstFixMs / 1000.0;
                }
                if (event.point) {
                    *event.point = lastLocation;
                }
                lastResults = response;

                if (event.sendResponse) {
                    locationLog.trace("Sending synchronous completion");
                    os_queue_put(_responseQueue, &response, 0, nullptr);
                }
                else if (event.doneCallback) {
                    locationLog.trace("Sending asynchronous completion");
                    event.doneCallback(response, lastLocation);
                }

                break;
            }

            case LocationCommand::Exit:
                // Get out of main loop and join
                loop = false;
                break;

            default:
                break;
        }
    }

    // Kill the thread if we get here
    _thread->cancel();
}

bool QuectelGnssRK::publishLocationEvent(const LocationPoint *point) {
    bool published = false;

    if (!point) {
        point = &lastLocation;
    }

    if (Particle.connected()) {
        locationLog.info("Publishing loc event");
        buildPublish(_publishBuffer, sizeof(_publishBuffer), *point, _reqid++);
        published = Particle.publish("loc", _publishBuffer);
    }

    return published;
}    

#ifdef SYSTEM_VERSION_v620
void QuectelGnssRK::getLocationEventVariant(Variant &obj, const LocationPoint *point) {
    if (!point) {
        point = &lastLocation;
    }

    obj.set("cmd", Variant("loc"));
    if (point->systemTime) {
        obj.set("time", Variant((unsigned int)point->systemTime));
    }
    Variant innerLoc;
    point->toVariant(innerLoc);
    obj.set("loc", innerLoc);

    obj.set("req_id", Variant(_reqid++));
}
#endif // SYSTEM_VERSION_v620



size_t QuectelGnssRK::buildPublish(char* buffer, size_t len, const LocationPoint& point, unsigned int seq) {
    memset(buffer, 0, len);
    JSONBufferWriter writer(buffer, len);
    writer.beginObject();
        writer.name("cmd").value("loc");
        if (point.systemTime) {
            writer.name("time").value((unsigned int)point.systemTime);
        }
        writer.name("loc");
        point.toJsonWriter(writer, true);
        writer.name("req_id").value(seq);
    writer.endObject();

    return writer.dataSize();
}

String QuectelGnssRK::LocationPoint::toStringSimple() const {
    return String::format("lat=%0.5lf, lon=%0.5lf, alt=%0.1f m, speed=%0.1f m/s, heading=%0.1f deg, ttff=%.2f", 
        latitude, longitude, altitude, speed, heading, timeToFirstFix);
}


void QuectelGnssRK::LocationPoint::toJsonWriter(JSONWriter &writer, bool wrapInObject) const {

    if (wrapInObject) {
        writer.beginObject();
    }

    if (0 == fix) {
        writer.name("lck").value(0);
    }
    else {
        writer.name("lck").value(1);
        writer.name("time").value((unsigned int)epochTime);
        writer.name("lat").value(latitude, 8);
        writer.name("lon").value(longitude, 8);
        writer.name("alt").value(altitude, 3);
        writer.name("hd").value(heading, 2);
        writer.name("spd").value(speed, 2);
        writer.name("hdop").value(horizontalDop, 1);
        if (0.0 < horizontalAccuracy) {
            writer.name("h_acc").value(horizontalAccuracy, 3);
        }
        if (0.0 < verticalAccuracy) {
            writer.name("v_acc").value(verticalAccuracy, 3);
        }
        writer.name("nsat").value(satsInUse);
        writer.name("ttff").value(timeToFirstFix, 1);
    }
    if (wrapInObject) {
        writer.endObject();
    }
}

#ifdef SYSTEM_VERSION_v620
void QuectelGnssRK::LocationPoint::toVariant(Variant &obj) const {

    if (0 == fix) {
        obj.set("lck", Variant(0));
    }
    else {
        obj.set("lck", Variant(1));
        obj.set("time", Variant((unsigned int)epochTime));
        obj.set("lat", Variant(latitude));
        obj.set("lon", Variant(longitude));
        obj.set("alt", Variant(altitude));
        obj.set("hd", Variant(heading));
        obj.set("spd", Variant(speed));
        obj.set("hdop", Variant(horizontalDop));
        if (0.0 < horizontalAccuracy) {
            obj.set("h_acc", Variant(horizontalAccuracy));
        }
        if (0.0 < verticalAccuracy) {
            obj.set("v_acc", Variant(verticalAccuracy));
        }
        obj.set("nsat", Variant(satsInUse));
        obj.set("ttff", Variant(timeToFirstFix));
    }

}
#endif // SYSTEM_VERSION_v620

bool QuectelGnssRK::concurrentGnssAndCellularSupported() const {
    if (_ModemType::BG95_M5 == _modemType) { // -M5 or -S5
        return false;
    }
    else {
        return true;
    }
}

// [static]
void QuectelGnssRK::addToEventHandler(Variant &eventData, Variant &locVariant) {
    bool done = false;

    locationLog.trace("addToEventHandler starting");

    LocationResults result = instance().getLocationAsync([&done, &locVariant](LocationResults, const LocationPoint& point) {
        point.toVariant(locVariant);
        done = true;
    });

    while(!done) {
        delay(1);
    }

    locationLog.trace("addToEventHandler getLocationAsync complete");
}

