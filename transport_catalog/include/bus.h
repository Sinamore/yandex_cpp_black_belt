#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "stop.h"

double GeoDistanceBetweenStops(const Stop& stop1, const Stop& stop2);

int MapDistanceBetweenStops(const Stop& stop1, const Stop& stop2);

enum class RouteType {
  ROUND,
  TWOWAY
};

class Bus {
 public:
  Bus(const std::string& name, std::vector<std::string> stops, RouteType type)
      : name_(name),
        stops_(stops),
        type_(type),
        geo_route_length_(std::nullopt),
        map_route_length_(0),
        curvature_(0) {
    std::sort(stops.begin(), stops.end());
    auto new_end_it = std::unique(stops.begin(), stops.end());
    num_unique_stops_ = new_end_it - stops.begin();

    if (type_ == RouteType::ROUND) {
      num_stops_ = stops_.size();
    }
    else {
      num_stops_ = 2 * stops_.size() - 1;
    }
  }
  // This is for deserializing only, concurrency is not checked!
  Bus(const std::string& name,
      std::vector<std::string> stops,
      RouteType type,
      int num_stops,
      int num_unique_stops,
      double geo_route_length,
      int map_route_length,
      double curvature)
      : name_(name),
        stops_(stops),
        type_(type),
        num_stops_(num_stops),
        num_unique_stops_(num_unique_stops),
        geo_route_length_(geo_route_length),
        map_route_length_(map_route_length),
        curvature_(curvature) {}

  std::string GetName() const {
    return name_;
  }

  const std::vector<std::string>& GetStops() const {
    return stops_;
  }

  int GetNumStops() const {
    return num_stops_;
  }

  int GetNumUniqueStops() const {
    return num_unique_stops_;
  }

  std::optional<double> GetGeoRouteLength() const {
    return geo_route_length_;
  }

  int GetMapRouteLength() const {
    return map_route_length_;
  }

  double GetCurvature() const {
    return curvature_;
  }

  RouteType GetType() const {
    return type_;
  }

  size_t GetColorId() const {
    return color_id_;
  }

  void SetColorId(size_t id) {
    color_id_ = id;
  }

  void EvaluateRoute(const std::map<std::string, Stop>& stops);

 private:
  std::string name_;
  std::vector<std::string> stops_;
  RouteType type_;
  int num_stops_;
  int num_unique_stops_;
  std::optional<double> geo_route_length_;
  int map_route_length_;
  double curvature_;
  uint32_t color_id_;
};
