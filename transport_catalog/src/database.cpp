#include <algorithm>
#include <cmath>

#include "database.h"

Database::Database(BaseInputStruct& db_settings) {
  output_file_ = db_settings.serialization_settings->GetFileName();
  UpdateSettings(std::move(db_settings.routing_settings));
  UpdateDatabase(std::move(db_settings.updates));
  EvaluateRouteLengths();
  InitRenderSettings(std::move(db_settings.render_settings));
  FillStopsNeighbours();
  FillCompanies(std::move(db_settings.yellow_pages_request));
  CompressCoordinates();
  PrepareRoutes();
}

void Database::EvaluateRouteLengths() {
  for (auto& [name, bus] : buses_) {
    bus.EvaluateRoute(stops_);
  }
}

void Database::FillStopsNeighbours() {
  for (const auto& [name, bus] : buses_) {
    const auto& bus_stops = bus.GetStops();
    for (size_t i = 0; i < bus_stops.size() - 1; i++) {
      stops_neighbours_[bus_stops[i]].insert(bus_stops[i + 1]);
      stops_neighbours_[bus_stops[i + 1]].insert(bus_stops[i]);
    }
  }
}

Database::Database(SerializationSettingsRequestPtr r) {
  DeserializeDatabase(std::move(r));

  FillStopsNeighbours();
}

void Database::PrepareRoutes() {
  trouter_ = std::make_unique<TransportRouter>(buses_,
                                               stops_,
                                               router_settings_);
}

static std::string Quote(const std::string& s) {
  size_t quote_count = 0;
  for (const char c : s) {
    quote_count += (c == '"');
  }
  std::string res(s.size() + quote_count, '0');
  size_t j = 0;
  for (size_t i = 0; i < s.size();) {
    if (s[i] == '"') {
      res[j++] = '\\';
    }
    res[j++] = s[i++];
  }
  return res;
}

std::vector<Json::Node>
Database::BuildRouteItemNodes(const TransportRouter::RouteInfo& route) {
  std::vector<Json::Node> route_vec;
  route_vec.reserve(route.items.size());
  for (size_t i = 0; i < route.items.size(); i++) {
    auto item = route.items[i];
    if (std::holds_alternative<TransportRouter::StopItem>(item)) {
      auto stop_item = std::get<TransportRouter::StopItem>(item);
      std::map<std::string, Json::Node> node;
      node["type"] = Json::Node(std::string("WaitBus"));
      node["stop_name"] = Json::Node(stop_item.name);
      node["time"] = Json::Node(stop_item.time);
      route_vec.push_back(node);
    }
    else if (std::holds_alternative<TransportRouter::BusItem>(item)) {
      auto bus_item = std::get<TransportRouter::BusItem>(item);
      std::map<std::string, Json::Node> node;
      node["type"] = Json::Node(std::string("RideBus"));
      node["bus"] = Json::Node(bus_item.name);
      node["span_count"] = Json::Node(bus_item.span_count);
      node["time"] = Json::Node(bus_item.time);
      route_vec.push_back(node);
    }
    else if (std::holds_alternative<TransportRouter::WalkItem>(item)) {
      std::map<std::string, Json::Node> node;
      auto walk_item = std::get<TransportRouter::WalkItem>(item);
      node["type"] = Json::Node(std::string("WalkToCompany"));
      node["time"] = Json::Node(walk_item.time);
      node["stop_name"] = Json::Node(walk_item.stop_name);
      node["company"] = Json::Node(GetCompanyMainName(*route.company));
      route_vec.push_back(node);
    }
    else {
      std::map<std::string, Json::Node> node;
      auto wait_item = std::get<TransportRouter::WaitItem>(item);
      node["type"] = Json::Node(std::string("WaitCompany"));
      node["time"] = Json::Node(wait_item.time);
      node["company"] = Json::Node(GetCompanyMainName(*route.company));
      route_vec.push_back(node);
    }
  }
  return route_vec;
}

