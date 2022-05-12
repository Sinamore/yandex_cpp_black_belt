#include "database.h"

std::unordered_map<std::string, void (Database::*)(Svg::Document&) const>
Database::RENDER_FUNCTIONS = {
  {"bus_labels", &Database::RenderBusNames},
  {"bus_lines",  &Database::RenderBuses},
  {"stop_labels", &Database::RenderStopNames},
  {"stop_points", &Database::RenderStops},
  {"company_lines", &Database::RenderNop},
  {"company_points", &Database::RenderNop},
  {"company_labels", &Database::RenderNop}
};

using RouteInfo = TransportRouter::RouteInfo;

std::unordered_map<std::string,
                   void (Database::*)(Svg::Document&, const RouteInfo&) const>
Database::ROUTE_RENDER_FUNCTIONS = {
  {"bus_labels", &Database::RenderBusNamesOnRoute},
  {"bus_lines",  &Database::RenderBusesOnRoute},
  {"stop_labels", &Database::RenderStopNamesOnRoute},
  {"stop_points", &Database::RenderStopsOnRoute},
  {"company_lines", &Database::RenderCompanyLines},
  {"company_points", &Database::RenderCompanyPoints},
  {"company_labels", &Database::RenderCompanyLabels}
};

void Database::RenderNop([[maybe_unused]] Svg::Document& doc) const {
  return;
}

void Database::SetBusColors() {
  size_t color_id = 0;
  const auto& palette = map_settings_.render_settings.color_palette;
  for (auto& [name, bus] : buses_) {
    bus.SetColorId(color_id++);
    color_id = color_id % palette.size();
  }
}

void Database::RenderBuses(Svg::Document& doc) const {
  for (auto& [name, bus] : buses_) {
    Svg::Polyline line;
    const auto& palette = map_settings_.render_settings.color_palette;
    line.SetStrokeColor(palette[bus.GetColorId()]);
    line.SetStrokeWidth(map_settings_.render_settings.line_width);
    line.SetStrokeLineCap("round");
    line.SetStrokeLineJoin("round");
    // set points
    if (bus.GetType() == RouteType::ROUND) {
      for (const auto& stop_name : bus.GetStops()) {
        const Stop& stop = stops_.at(stop_name);
        line.AddPoint({stop.GetX(), stop.GetY()});
      }
    }
    else {
      const auto& bus_stops = bus.GetStops();
      for (size_t i = 0; i < bus_stops.size(); i++) {
        const Stop& stop = stops_.at(bus_stops[i]);
        line.AddPoint({stop.GetX(), stop.GetY()});
      }
      for (int i = bus_stops.size() - 2; i >= 0; i--) {
        const Stop& stop = stops_.at(bus_stops[i]);
        line.AddPoint({stop.GetX(), stop.GetY()});
      }
    }
    doc.Add(line);
  }
}

void Database::RenderBusesOnRoute(Svg::Document& doc,
                                  const RouteInfo& route) const {
  for (size_t i = 0; i < route.items.size(); i++) {
    if (!std::holds_alternative<TransportRouter::BusItem>(route.items[i])) {
      continue;
    }
    const auto bus_item = std::get<TransportRouter::BusItem>(route.items[i]);
    const Bus& bus = buses_.at(bus_item.name);
    const auto& stops = bus.GetStops();
    Svg::Polyline line;
    const auto& palette = map_settings_.render_settings.color_palette;
    line.SetStrokeColor(palette[bus.GetColorId()]);
    line.SetStrokeWidth(map_settings_.render_settings.line_width);
    line.SetStrokeLineCap("round");
    line.SetStrokeLineJoin("round");
    if (bus.GetType() == RouteType::ROUND) {
      for (size_t j = 0; j < stops.size(); j++) {
        if (stops[j] == bus_item.stop_beg &&
            stops[j + bus_item.span_count] == bus_item.stop_end) {
          for (size_t k = j; k <= j + bus_item.span_count; k++) {
            const Stop& stop = stops_.at(stops[k]);
            line.AddPoint({stop.GetX(), stop.GetY()});
          }
          break;
        }
      }
    }
    else {
      for (int j = 0; j < static_cast<int>(stops.size()); j++) {
        if (stops[j] == bus_item.stop_beg) {
          if (j + bus_item.span_count < static_cast<int>(stops.size()) &&
              stops[j + bus_item.span_count] == bus_item.stop_end) {
            for (int k = j; k <= j + bus_item.span_count; k++) {
              const Stop& stop = stops_.at(stops[k]);
              line.AddPoint({stop.GetX(), stop.GetY()});
            }
            break;
          }
          else if (j >= bus_item.span_count &&
                   stops[j - bus_item.span_count] == bus_item.stop_end) {
            for (int k = j; k >= j - bus_item.span_count; k--) {
              const Stop& stop = stops_.at(stops[k]);
              line.AddPoint({stop.GetX(), stop.GetY()});
            }
            break;
          }
        }
      }
    }
    doc.Add(line);
  }
}

