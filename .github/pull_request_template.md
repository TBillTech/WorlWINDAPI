<!-- Keep PRs small, focused, and tied to a single Issue. -->
## Summary
Brief description of what this PR changes and why (tie directly to the Issue context).

## Related Issue
- Fixes #<issue-number>  
  or  
- Refs #<issue-number> (partial / groundwork only)

<!--
Replace <issue-number> above (do not leave a placeholder).
If this PR is truly not tied to an issue (rare), write "N/A" and explain why.
-->

(One primary issue per PR unless explicitly documented.)

## What Changed
- Bullet list of concrete changes (no speculation, no future work)

## How to Test
- [ ] Configure/build the relevant CMake target or JavaScript package
- [ ] Run the relevant test command
- [ ] Perform any required deterministic manual steps:
      1. …
      2. …
- [ ] Expected result:
      - …
- [ ] No new build, test, or runtime errors/warnings

## Architecture Notes
- New dependencies introduced? (Yes/No — list if yes)
- Any backend/frontend or transport boundary changes? (Describe or “None”)
- Any ownership, threading, cancellation, or shutdown changes? (Describe or “None”)
- Any wire-format, public API, or serialization changes? (Describe compatibility/migration or “None”)

## Non-Goals
Explicitly list what this PR does NOT attempt to address.

## Risks / Follow-ups
- Known risks, edge cases, or deferred work
- Follow-up issues (if any): #<issue-number>

## Performance / Allocation Notes
- Any unnecessary allocations, copies, blocking, or hot-path work introduced? (Yes/No)
- Any concurrency or resource-lifetime behavior changed? (Yes/No)
- If yes to either, explain briefly.

## Checklist
- [ ] Changes match Issue acceptance criteria
- [ ] Non-goals respected (no scope creep)
- [ ] No unrelated refactors
- [ ] Backend/frontend and transport boundaries remain explicit
- [ ] No new hidden global state or implicit runtime discovery
- [ ] Ownership, shutdown, and compatibility impacts documented
- [ ] Focused automated test or deterministic manual verification included
