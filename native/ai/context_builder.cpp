#include "context_builder.h"

#include <algorithm>
#include <sstream>
#include <string_view>
#include <vector>

#include "chess/pgn.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Token budget helpers
// -----------------------------------------------------------------------------

constexpr std::size_t kCharsPerEstimatedToken = 4;

std::size_t estimated_tokens(std::string_view text) {
  return (text.size() + kCharsPerEstimatedToken - 1) /
         kCharsPerEstimatedToken;
}

std::size_t token_budget(const QueryPlan& plan) {
  if (plan.response_depth == ResponseDepth::concise) return 800;
  if (plan.response_depth == ResponseDepth::detailed) return 2500;
  if (plan.intent == CoachIntent::chess_concept ||
      plan.intent == CoachIntent::chess_rules ||
      plan.intent == CoachIntent::chess_history) {
    return 800;
  }
  return 1500;
}

std::string trim_to_tokens(std::string text, std::size_t tokens) {
  const std::size_t max_chars = tokens * kCharsPerEstimatedToken;
  if (text.size() <= max_chars) return text;
  if (max_chars <= 3) return text.substr(0, max_chars);
  text.resize(max_chars - 3);
  text += "...";
  return text;
}

std::size_t used_tokens(const CoachContext& context) {
  return estimated_tokens(context.user_text) +
         estimated_tokens(context.locale) +
         estimated_tokens(context.position_fen.value_or("")) +
         estimated_tokens(context.session_summary) +
         estimated_tokens(context.pgn_excerpt);
}

// -----------------------------------------------------------------------------
// Section: Compact session context
// -----------------------------------------------------------------------------

void append_field(std::ostringstream& out, std::string_view label,
                  const std::string& value) {
  if (!value.empty()) out << label << ": " << value << '\n';
}

std::string summarize_session(const CoachSessionState* session,
                              std::size_t max_tokens) {
  if (session == nullptr || session->empty() || max_tokens == 0) return {};

  std::ostringstream out;
  append_field(out, "coach_question_waiting_for_learner", session->unresolved_question);
  append_field(out, "question_original_board_not_current_evidence", session->question_board);
  append_field(out, "goal", session->current_goal);
  append_field(out, "last_claim", session->last_claim);
  append_field(out, "last_recommendation", session->last_recommendation);
  append_field(out, "concept", session->referenced_concept);
  return trim_to_tokens(out.str(), max_tokens);
}

// -----------------------------------------------------------------------------
// Section: PGN reduction
// -----------------------------------------------------------------------------

std::string move_token(const kchess::ParsedMove& move, bool first) {
  if (move.side_to_move == "white") {
    return std::to_string(move.move_number) + ". " + move.san;
  }
  if (first) return std::to_string(move.move_number) + "... " + move.san;
  return move.san;
}

std::string compact_headers(const kchess::ParsedGame& game,
                            std::size_t max_tokens) {
  static constexpr std::string_view keys[] = {
      "White", "Black", "Result", "ECO", "Opening"};
  std::ostringstream out;
  for (const auto key : keys) {
    const auto it = game.tags.find(std::string(key));
    if (it != game.tags.end() && !it->second.empty()) {
      if (out.tellp() > 0) out << " | ";
      out << key << '=' << it->second;
    }
  }
  return trim_to_tokens(out.str(), max_tokens);
}

std::string compact_mainline(const kchess::ParsedGame& game,
                             std::size_t max_tokens, bool& truncated) {
  if (game.moves.empty() || max_tokens == 0) return {};

  const std::size_t header_budget = std::min<std::size_t>(max_tokens / 5, 120);
  const std::string headers = compact_headers(game, header_budget);
  const std::size_t header_tokens = estimated_tokens(headers);
  const std::size_t move_budget =
      header_tokens < max_tokens ? max_tokens - header_tokens : 0;
  if (move_budget == 0) return headers;

  const std::size_t max_chars = move_budget * kCharsPerEstimatedToken;
  std::vector<const kchess::ParsedMove*> selected;
  std::size_t chars = 0;
  for (auto it = game.moves.rbegin(); it != game.moves.rend(); ++it) {
    const std::size_t rough = it->san.size() + 8;
    if (!selected.empty() && chars + rough > max_chars) break;
    selected.push_back(&*it);
    chars += rough;
    if (chars >= max_chars) break;
  }
  std::reverse(selected.begin(), selected.end());
  truncated = selected.size() < game.moves.size();

  std::ostringstream moves;
  if (truncated) moves << "... ";
  for (std::size_t index = 0; index < selected.size(); ++index) {
    if (index != 0) moves << ' ';
    moves << move_token(*selected[index], index == 0);
  }

  std::ostringstream out;
  if (!headers.empty()) out << headers << '\n';
  out << trim_to_tokens(moves.str(), move_budget);
  return trim_to_tokens(out.str(), max_tokens);
}

std::string fallback_pgn_excerpt(const std::string& pgn,
                                 std::size_t max_tokens, bool& truncated) {
  const std::size_t max_chars = max_tokens * kCharsPerEstimatedToken;
  if (pgn.size() <= max_chars) return pgn;
  truncated = true;
  if (max_chars <= 4) return pgn.substr(pgn.size() - max_chars);
  return "... " + pgn.substr(pgn.size() - (max_chars - 4));
}

std::string reduce_pgn(const std::string& pgn, std::size_t max_tokens,
                       bool& truncated) {
  if (pgn.empty() || max_tokens == 0) return {};
  const auto parsed = kchess::parse_pgn(pgn);
  if (!parsed.valid) return fallback_pgn_excerpt(pgn, max_tokens, truncated);
  return compact_mainline(parsed.game, max_tokens, truncated);
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Context assembly
// -----------------------------------------------------------------------------

CoachContext ContextBuilder::build(const CoachRequest& request,
                                   const QueryPlan& plan,
                                   const CoachSessionState* session) const {
  CoachContext context;
  context.input_token_budget = token_budget(plan);
  context.locale = trim_to_tokens(request.locale, 16);
  context.user_text = trim_to_tokens(request.user_text, 600);
  if (request.position_fen) {
    context.position_fen = trim_to_tokens(*request.position_fen, 128);
  }

  const std::size_t base_tokens = used_tokens(context);
  const std::size_t available =
      base_tokens < context.input_token_budget
          ? context.input_token_budget - base_tokens
          : 0;
  const std::size_t session_budget = std::min<std::size_t>(available / 3, 240);
  context.session_summary = summarize_session(session, session_budget);

  const std::size_t after_session = used_tokens(context);
  const std::size_t pgn_budget =
      after_session < context.input_token_budget
          ? context.input_token_budget - after_session
          : 0;
  if (request.game_pgn) {
    context.pgn_excerpt =
        reduce_pgn(*request.game_pgn, pgn_budget, context.pgn_truncated);
  }

  context.estimated_tokens = used_tokens(context);
  return context;
}

}  // namespace kchess::ai
