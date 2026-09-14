# Coach dialogue review

`coach_dialogue_scenarios.jsonl` is a synthetic, private-data-free catalogue of questions for manual review. It is not an app runtime source and does not supply chess truth. Use a real native evidence packet and the copied Coach conversation only in a local review session; never commit a player's transcript or profile payload.

For each scenario, record the native Coach diagnostic trace and review four independent dimensions: factual support, correct uncertainty/scope, feedback continuity, and conversational usefulness. Mark each as pass, partial or fail and note the exact evidence ID or native validation issue. A fluent answer without supported claims fails factual support; a perfectly cited but generic lecture fails conversational usefulness. Keep response latency and provider/repair calls beside the qualitative review, without treating shorter responses as automatically better.

Do not use an LLM judge as the sole authority. Native chess legality, source scalars and a human reading of the actual conversation decide whether the coach helped. Add new scenarios only for recurring observed failures, with synthetic wording and an explicit forbidden behavior.

The local review record is JSONL with one object per scenario. Required fields are `scenarioId` and `verdicts`, containing `factual_support`, `scope_and_uncertainty`, `feedback_continuity`, and `conversational_usefulness`; each verdict is `pass`, `partial`, or `fail`. Optional `trace` may contain the native `providerCalls` count. Keep transcripts and profile payloads outside this repository. An example review record without private data:

```json
{"scenarioId":"general_rule","verdicts":{"factual_support":"pass","scope_and_uncertainty":"pass","feedback_continuity":"pass","conversational_usefulness":"partial"},"trace":{"providerCalls":1}}
```

`review_coach_dialogue.py` reads that local JSONL and summarizes coverage, verdicts and factual/scope failures. It does not call Gemini, KChess or an LLM judge. Run it only when the user allows evaluation commands. A reviewed answer should also record one human-edited teaching version outside source control; compare whether the edit corrects a fact, scope, timing or merely tone. Only the last category is plausible data for later style tuning.

## Reale Fälle und getrennte Bewertung

Reale Fälle bleiben in einer privaten JSONL-Datei außerhalb des Repositorys. Jeder Fall braucht eine eindeutige `id`, `source: "real_local"` und eine private `groupId`, die für alle Partien desselben Spielers gleich ist. Das Skript teilt diese Gruppe deterministisch in `development` oder `heldout` auf; damit landet auch keine Partie desselben Spielers auf beiden Seiten. Die Kennung wird nur lokal verarbeitet. Pro Fall privat auch Anfrage, sichtbare Antwortsegmente, provider-sichtbare Evidenz-IDs und Quellwerte, native Validierungsprobleme, Latenz und Reparaturaufrufe aufbewahren. Diese Nutzdaten nicht in den synthetischen Katalog oder die Ausgabe des Skripts kopieren.

Ein minimaler lokaler Indexeintrag ist `{"id":"local-001","source":"real_local","groupId":"private-stable-player-key"}`. Die zugehörige Review-Zeile benötigt zusätzlich zu den vier Urteilen `editCategory` (`none`, `evidence_gap`, `teaching_point`, `reveal_timing`, `repetition`, `tone_only`). `reviewerEditedAnswer` ist nur dann ein brauchbares Sprachpaar, wenn die ersten drei Urteile `pass` sind und `editCategory` ausschließlich `tone_only` ist. Die Bearbeitung und ihre Referenzantwort bleiben privat.

Bei erlaubter Offline-Evaluation kann `review_coach_dialogue.py PRIVATE_REVIEWS.jsonl --scenarios PRIVATE_CASES.jsonl` den synthetischen Katalog und die lokalen Fälle gemeinsam auswerten. Mehrere `--scenarios`-Dateien sind möglich; IDs müssen global eindeutig sein. `tuningReadiness` ist ein Entscheidungsfilter: mindestens 30 menschlich geprüfte Held-out-Fälle, dort keine nicht bestandene Fakt-/Scope-Prüfung und mindestens 10 reine Tonkorrekturen. Selbst `eligible_for_controlled_style_comparison` ist noch keine Freigabe zum Training: zunächst dieselben Evidenzpakete gegen unverändertes Gemini vergleichen und prüfen, ob der Lehrnutzen steigt, ohne Fakten oder Dialogkontinuität zu verschlechtern. Das Skript trainiert und startet kein Modell.
