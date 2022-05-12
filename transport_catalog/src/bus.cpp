#include "bus.h"

double GeoDistanceBetweenStops(const Stop& stop1, const Stop& stop2) {
  return acos(sin(stop1.GetLatitudeRad()) * sin(stop2.GetLatitudeRad()) +
              cos(stop1.GetLatitudeRad()) * cos(stop2.GetLatitudeRad()) *
              cos(std::abs(stop1.GetLongitudeRad() - stop2.GetLongitudeRad()))
              ) * 6371000;
}

int MapDistanceBetweenStops(const Stop& stop1, const Stop& stop2) {
  const auto& distances1 = stop1.GetDistances();
  auto d1_it = distances1.find(stop2.GetName());
  if (d1_it != distances1.end()) {
    return d1_it->second;
  }
  else {
    const auto& distances2 = stop2.GetDistances();
    auto d2_it = distances2.find(stop1.GetName());
    if (d2_it != distances2.end()) {
      return d2_it->second;
    }
    else {
      throw std::runtime_error("Can't find stop in either distances");
    }
  }
}

void Bus::EvaluateRoute(const std::map<std::string, Stop>& stops) {
  double geo_length = 0;
  int map_length = 0;

  if (type_ == RouteType::ROUND) {
    for (auto i = 0; i < num_stops_ - 1; i++) {
      const Stop& stop1 = stops.at(stops_[i]);
      const Stop& stop2 = stops.at(stops_[i + 1]);
      geo_length += GeoDistanceBetweenStops(stop1, stop2);
      map_length += MapDistanceBetweenStops(stop1, stop2);
    }
    geo_route_length_ = geo_length;
    map_route_length_ = map_length;
  }
  else {
    for (auto i = 0; i < (num_stops_ - 1) / 2; i++) {
      const Stop& stop1 = stops.at(stops_[i]);
      const Stop& stop2 = stops.at(stops_[i + 1]);
      geo_length += GeoDistanceBetweenStops(stop1, stop2);
      map_length += MapDistanceBetweenStops(stop1, stop2);
    }
    for (auto i = 0; i < (num_stops_ - 1) / 2; i++) {
      const Stop& stop1 = stops.at(stops_[i]);
      const Stop& stop2 = stops.at(stops_[i + 1]);
      map_length += MapDistanceBetweenStops(stop2, stop1);
    }
    geo_route_length_ = 2 * geo_length;
    map_route_length_ = map_length;
  }
  curvature_ = map_route_length_ / *geo_route_length_;
}
