#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Cheap all-game evidence used by the staged profile funnel
// -----------------------------------------------------------------------------

enum class ProfileEvidenceLevel {
  metadata_only,
  analyzed_summary,
};

// Stable persisted numbering for the nine-stage player-profile pipeline.
// Stage 3 is the representative historical sample introduced in Update 94;
// its selection algorithm is implemented by the following sampling updates.
enum class ProfilePipelineStage : int {
  initial_sample = 1,
  metadata_and_relevance = 2,
  representative_sample = 3,
  interesting_game = 4,
  position_candidates = 5,
  existing_evidence = 6,
  fast_probe = 7,
  verification = 8,
  deep_analysis = 9,
};

[[nodiscard]] constexpr int profile_pipeline_stage_number(
    const ProfilePipelineStage stage) noexcept {
  return static_cast<int>(stage);
}

inline constexpr int kProfileSamplingMinimumPlies = 16;

struct ProfileGameMetadata {
  std::string game_id;
  std::int64_t played_at{0};
  std::string player_color{"unknown"};
  std::string outcome{"unknown"};
  std::string time_control{"unknown"};
  std::string opening_eco;
  std::string opening_name;
  // Persisted coarse end reason sourced from the same native termination
  // classifier used by Statistics. This is metadata only; Stage 3 must not
  // reopen PGN text merely to recover it.
  std::string termination_type{"unknown"};
  std::optional<int> player_rating;
  std::optional<int> opponent_rating;
  int plies{0};
  // Short/aborted games remain part of library metadata/statistics but are not
  // eligible for expensive historical sampling. Eligibility is centralized in
  // kProfileSamplingMinimumPlies so service/diagnostic code cannot drift.
  bool sample_eligible{true};
  bool has_complete_analysis{false};
  std::optional<double> accuracy;
  int miss_count{0};
  int mistake_count{0};
  int blunder_count{0};
  ProfileEvidenceLevel evidence_level{ProfileEvidenceLevel::metadata_only};
};

// -----------------------------------------------------------------------------
// Section: Stage 1 - fast first impression from a representative sample
// -----------------------------------------------------------------------------

struct InitialSampleEntry {
  std::string game_id;
  double score{0.0};
};

// Selects at most `limit` sampling-eligible games without starting engine
// work. Very short/ineligible games remain metadata but cannot enter expensive
// profile preparation through this Stage-1 shortcut. The greedy selector
// favours recent/informative games while adding diversity bonuses for
// time control, outcome, opening and age bucket so the first profile is not just
// "the latest N games". Passing now_epoch_seconds keeps the domain code pure and
// deterministic for callers/tests.
[[nodiscard]] std::vector<InitialSampleEntry> build_initial_profile_sample(
    const std::vector<ProfileGameMetadata>& games,
    std::int64_t now_epoch_seconds,
    std::size_t limit = 20);

// Broad recency buckets are shared by later relevance stages. Old games are not
// discarded; they simply receive a lower default priority unless other evidence
// promotes them.
[[nodiscard]] int profile_age_bucket(
    std::int64_t played_at,
    std::int64_t now_epoch_seconds);



// -----------------------------------------------------------------------------
// Section: Stage 2 - all-game metadata classification
// -----------------------------------------------------------------------------

struct ProfileMetadataSummary {
  std::size_t total_games{0};
  std::size_t analyzed_games{0};
  std::size_t metadata_only_games{0};
  std::size_t games_with_accuracy{0};
  std::size_t games_with_error_signal{0};
  std::size_t recent_30d{0};
  std::size_t recent_90d{0};
  std::size_t older_than_year{0};
};

// Summarizes the cheap one-row-per-game sweep. This is deliberately metadata
// only: no PGN parsing, no move loading and no engine work.
[[nodiscard]] ProfileMetadataSummary summarize_profile_metadata(
    const std::vector<ProfileGameMetadata>& games,
    std::int64_t now_epoch_seconds);

// -----------------------------------------------------------------------------
// Section: Stage 2 - recency-aware all-game relevance ranking
// -----------------------------------------------------------------------------

struct ProfileRelevanceContext {
  // Optional profile-directed boosts are deliberately supplied by callers so
  // this pure domain layer never reads persistence or invents hypotheses.
  std::vector<std::string> priority_time_controls;
  std::vector<std::string> priority_openings;
  // Knowledge-gap active learning may nominate a bounded set of concrete
  // historical games. This is only a relevance/sampling hint; it never bypasses
  // the Stage-3 historical gate or starts engine work directly.
  std::vector<std::string> priority_game_ids;
  bool prioritize_losses{false};
  bool prioritize_endgame_length{false};
};