NameSvgItem Database::RenderBusName(const Stop& stop,
                                    const std::string& bus_name,
                                    size_t color_id) const {
  Svg::Text underlayer, toplayer;

  underlayer.SetPoint({stop.GetX(), stop.GetY()})
            .SetOffset(map_settings_.render_settings.bus_label_offset)
            .SetFontSize(map_settings_.render_settings.bus_label_font_size)
            .SetFontFamily("Verdana")
            .SetFontWeight("bold")
            .SetData(bus_name);
  toplayer = underlayer;
  underlayer.SetFillColor(map_settings_.render_settings.underlayer_color)
            .SetStrokeColor(map_settings_.render_settings.underlayer_color)
            .SetStrokeWidth(map_settings_.render_settings.underlayer_width)
            .SetStrokeLineCap("round")
            .SetStrokeLineJoin("round");
  const auto& palette = map_settings_.render_settings.color_palette;
  toplayer.SetFillColor(palette[color_id]);
  return NameSvgItem{std::move(underlayer), std::move(toplayer)};
}

void Database::RenderBusNames(Svg::Document& doc) const {
  for (const auto& [name, bus] : buses_) {
    const std::vector<std::string>& stops = bus.GetStops();
    const Stop& stop_start = stops_.at(stops[0]);

    auto [underlayer_start, toplayer_start] =
      RenderBusName(stop_start, bus.GetName(), bus.GetColorId());

    doc.Add(underlayer_start);
    doc.Add(toplayer_start);

    if (bus.GetType() == RouteType::TWOWAY) {
      const Stop& stop_finish = stops_.at(stops[stops.size() - 1]);

      auto [underlayer_finish, toplayer_finish] =
        RenderBusName(stop_finish, bus.GetName(), bus.GetColorId());

      doc.Add(underlayer_finish);
      doc.Add(toplayer_finish);
    }
  }
}

void Database::RenderBusNamesOnRoute(Svg::Document& doc,
                                     const RouteInfo& route) const {
  for (size_t i = 0; i < route.items.size(); i++) {
    if (!std::holds_alternative<TransportRouter::BusItem>(route.items[i])) {
      continue;
    }
    const auto bus_item = std::get<TransportRouter::BusItem>(route.items[i]);
    const Bus& bus = buses_.at(bus_item.name);
    const auto& stops = bus.GetStops();
    if (bus.GetType() == RouteType::ROUND) {
      for (size_t j = 0; j < stops.size(); j++) {
        if (stops[j] == bus_item.stop_beg &&
            stops[j + bus_item.span_count] == bus_item.stop_end) {
          if (stops[j] == stops[0]) {
            auto [ul, tl] = RenderBusName(stops_.at(stops[j]),
                                          bus.GetName(), bus.GetColorId());
            doc.Add(ul);
            doc.Add(tl);
          }
          if (stops[j + bus_item.span_count] == stops[stops.size() - 1]) {
            auto [ul, tl] =
              RenderBusName(stops_.at(stops[j + bus_item.span_count]),
                            bus.GetName(), bus.GetColorId());
            doc.Add(ul);
            doc.Add(tl);
          }
          break;
        }
      }
    }
    else {
      for (int j = 0; j < static_cast<int>(stops.size()); j++) {
        if (stops[j] == bus_item.stop_beg) {
          if (j + bus_item.span_count < static_cast<int>(stops.size()) &&
              stops[j + bus_item.span_count] == bus_item.stop_end) {
            if (stops[j] == stops[0] || stops[j] == stops.back()) {
              auto [ul, tl] = RenderBusName(stops_.at(stops[j]),
                                            bus.GetName(), bus.GetColorId());
              doc.Add(ul);
              doc.Add(tl);
            }
            if (stops[j + bus_item.span_count] == stops[stops.size() - 1] ||
                stops[j + bus_item.span_count] == stops[0]) {
              auto [ul, tl] =
                RenderBusName(stops_.at(stops[j + bus_item.span_count]),
                              bus.GetName(), bus.GetColorId());
              doc.Add(ul);
              doc.Add(tl);
            }
            break;
          }
          else if (j - bus_item.span_count >= 0 &&
                   stops[j - bus_item.span_count] == bus_item.stop_end) {
            if (stops[j] == stops[stops.size() - 1] || stops[j] == stops[0]) {
              auto [ul, tl] = RenderBusName(stops_.at(stops[j]),
                                            bus.GetName(), bus.GetColorId());
              doc.Add(ul);
              doc.Add(tl);
            }
            if (stops[j - bus_item.span_count] == stops[0] ||
                stops[j - bus_item.span_count] == stops.back()) {
              auto [ul, tl] =
                RenderBusName(stops_.at(stops[j - bus_item.span_count]),
                              bus.GetName(), bus.GetColorId());
              doc.Add(ul);
              doc.Add(tl);
            }
            break;
          }
        }
      }
    }
  }
}

