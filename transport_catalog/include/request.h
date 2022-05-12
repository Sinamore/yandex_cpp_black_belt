#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bus.h"
#include "json.h"
#include "stop.h"
#include "svg.h"
#include "yellow_pages_structures.h"

enum class RequestType {
  QueryCompanyRequest,
  QueryBusRequest,
  QueryMapRequest,
  QueryRouteRequest,
  QueryRouteToCompanyRequest,
  QueryStopRequest,
  RenderSettingsRequest,
  RoutingSettingsRequest,
  SerializationSettingsRequest,
  UpdateBusRequest,
  UpdateStopRequest,
  YellowPagesRequest
};

class Request {
 public:
  explicit Request(const RequestType type) : req_type_(type) {}
  virtual ~Request() = default;

  RequestType GetType() const { return req_type_; }

 private:
  RequestType req_type_;
};

using RequestPtr = std::unique_ptr<Request>;

class SerializationSettingsRequest : public Request {
 public:
  SerializationSettingsRequest()
  : Request(RequestType::SerializationSettingsRequest) {}
  ~SerializationSettingsRequest() = default;
  void ParseFromJson(const Json::Node& request) {
    const auto& request_m = request.AsMap();
    file_name_ = request_m.at("file").AsString();
  }
  std::string GetFileName() { return file_name_; }
 private:
  std::string file_name_;
};

using SerializationSettingsRequestPtr =
  std::unique_ptr<SerializationSettingsRequest>;

class QueryRequest : public Request {
 public:
  explicit QueryRequest(const RequestType type) : Request(type), id_(0) {}
  void SetId(int id) {
    id_ = id;
  }
  int GetId() const {
    return id_;
  }
  virtual ~QueryRequest() = default;
 private:
  int id_;
};

using QueryRequestPtr = std::unique_ptr<QueryRequest>;

class QueryBusRequest : public QueryRequest {
 public:
  QueryBusRequest() : QueryRequest(RequestType::QueryBusRequest) {}
  void SetName(const std::string& name) {
    name_ = name;
  }
  std::string GetName() const {
    return name_;
  }
  ~QueryBusRequest() = default;
 private:
  std::string name_;
};

using QueryBusRequestPtr = std::unique_ptr<QueryBusRequest>;

class QueryRouteRequest : public QueryRequest {
 public:
  QueryRouteRequest() : QueryRequest(RequestType::QueryRouteRequest) {}
  void SetFrom(const std::string& from) {
    from_ = from;
  }
  std::string GetFrom() {
    return from_;
  }
  void SetTo(const std::string& to) {
    to_ = to;
  }
  std::string GetTo() {
    return to_;
  }
 private:
  std::string from_;
  std::string to_;
};

using QueryRouteRequestPtr = std::unique_ptr<QueryRouteRequest>;

class QueryStopRequest : public QueryRequest {
 public:
  QueryStopRequest() : QueryRequest(RequestType::QueryStopRequest) {}
  void SetName(const std::string& name) {
    name_ = name;
  }
  std::string GetName() const {
    return name_;
  }
  ~QueryStopRequest() = default;
 private:
  std::string name_;
};

using QueryStopRequestPtr = std::unique_ptr<QueryStopRequest>;

class QueryMapRequest : public QueryRequest {
 public:
  QueryMapRequest() : QueryRequest(RequestType::QueryMapRequest) {}
  ~QueryMapRequest() = default;
 private:
};

using QueryMapRequestPtr = std::unique_ptr<QueryMapRequest>;

class QueryCompanyRequestBase : public QueryRequest {
 public:
  explicit QueryCompanyRequestBase(RequestType type) : QueryRequest(type) {}
  ~QueryCompanyRequestBase() = default;

  std::vector<uint64_t> BuildRubricsNum(const std::unordered_map<std::string,
                                        uint64_t>& um) const {
    std::vector<uint64_t> rubrics_num;
    rubrics_num.reserve(rubrics_.size());
    for (const auto& rubric : rubrics_) {
      rubrics_num.push_back(um.at(rubric));
    }
  return rubrics_num;
  }

  std::vector<std::string> names_;
  std::vector<std::string> urls_;
  std::vector<std::string> rubrics_;
  std::vector<QueryPhone> phones_;
 private:
};

using QueryCompanyRequestBasePtr = std::unique_ptr<QueryCompanyRequestBase>;