struct ProfileGameRelevance {
  std::string game_id;
  double score{0.0};
  int age_bucket{0};
  bool recent{false};
  bool has_error_signal{false};
  bool has_existing_analysis{false};
};

// Ranks every metadata row without starting engine work. Recency establishes
// the default priority, while existing evidence, error density and caller-
// supplied profile interests may promote an older game. No game is discarded
// at this stage; the following interesting-game filter owns that decision.
[[nodiscard]] std::vector<ProfileGameRelevance> rank_profile_games_by_relevance(
    const std::vector<ProfileGameMetadata>& games,
    std::int64_t now_epoch_seconds,
    const ProfileRelevanceContext& context = {});

// -----------------------------------------------------------------------------
// Section: Stage 3 - representative historical sampling strata
// -----------------------------------------------------------------------------

// Stage 3 does not inspect moves or start engine work. It keeps one cheap
// normalized metadata row per eligible game and samples that population through
// a hierarchy rather than a full cross-product of every dimension. This lets
// conditional groups such as "White -> Italian Game -> win -> rapid -> mate"
// receive proportional representation without creating thousands of mandatory
// one-game strata.
struct ProfileSamplingCandidate {
  std::string game_id;
  std::string opening_family;
  std::string player_color;
  std::string outcome;
  std::string time_control;
  std::string termination_type;
  int age_bucket{4};
  std::string ending_phase;
  std::string opponent_strength;

  // Stable diagnostic path through the primary hierarchy. It is metadata only
  // and must never be used as a translated/user-visible label.
  [[nodiscard]] std::string hierarchy_key() const;
};

struct ProfileSamplingPopulation {
  std::size_t total_games{0};
  std::size_t eligible_games{0};
  std::size_t excluded_games{0};
  std::vector<ProfileSamplingCandidate> candidates;
};

// Builds deterministic metadata-only candidates over all eligible games.
// Opening family prefers the persisted Statistics/Games opening name family and
// falls back to ECO when the name is unavailable. No PGN is reparsed and no
// candidate limit is applied here; the adaptive Stage-3 sampler owns selection.
[[nodiscard]] ProfileSamplingPopulation build_profile_sampling_population(
    const std::vector<ProfileGameMetadata>& games,
    std::int64_t now_epoch_seconds);

struct ProfileSamplingPolicy {
  // Initial profile preparation has a fixed floor and hard ceiling, but no
  // fixed target. The target between these bounds is derived from the observed
  // metadata distribution by estimate_profile_sampling_requirement(...).
  std::size_t minimum_games{40};
  std::size_t maximum_games{500};

  // How much of the observed joint metadata population the diversity estimator
  // treats as the representative core. This is a coverage objective, not a
  // percentage of games to analyse.
  double diversity_coverage_target{0.90};

  // Most of the resulting dynamic budget preserves the real population
  // distribution. Only the remainder may be reassigned by bounded relevance /
  // profile priorities.
  double representation_fraction{0.80};

  // Priority may steer the discretionary share but cannot multiply a stratum
  // without bound merely because strong-player games are generally complex.
  double maximum_priority_multiplier{1.50};
};

struct ProfileSamplingRequirement {
  std::size_t total_games{0};
  std::size_t eligible_games{0};
  std::size_t excluded_games{0};
  std::size_t minimum_games{0};
  std::size_t recommended_games{0};
  std::size_t maximum_games{0};
  std::size_t required_strata{0};
  double diversity_score{0.0};
  double covered_population_share{0.0};
  bool capped{false};
};

// Estimates how much expensive historical evidence is needed from the cheap
// Stage-2/3 metadata distribution. Library size alone never increases the
// target: a large homogeneous account may stay near the floor, while broader
// opening/result/time-control/termination structure raises the recommendation.
// The returned recommendation is always bounded by minimum_games/maximum_games
// and by the number of eligible games.
[[nodiscard]] ProfileSamplingRequirement estimate_profile_sampling_requirement(
    const ProfileSamplingPopulation& population,
    const ProfileSamplingPolicy& policy = {});

struct ProfileHistoricalSampleEntry {
  std::string game_id;
  std::string stratum_key;
  double relevance{0.0};
  bool priority_seat{false};
};