Svg::Circle Database::RenderStop(const Stop& stop) const {
  Svg::Circle circle;

  circle.SetCenter({stop.GetX(), stop.GetY()});
  circle.SetRadius(map_settings_.render_settings.stop_radius);
  circle.SetFillColor(Svg::Color("white"));
  return circle;
}

void Database::RenderStops(Svg::Document& doc) const {
  for (const auto& [name, stop] : stops_) {
    doc.Add(RenderStop(stop));
  }
}

void Database::RenderStopsOnRoute(Svg::Document& doc,
                                  const RouteInfo& route) const {
  for (size_t i = 0; i < route.items.size(); i++) {
    if (!std::holds_alternative<TransportRouter::BusItem>(route.items[i])) {
      continue;
    }
    const auto bus_item = std::get<TransportRouter::BusItem>(route.items[i]);
    const Bus& bus = buses_.at(bus_item.name);
    const auto& stops = bus.GetStops();
    if (bus.GetType() == RouteType::ROUND) {
      for (int j = 0; j < static_cast<int>(stops.size()); j++) {
        if (stops[j] == bus_item.stop_beg &&
            stops[j + bus_item.span_count] == bus_item.stop_end) {
          for (int k = j; k <= j + bus_item.span_count; k++) {
            const Stop& stop = stops_.at(stops[k]);
            doc.Add(RenderStop(stop));
          }
          break;
        }
      }
    }
    else {
      for (int j = 0; j < static_cast<int>(stops.size()); j++) {
        if (stops[j] == bus_item.stop_beg) {
          if (j + bus_item.span_count < static_cast<int>(stops.size()) &&
              stops[j + bus_item.span_count] == bus_item.stop_end) {
            for (int k = j; k <= j + bus_item.span_count; k++) {
              const Stop& stop = stops_.at(stops[k]);
              doc.Add(RenderStop(stop));
            }
            break;
          }
          else if (j - bus_item.span_count >= 0 &&
                   stops[j - bus_item.span_count] == bus_item.stop_end) {
            for (int k = j; k >= j - bus_item.span_count; k--) {
              const Stop& stop = stops_.at(stops[k]);
              doc.Add(RenderStop(stop));
            }
            break;
          }
        }
      }
    }
  }
}

NameSvgItem Database::RenderStopName(const Stop& stop) const {
  Svg::Text underlayer, toplayer;

  underlayer.SetPoint({stop.GetX(), stop.GetY()})
            .SetOffset(map_settings_.render_settings.stop_label_offset)
            .SetFontSize(map_settings_.render_settings.stop_label_font_size)
            .SetFontFamily("Verdana")
            .SetData(stop.GetName());
  toplayer = underlayer;
  underlayer.SetFillColor(map_settings_.render_settings.underlayer_color)
            .SetStrokeColor(map_settings_.render_settings.underlayer_color)
            .SetStrokeWidth(map_settings_.render_settings.underlayer_width)
            .SetStrokeLineCap("round")
            .SetStrokeLineJoin("round");
  toplayer.SetFillColor(Svg::Color("black"));
  return {underlayer, toplayer};
}

void Database::RenderStopNames(Svg::Document& doc) const {
  for (const auto& [name, stop] : stops_) {
    auto [underlayer, toplayer] = RenderStopName(stop);
    doc.Add(underlayer);
    doc.Add(toplayer);
  }
}

void Database::RenderStopNamesOnRoute(Svg::Document& doc,
                                      const RouteInfo& route) const {
  for (size_t i = 0; i < route.items.size(); i++) {
    if (!std::holds_alternative<TransportRouter::StopItem>(route.items[i])) {
      continue;
    }
    const auto stop_item = std::get<TransportRouter::StopItem>(route.items[i]);
    auto [underlayer, toplayer] = RenderStopName(stops_.at(stop_item.name));
    doc.Add(underlayer);
    doc.Add(toplayer);
  }
  if (route.items.size() > 0) {
    if (std::holds_alternative<TransportRouter::WalkItem>(route.items[0])) {
      const auto walk = std::get<TransportRouter::WalkItem>(route.items[0]);
      const Stop& stop = stops_.at(walk.stop_name);
      auto [underlayer, toplayer] = RenderStopName(stop);
      doc.Add(underlayer);
      doc.Add(toplayer);
    }
    else {
      TransportRouter::BusItem last_bus;
      for (int i = route.items.size() - 1; i >= 0; i--) {
        if (std::holds_alternative<TransportRouter::BusItem>(route.items[i])) {
          last_bus = std::get<TransportRouter::BusItem>(route.items[i]);
          const Stop& stop = stops_.at(last_bus.stop_end);
          auto [underlayer, toplayer] = RenderStopName(stop);
          doc.Add(underlayer);
          doc.Add(toplayer);
          break;
        }
      }
    }
  }
}

