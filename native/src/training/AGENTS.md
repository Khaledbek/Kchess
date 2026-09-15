# Native Training Agent

## Scope

Training-Kataloge, Practice-Positionen, Sessions, Lösungen, Versuche und Fortschritt.

## Token-Regel

Katalog-`.inc`-Dateien nicht in Kontext laden. Bei Logikaufgaben C++-Klassen/Services lesen; Dateninhalt nur bei ausdrücklich datenbezogener Aufgabe.

## Routing

- Practice → `practice_*`
- Training-Service/Katalog → `training_service.*`, `training_catalog.*`
- große Daten → `data/` (nur bei Bedarf)

Lösung/Legalität/Progress bleibt nativ; Flutter zeigt nur Zustand und Eingaben.
