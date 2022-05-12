#include "database.h"

void Database::FillCompanies(YellowPagesRequestPtr request) {
  for (const auto& [num, rubric] : request->rubrics_) {
    rubrics_.insert({rubric.name, num});
  }
  companies_ = request->companies_;
}
