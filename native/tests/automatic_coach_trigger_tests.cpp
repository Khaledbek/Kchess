#include <iostream>
#include <optional>
#include <string>

#include "automatic/automatic_coach_trigger.h"

namespace {

using kchess::ai::AutomaticCoachEvent;
using kchess::ai::AutomaticCoachTrigger;

int assertions = 0;
int failures = 0;

void expect(bool condition, const std::string& message) {
  ++assertions;
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

AutomaticCoachEvent event(std::string classification,
                          std::optional<double> loss = std::nullopt,
                          std::optional<std::int64_t> seconds = std::nullopt) {
  AutomaticCoachEvent value;
  value.classification = std::move(classification);
  value.expected_score_loss = loss;
  value.previous_fen =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  value.current_fen =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  value.seconds_since_last_automatic = seconds;
  return value;
}

void quiet_and_small_inaccuracy_stay_silent() {
  const auto quiet = AutomaticCoachTrigger{}.decide(event("good", 0.01));
  expect(!quiet.trigger, "quiet move must not trigger automatic coach");
  expect(!quiet.primary_reason_present,
         "quiet move must not manufacture a primary reason");

  const auto small_loss = AutomaticCoachTrigger{}.decide(event("okay", 0.08));
  expect(!small_loss.trigger,
         "okay move below tightened inaccuracy threshold must stay silent");

  const auto inaccuracy = AutomaticCoachTrigger{}.decide(event("okay", 0.10));
  expect(inaccuracy.primary_reason_present,
         "material okay loss may become an inaccuracy signal");
  expect(!inaccuracy.trigger,
         "inaccuracy signal alone remains below unsolicited threshold");
}

void recent_ordinary_events_are_blocked() {
  const auto mistake = AutomaticCoachTrigger{}.decide(event("mistake", 0.12, 5));
  expect(mistake.primary_reason_present,
         "mistake remains a primary teaching reason");
  expect(mistake.recency_blocked,
         "ordinary mistake inside quiet window must be recency-blocked");
  expect(!mistake.trigger,
         "ordinary mistake inside quiet window must not interrupt again");
}

void severe_events_can_break_the_quiet_window() {
  const auto blunder = AutomaticCoachTrigger{}.decide(event("blunder", 0.30, 5));
  expect(blunder.critical_override,
         "blunder must qualify for the severe-event override");
  expect(!blunder.recency_blocked,
         "severe override must not be hard-blocked by recency");
  expect(blunder.trigger,
         "severe blunder may interrupt even inside the quiet window");
}

void normal_mistake_returns_after_cooldown() {
  const auto mistake = AutomaticCoachTrigger{}.decide(event("mistake", 0.12, 95));
  expect(!mistake.recency_blocked,
         "mistake after quiet window must not be hard-blocked");
  expect(mistake.trigger,
         "material mistake may trigger again after the cooldown window");
}

void repeated_personal_error_can_raise_relevance_without_spam() {
  auto repeated = event("okay", 0.10, 30);
  repeated.repeated_personal_mistake = true;
  const auto decision = AutomaticCoachTrigger{}.decide(repeated);
  expect(decision.personal_relevance == 1.0,
         "repeated personal error must retain native relevance");
  expect(decision.trigger,
         "repeated personal error may clear the elevated post-interruption gate");
}

}  // namespace

int main() {
  quiet_and_small_inaccuracy_stay_silent();
  recent_ordinary_events_are_blocked();
  severe_events_can_break_the_quiet_window();
  normal_mistake_returns_after_cooldown();
  repeated_personal_error_can_raise_relevance_without_spam();

  if (failures != 0) {
    std::cerr << failures << " of " << assertions
              << " automatic Coach trigger assertions failed\n";
    return 1;
  }
  std::cout << "automatic Coach trigger tests passed (" << assertions
            << " assertions)\n";
  return 0;
}
