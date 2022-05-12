#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bus.h"
#include "graph.h"
#include "router.h"
#include "stop.h"
#include "transport_catalog.pb.h"
#include "yellow_pages_structures.h"

using RouterPtr = std::unique_ptr<Graph::Router<double>>;
using GraphPtr = std::unique_ptr<Graph::DirectedWeightedGraph<double>>;

class TransportRouter {
 public:
  // This one is for deserialization only!
  TransportRouter(const TCatalog::Graph &g, const TCatalog::RouterSettings &r) {
    bus_wait_time_ = r.bus_wait_time();
    bus_velocity_ = r.bus_velocity();
    pedestrian_velocity_ = r.pedestrian_velocity();

    size_t vertex_count = g.vertices_size();
    graph_ =
      std::make_unique<Graph::DirectedWeightedGraph<double>>(vertex_count);

    id_to_stop.reserve(vertex_count);
    edge_to_info.reserve(g.edges_info_size());
    for (auto i = 0; i < g.vertices_size(); i++) {
      id_to_stop.emplace_back(g.vertices(i));
      stop_to_id[g.vertices(i)] = i;
    }

    for (auto i = 0; i < g.edges_size(); i++) {
      const auto& e = g.edges(i);
      graph_->AddEdge(Graph::Edge<double>{e.id_from(), e.id_to(), e.w()});
    }

    for (auto i = 0; i < g.edges_info_size(); i++) {
      edge_to_info.emplace_back(g.edges_info(i).name(), g.edges_info(i).span());
    }

    router_ = std::make_unique<Graph::Router<double>>(*graph_);
  }

  TransportRouter(const std::map<std::string, Bus> buses,
                  const std::map<std::string, Stop> stops,
                  const RouterSettings& router_settings)
      : bus_wait_time_(static_cast<double>(router_settings.bus_wait_time)),
        bus_velocity_(router_settings.bus_velocity),
        pedestrian_velocity_(router_settings.pedestrian_velocity) {
    using namespace Graph;
    int vertex_count = stops.size();

    size_t unique_v_id = 0;
    stop_to_id.reserve(vertex_count);
    id_to_stop.reserve(vertex_count);

    graph_ = std::make_unique<DirectedWeightedGraph<double>>(vertex_count);

    for (const auto& [k, v] : stops) {
      stop_to_id.insert({k, unique_v_id++});
      id_to_stop.push_back(k);
    }

    for (const auto& [k, bus] : buses) {
      const auto& bus_stops = bus.GetStops();
      size_t bus_stops_size = bus_stops.size();
      RouteType type = bus.GetType();
      if (type == RouteType::ROUND) {
        for (size_t i = 0; i < bus_stops_size - 1; i++) {
          for (size_t j = i + 1; j < bus_stops_size; j++) {
            double t = TimeBetweenStops(bus_stops, stops, i, j);
            size_t idx_i = stop_to_id[bus_stops[i]];
            size_t idx_j = stop_to_id[bus_stops[j]];
            graph_->AddEdge(Edge<double>{idx_i, idx_j, t + bus_wait_time_});
            edge_to_info.push_back({bus.GetName(), j - i});
          }
        }
      }
      else if (type == RouteType::TWOWAY) {
        // -->
        for (size_t i = 0; i < bus_stops_size - 1; i++) {
          for (size_t j = i + 1; j < bus_stops_size; j++) {
            double t = TimeBetweenStops(bus_stops, stops, i, j);
            size_t idx_i = stop_to_id[bus_stops[i]];
            size_t idx_j = stop_to_id[bus_stops[j]];
            graph_->AddEdge(Edge<double>{idx_i, idx_j, t + bus_wait_time_});
            edge_to_info.push_back({bus.GetName(), j - i});
          }
        }
        // <--
        for (size_t i = 0; i < bus_stops_size - 1; i++) {
          for (size_t j = i + 1; j < bus_stops_size; j++) {
            double t = TimeBetweenStops(bus_stops, stops, j, i);
            size_t idx_j = stop_to_id[bus_stops[j]];
            size_t idx_i = stop_to_id[bus_stops[i]];
            graph_->AddEdge(Edge<double>{idx_j, idx_i, t + bus_wait_time_});
            edge_to_info.push_back({bus.GetName(), j - i});
          }
        }
      }
      else {
        throw std::runtime_error("Unknown bus type");
      }
    }
    // This is not needed due to parting the program in make/execute parts
    // router_ = std::make_unique<Graph::Router<double>>(*graph_);
  }

