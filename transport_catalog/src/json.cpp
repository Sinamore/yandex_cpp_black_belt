#include "json.h"

#include <iomanip>
#include <iostream>
#include <variant>

using namespace std;

namespace Json {

  static std::string Pretty() {
    return "\n";
  }

  Document::Document(Node root) : root(move(root)) {
  }

  const Node& Document::GetRoot() const {
    return root;
  }

  static Node LoadNode(istream& input);

  static Node LoadArray(istream& input) {
    vector<Node> result;

    for (char c; input >> c && c != ']'; ) {
      if (c != ',') {
        input.putback(c);
      }
      result.push_back(LoadNode(input));
    }

    return Node(move(result));
  }

  static Node LoadIntOrDouble(istream& input) {
    int result = 0;
    int sign = 1;
    if (input.peek() == '-') {
      sign = -1;
      input.get();
    }
    while (isdigit(input.peek())) {
      result *= 10;
      result += input.get() - '0';
    }
    if (input.peek() == '.') {
      input.get();
      double p = 1;
      double result_d = result;
      while (isdigit(input.peek())) {
        p /= 10;
        result_d += ((input.get() - '0') * p);
      }
      return Node(result_d * sign);
    }
    else {
      // May be check if result is good enough for int?
      return Node(result * sign);
    }
  }

  static Node LoadString(istream& input) {
    string line;
    getline(input, line, '"');
    return Node(move(line));
  }

  static Node LoadDict(istream& input) {
    map<string, Node> result;

    for (char c; input >> c && c != '}'; ) {
      if (c == ',') {
        input >> c;
      }

      string key = LoadString(input).AsString();
      input >> c;
      result.emplace(move(key), LoadNode(input));
    }

    return Node(move(result));
  }

  // This isn't a robust check for boolean value, it checks for first char only
  static Node LoadBool(istream& input) {
    char c;
    bool value = false;
    input >> c;
    if (c == 't') {
      value = true;
      // Just eat 3 more symbols
      input >> c >> c >> c;
    }
    else {
      input >> c >> c >> c >> c;
    }
    return Node(value);
  }

  static Node LoadNode(istream& input) {
    char c;
    input >> c;

    if (c == '[') {
      return LoadArray(input);
    } else if (c == '{') {
      return LoadDict(input);
    } else if (c == '"') {
      return LoadString(input);
    } else {
      input.putback(c);
      if (c == 't' || c == 'f') {
        return LoadBool(input);
      }
      return LoadIntOrDouble(input);
    }
  }

  Document Load(istream& input) {
    return Document{LoadNode(input)};
  }

  string PrintJsonAsString(const Node& node) {
    ostringstream os;
    if (holds_alternative<vector<Node>>(node)) {
      os << "[" << Pretty();
      const auto& node_vector = node.AsArray();
      bool first = true;
      for (const auto& elem : node_vector) {
        if (!first) {
          os << "," << Pretty();
        }
        first = false;
        os << PrintJsonAsString(elem);
      }
      os << Pretty() << "]";
    }
    else if (holds_alternative<map<string, Node>>(node)) {
      os << "{" << Pretty();
      const auto& node_map = node.AsMap();
      bool first = true;
      for (const auto& [k, v] : node_map) {
        if (!first) {
          os << "," << Pretty();
        }
        first = false;
        os << "\"" << k << "\": " << PrintJsonAsString(v);
      }
      os << Pretty() << "}";
    }
    else if (holds_alternative<string>(node)) {
      const auto& node_string = node.AsString();
      os << "\"" << node_string << "\"";
    }
    else if (holds_alternative<int>(node)) {
      const auto node_int = node.AsInt();
      os << node_int;
    }
    else if (holds_alternative<double>(node)) {
      const auto node_double = node.AsDouble();
      os << setprecision(6) << node_double;
    }
    return os.str();
  }
}  // namespace Json
