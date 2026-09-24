#include "interaction/action_fulfillment_validator.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

namespace kchess::ai::interaction {
namespace {

void add_unique(std::vector<std::string>& values, std::string value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(std::move(value));
  }
}

const CoachClientAction* find_client_action(const CoachResponse& response,
                                            std::string_view id) {
  const auto it = std::find_if(
      response.client_actions.begin(), response.client_actions.end(),
      [id](const CoachClientAction& action) { return action.id == id; });
  return it == response.client_actions.end() ? nullptr : &*it;
}

void require_transport(ActionFulfillmentReport& report,
                       const CoachResponse& response,
                       std::string_view requested_action,
                       std::string_view client_action) {
  report.required = true;
  add_unique(report.requested_actions, std::string{requested_action});
  if (find_client_action(response, client_action) == nullptr) {
    report.passed = false;
    report.issues.push_back("missing_client_action:" +
                            std::string{client_action});
  }
}

}  // namespace

ActionFulfillmentReport ActionFulfillmentValidator::validate(
    const ResolvedInteractionPlan& plan,
    const ActionChain& chain,
    const CoachResponse& response) const {
  ActionFulfillmentReport report;
  for (const auto& action : response.client_actions) {
    if (!action.id.empty()) add_unique(report.transported_actions, action.id);
  }

  // Validate the ordered action chain rather than free-form semantic tokens.
  // Only actions with a concrete Flutter/client transport belong here. Native
  // analysis, board overlays and language actions are fulfilled by other
  // pipeline stages and must not be confused with product-action transport.
  if (chain.contains(PrimitiveAction::start_human_bot)) {
    require_transport(report, response, "start_human_bot", "open_bot_game");
    if (const auto* client = find_client_action(response, "open_bot_game")) {
      if (plan.elo.has_value() && client->elo != plan.elo) {
        report.passed = false;
        report.issues.emplace_back("client_action_parameter_mismatch:elo");
      }
      if (plan.color.has_value()) {
        const std::string expected_color{to_string(*plan.color)};
        if (client->color != std::optional<std::string>{expected_color}) {
          report.passed = false;
          report.issues.emplace_back("client_action_parameter_mismatch:color");
        }
      }
    }
  } else if (chain.contains(PrimitiveAction::start_stockfish_game)) {
    if (plan.elo.has_value() || plan.color.has_value()) {
      require_transport(report, response, "start_stockfish_game",
                        "open_bot_game");
      if (const auto* client = find_client_action(response, "open_bot_game")) {
        if (plan.elo.has_value() && client->elo != plan.elo) {
          report.passed = false;
          report.issues.emplace_back("client_action_parameter_mismatch:elo");
        }
        if (plan.color.has_value()) {
          const std::string expected_color{to_string(*plan.color)};
          if (client->color != std::optional<std::string>{expected_color}) {
            report.passed = false;
            report.issues.emplace_back("client_action_parameter_mismatch:color");
          }
        }
      }
    } else {
      require_transport(report, response, "start_stockfish_game",
                        "open_bot_game_setup");
    }
  }

  if (chain.contains(PrimitiveAction::resume_bot_game)) {
    require_transport(report, response, "resume_bot_game", "resume_bot_game");
  }
  if (chain.contains(PrimitiveAction::resign_active_bot_game)) {
    require_transport(report, response, "resign_active_bot_game",
                      "resign_active_bot_game");
  }

  // change_rating is folded into the typed start-game client action. A rating
  // mutation without an Elo parameter is never considered fulfilled.
  if (chain.contains(PrimitiveAction::change_rating)) {
    report.required = true;
    add_unique(report.requested_actions, "change_rating");
    const auto* client = find_client_action(response, "open_bot_game");
    if (!plan.elo.has_value()) {
      report.passed = false;
      report.issues.emplace_back("missing_action_parameter:elo");
    } else if (client == nullptr || client->elo != plan.elo) {
      report.passed = false;
      report.issues.emplace_back("change_rating_not_transported");
    }
  }

  return report;
}

}  // namespace kchess::ai::interaction
