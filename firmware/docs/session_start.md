# Session Start Protocol

_This file contains instructions for Claude Code. Follow these steps at the start of every session without being asked._

---

## Step 1 — Load State
Read only the two most recent entries in `docs/session_end.md`.

## Step 2 — Load Issues
Read `docs/known_issues.md` to identify any blocking problems before starting work.

## Step 3 — Report to User
Without waiting to be asked, tell the user:

- **Active phase** and what it covers
- **Last session summary** — what was completed, what was left incomplete
- **Any blocking known issues** relevant to today's work
- **Suggested next step** based on session_end.md

Format it concisely — a few sentences, not a wall of text.

## Step 4 — Confirm Plan
Ask the user to confirm the suggested next step or redirect before writing any code.

## Step 5 — Load Additional Docs if Needed
Based on the confirmed plan, load only the docs relevant to today's work per the rules in `claude.md`. State which docs you are loading and why.

Common loads by phase:
- Phase 1–2: load `docs/architecture.md` (task table, display boundary, input model)
- Phase 3: load `docs/architecture.md` + `docs/hardware_config.md`
- Phase 4: load `docs/architecture.md` + `docs/fit_format_notes.md`
- Phase 5+: load `docs/architecture.md` as needed for component API reference

---

## Ongoing Session Rules
- Remind the user if they are about to implement something outside the active phase
- Flag any new power implications before implementing a feature
- If a new architecture decision is made mid-session, append it to `docs/decisions.md` immediately
- If a hardware quirk or bug is found, add it to `docs/known_issues.md` immediately
- Do not ask the user to update docs — handle it automatically
- Do not run code review, git commit, or session end unless the user explicitly calls them
- When the user says "code review" → follow `docs/code_review.md` Steps 1 and 2, report results
- When the user says "git commit" → follow `docs/code_review.md` Step 4
- When the user says "session end" → follow session end rules in `claude.md`