  struct BusItem {
    std::string name;
    std::string stop_beg;
    std::string stop_end;
    int span_count;
    double time;
  };

  struct StopItem {
    std::string name;
    double time;
  };

  struct WalkItem {
    std::string stop_name;
    double time;
  };

  struct WaitItem {
    double time;
  };

  struct RouteInfo {
    double total_time;
    std::optional<Company> company;
    std::vector<std::variant<BusItem, StopItem, WalkItem, WaitItem>> items;
  };

  std::optional<RouteInfo> BuildRoute(const std::string& from,
                                      const std::string& to) {
    size_t from_v = stop_to_id[from];
    size_t to_v = stop_to_id[to];

    const auto route = router_->BuildRoute(from_v, to_v);
    if (route == std::nullopt) {
      return std::nullopt;
    }
    RouteInfo route_info;
    route_info.total_time = route->weight;
    route_info.items.reserve(route->edge_count);
    for (size_t i = 0; i < route->edge_count; i++) {
      size_t edge_id = router_->GetRouteEdge(route->id, i);
      const Graph::Edge<double>& e = graph_->GetEdge(edge_id);
      const std::string& e_from = id_to_stop[e.from];
      const std::string& e_to = id_to_stop[e.to];
      route_info.items.push_back(
        StopItem{e_from, static_cast<double>(bus_wait_time_)});
      route_info.items.push_back(
        BusItem{edge_to_info[edge_id].first, e_from, e_to,
                edge_to_info[edge_id].second, e.weight - bus_wait_time_});
    }
    router_->ReleaseRoute(route->id);
    return route_info;
  }

  std::optional<RouteInfo> BuildRouteToCompany(const std::string& from,
                                               const NearbyStop& stop) {
    std::optional<RouteInfo> route_info;
    if (from != stop.name) {
      route_info = BuildRoute(from, stop.name);
      if (route_info == std::nullopt) {
        return std::nullopt;
      }
    }
    else {
      route_info = RouteInfo();
    }
    double walk_time = stop.meters / pedestrian_velocity_;
    route_info->total_time += walk_time;
    auto walk_item = WalkItem{stop.name, walk_time};
    route_info->items.push_back(walk_item);
    return route_info;
  }

  TCatalog::Graph SerializeGraph() const {
    TCatalog::Graph g;
    for (size_t i = 0; i < graph_->GetEdgeCount(); i++) {
      const Graph::Edge<double>& e = graph_->GetEdge(i);
      TCatalog::Edge edge;
      edge.set_id_from(e.from);
      edge.set_id_to(e.to);
      edge.set_w(e.weight);
      *g.add_edges() = edge;
      TCatalog::EdgeInfo edge_info;
      edge_info.set_name(edge_to_info[i].first);
      edge_info.set_span(edge_to_info[i].second);
      *g.add_edges_info() = edge_info;
    }
    for (size_t id = 0; id < id_to_stop.size(); id++) {
      g.add_vertices(id_to_stop[id]);
    }

    return g;
  }

 private:
  double TimeBetweenStops(const std::vector<std::string>& bus_stops,
                          const std::map<std::string, Stop>& stops_info,
                          size_t idx_l,
                          size_t idx_r) {
    double t = 0;
    if (idx_l < idx_r) {
      for (auto i = idx_l; i < idx_r; i++) {
        const Stop& stop1 = stops_info.at(bus_stops[i]);
        const Stop& stop2 = stops_info.at(bus_stops[i + 1]);
        t += MapDistanceBetweenStops(stop1, stop2) / bus_velocity_;
      }
    }
    else if (idx_l > idx_r) {
      for (auto i = idx_l; i > idx_r; i--) {
        const Stop& stop1 = stops_info.at(bus_stops[i]);
        const Stop& stop2 = stops_info.at(bus_stops[i - 1]);
        t += MapDistanceBetweenStops(stop1, stop2) / bus_velocity_;
      }
    }
    return t;
  }

  GraphPtr graph_;
  RouterPtr router_;
  int bus_wait_time_;
  double bus_velocity_;
  double pedestrian_velocity_;
  std::unordered_map<std::string, size_t> stop_to_id;
  std::vector<std::string> id_to_stop;
  std::vector<std::pair<std::string, int>> edge_to_info;
};

using TransportRouterPtr = std::unique_ptr<TransportRouter>;
