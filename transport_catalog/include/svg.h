#pragma once

#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "json.h"

namespace Svg {

struct Point {
  double x, y;

  Point() : x(0), y(0) {}
  Point(double x_, double y_) : x(x_), y(y_) {}
  explicit Point(const Json::Node& node) {
    const auto& array = node.AsArray();
    if (array.size() != 2)
      throw std::invalid_argument("Point is parsed from bad array");
    x = array[0].AsDouble();
    y = array[1].AsDouble();
  }
};

struct Rgb {
  int red, green, blue;

  Rgb() = default;
  Rgb(int r, int g, int b) : red(r), green(g), blue(b) {}
  std::string AsString() const {
    std::ostringstream os;
    os << "rgb(" << red << "," << green << "," << blue << ")";
    return os.str();
  }
};

struct Rgba {
  Rgb color;
  double alpha;
  Rgba() = default;
  Rgba(int r, int g, int b, double a) : color(r, g, b), alpha(a) {}
  Rgba(Rgb rgb, double a) : color(rgb), alpha(a) {}
  std::string AsString() const {
    std::ostringstream os;
    os << "rgba(" << color.red << "," << color.green << ","
       << color.blue << "," << alpha << ")";
    return os.str();
  }
};

struct Color {
  std::optional<std::variant<std::string, Rgb, Rgba>> color;

  Color() { color = std::nullopt; }
  explicit Color(std::string s) : color(s) {}
  explicit Color(const char *s) : color(std::string(s)) {}
  explicit Color(Rgb rgb) : color(rgb) {}
  explicit Color(Rgba rgba) : color(rgba) {}
  template <typename T>
  bool IsA() {
    return std::holds_alternative<T>(*color);
  }
  template <typename T>
  T& AsA() {
    return std::get<T>(*color);
  }
  std::string GetColorAsString() const {
    if (!color) {
      return "none";
    }
    if (std::holds_alternative<std::string>(*color)) {
      return std::get<std::string>(*color);
    }
    else if (std::holds_alternative<Rgb>(*color)) {
      return std::get<Rgb>(*color).AsString();
    }
    else {
      return std::get<Rgba>(*color).AsString();
    }
  }
  explicit Color(const Json::Node& node) {
    if (std::holds_alternative<std::string>(node)) {
      color = node.AsString();
    }
    else {
      const auto& array = node.AsArray();
      if (array.size() == 3) {
        color = Rgb(array[0].AsInt(), array[1].AsInt(), array[2].AsInt());
      }
      else if (array.size() == 4) {
        color = Rgba(array[0].AsInt(), array[1].AsInt(), array[2].AsInt(),
                     array[3].AsDouble());
      }
      else {
        throw std::invalid_argument("Color is parsed from bad array");
      }
    }
  }
};

const Color NoneColor;

class IObject {
 public:
  virtual void Render(std::ostream& os) const = 0;
  virtual ~IObject() = default;
};

template <typename T>
class Object : public IObject {
 public:
  T& SetFillColor(const Color& color) {
    fill_color_ = color;
    return static_cast<T&>(*this);
  }

  T& SetStrokeColor(const Color& color) {
    stroke_color_ = color;
    return static_cast<T&>(*this);
  }

  T& SetStrokeWidth(double w) {
    stroke_width_ = w;
    return static_cast<T&>(*this);
  }

  T& SetStrokeLineCap(const std::string& s) {
    stroke_line_cap_ = s;
    return static_cast<T&>(*this);
  }

  T& SetStrokeLineJoin(const std::string& s) {
    stroke_line_join_ = s;
    return static_cast<T&>(*this);
  }

  void Render(std::ostream& os) const override = 0;
  void RenderCommonOptions(std::ostream& os) const {
    os << "fill=\"" << fill_color_.GetColorAsString() << "\" ";
    os << "stroke=\"" << stroke_color_.GetColorAsString() << "\" ";
    os << "stroke-width=\"" << stroke_width_ << "\" ";
    if (stroke_line_cap_) {
      os << "stroke-linecap=\"" << *stroke_line_cap_ << "\" ";
    }
    if (stroke_line_join_) {
      os << "stroke-linejoin=\"" << *stroke_line_join_ << "\" ";
    }
  }

