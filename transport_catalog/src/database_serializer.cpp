#include <fstream>

#include "database.h"

#include "transport_catalog.pb.h"

static std::string ReadFileData(const std::string& file_name) {
  std::ifstream file(file_name, std::ios::binary | std::ios::ate);
  const std::ifstream::pos_type end_pos = file.tellg();
  file.seekg(0, std::ios::beg);

  std::string data(end_pos, '\0');
  file.read(&data[0], end_pos);
  return data;
}

static Svg::Color GetColorFromTCColor(const TCatalog::Color tccolor) {
  if (tccolor.color_case() == 1) {
    return Svg::Color(tccolor.s());
  }
  else if (tccolor.color_case() == 2) {
    return Svg::Color(Svg::Rgb{tccolor.rgb().red(), tccolor.rgb().green(),
                               tccolor.rgb().blue()});
  }
  else if (tccolor.color_case() == 3) {
    return Svg::Color(Svg::Rgba{
      {tccolor.rgba().red(), tccolor.rgba().green(), tccolor.rgba().blue()},
       tccolor.rgba().alpha()});
  }
  return Svg::Color();
}

static Bus GetBusFromTCBus(const std::string& name,
                           const TCatalog::Bus& tcbus) {
  std::vector<std::string> stops;
  stops.reserve(tcbus.stops_size());
  for (int i = 0; i < tcbus.stops_size(); i++) {
    stops.push_back(tcbus.stops(i));
  }
  return Bus(name,
             stops,
             (tcbus.is_round()) ? RouteType::ROUND : RouteType::TWOWAY,
             tcbus.num_stops(),
             tcbus.num_unique_stops(),
             tcbus.geo_route_length(),
             tcbus.map_route_length(),
             tcbus.curvature());
}

static Stop GetStopFromTCStop(const TCatalog::Stop& tcstop) {
  std::unordered_map<std::string, int> dist_map;
  const auto& dist = tcstop.distances();
  for (const auto& e : dist) {
    dist_map[e.first] = e.second;
  }
  return Stop(tcstop.name(),
              tcstop.latitude(),
              tcstop.longitude(),
              tcstop.real_latitude(),
              tcstop.real_longitude(),
              tcstop.x(),
              tcstop.y(),
              dist_map);
}

static MapSettings DeserializeMapSettings(const TCatalog::MapSettings& tc_ms) {
  MapSettings ms;
  ms.min_lon = tc_ms.min_lon();
  ms.max_lat = tc_ms.max_lat();
  ms.zoom_coef = tc_ms.zoom_coef();
  auto &rs = ms.render_settings;
  const auto &tc_rs = tc_ms.render_settings();
  rs.width = tc_rs.width();
  rs.height = tc_rs.height();
  rs.padding = tc_rs.padding();
  rs.stop_radius = tc_rs.stop_radius();
  rs.line_width = tc_rs.line_width();
  rs.stop_label_font_size = tc_rs.stop_label_font_size();
  rs.stop_label_offset.x = tc_rs.stop_label_offset().x();
  rs.stop_label_offset.y = tc_rs.stop_label_offset().y();
  rs.underlayer_color = GetColorFromTCColor(tc_rs.underlayer_color());
  rs.underlayer_width = tc_rs.underlayer_width();
  rs.color_palette.reserve(tc_rs.color_palette_size());
  for (int i = 0; i < tc_rs.color_palette_size(); i++) {
    rs.color_palette.push_back(GetColorFromTCColor(tc_rs.color_palette(i)));
  }
  rs.bus_label_font_size = tc_rs.bus_label_font_size();
  rs.bus_label_offset.x = tc_rs.bus_label_offset().x();
  rs.bus_label_offset.y = tc_rs.bus_label_offset().y();
  rs.layers.reserve(tc_rs.layers_size());
  for (int i = 0; i < tc_rs.layers_size(); i++) {
    rs.layers.push_back(tc_rs.layers(i));
  }
  rs.outer_margin = tc_rs.outer_margin();
  rs.company_radius = tc_rs.company_radius();
  rs.company_line_width = tc_rs.company_line_width();
  return ms;
}

static std::map<std::string, Stop>
DeserializeStops(const TCatalog::TransportCatalog& tc) {
  std::map<std::string, Stop> stops;
  for (int i = 0; i < tc.stops_size(); i++) {
    Stop stop = GetStopFromTCStop(tc.stops(i));
    stops.insert({stop.GetName(), stop});
  }
  return stops;
}