class QueryCompanyRequest : public QueryCompanyRequestBase {
 public:
  QueryCompanyRequest()
      : QueryCompanyRequestBase(RequestType::QueryCompanyRequest) {}
  ~QueryCompanyRequest() = default;
  void ParseFromJson(const std::map<std::string, Json::Node>& request_m) {
    if (request_m.count("id")) {
      SetId(request_m.at("id").AsInt());
    }
    if (request_m.count("names")) {
      for (const auto& name : request_m.at("names").AsArray()) {
        names_.push_back(name.AsString());
      }
    }
    if (request_m.count("urls")) {
      for (const auto& url : request_m.at("urls").AsArray()) {
        urls_.push_back(url.AsString());
      }
    }
    if (request_m.count("rubrics")) {
      for (const auto& rubric : request_m.at("rubrics").AsArray()) {
        rubrics_.push_back(rubric.AsString());
      }
    }
    if (request_m.count("phones")) {
      for (const auto& phone : request_m.at("phones").AsArray()) {
        const auto& phone_m = phone.AsMap();
        QueryPhone p;
        if (phone_m.count("type")) {
          std::string type = phone_m.at("type").AsString();
          if (type == "PHONE") {
            p.type = PhoneType::Phone;
          }
          else if (type == "FAX") {
            p.type = PhoneType::Fax;
          }
        }
        if (phone_m.count("country_code")) {
          p.country_code = phone_m.at("country_code").AsString();
        }
        if (phone_m.count("local_code")) {
          p.local_code = phone_m.at("local_code").AsString();
        }
        if (phone_m.count("number")) {
          p.number = phone_m.at("number").AsString();
        }
        if (phone_m.count("extension")) {
          p.extension = phone_m.at("extension").AsString();
        }
        phones_.push_back(p);
      }
    }
  }
};

using QueryCompanyRequestPtr = std::unique_ptr<QueryCompanyRequest>;

class QueryRouteToCompanyRequest : public QueryCompanyRequestBase {
 public:
  QueryRouteToCompanyRequest()
  : QueryCompanyRequestBase(RequestType::QueryRouteToCompanyRequest) {}
  ~QueryRouteToCompanyRequest() = default;
  void ParseFromJson(const std::map<std::string, Json::Node>& request_m) {
    SetId(request_m.at("id").AsInt());
    from_ = request_m.at("from").AsString();
    if (request_m.count("datetime")) {
      auto datetime = request_m.at("datetime").AsArray();
      start_minutes = datetime[0].AsInt() * 1440 +
                      datetime[1].AsInt() * 60 +
                      datetime[2].AsDouble();
    }
    const auto& company_m = request_m.at("companies").AsMap();
    if (company_m.count("names")) {
      for (const auto& name : company_m.at("names").AsArray()) {
        names_.push_back(name.AsString());
      }
    }
    if (company_m.count("urls")) {
      for (const auto& url : company_m.at("urls").AsArray()) {
        urls_.push_back(url.AsString());
      }
    }
    if (company_m.count("rubrics")) {
      for (const auto& rubric : company_m.at("rubrics").AsArray()) {
        rubrics_.push_back(rubric.AsString());
      }
    }
    if (company_m.count("phones")) {
      for (const auto& phone : company_m.at("phones").AsArray()) {
        const auto& phone_m = phone.AsMap();
        QueryPhone p;
        if (phone_m.count("type")) {
          std::string type = phone_m.at("type").AsString();
          if (type == "PHONE") {
            p.type = PhoneType::Phone;
          }
          else if (type == "FAX") {
            p.type = PhoneType::Fax;
          }
        }
        if (phone_m.count("country_code")) {
          p.country_code = phone_m.at("country_code").AsString();
        }
        if (phone_m.count("local_code")) {
          p.local_code = phone_m.at("local_code").AsString();
        }
        if (phone_m.count("number")) {
          p.number = phone_m.at("number").AsString();
        }
        if (phone_m.count("extension")) {
          p.extension = phone_m.at("extension").AsString();
        }
        phones_.push_back(p);
      }
    }
  }
  std::string from_;
  double start_minutes;
};

using QueryRouteToCompanyRequestPtr = std::unique_ptr<QueryRouteToCompanyRequest>;

class UpdateRequest : public Request {
 public:
  explicit UpdateRequest(const RequestType type) : Request(type) {}
  virtual ~UpdateRequest() = default;
};

using UpdateRequestPtr = std::unique_ptr<UpdateRequest>;

