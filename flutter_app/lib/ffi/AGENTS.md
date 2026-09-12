# Flutter FFI Agent

## Dateien

- Vertrag: `core_gateway.dart`
- `dart:ffi`-Implementierung: `ffi_core_gateway.dart`

## Token-sparender Trace

Bei einer FFI-Aufgabe immer **nur das konkrete Symbol** verfolgen:

1. Gateway-Methode
2. FFI-Binding/Lookup
3. gleichnamige/zugehörige C-ABI-Funktion in `native/src/api/core_api.cpp`
4. genau den aufgerufenen Native-Service

Nicht `core_api.cpp` oder alle Bindings komplett lesen.

## Regeln

ABI, Speicherbesitz, UTF-8 und Fehlerpfade stabil halten. Keine fachliche Neuberechnung in Dart. Native-JSON-Felder dürfen von Flutter ignoriert werden, ohne den Native-Vertrag automatisch zu löschen.


## Coach

`CoreGateway.coachAsk`/`kc_coach_ask_json` ist der einzige Transport für Coach-Antwortanfragen. `CoreGateway.coachContext`/`kc_coach_context_json` darf ausschließlich FEN/PGN in ein natives Board-DTO auflösen. Dart übergibt nur Kontext und zeigt Ergebnisse; keine Evidenz-, Routing-, PGN/FEN-Parsing- oder Validierungslogik in FFI ergänzen.

## Coach automatic gate

`coachAutomatic` is a thin JSON bridge to `kc_coach_automatic_json`. Flutter passes context only; all trigger decisions and the zero-LLM-call fast path remain native.

## Coach-Langläufer

Lokale LLM-Inferenz darf den Flutter-Isolate nicht blockieren. `coachAsk` und `coachAutomatic` starten native Coach-Jobs und pollen deren Status; direkte synchrone Coach-C-ABI-Funktionen bleiben nur aus Kompatibilitätsgründen bestehen.