static std::map<std::string, Bus>
DeserializeBuses(const TCatalog::TransportCatalog& tc) {
  std::map<std::string, Bus> buses;
  const auto& bs = tc.buses();

  for (const auto& e : bs) {
    Bus bus = GetBusFromTCBus(e.first, e.second);
    buses.insert({e.first, std::move(bus)});
  }
  return buses;
}

static std::unordered_map<std::string, std::set<std::string>>
DeserializeStopsToBuses(const TCatalog::TransportCatalog& tc) {
  std::unordered_map<std::string, std::set<std::string>> stops_to_buses;
  for (int i = 0; i < tc.stb_size(); i++) {
    std::string stop = tc.stb(i).stop();
    stops_to_buses[stop] = std::set<std::string>();
    for (int j = 0; j < tc.stb(i).buses_size(); j++) {
      stops_to_buses[stop].insert(tc.stb(i).buses(j));
    }
  }
  return stops_to_buses;
}

static std::unordered_map<std::string, uint64_t>
DeserializeRubrics(const YellowPages::Database& db) {
  std::unordered_map<std::string, uint64_t> rubrics;
  const auto& rubrics_map = db.rubrics();
  for (const auto& r : rubrics_map) {
    rubrics.insert({r.second.name(), r.first});
  }
  return rubrics;
}

static std::vector<Company>
DeserializeCompanies(const YellowPages::Database& db) {
  std::vector<Company> companies;
  companies.reserve(db.companies_size());
  for (const auto& company : db.companies()) {
    Company new_company;
    new_company.address.coords.lon = company.address().coords().lon();
    new_company.address.coords.lat = company.address().coords().lat();
    for (int i = 0; i < company.names_size(); i++) {
      Name new_name;
      new_name.value = company.names(i).value();
      if (company.names(i).type() == YellowPages::Name_Type::Name_Type_MAIN) {
        new_name.type = NameType::Main;
      }
      else if (company.names(i).type() == \
               YellowPages::Name_Type::Name_Type_SYNONYM) {
        new_name.type = NameType::Synonym;
      }
      else if (company.names(i).type() == \
               YellowPages::Name_Type::Name_Type_SHORT) {
        new_name.type = NameType::Short;
      }
      new_company.names.push_back(new_name);
    }
    for (int i = 0; i < company.phones_size(); i++) {
      Phone phone;
      if (company.phones(i).type() == \
          YellowPages::Phone_Type::Phone_Type_PHONE) {
        phone.type = PhoneType::Phone;
      }
      else if (company.phones(i).type() == \
               YellowPages::Phone_Type::Phone_Type_FAX) {
        phone.type = PhoneType::Fax;
      }
        phone.country_code = company.phones(i).country_code();
        phone.local_code = company.phones(i).local_code();
        phone.number = company.phones(i).number();
        phone.extension = company.phones(i).extension();
      new_company.phones.push_back(phone);
    }
    for (int i = 0; i < company.urls_size(); i++) {
      new_company.urls.push_back(company.urls(i).value());
    }
    for (int i = 0; i < company.rubrics_size(); i++) {
      new_company.rubrics.push_back(company.rubrics(i));
    }
    for (int i = 0; i < company.nearby_stops_size(); i++) {
      NearbyStop stop;
      stop.name = company.nearby_stops(i).name();
      stop.meters = company.nearby_stops(i).meters();
      new_company.nearby_stops.push_back(stop);
    }
    new_company.working_time.is_everyday = company.working_time().is_everyday();
    for (int i = 0; i < company.working_time().intervals_size(); i++) {
      WorkingTimeInterval wti;
      wti.minutes_from = company.working_time().intervals(i).minutes_from();
      wti.minutes_to = company.working_time().intervals(i).minutes_to();
      new_company.working_time.intervals.push_back(wti);
    }
    companies.push_back(new_company);
  }
  return companies;
}


void Database::BuildRubricsNum(const std::unordered_map<std::string,
                               uint64_t>& um) {
  rubrics_num_.reserve(um.size());
  for (const auto& [rubric, num] : um) {
    rubrics_num_.insert({num, rubric});
  }
}


void Database::DeserializeDatabase(SerializationSettingsRequestPtr r) {
  TCatalog::TransportCatalog catalog;
  catalog.ParseFromString(ReadFileData(r->GetFileName()));

  map_settings_ = DeserializeMapSettings(catalog.map_settings());

  stops_ = DeserializeStops(catalog);

  buses_ = DeserializeBuses(catalog);

  stops_to_buses_ = DeserializeStopsToBuses(catalog);

  trouter_ = std::make_unique<TransportRouter>(catalog.graph(),
                                               catalog.router_settings());

  // aka DeserializeYellowPages()
  rubrics_ = DeserializeRubrics(catalog.yellow_pages());
  BuildRubricsNum(rubrics_);
  companies_ = DeserializeCompanies(catalog.yellow_pages());
}