class UpdateBusRequest : public UpdateRequest {
 public:
  UpdateBusRequest() : UpdateRequest(RequestType::UpdateBusRequest) {}
  void SetBusName(const std::string& name) {
    name_ = name;
  }
  void SetStopNames(std::vector<std::string> stop_names) {
    stops_ = std::move(stop_names);
  }
  void SetRouteType(const RouteType type) {
    type_ = type;
  }
  std::string GetName() const {
    return name_;
  }
  Bus BuildBus() {
    return Bus(name_, std::move(stops_), type_);
  }
  ~UpdateBusRequest() = default;
 private:
  std::string name_;
  std::vector<std::string> stops_;
  RouteType type_;
};

using UpdateBusRequestPtr = std::unique_ptr<UpdateBusRequest>;

class UpdateStopRequest : public UpdateRequest {
 public:
  UpdateStopRequest() : UpdateRequest(RequestType::UpdateStopRequest) {}
  void SetStopName(const std::string& name) {
    name_ = name;
  }
  void SetLatitude(double latitude) {
    latitude_ = latitude;
  }
  void SetLongitude(double longitude) {
    longitude_ = longitude;
  }
  void AddDistance(const std::string& stop, int distance) {
    distances_.insert({stop, distance});
  }
  std::string GetName() const {
    return name_;
  }
  Stop BuildStop() const {
    return Stop(name_, latitude_, longitude_, std::move(distances_));
  }
  ~UpdateStopRequest() = default;

 private:
  std::string name_;
  double latitude_;
  double longitude_;
  std::unordered_map<std::string, int> distances_;
};

using UpdateStopRequestPtr = std::unique_ptr<UpdateStopRequest>;

class RoutingSettingsRequest : public UpdateRequest {
 public:
  RoutingSettingsRequest()
  : UpdateRequest(RequestType::RoutingSettingsRequest) {}
  void ParseFromJson(const Json::Node& request) {
    const auto& request_m = request.AsMap();
    bus_wait_time_ = request_m.at("bus_wait_time").AsInt();
    if (std::holds_alternative<double>(request_m.at("bus_velocity"))) {
      bus_velocity_ = request_m.at("bus_velocity").AsDouble();
    }
    else {
      bus_velocity_ = static_cast<double>(request_m.at("bus_velocity").AsInt());
    }
    bus_velocity_ *= (1000 / 60.0);  // KpH to MpM
    if (std::holds_alternative<double>(request_m.at("pedestrian_velocity"))) {
      pedestrian_velocity_ = request_m.at("pedestrian_velocity").AsDouble();
    }
    else {
      pedestrian_velocity_ =
        static_cast<double>(request_m.at("pedestrian_velocity").AsInt());
    }
    pedestrian_velocity_ *= (1000 / 60.0);  // KpH to MpM
  }
  void SetBusWaitTime(int bus_wait_time) {
    bus_wait_time_ = bus_wait_time;
  }
  int GetBusWaitTime() {
    return bus_wait_time_;
  }
  void SetBusVelocity(double bus_velocity) {
    bus_velocity_ = bus_velocity;
  }
  double GetBusVelocity() {
    return bus_velocity_;
  }
  void SetPedestrianVelocity(double pedestrian_velocity) {
    pedestrian_velocity_ = pedestrian_velocity;
  }
  double GetPedestrianVelocity() {
    return pedestrian_velocity_;
  }
  ~RoutingSettingsRequest() = default;

 private:
  int bus_wait_time_;
  double bus_velocity_;
  double pedestrian_velocity_;
};

using RoutingSettingsRequestPtr = std::unique_ptr<RoutingSettingsRequest>;

struct RenderSettings {
  double width;
  double height;
  double padding;
  double stop_radius;
  double line_width;
  int stop_label_font_size;
  Svg::Point stop_label_offset;
  Svg::Color underlayer_color;
  double underlayer_width;
  std::vector<Svg::Color> color_palette;
  int bus_label_font_size;
  Svg::Point bus_label_offset;
  std::vector<std::string> layers;
  double outer_margin;
  double company_radius;
  double company_line_width;
};