std::optional<TransportRouter::RouteInfo>
Database::BuildRouteToClosestCompany(const std::string from,
                                     const double start,
                                     const std::vector<Company>& candidates) {
  std::optional<TransportRouter::RouteInfo> best;
  for (const auto& company : candidates) {
    for (const auto& stop : company.nearby_stops) {
      auto route = trouter_->BuildRouteToCompany(from, stop);
      if (route) {
        if (best && route->total_time > best->total_time) {
          continue;
        }
        // check if company is open and add wait if not
        double finish = std::fmod(start + route->total_time, 60.0 * 24 * 7);
        double wait_time = company.WaitForCompanyOpen(finish);
        if (wait_time > 0) {
          route->items.push_back(TransportRouter::WaitItem{wait_time});
          route->total_time += wait_time;
        }
      }
      if (route && (!best || route->total_time < best->total_time)) {
        best = route;
        best->company = company;
      }
    }
  }
  return best;
}

std::vector<Response>
Database::ProcessQueries(const std::vector<RequestPtr>& requests) {
  std::vector<Response> responses;
  responses.reserve(requests.size());
  for (const auto& req_ptr : requests) {
    RequestType req_type = req_ptr->GetType();
    std::map<std::string, Json::Node> response;
    if (req_type == RequestType::QueryBusRequest) {
      auto &request = static_cast<QueryBusRequest&>(*req_ptr);
      response["request_id"] = Json::Node(request.GetId());
      auto b_it = buses_.find(request.GetName());
      if (b_it == buses_.end()) {
        response["error_message"] = Json::Node(std::string("not found"));
        responses.push_back(std::move(response));
        continue;
      }
      Bus& bus = b_it->second;
      response["stop_count"] = Json::Node(bus.GetNumStops());
      response["unique_stop_count"] = Json::Node(bus.GetNumUniqueStops());
      response["route_length"] = Json::Node(bus.GetMapRouteLength());
      response["curvature"] = Json::Node(bus.GetCurvature());
      responses.push_back(std::move(response));
    }
    else if (req_type == RequestType::QueryStopRequest) {
      auto &request = static_cast<QueryStopRequest&>(*req_ptr);
      response["request_id"] = Json::Node(request.GetId());
      auto s_it = stops_to_buses_.find(request.GetName());
      if (s_it == stops_to_buses_.end()) {
        response["error_message"] = Json::Node(std::string("not found"));
        responses.push_back(std::move(response));
      }
      else if (s_it->second.empty()) {
        response["buses"] = Json::Node(std::vector<Json::Node>());
        responses.push_back(std::move(response));
      }
      else {
        std::vector<Json::Node> buses;
        buses.reserve(s_it->second.size());
        for (const auto& b : s_it->second) {
          buses.push_back(Json::Node(b));
        }
        response["buses"] = std::move(buses);
        responses.push_back(std::move(response));
      }
    }
    else if (req_type == RequestType::QueryRouteRequest) {
      auto& request = static_cast<QueryRouteRequest&>(*req_ptr);
      response["request_id"] = Json::Node(request.GetId());
      std::string from = request.GetFrom();
      std::string to = request.GetTo();

      std::optional<TransportRouter::RouteInfo> route =
        trouter_->BuildRoute(from, to);

      if (route == std::nullopt) {
        response["error_message"] = Json::Node(std::string("not found"));
      }
      else {
        response["total_time"] = Json::Node(route->total_time);
        response["items"] = BuildRouteItemNodes(*route);
        response["map"] = Quote(RenderRouteAsSvg(*route));
      }
      responses.push_back(std::move(response));
    }
    else if (req_type == RequestType::QueryMapRequest) {
      auto& request = static_cast<QueryMapRequest&>(*req_ptr);
      response["request_id"] = Json::Node(request.GetId());
      response["map"] = Quote(RenderAsSvg());
      responses.push_back(std::move(response));
    }
    else if (req_type == RequestType::QueryCompanyRequest) {
      auto& request = static_cast<QueryCompanyRequest&>(*req_ptr);
      response["request_id"] = Json::Node(request.GetId());
      response["companies"] =
        Json::Node(GetNamesFromCompanies(FilterCompaniesByRequest(request)));
      responses.push_back(std::move(response));
    }
    else if (req_type == RequestType::QueryRouteToCompanyRequest) {
      auto& request = static_cast<QueryRouteToCompanyRequest&>(*req_ptr);
      response["request_id"] = Json::Node(request.GetId());
      std::vector<Company> candidates = FilterCompaniesByRequest(request);
      std::optional<TransportRouter::RouteInfo> best_route =
        BuildRouteToClosestCompany(request.from_,
                                   request.start_minutes,
                                   candidates);
      if (best_route == std::nullopt) {
        response["error_message"] = Json::Node(std::string("not found"));
      }
      else {
        response["total_time"] = Json::Node(best_route->total_time);
        response["items"] = BuildRouteItemNodes(*best_route);
        response["map"] = Quote(RenderRouteAsSvg(*best_route));
      }
      responses.push_back(std::move(response));
    }
    else {
      throw std::runtime_error("Unknown request in ProcessQueries");
    }
  }
  return responses;
}