/*****************************************************************************
 *                            SERIALIZE                                      *
 *****************************************************************************/

static void SetTCColor(TCatalog::Color *tc_color,
                       Svg::Color& svg_color) {
  if (svg_color.IsA<std::string>()) {
    *tc_color->mutable_s() = svg_color.AsA<std::string>();
  }
  else if (svg_color.IsA<Svg::Rgb>()) {
    tc_color->mutable_rgb()->set_red(svg_color.AsA<Svg::Rgb>().red);
    tc_color->mutable_rgb()->set_green(svg_color.AsA<Svg::Rgb>().green);
    tc_color->mutable_rgb()->set_blue(svg_color.AsA<Svg::Rgb>().blue);
  }
  else if (svg_color.IsA<Svg::Rgba>()) {
    tc_color->mutable_rgba()->set_alpha(svg_color.AsA<Svg::Rgba>().alpha);
    tc_color->mutable_rgba()->set_red(svg_color.AsA<Svg::Rgba>().color.red);
    tc_color->mutable_rgba()->set_green(svg_color.AsA<Svg::Rgba>().color.green);
    tc_color->mutable_rgba()->set_blue(svg_color.AsA<Svg::Rgba>().color.blue);
  }
}


static void SerializeBuses(TCatalog::TransportCatalog& tc,
                           const std::map<std::string, Bus>& buses) {
  auto& buses_map = *tc.mutable_buses();
  for (const auto& [name, bus] : buses) {
    TCatalog::Bus b;
    b.set_name(bus.GetName());

    for (const auto& stop_name : bus.GetStops()) {
      b.add_stops(stop_name);
    }
    if (bus.GetType() == RouteType::ROUND) {
      b.set_is_round(true);
    }
    else {
      b.set_is_round(false);
    }
    b.set_num_stops(bus.GetNumStops());
    b.set_num_unique_stops(bus.GetNumUniqueStops());
    b.set_geo_route_length(*bus.GetGeoRouteLength());
    b.set_map_route_length(bus.GetMapRouteLength());
    b.set_curvature(bus.GetCurvature());
    buses_map[name] = b;
  }
}

static void SerializeStops(TCatalog::TransportCatalog& tc,
                           const std::map<std::string, Stop>& stops) {
  for (const auto& [name, stop] : stops) {
    TCatalog::Stop s;
    s.set_name(stop.GetName());
    s.set_latitude(stop.GetLatitude());
    s.set_longitude(stop.GetLongitude());
    s.set_real_latitude(stop.GetRealLatitude());
    s.set_real_longitude(stop.GetRealLongitude());
    s.set_x(stop.GetX());
    s.set_y(stop.GetY());
    auto &dist_map = *s.mutable_distances();
    for (const auto& [n, d] : stop.GetDistances()) {
      dist_map[n] = d;
    }
    *tc.add_stops() = s;
  }
}

static void SerializeStopsToBuses(TCatalog::TransportCatalog& tc,
                                  std::unordered_map<
                                    std::string,
                                    std::set<std::string>> stops_to_buses) {
  for (const auto& [stop, buses] : stops_to_buses) {
    TCatalog::StopToBuses stb;
    stb.set_stop(stop);
    for (const auto& bus : buses) {
      stb.add_buses(bus);
    }
    *tc.add_stb() = stb;
  }
}

static void SerializeMapSettings(TCatalog::TransportCatalog& tc,
                                 MapSettings& ms) {
  TCatalog::MapSettings* tc_ms = tc.mutable_map_settings();
  auto& tc_rs = *tc_ms->mutable_render_settings();
  tc_rs.set_width(ms.render_settings.width);
  tc_rs.set_height(ms.render_settings.height);
  tc_rs.set_padding(ms.render_settings.padding);
  tc_rs.set_stop_radius(ms.render_settings.stop_radius);
  tc_rs.set_line_width(ms.render_settings.line_width);
  tc_rs.set_stop_label_font_size(ms.render_settings.stop_label_font_size);
  tc_rs.mutable_stop_label_offset()->set_x(
      ms.render_settings.stop_label_offset.x);
  tc_rs.mutable_stop_label_offset()->set_y(
      ms.render_settings.stop_label_offset.y);

  SetTCColor(tc_rs.mutable_underlayer_color(),
             ms.render_settings.underlayer_color);

  tc_rs.set_underlayer_width(ms.render_settings.underlayer_width);

  for (auto& color : ms.render_settings.color_palette) {
    TCatalog::Color c;
    SetTCColor(&c, color);
    *tc_rs.add_color_palette() = c;
  }

  tc_rs.set_bus_label_font_size(ms.render_settings.bus_label_font_size);
  tc_rs.mutable_bus_label_offset()->set_x(
      ms.render_settings.bus_label_offset.x);
  tc_rs.mutable_bus_label_offset()->set_y(
      ms.render_settings.bus_label_offset.y);

  for (const auto& layer : ms.render_settings.layers) {
    tc_rs.add_layers(layer);
  }

  tc_rs.set_outer_margin(ms.render_settings.outer_margin);
  tc_rs.set_company_radius(ms.render_settings.company_radius);
  tc_rs.set_company_line_width(ms.render_settings.company_line_width);

  tc_ms->set_min_lon(ms.min_lon);
  tc_ms->set_max_lat(ms.max_lat);
  tc_ms->set_zoom_coef(ms.zoom_coef);
}

