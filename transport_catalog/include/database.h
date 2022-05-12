#pragma once

#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "bus.h"
#include "json.h"
#include "request.h"
#include "stop.h"
#include "transport_router.h"

using Response = Json::Node;

struct MapSettings {
  RenderSettings render_settings;
  double min_lon;
  double max_lat;
  double zoom_coef;
};

struct BaseInputStruct {
  RoutingSettingsRequestPtr routing_settings;
  RenderSettingsRequestPtr render_settings;
  std::vector<RequestPtr> updates;
  SerializationSettingsRequestPtr serialization_settings;
  YellowPagesRequestPtr yellow_pages_request;
};

struct StatInputStruct {
  std::vector<RequestPtr> queries;
  SerializationSettingsRequestPtr serialization_settings;
};

struct NameSvgItem {
  Svg::Text underlayer;
  Svg::Text toplayer;
};

struct MapItem {
  double coord;
  Stop* stop_ptr;
  Company* company_ptr;
  int id;
};

std::string GetCompanyMainName(const Company& company);

class Database {
 public:
  explicit Database(BaseInputStruct& db_settings);
  explicit Database(SerializationSettingsRequestPtr r);
  void SaveToFile();
  std::vector<Response> ProcessQueries(const std::vector<RequestPtr>& queries);

 private:
  void SerializeDatabase();
  void DeserializeDatabase(SerializationSettingsRequestPtr r);

  void UpdateSettings(RoutingSettingsRequestPtr request);
  void UpdateDatabase(std::vector<RequestPtr> requests);
  void EvaluateRouteLengths();
  void InitRenderSettings(RenderSettingsRequestPtr request);
  void UpdateRenderSettings();
  void MarkBaseStops();
  void MoveIntermediateStops();
  void CompressCoordinates();
  void PrepareRoutes();
  void FillStopsNeighbours();

  bool AreNeighbours(const MapItem&, const MapItem&) const;

  void FillCompanies(YellowPagesRequestPtr request);
  std::vector<Company> FilterCompaniesByRequest(
      const QueryCompanyRequestBase& r);
  std::vector<Json::Node> GetNamesFromCompanies(const std::vector<Company>& c);
  std::vector<Json::Node> BuildRouteItemNodes(
      const TransportRouter::RouteInfo& r);

  void BuildRubricsNum(const std::unordered_map<std::string, uint64_t>& um);

  std::optional<TransportRouter::RouteInfo>
  BuildRouteToClosestCompany(const std::string,
                             const double,
                             const std::vector<Company>&);

  double GetXOnMap(const Stop& stop) const;
  double GetYOnMap(const Stop& stop) const;
  double GetXOnMap(const double lon) const;
  double GetYOnMap(const double lat) const;

  void RenderNop(Svg::Document& doc) const;
  void SetBusColors();
  void RenderBuses(Svg::Document& doc) const;
  NameSvgItem RenderBusName(const Stop&, const std::string&, size_t) const;
  void RenderBusNames(Svg::Document& doc) const;
  Svg::Circle RenderStop(const Stop&) const;
  void RenderStops(Svg::Document& doc) const;
  NameSvgItem RenderStopName(const Stop&) const;
  void RenderStopNames(Svg::Document& doc) const;
  void RenderSemiTransparentRectangle(Svg::Document& doc) const;
  void RenderBusesOnRoute(Svg::Document&,
                          const TransportRouter::RouteInfo&) const;
  void RenderBusNamesOnRoute(Svg::Document&,
                             const TransportRouter::RouteInfo&) const;
  void RenderStopsOnRoute(Svg::Document&,
                          const TransportRouter::RouteInfo&) const;
  void RenderStopNamesOnRoute(Svg::Document&,
                          const TransportRouter::RouteInfo&) const;
  void RenderCompanyLines(Svg::Document&,
                          const TransportRouter::RouteInfo&) const;
  void RenderCompanyPoints(Svg::Document&,
                           const TransportRouter::RouteInfo&) const;
  void RenderCompanyLabels(Svg::Document&,
                           const TransportRouter::RouteInfo&) const;
  Svg::Document RenderBaseMap();
  std::string RenderAsSvg();
  std::string RenderRouteAsSvg(const TransportRouter::RouteInfo& route);

  std::map<std::string, Bus> buses_;
  std::map<std::string, Stop> stops_;
  std::unordered_map<std::string, std::set<std::string>> stops_to_buses_;
  std::map<std::string, std::set<std::string>> stops_neighbours_;
  RouterSettings router_settings_;
  TransportRouterPtr trouter_;
  MapSettings map_settings_;
  std::string output_file_;

  using RenderFunction = void (Database::*)(Svg::Document&) const;
  using RoutePartRenderFunction =
    void (Database::*)(Svg::Document&, const TransportRouter::RouteInfo&) const;
  static std::unordered_map <std::string, RenderFunction> RENDER_FUNCTIONS;
  static std::unordered_map <std::string, RoutePartRenderFunction> ROUTE_RENDER_FUNCTIONS;
  std::optional<Svg::Document> route_map_background_ = std::nullopt;

  YellowPagesRequestPtr yellow_pages_request_;

  std::vector<Company> companies_;
  std::unordered_map<std::string, uint64_t> rubrics_;
  std::unordered_map<uint64_t, std::string> rubrics_num_;
};
