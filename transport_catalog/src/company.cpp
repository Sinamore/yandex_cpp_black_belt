#include <algorithm>
#include <cmath>
#include <limits>

#include "yellow_pages_structures.h"

double Company::WaitForCompanyOpen(double finish) const {
  if (working_time.intervals.empty()) {
    return 0;
  }

  if (working_time.is_everyday) {
    finish = std::fmod(finish, 1440.0);
  }

  auto it = std::upper_bound(working_time.intervals.begin(),
                             working_time.intervals.end(),
                             WorkingTimeInterval{0, static_cast<int>(finish)},
                             [](const auto lhs, const auto rhs) {
                                return lhs.minutes_to < rhs.minutes_to;
                             });

  if (it != working_time.intervals.end()) {
    if (finish >= it->minutes_from) {
      return 0;
    }
    else {
      return it->minutes_from - finish;
    }
  }
  else {
    if (working_time.is_everyday) {
      // Go for first element in day
      return 1440 - finish + working_time.intervals[0].minutes_from;
    }
    else {
      // Go for first element in week
      return 7 * 1440 - finish + working_time.intervals[0].minutes_from;
    }
  }
}
