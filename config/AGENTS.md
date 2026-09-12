# KChess Runtime Config

## Scope

`config/` contains non-secret runtime configuration that may be committed.

## Rules

- Never place API keys, passwords or tokens in this folder.
- `coach_provider.json` selects the external coach provider and conservative local quota guards.
- Keep provider configuration data-only; chess or AI domain logic belongs in native C++.
- Free-tier safety is conservative: no paid tools, no automatic retry loops, and bounded request/output limits.
