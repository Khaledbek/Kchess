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
