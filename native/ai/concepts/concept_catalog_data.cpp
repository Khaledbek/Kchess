#include "concept_catalog_data.h"

namespace kchess::ai::concepts_internal {

// -----------------------------------------------------------------------------
// Section: Canonical chess concept identifiers
// -----------------------------------------------------------------------------

const std::vector<ChessConcept>& catalog_data() {
  static const std::vector<ChessConcept> kConcepts{
      {"fork", ConceptCategory::tactics, "", {"double attack", "gabel"}},
      {"pin", ConceptCategory::tactics, "", {"absolute pin", "relative pin", "fesselung"}},
      {"skewer", ConceptCategory::tactics, "", {"spieß", "spiess"}},
      {"discovered_attack", ConceptCategory::tactics, "", {"discovered attack"}},
      {"deflection", ConceptCategory::tactics, "", {"deflect defender", "ablenkung"}},
      {"decoy", ConceptCategory::tactics, "", {"attraction", "hinlenkung"}},
      {"clearance", ConceptCategory::tactics, "", {"clearance sacrifice", "räumungsopfer", "raeumungsopfer"}},
      {"removal_of_defender", ConceptCategory::tactics, "", {"remove defender"}},
      {"overloading", ConceptCategory::tactics, "", {"overloaded defender", "überlastung", "ueberlastung"}},
      {"interference", ConceptCategory::tactics, "", {}},
      {"zwischenzug", ConceptCategory::tactics, "", {"intermezzo", "in-between move"}},
      {"x_ray", ConceptCategory::tactics, "", {"x-ray"}},
      {"back_rank", ConceptCategory::tactics, "", {"back rank mate"}},
      {"greek_gift", ConceptCategory::tactics, "", {"greek gift sacrifice", "Bxh7+"}},
      {"smothered_mate", ConceptCategory::tactics, "", {"smothered mate"}},

      {"initiative", ConceptCategory::strategy, "", {}},
      {"prophylaxis", ConceptCategory::strategy, "", {"prophylactic play"}},
      {"space", ConceptCategory::strategy, "", {"space advantage"}},
      {"weak_square", ConceptCategory::strategy, "", {"weak square", "schwaches feld"}},
      {"outpost", ConceptCategory::strategy, "weak_square", {"outpost square", "vorposten"}},
      {"open_file", ConceptCategory::strategy, "", {"open file"}},
      {"semi_open_file", ConceptCategory::strategy, "", {"semi-open file"}},
      {"color_complex", ConceptCategory::strategy, "", {"colour complex"}},

      {"isolated_pawn", ConceptCategory::pawn_structures, "", {"isolani", "isolated pawn", "isolierter bauer"}},
      {"doubled_pawns", ConceptCategory::pawn_structures, "", {"doubled pawns", "doppelbauer"}},
      {"backward_pawn", ConceptCategory::pawn_structures, "", {"backward pawn", "rückständiger bauer", "rueckstaendiger bauer"}},
      {"passed_pawn", ConceptCategory::pawn_structures, "", {"passed pawn", "freibauer"}},
      {"hanging_pawns", ConceptCategory::pawn_structures, "", {"hanging pawns"}},
      {"pawn_majority", ConceptCategory::pawn_structures, "", {"pawn majority"}},

      {"piece_activity", ConceptCategory::piece_play, "", {"piece activity"}},
      {"bad_bishop", ConceptCategory::piece_play, "", {"bad bishop"}},
      {"good_bishop", ConceptCategory::piece_play, "", {"good bishop"}},
      {"knight_outpost", ConceptCategory::piece_play, "outpost", {"knight outpost"}},
      {"rook_activity", ConceptCategory::piece_play, "", {"active rook"}},
      {"piece_coordination", ConceptCategory::piece_play, "", {"coordination"}},

      {"king_safety", ConceptCategory::king_safety, "", {"king safety", "königssicherheit", "koenigssicherheit"}},
      {"pawn_shield", ConceptCategory::king_safety, "king_safety", {"pawn shield"}},
      {"weak_back_rank", ConceptCategory::king_safety, "king_safety", {"weak back rank"}},
      {"open_king", ConceptCategory::king_safety, "king_safety", {"exposed king"}},

      {"development", ConceptCategory::openings, "", {"piece development", "entwicklung"}},
      {"center_control", ConceptCategory::openings, "", {"centre control", "control the center"}},
      {"opening_principles", ConceptCategory::openings, "", {"opening principles", "eröffnungsprinzipien", "eroeffnungsprinzipien"}},
      {"opening_repertoire", ConceptCategory::openings, "", {"repertoire"}},

      {"opposition", ConceptCategory::endgames, "", {"king opposition", "opposition im endspiel"}},
      {"key_squares", ConceptCategory::endgames, "", {"key squares"}},
      {"rook_behind_passed_pawn", ConceptCategory::endgames, "", {"rook behind passed pawn"}},
      {"lucena", ConceptCategory::endgames, "", {"lucena position"}},
      {"philidor", ConceptCategory::endgames, "", {"philidor position"}},

      {"candidate_moves", ConceptCategory::calculation, "", {"candidate moves", "kandidatenzüge", "kandidatenzuege"}},
      {"forcing_moves", ConceptCategory::calculation, "", {"checks captures threats", "forcierte züge", "forcierte zuege"}},
      {"calculation_tree", ConceptCategory::calculation, "", {"calculation tree"}},

      {"active_defence", ConceptCategory::defence, "", {"active defense", "active defence"}},
      {"exchange_attackers", ConceptCategory::defence, "", {"exchange attacking pieces"}},
      {"create_escape_square", ConceptCategory::defence, "", {"luft", "escape square"}},

      {"attack_the_king", ConceptCategory::attack, "", {"king attack"}},
      {"open_lines", ConceptCategory::attack, "", {"open lines"}},
      {"bring_more_attackers", ConceptCategory::attack, "", {"add attackers"}},

      {"tactical_training", ConceptCategory::training, "", {"tactics training"}},
      {"calculation_training", ConceptCategory::training, "", {"calculation training"}},
      {"endgame_training", ConceptCategory::training, "", {"endgame practice"}},
      {"game_review", ConceptCategory::training, "", {"review games", "game analysis"}},
  };
  return kConcepts;
}

}  // namespace kchess::ai::concepts_internal
