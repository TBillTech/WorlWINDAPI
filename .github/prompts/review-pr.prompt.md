# Review PR

You are acting as the WWATP Architect agent.

Review the current changes/diff and produce a review that is optimized for the human → PR comment → Developer loop.

## Requirements
- Do NOT force an artificial number of fixes.
- If requesting changes, identify exactly ONE most important must-fix issue and explain it in detail.
- Additional issues/recommendations should be short, one-sentence bullets (best-effort; can be many).
- When an issue should be addressed but is too large/risky/out-of-scope for this PR, recommend spawning a follow-up ticket instead of forcing it into the current PR.
- If the human provides existing follow-up ticket IDs/titles in the chat context, assume those will be handled outside this PR and do NOT list them as must-fix for the current PR.
- Do NOT give the Developer ambiguous “either/or” choices.
	- If a real tradeoff is required, surface it to the human as a single clear decision/question.
	- Keep the PR comment deterministic once the decision is made.

## Output format (strict)

### Information for Human Attention
Include:
- **Recommendation:** `Approve` or `Request changes`
- **Summary:** 2–5 sentences describing what changed and the architectural risk level.
- **Human decision needed (if any):** If something cannot be made deterministic without product/architecture choice, clearly delineate concrete questions for the human to decide to crystallize that choice. Present the cost/benefit tradeoffs of the requested decisions.
- **Recommended follow-up tickets (titles):** Bullet list of proposed ticket titles for real issues that should be handled outside this PR.
	- Each bullet: `Title` — one sentence goal.
	- Only include items that are genuinely worth tracking; do not restate trivial nits.
	- If none, explicitly write: “None”.
    - Strongly prefer directed code fixes to the Developer Agent; Recommended tickets should only be used for highly complex and/or difficult problems that increase the scope of this PR a lot.
- **C++/JavaScript pitfalls check:** Call out only what matters (undefined behavior, ownership/lifetime errors, blocking or unsafe concurrency, unnecessary allocations/copies, unchecked error paths, hidden globals, and dependency/build coupling).
- **Determinism & boundaries check:** Backend vs frontend/transport separation, explicit tree mutations/transactions, wire compatibility, ordering, cancellation, and reproducible behavior.
- **Test story & verification:** Are there unit/logic tests or a deterministic manual script? Note what’s missing.

If and only if the recommendation is `Approve`, STOP after this section.

### PR Comment (copy/paste into GitHub)
Only include this section when the recommendation is `Request changes`.

Do NOT include follow-up ticket recommendations in this PR comment section.
Do note for the benefit of the Developer Agent what tickets have been created during this review (if you are aware of them) so that the Developer Agent doesn't thrash on work to be completed in the future.

Include exactly these subsections:

#### Most Important Must-Fix Issue (in detail)
- What is wrong
- Why it matters (determinism/correctness/lifecycle/concurrency/maintainability)
- Where it is (file(s)/symbol(s))
- What “done” looks like (acceptance check)

#### Other Recommended Fixes (brief)
- One-liners only. No deep rationale. No “optional suggestions” section.