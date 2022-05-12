#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

enum class AddressComponentType {
  Country,
  Region,
  City,
  Street,
  House
};

struct AddressComponent {
  std::string value;
  AddressComponentType type;
};

struct Coords {
  double lat, lon;
};


struct Address {
  std::string formatted;
  std::vector<AddressComponent> components;
  Coords coords;
  std::string comment;
};

enum class NameType {
  Main,
  Synonym,
  Short
};

struct Name {
  std::string value;
  NameType type;
};

enum class PhoneType {
  Phone,
  Fax
};

struct Phone {
  std::string formatted;
  PhoneType type;
  std::string country_code;
  std::string local_code;
  std::string number;
  std::string extension;
  std::string description;
};

struct QueryPhone {
  std::string formatted;
  std::optional<PhoneType> type = std::nullopt;
  std::string country_code;
  std::string local_code;
  std::string number;
  std::string extension;
  std::string description;
};

enum class Day {
  Everyday,
  Monday,
  Tuesday,
  Wednesday,
  Thursday,
  Friday,
  Saturday,
  Sunday
};

struct WorkingTimeInterval {
  int minutes_from;
  int minutes_to;
};

struct WorkingTime {
  bool is_everyday;
  std::vector<WorkingTimeInterval> intervals;
};

struct NearbyStop {
  std::string name;
  uint32_t meters;
};

struct Datetime {
  int day;
  int hour;
  double minutes;
};

struct Company {
  Address address;
  std::vector<Name> names;
  std::vector<Phone> phones;
  std::vector<std::string> urls;
  std::vector<uint64_t> rubrics;
  WorkingTime working_time;
  std::vector<NearbyStop> nearby_stops;

  double WaitForCompanyOpen(double d) const;
};

struct Rubric {
  std::string name;
  std::set<std::string> keywords;
};