class RenderSettingsRequest : public UpdateRequest {
 public:
  RenderSettingsRequest() : UpdateRequest(RequestType::RenderSettingsRequest) {}
  void ParseFromJson(const Json::Node& request) {
    const auto& request_m = request.AsMap();
    settings_.width = request_m.at("width").AsDouble();
    settings_.height = request_m.at("height").AsDouble();
    settings_.padding = request_m.at("padding").AsDouble();
    settings_.stop_radius = request_m.at("stop_radius").AsDouble();
    settings_.line_width = request_m.at("line_width").AsDouble();
    settings_.stop_label_font_size = \
        request_m.at("stop_label_font_size").AsInt();
    settings_.stop_label_offset = \
        Svg::Point(request_m.at("stop_label_offset"));
    settings_.underlayer_color = \
        Svg::Color(request_m.at("underlayer_color"));
    settings_.underlayer_width = request_m.at("underlayer_width").AsDouble();
    const Json::Node& color_palette_node = request_m.at("color_palette");
    size_t color_palette_size = color_palette_node.AsArray().size();
    settings_.color_palette.reserve(color_palette_size);
    for (size_t i = 0; i < color_palette_size; i++) {
      settings_.color_palette.push_back(
        Svg::Color(color_palette_node.AsArray()[i]));
    }
    settings_.bus_label_font_size = \
        request_m.at("bus_label_font_size").AsInt();
    settings_.bus_label_offset = Svg::Point(request_m.at("bus_label_offset"));
    const Json::Node& layers_node = request_m.at("layers");
    size_t layers_size = layers_node.AsArray().size();
    settings_.layers.reserve(layers_size);
    for (size_t i = 0; i < layers_size; i++) {
      settings_.layers.push_back(layers_node.AsArray()[i].AsString());
    }
    settings_.outer_margin = request_m.at("outer_margin").AsDouble();
    settings_.company_radius = request_m.at("company_radius").AsDouble();
    settings_.company_line_width = request_m.at("company_line_width").AsDouble();
  }
  RenderSettings GetRenderSettings() const {
    return settings_;
  }
  ~RenderSettingsRequest() = default;

 private:
  RenderSettings settings_;
};

using RenderSettingsRequestPtr = std::unique_ptr<RenderSettingsRequest>;

