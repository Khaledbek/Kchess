#include "plan_generator.h"

#include <algorithm>
#include <utility>

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Candidate construction
// -----------------------------------------------------------------------------

void add_plan(std::vector<StrategicPlan>& plans, StrategicPlanTheme theme,
              double priority, double confidence, int target_square = -1,
              std::vector<ExploitationMethod> methods = {}) {
  plans.push_back({
      .theme = theme,
      .target_square = target_square,
      .priority = std::clamp(priority, 0.0, 1.0),
      .confidence = std::clamp(confidence, 0.0, 1.0),
      .methods = std::move(methods),
  });
}

bool has_weakness(const SideWeaknesses& side, WeaknessKind kind) {
  return std::any_of(side.items.begin(), side.items.end(),
                     [kind](const PositionWeakness& item) {
                       return item.kind == kind;
                     });
}

std::vector<StrategicPlan> plans_for_side(
    const SidePositionFeatures& side, const SidePositionFeatures& opponent,
    const SideWeaknesses& own_weaknesses,
    const std::vector<ExploitationPlan>& exploitation) {
  std::vector<StrategicPlan> plans;

  const int development_gap =
      opponent.minor_pieces_off_home - side.minor_pieces_off_home;
  if (side.minor_pieces_off_home < 2 || development_gap >= 1) {
    add_plan(plans, StrategicPlanTheme::improve_development,
             0.58 + 0.10 * std::max(0, development_gap), 0.90);
  }

  if (!side.strategic.bad_piece_candidates.empty()) {
    add_plan(plans, StrategicPlanTheme::improve_worst_piece, 0.64, 0.82,
             side.strategic.bad_piece_candidates.front());
  }

  const int activity_gap = opponent.activity_squares - side.activity_squares;
  if (activity_gap >= 4) {
    add_plan(plans, StrategicPlanTheme::improve_activity,
             0.50 + 0.02 * activity_gap, 0.78);
  }

  const int space_gap = opponent.space_squares - side.space_squares;
  if (space_gap >= 5) {
    add_plan(plans, StrategicPlanTheme::gain_space,
             0.48 + 0.025 * space_gap, 0.76);
  }

  if (has_weakness(own_weaknesses, WeaknessKind::weak_king)) {
    add_plan(plans, StrategicPlanTheme::secure_king, 0.78, 0.86,
             side.strategic.king.king_square);
  }

  if (!side.strategic.outpost_candidates.empty()) {
    add_plan(plans, StrategicPlanTheme::occupy_outpost, 0.55, 0.72,
             side.strategic.outpost_candidates.front());
  }

  if (!side.strategic.pawns.passed_pawn_squares.empty()) {
    add_plan(plans, StrategicPlanTheme::support_passed_pawn, 0.62, 0.84,
             side.strategic.pawns.passed_pawn_squares.front());
  }

  const std::size_t limit = std::min<std::size_t>(2, exploitation.size());
  for (std::size_t i = 0; i < limit; ++i) {
    const auto& exploit = exploitation[i];
    add_plan(plans, StrategicPlanTheme::pressure_weakness,
             exploit.priority, exploit.confidence, exploit.target_square,
             exploit.methods);
  }

  std::stable_sort(plans.begin(), plans.end(), [](const auto& a, const auto& b) {
    return a.priority > b.priority;
  });
  if (plans.size() > 4) plans.resize(4);
  return plans;
}

// -----------------------------------------------------------------------------
// Section: Evidence serialization
// -----------------------------------------------------------------------------

const char* theme_name(StrategicPlanTheme theme) {
  switch (theme) {
    case StrategicPlanTheme::improve_development: return "improve_development";
    case StrategicPlanTheme::improve_worst_piece: return "improve_worst_piece";
    case StrategicPlanTheme::improve_activity: return "improve_activity";
    case StrategicPlanTheme::gain_space: return "gain_space";
    case StrategicPlanTheme::secure_king: return "secure_king";
    case StrategicPlanTheme::pressure_weakness: return "pressure_weakness";
    case StrategicPlanTheme::occupy_outpost: return "occupy_outpost";
    case StrategicPlanTheme::support_passed_pawn: return "support_passed_pawn";
  }
  return "unknown";
}

const char* method_name(ExploitationMethod method) {
  switch (method) {
    case ExploitationMethod::open_lines: return "open_lines";
    case ExploitationMethod::add_attackers: return "add_attackers";
    case ExploitationMethod::pressure_target: return "pressure_target";
    case ExploitationMethod::fix_target: return "fix_target";
    case ExploitationMethod::blockade: return "blockade";
    case ExploitationMethod::occupy_square: return "occupy_square";
    case ExploitationMethod::support_occupant: return "support_occupant";
    case ExploitationMethod::invade_color_complex: return "invade_color_complex";
    case ExploitationMethod::exchange_key_defender: return "exchange_key_defender";
    case ExploitationMethod::create_second_threat: return "create_second_threat";
    case ExploitationMethod::restrict_piece: return "restrict_piece";
    case ExploitationMethod::deny_improvement: return "deny_improvement";
    case ExploitationMethod::create_back_rank_threat: return "create_back_rank_threat";
    case ExploitationMethod::deflect_defender: return "deflect_defender";
    case ExploitationMethod::gain_space: return "gain_space";
    case ExploitationMethod::open_position: return "open_position";
    case ExploitationMethod::create_forcing_play: return "create_forcing_play";
    case ExploitationMethod::prevent_development: return "prevent_development";
  }
  return "unknown";
}

nlohmann::json plans_json(const std::vector<StrategicPlan>& plans) {
  nlohmann::json out = nlohmann::json::array();
  for (const auto& plan : plans) {
    nlohmann::json item{
        {"theme", theme_name(plan.theme)},
        {"priority", plan.priority},
        {"confidence", plan.confidence},
    };
    if (plan.target_square >= 0) item["target_square"] = plan.target_square;
    if (!plan.methods.empty()) {
      item["methods"] = nlohmann::json::array();
      for (const auto method : plan.methods) {
        item["methods"].push_back(method_name(method));
      }
    }
    out.push_back(std::move(item));
  }
  return out;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public generation
// -----------------------------------------------------------------------------

PositionPlans PlanGenerator::generate(
    const PositionFeatures& features, const PositionWeaknesses& weaknesses,
    const PositionExploitationPlans& exploitation) const {
  return {
      .by_white = plans_for_side(features.white, features.black,
                                 weaknesses.white, exploitation.by_white),
      .by_black = plans_for_side(features.black, features.white,
                                 weaknesses.black, exploitation.by_black),
  };
}

EvidenceItem PlanGenerator::evidence(const PositionPlans& plans) const {
  const nlohmann::json payload{
      {"version", 1},
      {"by_white", plans_json(plans.by_white)},
      {"by_black", plans_json(plans.by_black)},
  };
  return {
      .id = "position.plans.v1",
      .kind = EvidenceKind::strategic_plans,
      .payload = payload.dump(),
      .confidence = 0.78,
  };
}

}  // namespace kchess::ai
