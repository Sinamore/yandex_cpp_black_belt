#include <fstream>
#include <memory>
#include <vector>

#include "database.h"
#include "json.h"
#include "request.h"

using namespace std;

static UpdateBusRequestPtr
ParseUpdateBusRequest(const map<string, Json::Node>& request_m) {
  UpdateBusRequestPtr res = make_unique<UpdateBusRequest>();
  res->SetBusName(request_m.at("name").AsString());
  const auto& bus_stops = request_m.at("stops").AsArray();
  vector<string> stop_names;
  stop_names.reserve(bus_stops.size());
  for (const auto& node : bus_stops) {
    stop_names.push_back(node.AsString());
  }
  res->SetStopNames(move(stop_names));
  bool roundtrip = request_m.at("is_roundtrip").AsBool();
  if (roundtrip) {
    res->SetRouteType(RouteType::ROUND);
  }
  else {
    res->SetRouteType(RouteType::TWOWAY);
  }
  return res;
}

static UpdateStopRequestPtr
ParseUpdateStopRequest(const map<string, Json::Node>& request_m) {
  UpdateStopRequestPtr res = make_unique<UpdateStopRequest>();
  res->SetStopName(request_m.at("name").AsString());
  if (holds_alternative<double>(request_m.at("latitude"))) {
    res->SetLatitude(request_m.at("latitude").AsDouble());
  }
  else {
    res->SetLatitude(static_cast<double>(request_m.at("latitude").AsInt()));
  }
  if (holds_alternative<double>(request_m.at("longitude"))) {
    res->SetLongitude(request_m.at("longitude").AsDouble());
  }
  else {
    res->SetLongitude(static_cast<double>(request_m.at("longitude").AsInt()));
  }
  const auto& distances = request_m.at("road_distances").AsMap();
  for (const auto& [stop, distance] : distances) {
    res->AddDistance(stop, distance.AsInt());
  }
  return res;
}

static RequestPtr ParseBaseRequest(const Json::Node& request) {
  const auto& request_m = request.AsMap();
  const string &type = request_m.at("type").AsString();
  if (type == "Bus") {
    return ParseUpdateBusRequest(request_m);
  }
  else if (type == "Stop") {
    return ParseUpdateStopRequest(request_m);
  }
  else {
    throw runtime_error("Unknown base request");
  }
}

static QueryBusRequestPtr
ParseQueryBusRequest(const map<string, Json::Node>& request_m) {
  QueryBusRequestPtr res = make_unique<QueryBusRequest>();
  res->SetName(request_m.at("name").AsString());
  res->SetId(request_m.at("id").AsInt());
  return res;
}

static QueryRouteRequestPtr
ParseQueryRouteRequest(const map<string, Json::Node>& request_m) {
  QueryRouteRequestPtr res = make_unique<QueryRouteRequest>();
  res->SetId(request_m.at("id").AsInt());
  res->SetFrom(request_m.at("from").AsString());
  res->SetTo(request_m.at("to").AsString());
  return res;
}

static QueryStopRequestPtr
ParseQueryStopRequest(const map<string, Json::Node>& request_m) {
  QueryStopRequestPtr res = make_unique<QueryStopRequest>();
  res->SetName(request_m.at("name").AsString());
  res->SetId(request_m.at("id").AsInt());
  return res;
}

static QueryMapRequestPtr
ParseQueryMapRequest(const map<string, Json::Node>& request_m) {
  QueryMapRequestPtr res = make_unique<QueryMapRequest>();
  res->SetId(request_m.at("id").AsInt());
  return res;
}

static QueryCompanyRequestPtr
ParseQueryCompanyRequest(const map<string, Json::Node>& request_m) {
  QueryCompanyRequestPtr res = make_unique<QueryCompanyRequest>();
  res->ParseFromJson(request_m);
  return res;
}

static QueryRouteToCompanyRequestPtr
ParseQueryRouteToCompanyRequest(const map<string, Json::Node>& request_m) {
  QueryRouteToCompanyRequestPtr res = make_unique<QueryRouteToCompanyRequest>();
  res->ParseFromJson(request_m);
  return res;
}

