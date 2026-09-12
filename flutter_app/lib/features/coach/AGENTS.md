# Coach UI Agent

## Scope

`flutter_app/lib/features/coach/` enthält ausschließlich die Flutter-Oberfläche des AI Chess Coach.

## Start hier

- Screen/Responsive Layout: `presentation/coach_screen.dart`
- Native Session/Navigation: `presentation/coach_session_screen.dart`
- Board-Spalte: `presentation/coach_board_panel.dart`
- PGN-Steuerleiste: `presentation/coach_board_controls.dart`
- Trainer-Ausgabe: `presentation/coach_output_panel.dart`
- Eingabe + Hint/Depth/Overflow-Kontextmenü: `presentation/coach_input_bar.dart`
- FEN/PGN-Auswahldialoge: `presentation/coach_context_dialogs.dart`
- Reiner View-State: `models/coach_ui_models.dart`

## UI-Ziel

Die erste Coach-Oberfläche übernimmt die visuelle Sprache des Analysis-Screens:

```text
Desktop / breit:
Board links | Trainer-Ausgabe rechts oben
            | Eingabeleiste rechts unten
```

Bei schmalem Platz darf dieselbe Struktur responsiv untereinander angeordnet werden.

## Harte Domain-Grenze

Flutter darf nur darstellen, Eingaben sammeln, Board-Orientierung als UI-Zustand halten und native Aktionen über Callbacks/FFI auslösen.

Nicht in diesem Ordner implementieren:

- Zuglegalität oder Zugbewertung
- Tactical-/Weakness-/Plan-Erkennung
- Coach Routing / Evidence Selection
- Engine-Entscheidungen
- Prompt-/Provider-Logik
- Response Validation
- Profil-Lernen

Diese Verantwortlichkeiten bleiben in C++ unter `native/ai/` bzw. bestehenden nativen KChess-Systemen.

## Lokalisierung

Jeder feste sichtbare Text kommt aus `flutter_app/l10n/app_en.arb`, `app_de.arb` und `app_ar.arb`. Generierte Dateien unter `lib/localization/generated/` nicht manuell ändern.

## Dateigröße

Widgets klein halten und neue Verantwortlichkeiten in eigene Dateien teilen, statt `coach_screen.dart` wachsen zu lassen. Keine Feature-spezifische Logik in Shared verschieben, nur um Dateien zu kürzen.

## Integration

`coach_session_screen.dart` ist die dünne Integrationsschicht: sie hält nur Chat-View-State, baut den Transport-Request und ruft `CoreGateway.coachAsk` auf. Analysis/FEN/PGN, Play, Training/Opening und Profile dürfen nur Kontext (FEN, IDs, Orientierung) übergeben; Routing, PGN-Anreicherung, Profil-Evidenz und Antwortvalidierung bleiben nativ.

## Automatic Coach

Coach-Reaktionen werden ohne Drei-Sekunden-Timer nach jedem gültigen Zug angestoßen, bei Varianten sobald die native Analyse vorliegt. Flutter übermittelt nur Kontext. Der native Job-Layer verwendet latest-position-wins: ein neuer Zug cancelt ältere Automatic-Coach-Jobs; veraltete Antworten dürfen nie in den Chat gelangen.

## Interaktiver Trainer – ergänzter Vertrag

- Normale Chat-/Hint-Texte sind offene Abschnitte ohne Blase. Ereigniskarten verwenden ausschließlich natives `eventKind` (blunder/best/brilliant), niemals Textsuche.
- `followUpQuestion`, `boardMoves`, `focusSquares` und `positionFen` werden nur dargestellt. „Show on the board“ darf zu der gespeicherten Nachrichten-FEN innerhalb der bekannten Haupt-/Side-Line navigieren und danach nur die gelieferten Overlays anzeigen; es spielt keinen Zug und startet keinen Coach-Request.
- Vergleich und Aufgabe übermitteln die Modi `compare`/`quiz`; Engine-/Kandidatenauswahl bleibt nativ. Quiz und erste Hint-Stufe verraten keine Empfehlungspfeile.
- Der Automatic-Coach-Vertrag erlaubt zusätzlich zu gespeicherter Context-ID/Ply eine native Varianten-Job-ID plus vorherige FEN. C++ prüft den Zug/Stellungsbezug; Veraltete Antworten werden weiterhin verworfen; es gibt kein Debounce-Fenster.
- `coach_board_focus.dart` zeichnet nur die gelieferten Fokusfelder. Gesprächslisten als unveränderlichen Snapshot weiterreichen, damit automatische Nachrichten das Scrollen korrekt auslösen.

## Manuelle Kontexte

- Ein allgemeiner Coach-Start zeigt die normale Schach-Ausgangsstellung statt eines leeren Bretts.
- FEN wird in Flutter nur gesammelt; Validierung und Board-DTO kommen über `CoreGateway.coachContext` aus C++.
- PGN kann eingefügt oder aus der bestehenden Game-Library gewählt werden. Bei gespeicherten Partien wird nur die Game-ID weitergereicht; PGN-Rekonstruktion bleibt nativ.

## Brettinteraktion

Das Coach-Brett ist interaktiv. Flutter hält nur Auswahl-/Busy-View-State; Zuglegalität, Promotion und resultierende Stellung werden ausschließlich über `boardPromotionOptions` und `resolveFreeBoardMove` nativ aufgelöst. Ein geladener PGN-Kontext startet immer auf seiner Ausgangsstellung; manuelle Brettzüge danach sind flüchtige Coach-Varianten und dürfen keine gespeicherte Hauptlinie überschreiben. Pro Coach-Session wird höchstens eine solche Side-Line gehalten; sie bleibt unsichtbar und wird ausschließlich mit derselben Brett-/Zugsteuerung navigiert. Die PGN-Leiste bietet First/Previous/Next/Last und zeigt keine separate Variation-/Graph-UI.

## Analysis-Brett und Hint

Das Coach-Brett übernimmt die Analysis-Präsentation mit nativer Evalbar, Best-Move-Pfeilen und Move-Classification. Flutter rendert nur die gelieferten nativen Werte.

Der Hint arbeitet pro Stellung als unbegrenzter dreistufiger Zyklus: (1) die native Engine bestimmt bis zu zwei gleichwertige Kandidaten, Flutter markiert die Ausgangsfelder und ergänzt eine kurze lokalisierte Trainer-Nachricht, (2) Zielfelder/Pfeile werden sichtbar und mit einer zweiten Trainer-Nachricht kombiniert, (3) eine geerdete Coach-Erklärung darf angefragt werden. Danach springt nur der UI-Hint-Zustand zurück auf Schritt 1; die bereits berechneten Engine-Kandidaten werden für dieselbe unveränderte Stellung wiederverwendet. Ein Stellungswechsel verwirft den Hint-Cache. FEN/PGN liegen ausschließlich im Drei-Punkte-Menü der Eingabeleiste.

Der Hauptnavigationseintrag des Coach läuft ohne KChess-Sidebar/Drawer; `onExit` führt zurück in die normale App-Navigation.
