// -----------------------------------------------------------------------------
// Section: Semantic native identifiers mapped to ARB presentation strings
// -----------------------------------------------------------------------------
import '../../../localization/generated/app_localizations.dart';

String drillTitle(AppLocalizations s, String id) => switch (id) {
  'endgame_kq_vs_k' => s.practiceQueenTitle,
  'endgame_kr_vs_k' => s.practiceRookTitle,
  'endgame_q_vs_r' => s.practiceQueenRookTitle,
  _ => s.practiceDrills,
};
String drillHint(AppLocalizations s, String id) => switch (id) {
  'endgame_kq_vs_k' => s.practiceQueenHint,
  'endgame_kr_vs_k' => s.practiceRookHint,
  'endgame_q_vs_r' => s.practiceQueenRookHint,
  _ => s.trainingYourMove,
};
String practiceDifficulty(AppLocalizations s, String id) => switch (id) {
  'beginner' => s.practiceBeginner,
  'master' => s.practiceMaster,
  _ => s.practiceIntermediate,
};
String studySectionTitle(AppLocalizations s, String id) => switch (id) {
  'king_and_pawn' => s.practiceSectionKingPawn,
  'kings_bishops_and_pawns' => s.practiceSectionBishops,
  'knights_bishops_and_pawns' => s.practiceSectionKnightsBishops,
  'two_minor_pieces_against_one' => s.practiceSectionTwoMinor,
  'rook_against_pawns' => s.practiceSectionRookPawns,
  'rook_against_minor_pieces' => s.practiceSectionRookMinor,
  'minor_pieces_against_rook' => s.practiceSectionMinorRook,
  'queen_against_pawns' => s.practiceSectionQueenPawns,
  'queens_and_pawns' => s.practiceSectionQueens,
  'queen_against_rook' => s.practiceSectionQueenRook,
  'queen_against_minor_pieces' => s.practiceSectionQueenMinor,
  _ => s.practiceStudies,
};