static void SerializeRouterSettings(TCatalog::TransportCatalog& tc,
                                    const RouterSettings& rs) {
  TCatalog::RouterSettings* router_s = tc.mutable_router_settings();
  router_s->set_bus_wait_time(rs.bus_wait_time);
  router_s->set_bus_velocity(rs.bus_velocity);
  router_s->set_pedestrian_velocity(rs.pedestrian_velocity);
}

static YellowPages::Company SerializeCompany(const Company& company) {
  YellowPages::Company new_company;
  new_company.mutable_address()->mutable_coords()->set_lon(
      company.address.coords.lon);
  new_company.mutable_address()->mutable_coords()->set_lat(
      company.address.coords.lat);
  for (const auto& name : company.names) {
    YellowPages::Name new_name;
    new_name.set_value(name.value);
    new_name.set_type(YellowPages::Name::Type(name.type));
    *new_company.add_names() = new_name;
  }
  for (const auto& phone : company.phones) {
    YellowPages::Phone new_phone;
    new_phone.set_type(YellowPages::Phone::Type(phone.type));
    new_phone.set_country_code(phone.country_code);
    new_phone.set_local_code(phone.local_code);
    new_phone.set_number(phone.number);
    new_phone.set_extension(phone.extension);
    *new_company.add_phones() = new_phone;
  }
  for (const auto& url : company.urls) {
    YellowPages::Url new_url;
    new_url.set_value(url);
    *new_company.add_urls() = new_url;
  }
  for (const auto rubric : company.rubrics) {
    new_company.add_rubrics(rubric);
  }
  for (const auto& stop : company.nearby_stops) {
    YellowPages::NearbyStop nearby_stop;
    nearby_stop.set_name(stop.name);
    nearby_stop.set_meters(stop.meters);
    *new_company.add_nearby_stops() = nearby_stop;
  }
  new_company.mutable_working_time()->set_is_everyday(
      company.working_time.is_everyday);
  for (const auto& wti : company.working_time.intervals) {
    YellowPages::WorkingTimeInterval new_wti;
    new_wti.set_minutes_from(wti.minutes_from);
    new_wti.set_minutes_to(wti.minutes_to);
    *new_company.mutable_working_time()->add_intervals() = new_wti;
  }
  return new_company;
}

static void SerializeRubrics(TCatalog::TransportCatalog& tc,
                             std::unordered_map<std::string,
                             uint64_t>& rubrics) {
  YellowPages::Database* db = tc.mutable_yellow_pages();
  auto& rubrics_map = *db->mutable_rubrics();
  for (const auto& [rubric, key] : rubrics) {
    YellowPages::Rubric new_rubric;
    new_rubric.set_name(rubric);
    rubrics_map[key] = new_rubric;
  }
}

static void SerializeCompanies(TCatalog::TransportCatalog& tc,
                               const std::vector<Company>& companies) {
  YellowPages::Database* db = tc.mutable_yellow_pages();
  for (const auto& company : companies) {
    *db->add_companies() = SerializeCompany(company);
  }
}

void Database::SerializeDatabase() {
  std::ofstream of(output_file_, std::ios_base::out | std::ios_base::binary);
  TCatalog::TransportCatalog catalog;

  SerializeBuses(catalog, buses_);

  SerializeStops(catalog, stops_);

  SerializeStopsToBuses(catalog, stops_to_buses_);

  SerializeMapSettings(catalog, map_settings_);

  SerializeRouterSettings(catalog, router_settings_);

  *catalog.mutable_graph() = trouter_->SerializeGraph();

  // aka SerializeYellowPages()
  SerializeRubrics(catalog, rubrics_);
  SerializeCompanies(catalog, companies_);

  catalog.SerializeToOstream(&of);
}
