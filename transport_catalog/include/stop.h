#pragma once

#include <string>
#include <unordered_map>
#include <utility>

class Stop {
 public:
  Stop(const std::string& name,
       double latitude,
       double longitude,
       std::unordered_map<std::string, int> distances)
      : name_(name),
        latitude_(latitude),
        longitude_(longitude),
        real_latitude_(latitude),
        real_longitude_(longitude),
        x_(0),
        y_(0),
        distances_(std::move(distances)) {}
  // This is for deserializing only, it relies on previous data corrections
  Stop(const std::string& name,
       double latitude,
       double longitude,
       double real_latitude,
       double real_longitude,
       double x,
       double y,
       std::unordered_map<std::string, int> distances)
      : name_(name),
        latitude_(latitude),
        longitude_(longitude),
        real_latitude_(real_latitude),
        real_longitude_(real_longitude),
        x_(x),
        y_(y),
        distances_(distances) {}
  explicit Stop(const std::string& name) : name_(name) {}
  std::string GetName() const { return name_; }
  double GetLatitude() const { return latitude_; }
  double GetLongitude() const { return longitude_; }
  void SetLatitude(double lat) { latitude_ = lat; }
  void SetLongitude(double lon) { longitude_ = lon; }
  double GetRealLatitude() const { return real_latitude_; }
  double GetRealLongitude() const { return real_longitude_; }
  double GetLatitudeRad() const { return real_latitude_ * PI / 180; }
  double GetLongitudeRad() const { return real_longitude_ * PI / 180; }
  double GetX() const { return x_; }
  double GetY() const { return y_; }
  bool IsBase() const { return is_base_; }
  void SetX(double x) { x_ = x; }
  void SetY(double y) { y_ = y; }
  void SetIsBase(bool b) { is_base_ = b; }
  const std::unordered_map<std::string, int>& GetDistances() const {
    return distances_;
  }

 private:
  constexpr static double PI = 3.1415926535;
  std::string name_;
  double latitude_, longitude_;
  double real_latitude_, real_longitude_;
  double x_, y_;
  std::unordered_map<std::string, int> distances_;
  bool is_base_ = false;
};