 private:
  Color fill_color_ = NoneColor;
  Color stroke_color_ = NoneColor;
  double stroke_width_ = 1.0;
  std::optional<std::string> stroke_line_cap_ = std::nullopt;
  std::optional<std::string> stroke_line_join_ = std::nullopt;
};

class Circle : public Object<Circle> {
 public:
  Circle() {}
  Circle(const Circle& other) = default;
  Circle(Circle&& other) = default;

  Circle& SetCenter(Point p) {
    center_ = p;
    return *this;
  }

  Circle& SetRadius(double r) {
    radius_ = r;
    return *this;
  }

  void Render(std::ostream& os = std::cout) const override {
    os << "<circle cx=\"" << center_.x << "\" cy=\"" << center_.y
       << "\" r=\"" << radius_ << "\" ";
    RenderCommonOptions(os);
    os << "/>";
  }

 private:
  Point center_ = {0, 0};
  double radius_ = 1.0;
};

class Polyline : public Object<Polyline> {
 public:
  Polyline() {}
  Polyline(const Polyline& other) = default;
  Polyline(Polyline&& other) = default;

  Polyline& AddPoint(Point p) {
    points_.push_back(p);
    return *this;
  }

  void Render(std::ostream& os = std::cout) const override {
    os << "<polyline points=\"";
    for (const auto& p : points_) {
      os << p.x << "," << p.y << " ";
    }
    os << "\" ";
    RenderCommonOptions(os);
    os << "/>";
  }

 private:
  std::vector<Point> points_;
};

class Rectangle : public Object<Rectangle> {
 public:
  Rectangle() {}
  Rectangle(const Rectangle& other) = default;
  Rectangle(Rectangle&& other) = default;
  Rectangle& operator=(const Rectangle& other) = default;
  Rectangle& operator=(Rectangle&& other) = default;
  Rectangle& SetPoint(Point p) {
    point_ = p;
    return *this;
  }
  Rectangle& SetWidth(double w) {
    w_ = w;
    return *this;
  }
  Rectangle& SetHeight(double h) {
    h_ = h;
    return *this;
  }
  void Render(std::ostream& os = std::cout) const override {
    os << "<rect x=\"" << point_.x << "\" y=\"" << point_.y
       << "\" width=\"" << w_ << "\" height=\"" << h_ << "\" ";
    RenderCommonOptions(os);
    os << "/>";
  }

 private:
  Point point_;
  double w_, h_;
};

class Document {
 public:
  Document() {}

  template <typename T>
  // use move(obj)
  void Add(T obj) {
    objects_.push_back(std::make_shared<T>(obj));
  }

  void Render(std::ostream& os = std::cout) const {
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
       << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">";
    for (const auto& obj : objects_) {
      obj->Render(os);
    }
    os << "</svg>";
  }

 private:
  std::vector<std::shared_ptr<IObject>> objects_;
};



class Text : public Object<Text> {
 public:
  Text() {}
  Text(const Text& other) = default;
  Text(Text&& other) = default;
  Text& operator=(const Text& other) = default;
  Text& operator=(Text&& other) = default;

  Text& SetPoint(Point p) {
    point_ = p;
    return *this;
  }

  Text& SetOffset(Point p) {
    offset_ = p;
    return *this;
  }

  Text& SetFontSize(uint32_t sz) {
    font_size_ = sz;
    return *this;
  }

  Text& SetFontFamily(const std::string& s) {
    font_family_ = s;
    return *this;
  }

  Text& SetFontWeight(const std::string& s) {
    font_weight_ = s;
    return *this;
  }

  Text& SetData(const std::string& s) {
    data_ = s;
    return *this;
  }
  std::string GetData() const {
    return data_;
  }

  void Render(std::ostream& os = std::cout) const override {
    os << "<text ";
    os << "x=\"" << point_.x << "\" y=\"" << point_.y << "\" ";
    os << "dx=\"" << offset_.x << "\" dy=\"" << offset_.y << "\" ";
    os << "font-size=\"" << font_size_ << "\" ";
    if (font_family_) {
      os << "font-family=\"" << *font_family_ << "\" ";
    }
    if (font_weight_) {
      os << "font-weight=\"" << *font_weight_ << "\" ";
    }
    RenderCommonOptions(os);
    os << ">";
    os << data_;
    os << "</text>";
  }

 private:
  Point point_ = {0, 0};
  Point offset_ = {0, 0};
  uint32_t font_size_ = 1;
  std::optional<std::string> font_family_ = std::nullopt;
  std::optional<std::string> font_weight_ = std::nullopt;
  std::string data_ = "";
};

};  // namespace Svg
