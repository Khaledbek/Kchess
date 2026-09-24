#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace kchess::ai::interaction {

enum class RequestedColor {
  white,
  black,
};

enum class MoveReference {
  last_move,
  selected_move,
  current_move,
};

enum class RequestedGameMode {
  play,
  analysis,
  training,
};

enum class RequestedTimeControl {
  bullet,
  blitz,
  rapid,
  classical,
};

struct ParsedInteractionParameters {
  std::optional<int> elo;
  std::optional<int> move_number;
  std::optional<RequestedColor> color;
  std::optional<MoveReference> move_reference;
  std::optional<RequestedGameMode> game_mode;
  std::optional<RequestedTimeControl> time_control;

  [[nodiscard]] bool empty() const noexcept;
};

// Cheap deterministic extraction for explicit parameters in the current user
// utterance. This parser intentionally does not infer intent or chess meaning;
// it only captures values that are safe to derive directly from text.
[[nodiscard]] ParsedInteractionParameters parse_interaction_parameters(
    std::string_view user_text);

[[nodiscard]] std::string_view to_string(RequestedColor value) noexcept;
[[nodiscard]] std::string_view to_string(MoveReference value) noexcept;
[[nodiscard]] std::string_view to_string(RequestedGameMode value) noexcept;
[[nodiscard]] std::string_view to_string(RequestedTimeControl value) noexcept;

}  // namespace kchess::ai::interaction