static bool DoesPhoneMatch(const QueryPhone& query_phone, const Phone& object) {
  if (!query_phone.extension.empty() &&
      query_phone.extension != object.extension) {
    return false;
  }
  if (query_phone.type && *query_phone.type != object.type) {
    return false;
  }
  if (!query_phone.country_code.empty() &&
      query_phone.country_code != object.country_code) {
    return false;
  }
  if (
      (!query_phone.local_code.empty() || !query_phone.country_code.empty())
      && query_phone.local_code != object.local_code
  ) {
    return false;
  }
  return query_phone.number == object.number;
}

std::string GetCompanyMainName(const Company& company) {
  for (const auto& name : company.names) {
    if (name.type == NameType::Main) {
      return name.value;
    }
  }
  return "";
}

std::vector<Json::Node>
Database::GetNamesFromCompanies(const std::vector<Company>& companies) {
  std::vector<Json::Node> res;
  res.reserve(companies.size());
  for (const auto& company : companies) {
    res.push_back(Json::Node(GetCompanyMainName(company)));
  }
  return res;
}

std::vector<Company>
Database::FilterCompaniesByRequest(const QueryCompanyRequestBase &req) {
  auto rubrics_num = req.BuildRubricsNum(rubrics_);
  std::vector<Company> res;
  res.reserve(companies_.size());
  for (const auto& company : companies_) {
    bool good = true;
    bool found = false | req.names_.empty();
    for (const auto& name_filter : req.names_) {
      for (const auto& name : company.names) {
        if (name.value == name_filter) {
          found = true;
          break;
        }
      }
      if (found) break;
    }
    good &= found;
    if (!good) continue;
    found = false | req.urls_.empty();
    for (const auto& url_filter : req.urls_) {
      for (const auto& url : company.urls) {
        if (url == url_filter) {
          found = true;
          break;
        }
      }
      if (found) break;
    }
    good &= found;
    if (!good) continue;
    found = false | req.rubrics_.empty();
    for (const auto rubric_filter : rubrics_num) {
      for (const auto rubric : company.rubrics) {
        if (rubric == rubric_filter) {
          found = true;
          break;
        }
      }
      if (found) break;
    }
    good &= found;
    if (!good) continue;
    found = false | req.phones_.empty();
    for (const auto& phone_filter : req.phones_) {
      for (const auto& phone : company.phones) {
        if (DoesPhoneMatch(phone_filter, phone)) {
          found = true;
          break;
        }
      }
      if (found) break;
    }
    good &= found;
    if (good) {
      res.push_back(company);
    }
  }

  return res;
}