static RequestPtr ParseStatRequest(const Json::Node& request) {
  const auto& request_m = request.AsMap();
  const string &type = request_m.at("type").AsString();
  if (type == "Bus") {
    return ParseQueryBusRequest(request_m);
  }
  else if (type == "Stop") {
    return ParseQueryStopRequest(request_m);
  }
  else if (type == "Route") {
    return ParseQueryRouteRequest(request_m);
  }
  else if (type == "Map") {
    return ParseQueryMapRequest(request_m);
  }
  else if (type == "FindCompanies") {
    return ParseQueryCompanyRequest(request_m);
  }
  else if (type == "RouteToCompany") {
    return ParseQueryRouteToCompanyRequest(request_m);
  }
  else {
    throw runtime_error("Unknown stat request");
  }
}

static RoutingSettingsRequestPtr
ParseRoutingSettingsRequest(const Json::Node& request) {
  RoutingSettingsRequestPtr res = make_unique<RoutingSettingsRequest>();
  res->ParseFromJson(request);
  return res;
}

static RenderSettingsRequestPtr
ParseRenderSettingsRequest(const Json::Node& request) {
  RenderSettingsRequestPtr res = make_unique<RenderSettingsRequest>();
  res->ParseFromJson(request);
  return res;
}

static SerializationSettingsRequestPtr
ParseSerializationSettingsRequest(const Json::Node& request) {
  SerializationSettingsRequestPtr res =
    make_unique<SerializationSettingsRequest>();
  res->ParseFromJson(request);
  return res;
}

static YellowPagesRequestPtr
ParseYellowPagesRequest(const Json::Node& request) {
  YellowPagesRequestPtr res = make_unique<YellowPagesRequest>();
  res->ParseFromJson(request);
  return res;
}

static BaseInputStruct ReadBaseRequests(istream& is = cin) {
  vector<RequestPtr> updates, queries;
  Json::Document in_document = Json::Load(is);

  const auto& requests = in_document.GetRoot().AsMap();

  const auto& rs_request = requests.at("routing_settings");
  RoutingSettingsRequestPtr routing_settings =
    ParseRoutingSettingsRequest(rs_request);

  const auto& render_request = requests.at("render_settings");
  RenderSettingsRequestPtr render_settings =
    ParseRenderSettingsRequest(render_request);

  const auto& base_requests = requests.at("base_requests").AsArray();
  updates.reserve(base_requests.size());
  for (const auto& request : base_requests) {
    updates.push_back(ParseBaseRequest(request));
  }

  const auto& serialization_request = requests.at("serialization_settings");
  SerializationSettingsRequestPtr serialization_settings =
    ParseSerializationSettingsRequest(serialization_request);

  const auto& yellow_pages_request = requests.at("yellow_pages");
  YellowPagesRequestPtr yellow_pages =
    ParseYellowPagesRequest(yellow_pages_request);

  return {move(routing_settings), move(render_settings), move(updates),
          move(serialization_settings), move(yellow_pages)};
}

static StatInputStruct ReadStatRequests(istream& is = cin) {
  vector<RequestPtr> queries;
  Json::Document in_document = Json::Load(is);
  const auto& requests = in_document.GetRoot().AsMap();
  const auto& stat_requests = requests.at("stat_requests").AsArray();
  queries.reserve(stat_requests.size());
  for (const auto& request : stat_requests) {
    queries.push_back(ParseStatRequest(request));
  }
  const auto& serialization_request = requests.at("serialization_settings");
  SerializationSettingsRequestPtr serialization_settings =
    ParseSerializationSettingsRequest(serialization_request);

  return {move(queries), move(serialization_settings)};
}

static void PrintResponses(const vector<Response>& responses,
                           ostream& os = cout) {
  os << Json::PrintJsonAsString(Json::Node(responses));
}

int main(int argc, const char* argv[]) {
  if (argc < 2 || argc > 3) {
    cerr << "Usage: ./main [make_base|process_requests] <opt. input file>\n";
    return 5;
  }
  const string_view mode(argv[1]);
  if (mode == "make_base") {
    BaseInputStruct input_base;
    if (argc == 3) {
      std::fstream f(argv[2]);
      input_base = ReadBaseRequests(f);
    }
    else {
      input_base = ReadBaseRequests();
    }
    Database db(input_base);
    db.SaveToFile();
  }
  else if (mode == "process_requests") {
    StatInputStruct input_stat;
    if (argc == 3) {
      std::fstream f(argv[2]);
      input_stat = ReadStatRequests(f);
    }
    else {
      input_stat = ReadStatRequests();
    }
    Database db(move(input_stat.serialization_settings));

    // Process stat requests
    auto responses = db.ProcessQueries(input_stat.queries);
    PrintResponses(responses);
  }
  return 0;
}