class YellowPagesRequest : public UpdateRequest {
 public:
  YellowPagesRequest() : UpdateRequest(RequestType::YellowPagesRequest) {}
  ~YellowPagesRequest() = default;
  void ParseFromJson(const Json::Node& request) {
    const auto& request_m = request.AsMap();
    // Parse rubrics
    if (request_m.count("rubrics")) {
      for (const auto& [key, rubric] : request_m.at("rubrics").AsMap()) {
        const auto& rubric_m = rubric.AsMap();
        std::string name = "";
        if (rubric_m.count("name")) {
          name = rubric_m.at("name").AsString();
        }
        std::set<std::string> keywords;
        if (rubric_m.count("keywords")) {
          for (const auto& keyword : rubric_m.at("keywords").AsArray()) {
            keywords.insert(keyword.AsString());
          }
        }
        rubrics_[stoull(key)] = Rubric{name, keywords};
      }
    }
    // Parse companies
    if (request_m.count("companies")) {
      for (const auto& c : request_m.at("companies").AsArray()) {
        const auto& company_m = c.AsMap();
        Company company;
        if (company_m.count("address")) {
          const auto& address_m = company_m.at("address").AsMap();
          Address address;
          if (address_m.count("formatted")) {
            address.formatted = address_m.at("formatted").AsString();
          }
          if (address_m.count("components")) {
            for (const auto& component : address_m.at("components").AsArray()) {
              const auto& component_m = component.AsMap();
              AddressComponent ac;
              if (component_m.count("value")) {
                ac.value = component_m.at("value").AsString();
              }
              if (component_m.count("type")) {
                std::string type = component_m.at("type").AsString();
                if (type == "COUNTRY") {
                  ac.type = AddressComponentType::Country;
                }
                else if (type == "REGION") {
                  ac.type = AddressComponentType::Region;
                }
                else if (type == "CITY") {
                  ac.type = AddressComponentType::City;
                }
                else if (type == "STREET") {
                  ac.type = AddressComponentType::Street;
                }
                else if (type == "HOUSE") {
                  ac.type = AddressComponentType::House;
                }
              }
              address.components.push_back(ac);
            }
          }
          if (address_m.count("coords")) {
            const auto& coords_m = address_m.at("coords").AsMap();
            if (coords_m.count("lat") && coords_m.count("lon")) {
              address.coords = Coords{std::stod(coords_m.at("lat").AsString()),
                                      std::stod(coords_m.at("lon").AsString())};
            }
          }
          if (address_m.count("comment")) {
            address.comment = address_m.at("comment").AsString();
          }
          company.address = address;
        }
        if (company_m.count("names")) {
          for (const auto& name : company_m.at("names").AsArray()) {
            const auto& name_m = name.AsMap();
            Name n;
            if (name_m.count("value")) {
              n.value = name_m.at("value").AsString();
            }
            n.type = NameType::Main;
            if (name_m.count("type")) {
              std::string type = name_m.at("type").AsString();
              if (type == "SYNONYM") {
                n.type = NameType::Synonym;
              }
              else if (type == "SHORT") {
                n.type = NameType::Short;
              }
            }
            company.names.push_back(n);
          }
        }
        if (company_m.count("phones")) {
          for (const auto& phone : company_m.at("phones").AsArray()) {
            const auto& phone_m = phone.AsMap();
            Phone p;
            p.type = PhoneType::Phone;
            if (phone_m.count("formatted")) {
              p.formatted = phone_m.at("formatted").AsString();
            }
            if (phone_m.count("type")) {
              std::string type = phone_m.at("type").AsString();
              if (type == "PHONE") {
                p.type = PhoneType::Phone;
              }
              else if (type == "FAX") {
                p.type = PhoneType::Fax;
              }
            }
            if (phone_m.count("country_code")) {
              p.country_code = phone_m.at("country_code").AsString();
            }
            if (phone_m.count("local_code")) {
              p.local_code = phone_m.at("local_code").AsString();
            }
            if (phone_m.count("number")) {
              p.number = phone_m.at("number").AsString();
            }
            if (phone_m.count("extension")) {
              p.extension = phone_m.at("extension").AsString();
            }
            if (phone_m.count("description")) {
              p.description = phone_m.at("description").AsString();
            }
            company.phones.push_back(p);
          }
        }
        if (company_m.count("urls")) {
          for (const auto& url : company_m.at("urls").AsArray()) {
            const auto& url_m = url.AsMap();
            if (url_m.count("value")) {
              company.urls.push_back(url_m.at("value").AsString());
            }
          }
        }
        if (company_m.count("rubrics")) {
          for (const auto& rubric : company_m.at("rubrics").AsArray()) {
            company.rubrics.push_back(static_cast<uint64_t>(rubric.AsInt()));
          }
        }

        company.working_time.is_everyday = true;
        if (company_m.count("working_time")) {
          const auto& working_time_m = company_m.at("working_time").AsMap();
          if (working_time_m.count("intervals")) {
            for (const auto& i : working_time_m.at("intervals").AsArray()) {
              const auto& interval_m = i.AsMap();
              WorkingTimeInterval wti;
              if (interval_m.count("day")) {
                std::string day = interval_m.at("day").AsString();
                if (day != "EVERYDAY") {
                  company.working_time.is_everyday = false;
                  if (day == "MONDAY") {
                    wti.minutes_from = 0 * 1440;
                    wti.minutes_to = 0 * 1440;
                  }
                  else if (day == "TUESDAY") {
                    wti.minutes_from = 1 * 1440;
                    wti.minutes_to = 1 * 1440;
                  }
                  else if (day == "WEDNESDAY") {
                    wti.minutes_from = 2 * 1440;
                    wti.minutes_to = 2 * 1440;
                  }
                  else if (day == "THURSDAY") {
                    wti.minutes_from = 3 * 1440;
                    wti.minutes_to = 3 * 1440;
                  }
                  else if (day == "FRIDAY") {
                    wti.minutes_from = 4 * 1440;
                    wti.minutes_to = 4 * 1440;
                  }
                  else if (day == "SATURDAY") {
                    wti.minutes_from = 5 * 1440;
                    wti.minutes_to = 5 * 1440;
                  }
                  else if (day == "SUNDAY") {
                    wti.minutes_from = 6 * 1440;
                    wti.minutes_to = 6 * 1440;
                  }
                }
                else {
                  wti.minutes_from = 0;
                  wti.minutes_to = 0;
                }
              }
              if (interval_m.count("minutes_from")) {
                wti.minutes_from += interval_m.at("minutes_from").AsInt();
              }
              if (interval_m.count("minutes_to")) {
                wti.minutes_to += interval_m.at("minutes_to").AsInt();
              }
              company.working_time.intervals.push_back(wti);
            }
          }
        }
        if (company_m.count("nearby_stops")) {
          for (const auto& stop : company_m.at("nearby_stops").AsArray()) {
            const auto& stop_m = stop.AsMap();
            NearbyStop nstop;
            if (stop_m.count("name")) {
              nstop.name = stop_m.at("name").AsString();
            }
            if (stop_m.count("meters")) {
              nstop.meters = stop_m.at("meters").AsInt();
            }
            company.nearby_stops.push_back(nstop);
          }
        }
        companies_.push_back(company);
      }
    }
  }
  std::unordered_map<uint64_t, Rubric> rubrics_;
  std::vector<Company> companies_;
};

using YellowPagesRequestPtr = std::unique_ptr<YellowPagesRequest>;
