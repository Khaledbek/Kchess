#pragma once

#include <string_view>

namespace kchess::ai::router_terms {

// -----------------------------------------------------------------------------
// Section: Conversation markers
// -----------------------------------------------------------------------------

inline constexpr std::string_view follow_up[]{
    "why", "why not", "what about", "and then", "and in", "and for",
    "show examples", "instead", "that move", "this move", "how can i prevent",
    "how do i prevent", "how can i avoid", "how do i avoid", "can i avoid",
    "warum", "wieso", "weshalb", "woher", "und dann", "und im", "und bei",
    "zeig beispiele", "stattdessen", "dieser zug", "der zug",
    "wie kann ich verhindern", "wie verhindere ich", "kann ich verhindern",
    "wie kann ich vermeiden", "wie vermeide ich", "kann ich vermeiden",
    "لماذا", "وماذا", "بدلا"};

// -----------------------------------------------------------------------------
// Section: Chess-domain anchors
// -----------------------------------------------------------------------------

inline constexpr std::string_view chess[]{
    "chess", "schach", "شطرنج", "fen", "pgn", "stockfish", "checkmate",
    "mate", "matt", "stalemate", "patt", "king", "queen", "rook", "bishop",
    "knight", "pawn", "könig", "dame", "turm", "läufer", "springer", "bauer",
    "ملك", "وزير", "رخ", "فيل", "حصان", "بيدق", "elo", "rating", "uci"};

inline constexpr std::string_view profile_data[]{
    "statistik", "statistics", "siegquote", "win rate", "partien",
    "games", "resultate", "results", "repertoire", "spielstil", "play style"};

inline constexpr std::string_view player_names[]{
    "carlsen", "kasparov", "fischer", "capablanca", "tal", "anand",
    "kramnik", "botvinnik", "alekhine", "polgar", "ding liren", "gukesh"};

// -----------------------------------------------------------------------------
// Section: Intent anchors
// -----------------------------------------------------------------------------

inline constexpr std::string_view move_explanation[]{
    "best move", "better move", "worse move", "worst move", "fastest loss",
    "lose fastest", "loses fastest", "why is", "why was",
    "bester zug", "besserer zug", "schlechter zug", "schlechtester zug",
    "schlechteste zug", "schlechtesten zug", "schnellster verlust",
    "schneller verlust", "schnelleren verlust", "am schnellsten verlieren",
    "warum ist", "warum war", "أفضل نقلة", "نقلة أفضل",
    "لماذا هذه النقلة"};

inline constexpr std::string_view plan[]{
    "plan", "strategy", "strategic", "attack", "improve the position",
    "exploit", "target", "prevent", "avoid", "stop", "keep", "preserve",
    "strategie", "planen", "angreifen", "angriff", "ausnutzen", "ziel",
    "verhindern", "vermeiden", "behalten", "erhalten", "خطة", "استراتيجية",
    "هجوم"};

inline constexpr std::string_view tactic[]{
    "tactic", "tactical", "fork", "royal fork", "pin", "skewer", "deflection",
    "decoy", "clearance", "sacrifice", "overload", "interference", "zwischenzug",
    "desperado", "x-ray", "battery", "greek gift", "smothered mate", "combination",
    "taktik", "gabel", "fesselung", "spieß", "ablenkung", "hinlenkung", "opfer",
    "überlastung", "zwischenzug", "تكتيك", "شوكة", "تثبيت", "تضحية"};

inline constexpr std::string_view opening[]{
    "opening", "openning", "debut", "gambit", "sicilian", "french defense", "caro-kann",
    "queens gambit", "king's indian", "opening theory", "eröffnung", "gambit",
    "sizilian", "französisch", "caro-kann", "eröffnungstheorie", "افتتاح", "غامبيت"};

inline constexpr std::string_view endgame[]{
    "endgame", "opposition", "lucena", "philidor", "tablebase", "pawn ending",
    "rook ending", "endspiel", "opposition", "bauernendspiel", "turmendspiel",
    "نهاية اللعب", "نهايات"};

inline constexpr std::string_view rules[]{
    "rule", "rules", "legal", "illegal", "castling", "castle", "en passant",
    "repetition", "fifty-move", "50-move", "draw rule", "promotion", "regel",
    "regeln", "legal", "illegal", "rochade", "en passant", "dreifache wiederholung",
    "50-züge", "umwandlung", "قانون", "قواعد", "تبييت", "ترقية"};

inline constexpr std::string_view history[]{
    "history", "world champion", "champion", "historic", "born", "biography",
    "geschichte", "weltmeister", "weltmeisterin", "historisch", "biografie",
    "تاريخ", "بطل العالم", "سيرة"};

inline constexpr std::string_view development[]{
    "improve my chess", "improve my game", "my rating", "my elo", "my weakness",
    "my weaknesses", "my strength", "my strengths", "my style", "play style",
    "my games", "my profile", "my blitz", "my rapid", "my bullet", "my progress",
    "why do i lose", "why do i keep", "am i improving", "my time management",
    "what kind of player am i", "how do i play", "get better", "verbessern",
    "mein elo", "meine elo", "meine schwäche", "meine schwächen", "meine stärke",
    "welche elo habe ich", "wie hoch ist mein elo", "meine eröffnung",
    "welche eröffnung spiele ich", "welche opening spiele ich",
    "meine stärken", "mein spielstil", "meinen spielstil", "wie spiele ich",
    "was für ein spieler bin ich", "habe ich mich verbessert", "besser werden",
    "meine partien", "mein profil", "mein blitz", "mein rapid", "mein fortschritt",
    "warum verliere ich", "mein zeitmanagement", "meine zeitnot",
    "أطور لعبي", "تصنيفي"};

inline constexpr std::string_view game_review[]{
    "review my game", "analyze my game", "game review", "analyse my game",
    "partie analysieren", "meine partie", "partie review", "راجع مباراتي", "حلل مباراتي"};

inline constexpr std::string_view training[]{
    "train", "training", "practice", "exercise", "quiz", "puzzle", "drill",
    "trainieren", "training", "übung", "quiz", "rätsel", "aufgabe",
    "تدريب", "تمرين", "لغز", "اختبار"};

inline constexpr std::string_view concept_terms[]{
    "concept", "weak square", "isolated pawn", "backward pawn", "doubled pawn",
    "passed pawn", "outpost", "space advantage", "king safety", "piece activity",
    "pawn structure", "initiative", "tempo", "zugzwang", "konzept", "schwaches feld",
    "isolierter bauer", "rückständiger bauer", "doppelbauer", "freibauer", "vorposten",
    "bauernstruktur", "initiative", "مفهوم", "بنية البيادق", "مربع ضعيف"};

}  // namespace kchess::ai::router_terms