double Database::GetXOnMap(const Stop& stop) const {
  return (stop.GetLongitude() - map_settings_.min_lon) * map_settings_.zoom_coef
          + map_settings_.render_settings.padding;
}

double Database::GetXOnMap(const double lon) const {
  return (lon - map_settings_.min_lon) * map_settings_.zoom_coef
          + map_settings_.render_settings.padding;
}

double Database::GetYOnMap(const Stop& stop) const {
  return (map_settings_.max_lat - stop.GetLatitude()) * map_settings_.zoom_coef
          + map_settings_.render_settings.padding;
}

double Database::GetYOnMap(const double lat) const {
  return (map_settings_.max_lat - lat) * map_settings_.zoom_coef
          + map_settings_.render_settings.padding;
}

void Database::UpdateDatabase(std::vector<RequestPtr> requests) {
  for (auto& req_ptr : requests) {
    RequestType req_type = req_ptr->GetType();
    if (req_type == RequestType::UpdateBusRequest) {
      auto request = static_cast<UpdateBusRequest&>(*req_ptr);
      Bus bus = request.BuildBus();
      const auto& bus_stops = bus.GetStops();
      for (const auto& stop : bus_stops) {
        stops_to_buses_[stop].insert(bus.GetName());
      }
      buses_.insert({request.GetName(), std::move(bus)});
    }
    else if (req_type == RequestType::UpdateStopRequest) {
      auto request = static_cast<UpdateStopRequest&>(*req_ptr);
      Stop stop = request.BuildStop();
      if (stops_to_buses_.find(stop.GetName()) == stops_to_buses_.end()) {
        stops_to_buses_[stop.GetName()] = std::set<std::string>();
      }
      stops_.insert({request.GetName(), std::move(stop)});
    }
    else {
      throw std::runtime_error("Unknown request type in UpdateDatabase");
    }
  }
}

void Database::MarkBaseStops() {
  /*
   * 1. Через остановку проходит больше одного автобуса
   * 2. Через остановку не проходит ни одного автобуса
   * 3. Один и тот же автобус проходит через остановку больше двух раз
   * 4. Остановка конечная
   */
  for (auto& [name, stop] : stops_) {
    if (stop.IsBase()) {
      continue;
    }
    auto it = stops_to_buses_.find(name);
    // No buses
    if (it == stops_to_buses_.end()) {
      stop.SetIsBase(true);
    }
    // More than 1 bus
    else if (it->second.size() > 1) {
      stop.SetIsBase(true);
    }
  }
  for (auto& [name, bus] : buses_) {
    const auto& stops = bus.GetStops();
    // Endpoints
    stops_.at(stops.front()).SetIsBase(true);
    stops_.at(stops.back()).SetIsBase(true);
    std::unordered_map<std::string, int> counter;
    for (const auto& name : stops) {
      counter[name] += 1;
    }
    // More than 2 times
    for (auto& [name, count] : counter) {
      if (bus.GetType() == RouteType::ROUND) {
        if (count > 2) {
          stops_.at(name).SetIsBase(true);
        }
      }
      else {
        // Every passage counts as two
        if (count >= 2) {
          stops_.at(name).SetIsBase(true);
        }
      }
    }
  }
}

void Database::MoveIntermediateStops() {
  for (auto& [name, bus] : buses_) {
    const auto& stops = bus.GetStops();
    for (size_t i = 0; i < stops.size() - 1; i++) {
      double base_lat = stops_.at(stops[i]).GetLatitude();
      double base_lon = stops_.at(stops[i]).GetLongitude();
      for (size_t j = i + 1; j < stops.size(); j++) {
        if (stops_.at(stops[j]).IsBase()) {
          double lat_step =
            (stops_.at(stops[j]).GetLatitude() - base_lat) / (j - i);
          double lon_step =
            (stops_.at(stops[j]).GetLongitude() - base_lon) / (j - i);
          for (auto k = i; k <= j; k++) {
            stops_.at(stops[k]).SetLatitude(base_lat + lat_step * (k - i));
            stops_.at(stops[k]).SetLongitude(base_lon + lon_step * (k - i));
          }
          i = j - 1;
          break;
        }
      }
    }
  }
}

