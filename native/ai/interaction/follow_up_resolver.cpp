#include "interaction/follow_up_resolver.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace kchess::ai::interaction {
namespace {

std::string normalized(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (const unsigned char ch : input) {
    if (std::isalnum(ch) != 0) out.push_back(static_cast<char>(std::tolower(ch)));
    else if (!out.empty() && out.back() != ' ') out.push_back(' ');
  }
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}

bool equals_any(const std::string& value,
                std::initializer_list<std::string_view> choices) {
  return std::any_of(choices.begin(), choices.end(), [&](std::string_view choice) {
    return value == choice;
  });
}

std::optional<FollowUpAction> first_available(
    const ConversationState& state,
    std::initializer_list<std::string_view> preferred) {
  for (const auto action : preferred) {
    const auto it = std::find_if(state.available_followups.begin(),
                                 state.available_followups.end(),
                                 [&](const FollowUpAction& followup) {
                                   return followup.action_id == action;
                                 });
    if (it != state.available_followups.end()) return *it;
  }
  return std::nullopt;
}

}  // namespace

FollowUpResolution FollowUpResolver::resolve_action_id(
    std::string_view action_id, const ConversationState& state) const {
  const auto it = std::find_if(state.available_followups.begin(),
                               state.available_followups.end(),
                               [&](const FollowUpAction& followup) {
                                 return followup.action_id == action_id;
                               });
  if (it == state.available_followups.end()) {
    return {FollowUpResolutionKind::none, std::nullopt,
            "action_not_available_in_session"};
  }
  return {FollowUpResolutionKind::deterministic, *it,
          "explicit_structured_followup"};
}

FollowUpResolution FollowUpResolver::resolve(
    std::string_view user_text, const ConversationState& state) const {
  if (state.available_followups.empty()) {
    return {FollowUpResolutionKind::none, std::nullopt,
            "no_available_followups"};
  }

  const std::string text = normalized(user_text);
  if (text.empty()) {
    return {FollowUpResolutionKind::none, std::nullopt, "empty_text"};
  }

  // Intentionally conservative. These short utterances are only resolved when
  // the current session exposes an already-valid action. Otherwise the future
  // deterministic semantic planner receives the question instead of us guessing.
  if (equals_any(text, {"zeig", "zeig mal", "zeige es", "show", "show me",
                        "wie", "wie denn", "how"})) {
    const auto action = first_available(
        state, {"show_line", "show_arrows", "show_move", "explain_line"});
    if (action.has_value()) {
      return {FollowUpResolutionKind::deterministic, *action,
              "short_show_or_how_followup"};
    }
    return {FollowUpResolutionKind::ambiguous, std::nullopt,
            "show_or_how_without_matching_action"};
  }

  if (equals_any(text, {"warum", "wieso", "weshalb", "why"})) {
    const auto action = first_available(
        state, {"explain_line", "explain_move", "compare_moves"});
    if (action.has_value()) {
      return {FollowUpResolutionKind::deterministic, *action,
              "short_why_followup"};
    }
    return {FollowUpResolutionKind::ambiguous, std::nullopt,
            "why_without_matching_action"};
  }

  if (equals_any(text, {"und danach", "danach", "weiter", "next", "then"})) {
    const auto action = first_available(
        state, {"show_line", "explain_line", "play_move"});
    if (action.has_value()) {
      return {FollowUpResolutionKind::deterministic, *action,
              "short_next_followup"};
    }
    return {FollowUpResolutionKind::ambiguous, std::nullopt,
            "next_without_matching_action"};
  }

  if (equals_any(text, {"noch einer", "noch eine", "alternative", "another"})) {
    const auto action = first_available(
        state, {"show_candidates", "compare_moves"});
    if (action.has_value()) {
      return {FollowUpResolutionKind::deterministic, *action,
              "short_alternative_followup"};
    }
    return {FollowUpResolutionKind::ambiguous, std::nullopt,
            "alternative_without_matching_action"};
  }

  return {FollowUpResolutionKind::none, std::nullopt,
          "not_a_deterministic_short_followup"};
}

}  // namespace kchess::ai::interaction
