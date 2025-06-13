/*
 * File:   TimeUtils.h
 * Author: thaipq
 *
 * Created on Fri Jun 13 2025 10:14:50 AM
 */

#ifndef LIBS_TIMEUTILS_H
#define LIBS_TIMEUTILS_H

#include <chrono>
#include <string>
#include <stdexcept>

namespace libs {
    inline std::string getDateTimeStr(
            const std::chrono::system_clock::time_point &tp = std::chrono::system_clock::now(),
            const char *format = "%F %T") {
        char timeString[128];
        std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::strftime(timeString, sizeof(timeString), format, std::localtime(&t));
        return timeString;
    }

    template<typename ClkType, typename Dur = typename ClkType::duration>
    inline std::string getEpochsStr(const std::chrono::time_point<ClkType, Dur> &tp = typename ClkType::now()) {
        using std::chrono::duration_cast;
        using std::chrono::seconds;
        auto epochsNow = duration_cast<seconds>(tp.time_since_epoch()).count();
        return std::to_string(epochsNow);
    }

    template<typename Rep, typename Period>
    inline std::chrono::system_clock::time_point nextTimePoint(
            std::chrono::duration<Rep, Period> &&d,
            std::chrono::system_clock::time_point base = std::chrono::system_clock::now()) {
        base += d;
        return base;
    }

    inline std::chrono::system_clock::time_point nextTimePointAt(
            int hour, int min, int sec,
            std::chrono::system_clock::time_point baseTp = std::chrono::system_clock::now()) {
        // calculate next time point at HH:MM:SS
        using std::chrono::system_clock;
        if (hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 60) {
            throw std::runtime_error("invalid time params");
        }
        std::time_t t = system_clock::to_time_t(baseTp);
        auto basetm = std::localtime(&t);
        basetm->tm_hour = hour;
        basetm->tm_min = min;
        basetm->tm_sec = sec;
        auto nextTp = system_clock::from_time_t(std::mktime(basetm));
        if (nextTp <= baseTp) {// schedule for next day
            nextTp += std::chrono::hours(24);
        }
        return nextTp;
    }
}// namespace libs

#endif// LIBS_TIMEUTILS_H