#pragma once
#include <boost/date_time/time_duration.hpp>

/**
 *  * just to ease the test writing
 *   * enable the creation of duration just by writing 123_s => creates a boost::time_duration of 123 seconds
 *    * Note: I had to create a new class since boost does not define the constructors constexpr
 *     */
struct custom_seconds
{
        int dur;
            constexpr custom_seconds(int s) : dur(s) {}
                custom_seconds(const custom_seconds&) = default;
                    custom_seconds& operator=(const custom_seconds&) = default;
                        operator bt::time_duration() const { return bt::seconds(dur); }
};
std::ostream & operator<<(std::ostream& os, custom_seconds s) {
        os << s.dur << " seconds";
            return os;
}

inline constexpr custom_seconds operator"" _s(unsigned long long v) {
        return custom_seconds(v);
}
