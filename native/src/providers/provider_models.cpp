#include "providers/provider_models.h"

namespace kchess {

std::string provider_type_name(const ProviderType type) {
  switch (type) {
    case ProviderType::chess_com: return "chessCom";
    case ProviderType::lichess: return "lichess";
    case ProviderType::local: return "local";
  }
  return "local";
}

std::string time_control_name(const TimeControlType type) {
  switch (type) {
    case TimeControlType::daily: return "daily";
    case TimeControlType::rapid: return "rapid";
    case TimeControlType::blitz: return "blitz";
    case TimeControlType::bullet: return "bullet";
    case TimeControlType::classical: return "classical";
    case TimeControlType::correspondence: return "correspondence";
    case TimeControlType::unknown: return "unknown";
  }
  return "unknown";
}

std::string provider_outcome_name(const ProviderOutcome outcome) {
  switch (outcome) {
    case ProviderOutcome::win: return "win";
    case ProviderOutcome::loss: return "loss";
    case ProviderOutcome::draw: return "draw";
    case ProviderOutcome::unknown: return "unknown";
  }
  return "unknown";
}

}  // namespace kchess