struct ProfileHistoricalSample {
  ProfileSamplingRequirement requirement;
  std::size_t covered_strata{0};
  double covered_population_share{0.0};
  std::vector<ProfileHistoricalSampleEntry> games;
};

// Selects the Stage-3 historical sample deterministically. The representative
// share is recursively allocated through color -> opening -> outcome -> time
// control -> termination -> age -> opponent strength -> ending-length context.
// Only the bounded discretionary tail may be steered by Stage-2 relevance and
// Knowledge-Gap priorities, and even that tail is capped per primary hierarchy
// path. The adaptive requirement never exceeds policy.maximum_games (500 by
// default). No PGN/move/engine work occurs here.
[[nodiscard]] ProfileHistoricalSample select_profile_historical_sample(
    const ProfileSamplingPopulation& population,
    const std::vector<ProfileGameRelevance>& ranked,
    const ProfileSamplingPolicy& policy = {});

// -----------------------------------------------------------------------------
// Section: Stage 4 - interesting-game filter
// -----------------------------------------------------------------------------

struct InterestingGamePolicy {
  // The filter remains intentionally conservative: only a bounded share of the
  // library is promoted toward move/position work. Small libraries still keep
  // enough candidates to avoid a brittle profile.
  double minimum_relevance{0.46};
  double target_fraction{0.22};
  std::size_t minimum_candidates{20};
  std::size_t maximum_candidates{600};
};

struct InterestingProfileGame {
  std::string game_id;
  double relevance{0.0};
  int age_bucket{0};
  bool recent{false};
  bool error_signal{false};
  bool accuracy_outlier{false};
  bool existing_analysis{false};
  bool profile_promoted{false};
};

// Filters the all-game relevance ranking into the subset that is worth
// inspecting at move/position granularity. This stage performs no engine work.
// The bounded target is distributed across populated recency bands so a cold
// profile can discover useful older, not-yet-analysed games instead of requiring
// analysis-derived signals before those games are ever eligible for analysis.
// Within each age band, concrete evidence and existing analysis still rank first.
[[nodiscard]] std::vector<InterestingProfileGame> select_interesting_profile_games(
    const std::vector<ProfileGameMetadata>& games,
    const std::vector<ProfileGameRelevance>& ranked,
    const ProfileRelevanceContext& context = {},
    const InterestingGamePolicy& policy = {});


// -----------------------------------------------------------------------------
// Section: Stage 5 - move/position candidate filter
// -----------------------------------------------------------------------------

struct ProfilePositionSignal {
  int ply{0};
  std::string classification;
  std::optional<double> expected_score_loss;
  bool theory{false};
  std::string fen_before;
  std::string uci;
};

struct ProfileCandidatePolicy {
  std::size_t maximum_candidates_per_game{6};
  double minimum_candidate_score{0.28};
  double minimum_expected_score_loss{0.08};
};

struct ProfilePositionCandidate {
  std::string game_id;
  int ply{0};
  double score{0.0};
  std::string reason;
  std::string classification;
  std::optional<double> expected_score_loss;
  std::string fen_before;
  std::string uci;
};

// Converts an already-promoted game into a small set of positions using only
// existing persisted move evidence. No Stockfish call is allowed here.
[[nodiscard]] std::vector<ProfilePositionCandidate> select_profile_position_candidates(
    const InterestingProfileGame& game,
    const std::vector<ProfilePositionSignal>& positions,
    const ProfileCandidatePolicy& policy = {});

// -----------------------------------------------------------------------------
// Section: Stage 6 - existing-evidence filter
// -----------------------------------------------------------------------------

enum class ProfileEvidenceDecisionKind {
  reuse_existing,
  fast_probe,
  discard,
};

struct ProfileEvidenceDecision {
  ProfilePositionCandidate candidate;
  ProfileEvidenceDecisionKind decision{ProfileEvidenceDecisionKind::discard};
  std::string reason;
};

struct ProfileExistingEvidencePolicy {
  double decisive_expected_score_loss{0.16};
  double quiet_expected_score_loss{0.03};
};

// Every candidate must pass this gate before any new engine work. Stable
// existing classification/loss evidence is reused; low-signal positions end
// here; only unresolved positions with a concrete board position are promoted
// to the fast-probe stage.
[[nodiscard]] std::vector<ProfileEvidenceDecision> filter_existing_profile_evidence(
    const std::vector<ProfilePositionCandidate>& candidates,
    const ProfileExistingEvidencePolicy& policy = {});


// -----------------------------------------------------------------------------
// Section: Stage 7 - bounded fast engine probe and early-stop gate
// -----------------------------------------------------------------------------