bool Database::AreNeighbours(const MapItem& lhs, const MapItem& rhs) const {
  if (lhs.stop_ptr && rhs.stop_ptr) {
    const auto& lname = lhs.stop_ptr->GetName();
    const auto& rname = rhs.stop_ptr->GetName();
    if (stops_neighbours_.at(lname).count(rname)) {
      return true;
    }
  }
  else if (lhs.company_ptr && rhs.stop_ptr) {
    const auto& rname = rhs.stop_ptr->GetName();
    const auto& nstops = lhs.company_ptr->nearby_stops;
    for (const auto& stop : nstops) {
      if (stop.name == rname) {
        return true;
      }
    }
  }
  else if (lhs.stop_ptr && rhs.company_ptr) {
    const auto& lname = lhs.stop_ptr->GetName();
    const auto& nstops = rhs.company_ptr->nearby_stops;
    for (const auto& stop : nstops) {
      if (stop.name == lname) {
        return true;
      }
    }
  }
  else {
    return false;
  }
  return false;
}

void Database::CompressCoordinates() {
  MarkBaseStops();
  MoveIntermediateStops();

  UpdateRenderSettings();

  std::vector<MapItem> xs;
  xs.reserve(stops_.size() + companies_.size());
  for (auto& [name, stop] : stops_) {
    xs.push_back(MapItem{GetXOnMap(stop), &stop, nullptr, 0});
  }
  for (size_t i = 0; i < companies_.size(); i++) {
    xs.push_back(MapItem{GetXOnMap(companies_[i].address.coords.lon),
                         nullptr,
                         &companies_[i],
                         0});
  }
  double xstep = 0;
  std::sort(xs.begin(), xs.end(), [](const auto& lhs, const auto& rhs) {
                                    return lhs.coord < rhs.coord;
                                  });

  int max_id_x = 0;
  for (size_t i = 1; i < xs.size(); i++) {
    int id_i = 0;
    for (size_t j = 0; j < i; j++) {
      if (AreNeighbours(xs[i], xs[j])) {
        id_i = std::max(id_i, xs[j].id + 1);
        max_id_x = std::max(max_id_x, id_i);
      }
    }
    xs[i].id = id_i;
  }

  if (max_id_x > 0) {
    xstep = (map_settings_.render_settings.width -
             2 * map_settings_.render_settings.padding) / (max_id_x);

    for (size_t i = 0; i < xs.size(); i++) {
      if (xs[i].stop_ptr) {
        const auto& name = xs[i].stop_ptr->GetName();
        stops_.at(name).SetX(
          map_settings_.render_settings.padding + xs[i].id * xstep);
      }
      else {
        auto& company = *xs[i].company_ptr;
        company.address.coords.lon =
          map_settings_.render_settings.padding + xs[i].id * xstep;
      }
    }
  }
  else {
    for (size_t i = 0; i < xs.size(); i++) {
      if (xs[i].stop_ptr) {
        const auto& name = xs[i].stop_ptr->GetName();
        stops_.at(name).SetX(map_settings_.render_settings.padding);
      }
      else {
        auto& company = *xs[i].company_ptr;
        company.address.coords.lon = map_settings_.render_settings.padding;
      }
    }
  }

  std::vector<MapItem> ys;
  ys.reserve(stops_.size() + companies_.size());
  for (auto& [name, stop] : stops_) {
    ys.push_back(MapItem{GetYOnMap(stop), &stop, nullptr, 0});
  }
  for (auto& company : companies_) {
    ys.push_back(MapItem{GetYOnMap(company.address.coords.lat),
                         nullptr,
                         &company,
                         0});
  }

  double ystep = 0;
  std::sort(ys.begin(), ys.end(), [](const auto& lhs, const auto& rhs) {
                                    return lhs.coord > rhs.coord;
                                  });

  int max_id_y = 0;
  for (size_t i = 1; i < ys.size(); i++) {
    int id_i = 0;
    for (size_t j = 0; j < i; j++) {
      if (AreNeighbours(ys[i], ys[j])) {
        id_i = std::max(id_i, ys[j].id + 1);
        max_id_y = std::max(max_id_y, id_i);
      }
    }
    ys[i].id = id_i;
  }

  if (max_id_y > 0) {
    ystep = (map_settings_.render_settings.height -
             2 * map_settings_.render_settings.padding) / (max_id_y);

    for (size_t i = 0; i < ys.size(); i++) {
      if (ys[i].stop_ptr) {
        const auto& name = ys[i].stop_ptr->GetName();
        stops_.at(name).SetY(map_settings_.render_settings.height -
          map_settings_.render_settings.padding - ys[i].id * ystep);
      }
      else {
        auto& company = *ys[i].company_ptr;
        company.address.coords.lat = map_settings_.render_settings.height -
          map_settings_.render_settings.padding - ys[i].id * ystep;
      }
    }
  }
  else {
    for (size_t i = 0; i < ys.size(); i++) {
      if (ys[i].stop_ptr) {
        const auto& name = ys[i].stop_ptr->GetName();
        stops_.at(name).SetY(map_settings_.render_settings.height -
                             map_settings_.render_settings.padding);
      }
      else {
        auto& company = *ys[i].company_ptr;
        company.address.coords.lat = map_settings_.render_settings.height -
                                   map_settings_.render_settings.padding;
      }
    }
  }
}