void Database::RenderCompanyLines(Svg::Document& doc,
                                  const RouteInfo& route) const {
  for (int i = route.items.size() - 1; i >= 0; i--) {
    if (!std::holds_alternative<TransportRouter::WalkItem>(route.items[i])) {
      continue;
    }
    Svg::Polyline line;
    line.SetStrokeColor(Svg::Color("black"));
    line.SetStrokeWidth(map_settings_.render_settings.company_line_width);
    line.SetStrokeLineCap("round");
    line.SetStrokeLineJoin("round");
    const auto walk_item = std::get<TransportRouter::WalkItem>(route.items[i]);
    const auto stop = stops_.at(walk_item.stop_name);
    line.AddPoint({stop.GetX(), stop.GetY()});
    line.AddPoint({route.company->address.coords.lon,
                   route.company->address.coords.lat});
    doc.Add(line);
    return;
  }
}

void Database::RenderCompanyPoints(Svg::Document& doc,
                                   const RouteInfo& route) const {
  for (int i = route.items.size() - 1; i >= 0; i--) {
    if (!std::holds_alternative<TransportRouter::WalkItem>(route.items[i])) {
      continue;
    }
    Svg::Circle circle;
    const auto walk_item = std::get<TransportRouter::WalkItem>(route.items[i]);
    circle.SetCenter({route.company->address.coords.lon,
                      route.company->address.coords.lat});
    circle.SetRadius(map_settings_.render_settings.company_radius);
    circle.SetFillColor(Svg::Color("black"));
    doc.Add(circle);
    return;
  }
}

void Database::RenderCompanyLabels(Svg::Document& doc,
                                   const RouteInfo& route) const {
  for (int i = route.items.size() - 1; i >= 0; i--) {
    if (!std::holds_alternative<TransportRouter::WalkItem>(route.items[i])) {
      continue;
    }
    const auto walk_item = std::get<TransportRouter::WalkItem>(route.items[i]);
    std::string name = GetCompanyMainName(*route.company);
    if (!route.company->rubrics.empty()) {
      name = rubrics_num_.at(route.company->rubrics[0]) + " " + name;
    }
    Svg::Text underlayer, toplayer;

    underlayer.SetPoint({route.company->address.coords.lon,
                         route.company->address.coords.lat})
              .SetOffset(map_settings_.render_settings.stop_label_offset)
              .SetFontSize(map_settings_.render_settings.stop_label_font_size)
              .SetFontFamily("Verdana")
              .SetData(name);
    toplayer = underlayer;
    underlayer.SetFillColor(map_settings_.render_settings.underlayer_color)
              .SetStrokeColor(map_settings_.render_settings.underlayer_color)
              .SetStrokeWidth(map_settings_.render_settings.underlayer_width)
              .SetStrokeLineCap("round")
              .SetStrokeLineJoin("round");
    toplayer.SetFillColor(Svg::Color("black"));
    doc.Add(underlayer);
    doc.Add(toplayer);
    return;
  }
}

void Database::RenderSemiTransparentRectangle(Svg::Document& doc) const {
  Svg::Rectangle rect;
  rect.SetPoint({-map_settings_.render_settings.outer_margin,
                 -map_settings_.render_settings.outer_margin})
      .SetWidth(map_settings_.render_settings.width +
                2 * map_settings_.render_settings.outer_margin)
      .SetHeight(map_settings_.render_settings.height +
                 2 * map_settings_.render_settings.outer_margin)
      .SetFillColor(map_settings_.render_settings.underlayer_color);
  doc.Add(rect);
}

Svg::Document Database::RenderBaseMap() {
  Svg::Document doc;

  // Render back map
  if (!route_map_background_) {
    SetBusColors();
    for (const auto& layer : map_settings_.render_settings.layers) {
      (this->*RENDER_FUNCTIONS.at(layer))(doc);
    }
    route_map_background_ = doc;
  }
  else {
    doc = *route_map_background_;
  }
  return doc;
}

std::string Database::RenderAsSvg() {
  std::ostringstream os;
  RenderBaseMap().Render(os);
  return os.str();
}

std::string
Database::RenderRouteAsSvg(const TransportRouter::RouteInfo& route) {
  Svg::Document doc;
  // Render back map
  doc = RenderBaseMap();

  RenderSemiTransparentRectangle(doc);

  for (const auto& layer : map_settings_.render_settings.layers) {
    (this->*ROUTE_RENDER_FUNCTIONS.at(layer))(doc, route);
  }

  std::ostringstream os;
  doc.Render(os);

  return os.str();
}