enum class ProfileProbeTier {
  fast,
  verification,
  deep,
};

struct ProfileProbeBudget {
  ProfileProbeTier tier{ProfileProbeTier::fast};
  std::uint64_t node_limit{12000};
  int hard_depth{4};
  int multi_pv{1};
  int threads{1};
  int hash_mb{1024};
  int time_limit_ms{300};
  bool dynamic_early_stop{true};
  int early_stop_min_depth{3};
  int stable_iterations{2};
  int eval_tolerance_cp{18};
};

struct ProfileProbeRequest {
  ProfilePositionCandidate candidate;
  ProfileProbeBudget budget;
  std::string reason;
};

struct ProfileEscalationPolicy {
  // Hard per-game ceilings. High player strength or general position complexity
  // must never turn the historical sample into an unbounded engine workload.
  std::size_t maximum_fast_probes_per_game{3};
  std::size_t maximum_verification_probes_per_game{2};
  std::size_t maximum_deep_probes_per_game{1};
  double verification_minimum_relevance{0.52};
  double deep_minimum_relevance{0.68};
};

// Produces a tiny, position-targeted Stockfish budget for candidates that
// survived the existing-evidence gate. This function only describes work; the
// service layer executes it through AnalysisService.
[[nodiscard]] std::vector<ProfileProbeRequest> plan_fast_profile_probes(
    const std::vector<ProfileEvidenceDecision>& decisions,
    double game_relevance,
    const ProfileEscalationPolicy& escalation = {});

struct ProfileFastProbeObservation {
  ProfilePositionCandidate candidate;
  std::optional<double> expected_score_loss;
  bool best_move_stable{false};
  bool tactical_signal{false};
  bool mate_signal{false};
  int reached_depth{0};
  std::uint64_t nodes{0};
};

enum class ProfileFastProbeDecisionKind {
  accept_evidence,
  verification_probe,
  discard,
};

struct ProfileFastProbeDecision {
  ProfileFastProbeObservation observation;
  ProfileFastProbeDecisionKind decision{ProfileFastProbeDecisionKind::discard};
  std::string reason;
};

struct ProfileFastProbePolicy {
  // Expected-score loss is expressed in 0..1 expected-score units.
  double discard_below_loss{0.035};
  double verify_from_loss{0.11};
  double direct_accept_from_loss{0.20};
};

// The fast result is itself a filter. Quiet/stable positions stop here; strong
// stable evidence is immediately reusable; only important unresolved signals
// receive a larger verification budget in the next stage.
[[nodiscard]] ProfileFastProbeDecision classify_fast_profile_probe(
    const ProfileFastProbeObservation& observation,
    const ProfileFastProbePolicy& policy = {});

[[nodiscard]] std::optional<ProfileProbeRequest> plan_profile_verification_probe(
    const ProfileFastProbeDecision& decision,
    double game_relevance,
    const ProfileEscalationPolicy& escalation = {});

// -----------------------------------------------------------------------------
// Section: Stage 8/9 - verification result gate and selective deep analysis
// -----------------------------------------------------------------------------

struct ProfileVerificationObservation {
  ProfilePositionCandidate candidate;
  std::optional<double> expected_score_loss;
  bool best_move_stable{false};
  bool tactical_signal{false};
  bool mate_signal{false};
  bool alternative_moves_close{false};
  int reached_depth{0};
  std::uint64_t nodes{0};
};

enum class ProfileVerificationDecisionKind {
  accept_evidence,
  deep_probe,
  discard,
};

struct ProfileVerificationDecision {
  ProfileVerificationObservation observation;
  ProfileVerificationDecisionKind decision{ProfileVerificationDecisionKind::discard};
  std::string reason;
};

struct ProfileVerificationPolicy {
  double discard_below_loss{0.03};
  double deep_from_loss{0.16};
};

// Verification is the final filter before expensive work. Stable quiet results
// stop here; stable material evidence is accepted directly. Deep analysis is
// reserved for still-uncertain, tactical/mating or candidate-sensitive roots.
[[nodiscard]] ProfileVerificationDecision classify_profile_verification(
    const ProfileVerificationObservation& observation,
    const ProfileVerificationPolicy& policy = {});

[[nodiscard]] std::optional<ProfileProbeRequest> plan_deep_profile_probe(
    const ProfileVerificationDecision& decision,
    double game_relevance,
    const ProfileEscalationPolicy& escalation = {});

}  // namespace kchess::ai