void Database::InitRenderSettings(RenderSettingsRequestPtr request) {
  map_settings_.render_settings = request->GetRenderSettings();
}

void Database::UpdateRenderSettings() {
  double min_lat, max_lat, min_lon, max_lon;
  min_lat = min_lon = std::numeric_limits<double>::max();
  max_lat = max_lon = std::numeric_limits<double>::min();
  for (const auto& [name, stop] : stops_) {
    double lat = stop.GetLatitude();
    min_lat = std::min(min_lat, lat);
    max_lat = std::max(max_lat, lat);
    double lon = stop.GetLongitude();
    min_lon = std::min(min_lon, lon);
    max_lon = std::max(max_lon, lon);
  }
  // Do we need this?
  for (const auto& company : companies_) {
    const auto& coords = company.address.coords;
    double lat = coords.lat;
    min_lat = std::min(min_lat, lat);
    max_lat = std::max(max_lat, lat);
    double lon = coords.lon;
    min_lon = std::min(min_lon, lon);
    max_lon = std::max(max_lon, lon);
  }

  map_settings_.min_lon = min_lon;
  map_settings_.max_lat = max_lat;
  double width_zoom_coef = 0, height_zoom_coef = 0;
  if (min_lon != max_lon) {
    width_zoom_coef = (map_settings_.render_settings.width -
                      2 * map_settings_.render_settings.padding) /
                          (max_lon - min_lon);
  }
  if (min_lat != max_lat) {
    height_zoom_coef = (map_settings_.render_settings.height -
                        2 * map_settings_.render_settings.padding) /
                          (max_lat - min_lat);
  }
  if (width_zoom_coef > 0 && height_zoom_coef > 0) {
    map_settings_.zoom_coef = std::min(width_zoom_coef, height_zoom_coef);
  }
  else {
    map_settings_.zoom_coef = width_zoom_coef + height_zoom_coef;
  }
}

void Database::UpdateSettings(RoutingSettingsRequestPtr request) {
  router_settings_.bus_wait_time = request->GetBusWaitTime();
  router_settings_.bus_velocity = request->GetBusVelocity();
  router_settings_.pedestrian_velocity = request->GetPedestrianVelocity();
}

void Database::SaveToFile() {
  SerializeDatabase();
}

