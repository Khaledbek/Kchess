#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "interaction/action_chain_engine.h"
#include "interaction/action_fulfillment_validator.h"
#include "interaction/answer_gate.h"
#include "interaction/interaction_requirement_resolver.h"
#include "interaction/parameter_parser.h"
#include "interaction/primary_interaction_planner.h"

namespace {

using namespace kchess::ai;
using namespace kchess::ai::interaction;

int assertions = 0;
int failures = 0;

void expect(bool condition, const std::string& message) {
  ++assertions;
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

struct PipelineResult {
  PlannerSelection selection;
  ParsedInteractionParameters parameters;
  ResolvedInteractionPlan plan;
  ActionChain chain;
  AnswerGateDecision gate;
};

PipelineResult resolve(std::string_view text, bool with_position = true) {
  ConversationState state;
  if (with_position) state.position_key = "start-position";
  PrimaryInteractionPlanner planner;
  InteractionRequirementResolver resolver;
  ActionChainEngine chain_engine;
  AnswerGate gate;

  PipelineResult result;
  result.selection = planner.classify(text, state);
  result.parameters = parse_interaction_parameters(text);
  result.plan = resolver.resolve(result.selection, result.parameters, state);
  result.chain = chain_engine.build(result.plan, state);
  result.gate = gate.decide(result.plan, result.chain);
  return result;
}

void direct_start_game_is_native_in_all_app_languages() {
  for (const auto& text : {
           std::string{"lass uns einer partie spielen"},
           std::string{"let us start a game"},
           std::string{"لنلعب مباراة"},
       }) {
    const auto result = resolve(text);
    expect(result.selection.request_kind == InteractionRequestKind::app_action,
           text + " must classify as app_action");
    expect(result.selection.contains("start_game"),
           text + " must select start_game");
    expect(result.plan.has_action(PrimitiveAction::start_stockfish_game),
           text + " must resolve to native stockfish game start");
    expect(result.chain.contains(PrimitiveAction::start_stockfish_game),
           text + " must keep the game action in the ordered chain");
    expect(!result.gate.answer_llm_required,
           text + " must not call the answer LLM for an action-only turn");
  }
}

void negative_language_examples_do_not_start_games() {
  for (const auto& text : {
           std::string{"Warum sollte ich mit e4 anfangen?"},
           std::string{"What do you think about my last game?"},
           std::string{"لماذا أبدأ بـ e4؟"},
       }) {
    const auto result = resolve(text);
    expect(!result.selection.contains("start_game"),
           text + " must not select start_game");
    expect(result.selection.request_kind != InteractionRequestKind::app_action,
           text + " must not classify as app_action");
    expect(!result.chain.contains(PrimitiveAction::start_stockfish_game) &&
               !result.chain.contains(PrimitiveAction::start_human_bot),
           text + " must not build a game-start action");
  }
}

void mixed_action_and_coaching_keeps_both_contracts() {
  const auto result = resolve("Start a game and coach me while we play.");
  expect(result.selection.request_kind == InteractionRequestKind::app_action,
         "mixed start+coach remains app_action");
  expect(result.selection.contains("start_game"),
         "mixed start+coach selects start_game");
  expect(result.selection.contains("live_coaching"),
         "mixed start+coach selects live_coaching");
  expect(result.plan.has_action(PrimitiveAction::start_stockfish_game),
         "mixed start+coach starts the game natively");
  expect(result.plan.has_action(PrimitiveAction::enable_live_coaching),
         "mixed start+coach enables live coaching natively");
  expect(result.gate.answer_llm_required,
         "mixed start+coach still requests bounded language acknowledgement");
}

void explicit_game_elo_is_an_action_parameter_not_prediction() {
  const auto result = resolve("Start a game at 1800 Elo.");
  expect(result.selection.contains("start_game"),
         "explicit game Elo still selects start_game");
  expect(!result.selection.contains("predict"),
         "explicit game Elo must not become rating prediction");
  expect(result.parameters.elo == std::optional<int>{1800},
         "explicit game Elo is parsed deterministically");
  expect(result.plan.has_action(PrimitiveAction::start_human_bot),
         "explicit game Elo selects the human-strength bot path");
  expect(result.plan.has_action(PrimitiveAction::change_rating),
         "explicit game Elo transports the requested rating");
  expect(!result.gate.answer_llm_required,
         "explicit game Elo remains an action-only request");
}

void german_rating_class_and_live_coaching_are_compositional() {
  const auto result = resolve(
      "Spiel gegen mich wie ein 1800er und gib mir während der Partie Tipps.");
  expect(result.parameters.elo == std::optional<int>{1800},
         "German 1800er game wording parses the rating");
  expect(result.selection.contains("human_style"),
         "German human-style game wording selects human_style");
  expect(result.plan.has_action(PrimitiveAction::start_human_bot),
         "German 1800er game starts human-strength bot");
  expect(result.plan.has_action(PrimitiveAction::enable_live_coaching),
         "German mixed request enables live coaching");
  expect(result.gate.answer_llm_required,
         "German mixed request keeps language acknowledgement");
}

void explicit_black_color_reaches_fulfillment_contract() {
  const auto result = resolve("I want to play black.");
  expect(result.selection.contains("start_game"),
         "black-color request selects start_game");
  expect(result.parameters.color == std::optional<RequestedColor>{RequestedColor::black},
         "black-color request parses color");
  expect(result.plan.color == std::optional<RequestedColor>{RequestedColor::black},
         "black-color request carries color into resolved plan");

  CoachResponse good_response;
  good_response.client_actions.push_back(
      {.id = "open_bot_game", .elo = std::nullopt, .color = std::string{"black"}});
  ActionFulfillmentValidator validator;
  const auto good = validator.validate(result.plan, result.chain, good_response);
  expect(good.required && good.passed,
         "matching black client action fulfills native game request");

  CoachResponse wrong_response;
  wrong_response.client_actions.push_back(
      {.id = "open_bot_game", .elo = std::nullopt, .color = std::string{"white"}});
  const auto wrong = validator.validate(result.plan, result.chain, wrong_response);
  expect(wrong.required && !wrong.passed,
         "mismatched client color fails action fulfillment");
}

}  // namespace

int main() {
  direct_start_game_is_native_in_all_app_languages();
  negative_language_examples_do_not_start_games();
  mixed_action_and_coaching_keeps_both_contracts();
  explicit_game_elo_is_an_action_parameter_not_prediction();
  german_rating_class_and_live_coaching_are_compositional();
  explicit_black_color_reaches_fulfillment_contract();

  if (failures != 0) {
    std::cerr << failures << " of " << assertions
              << " interaction-action assertions failed\n";
    return 1;
  }
  std::cout << "interaction action routing tests passed (" << assertions
            << " assertions)\n";
  return 0;
}
