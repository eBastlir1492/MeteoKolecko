# MeteoKolecko Git Publication Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bez přepisu historie napojit lokální projekt na fork `eBastlir1492/MeteoKolecko`, promítnout fázové větvení do všech Level 1 plánů a připravit publikovanou `feature/level1-phase-a`.

**Architecture:** Dokumentační pravidla větví se commitnou ještě před změnou remotů, aby byla součástí prvního pushnutého `main`. Remoty se přepojí pouze po ancestry a diff kontrole; publikace používá výhradně fast-forward push. Výsledek se zaznamená do verzovaného verification dokumentu a Phase A vznikne z přesně ověřeného `main`.

**Tech Stack:** Git for Windows, PowerShell, GitHub HTTPS, Git Credential Manager, GitHub web settings; bez GitHub CLI a bez force-push.

## Global Constraints

- Autoritativní specifikace: `docs/superpowers/specs/2026-08-08-meteokolecko-git-workflow-design.md`.
- Původní fork base musí zůstat `fcf0ced215632be0bc3ca42b0b0b8468a850b430`.
- `origin` musí být `https://github.com/eBastlir1492/MeteoKolecko.git`.
- `upstream` musí být `https://github.com/petus/MeteoPlaneRadar.git` s push URL `DISABLED`.
- Zakázány jsou `--force`, `--force-with-lease`, orphan branch, reset/rebase publikovaného `main` a přepis tagů.
- Původní tagy `v0.4`, `v0.5.2`, `v0.5.3`, `v0.5.4` zachovávají původní object IDs.
- Token, heslo a recovery kód se nikdy nevkládají do příkazu, chatu, souboru ani logu.
- Před každou mutací se kontroluje čistý worktree, aktuální branch a ancestry.
- Každý task končí vlastním commitem; po Tasku 2 a 3 se commit navíc pushne a ověří podle object ID.
- Při neočekávaném vzdáleném commitu nebo ne-fast-forward vztahu se task zastaví bez pokusu o sjednocení historie.

---

### Task 1: Promítnout branch workflow do všech Level 1 plánů

**Files:**
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-level1-master-plan.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteokolecko-git-publish-plan.md`

**Interfaces:**
- Consumes: five phase names and ordered plans in the Level 1 master plan.
- Produces: exact required branch in every phase, start gate, per-task branch guard, phase integration gate and resume instructions understandable without this chat.

- [x] **Step 1: Run the static branch-contract test and verify it fails**

```powershell
$expected = [ordered]@{
  'docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md' = 'feature/level1-phase-a'
  'docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md' = 'feature/level1-phase-b'
  'docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md' = 'feature/level1-phase-c'
  'docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md' = 'feature/level1-phase-d'
  'docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md' = 'feature/level1-phase-e'
}
$errors = @()
foreach ($entry in $expected.GetEnumerator()) {
  $body = Get-Content -Raw -LiteralPath $entry.Key
  foreach ($required in @(
    "Required branch: ``$($entry.Value)``",
    'git branch --show-current',
    'Phase Integration Gate',
    'git merge --no-ff'
  )) {
    if (-not $body.Contains($required)) {
      $errors += "$($entry.Key): missing $required"
    }
  }
}
if ($errors.Count -gt 0) { $errors; exit 1 }
```

Expected: exit 1; každému fázovému plánu chybí nejméně jeden nový branch-contract prvek.

- [x] **Step 2: Add the master branch matrix and resume rule**

Do master plánu vložit tuto závaznou matici:

```markdown
## Branch Contract

| Phase | Required branch | Start point | Integration target |
|---|---|---|---|
| A | `feature/level1-phase-a` | verified `origin/main` | `main` |
| B | `feature/level1-phase-b` | `main` after Phase A merge | `main` |
| C | `feature/level1-phase-c` | `main` after Phase B merge | `main` |
| D | `feature/level1-phase-d` | `main` after Phase C merge | `main` |
| E | `feature/level1-phase-e` | `main` after Phase D merge | `main` |

Never implement a phase task directly on `main`. Before every task run
`git branch --show-current`, `git status --short`, and `git log -5 --oneline`.
The branch name must match the phase row and the worktree must contain only
the intentional in-progress task. A completed phase is merged with an
explicit `--no-ff` merge only after its full verification gate passes.
```

Do existujícího resume protokolu doplnit kontrolu, že current branch odpovídá prvnímu nedokončenému fázovému plánu. Pokud neodpovídá, nepokračovat v editaci a nejprve určit, zda je fáze již mergenutá.

Do master plánu zároveň přidat tento upstream kontrakt:

```markdown
## Upstream Sync Contract

`upstream` is fetch-only. Never merge `upstream/main` directly into an active
Level 1 phase. Review a needed upstream change on
`chore/sync-upstream-YYYYMMDD`, verify it against the display/touch/OTA
invariants, and merge only that reviewed branch. Automatic upstream sync and
force-push are forbidden.
```

- [x] **Step 3: Add a self-contained branch guard to every phase plan**

Za `## Global Constraints` každého fázového plánu přidat konkrétní branch řádek podle této přesné mapy:

```text
phase-a-foundation-plan.md -> feature/level1-phase-a
phase-b-weather-data-plan.md -> feature/level1-phase-b
phase-c-renderer-ui-plan.md -> feature/level1-phase-c
phase-d-portal-ota-plan.md -> feature/level1-phase-d
phase-e-hardening-plan.md -> feature/level1-phase-e
```

Každý soubor musí obsahovat čtyři body s jeho skutečným názvem větve:

```markdown
- **Required branch:** the exact phase branch named in the mapping above.
- Before every task, verify the exact branch with `git branch --show-current`.
- Never commit a phase task directly to `main` or to another phase branch.
- If the branch is missing, create it only from the verified `main` start point
  stated in the master Branch Contract and publish it with `git push -u origin`.
```

V committed souboru nahraďte první anglický bod skutečným názvem z mapy; obecná věta z ukázky se nesmí commitnout. Na konec každého fázového plánu přidat následující gate a v merge příkazu použít jeho přesnou branch:

```markdown
## Phase Integration Gate

1. Confirm every task checkbox in this phase is committed.
2. Run the phase's complete verification command and read its full output.
3. Confirm `git status --short` is empty and the current branch is the required
   phase branch.
4. Push the phase branch to `origin` and review the full `main...branch` diff.
5. Switch to `main`, fast-forward it from `origin/main`, then merge the phase
   with `git merge --no-ff` followed by the exact branch named in this file.
6. Re-run the phase verification on the merge result.
7. Push `main` normally, verify local and `origin/main` object IDs match, and
   only then create the next phase branch from that `main`.

Do not delete the phase branch until the merge commit and remote `main` have
been verified. Never use force-push to repair a failed gate.
```

Přesné merge příkazy jsou `git merge --no-ff feature/level1-phase-a` až `git merge --no-ff feature/level1-phase-e` podle daného souboru. Obecné instrukční formulace z tohoto plánu se do fázových plánů nekopírují.

- [x] **Step 4: Re-run static and documentation verification**

Spustit test ze Step 1; očekává se exit 0. Poté:

```powershell
git diff --check
$plans = Get-ChildItem docs/superpowers/plans -Filter '*.md'
$requiredBranches = @(
  'feature/level1-phase-a', 'feature/level1-phase-b',
  'feature/level1-phase-c', 'feature/level1-phase-d',
  'feature/level1-phase-e'
)
foreach ($branch in $requiredBranches) {
  if (-not ($plans | Select-String -SimpleMatch $branch)) {
    throw "Missing phase branch contract: $branch"
  }
}
```

Then run this focused integration-gate assertion:

```powershell
$phasePlans = @(
  'docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md',
  'docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md',
  'docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md',
  'docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md',
  'docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md'
)
$errors = @()
$reviewFetchPattern = 'git fetch --prune origin\r?\n\s*if \(\$LASTEXITCODE -ne 0\) \{ throw ''Could not refresh origin/main before review'' \}\r?\n\s*\$expectedOriginMain = git rev-parse origin/main'
$mergeFetchPattern = 'git fetch --prune origin\r?\n\s*if \(\$LASTEXITCODE -ne 0\) \{ throw ''Could not refresh origin/main before integration'' \}\r?\n\s*if \(\(git rev-parse origin/main\) -ne \$expectedOriginMain\)'
foreach ($plan in $phasePlans) {
  $body = Get-Content -Raw -LiteralPath $plan
  $gateStart = $body.IndexOf('## Phase Integration Gate')
  $gate = if ($gateStart -ge 0) { $body.Substring($gateStart) } else { '' }
  $expectedRemote = $body.IndexOf('$expectedOriginMain = git rev-parse origin/main', $gateStart)
  $ancestry = $body.IndexOf('git merge-base --is-ancestor main origin/main', $gateStart)
  $remoteStop = $body.IndexOf('Unexpected remote main changed after review', $gateStart)
  $fastForwardStop = $body.IndexOf('main cannot fast-forward to origin/main', $gateStart)
  $fastForward = $body.IndexOf('git merge --ff-only origin/main', $gateStart)
  if ($gateStart -lt 0 -or $expectedRemote -lt $gateStart -or $ancestry -lt $gateStart -or $remoteStop -lt $gateStart -or $fastForwardStop -lt $gateStart -or $fastForward -lt 0 -or $expectedRemote -gt $fastForward -or $ancestry -gt $fastForward -or $remoteStop -gt $fastForward -or $fastForwardStop -gt $fastForward -or -not [regex]::IsMatch($gate, $reviewFetchPattern) -or -not [regex]::IsMatch($gate, $mergeFetchPattern)) {
    $errors += "$($plan): missing ordered fetch, expected-remote, or ancestry stop contract before fast-forward"
  }
}
if ($errors.Count -gt 0) { $errors; exit 1 }
```

Expected: všechny verification commands exit 0, žádná nevyřešená šablona ani whitespace error.

- [x] **Step 5: Commit the Level 1 branch contract**

Zaškrtnout Task 1 kroky v tomto plánu a commitnout:

```powershell
git add docs/superpowers/plans/2026-08-08-meteolcd-level1-master-plan.md `
        docs/superpowers/plans/2026-08-08-meteolcd-phase-a-foundation-plan.md `
        docs/superpowers/plans/2026-08-08-meteolcd-phase-b-weather-data-plan.md `
        docs/superpowers/plans/2026-08-08-meteolcd-phase-c-renderer-ui-plan.md `
        docs/superpowers/plans/2026-08-08-meteolcd-phase-d-portal-ota-plan.md `
        docs/superpowers/plans/2026-08-08-meteolcd-phase-e-hardening-plan.md `
        docs/superpowers/plans/2026-08-08-meteokolecko-git-publish-plan.md
git commit -m "docs: integrate Level 1 phase branch workflow"
git status --short
```

Expected: commit succeeds and status is empty.

### Task 2: Rewire remotes and publish main with preserved tags

**Files:**
- Create: `docs/verification/git-publication.md`
- Modify: `.git/config` through Git commands; this file is local metadata and is not committed.
- Modify: `docs/superpowers/plans/2026-08-08-meteokolecko-git-publish-plan.md`

**Interfaces:**
- Consumes: clean `main` after Task 1; fork base `fcf0ced`; Git Credential Manager.
- Produces: protected local remote topology, published fast-forward `origin/main`, preserved original tags and a committed evidence record.

- [x] **Step 1: Run immutable preflight checks**

```powershell
if (git status --porcelain=v1) { throw 'Worktree must be clean' }
if ((git branch --show-current) -ne 'main') { throw 'Must run on main' }
git merge-base --is-ancestor fcf0ced215632be0bc3ca42b0b0b8468a850b430 HEAD
if ($LASTEXITCODE -ne 0) { throw 'Original fork base is not an ancestor' }
$unexpected = git diff --name-only fcf0ced215632be0bc3ca42b0b0b8468a850b430..HEAD |
  Where-Object { $_ -notlike 'docs/*' }
if ($unexpected) { throw "Unexpected non-documentation changes: $unexpected" }
git remote -v
git log --oneline --decorate fcf0ced215632be0bc3ca42b0b0b8468a850b430..HEAD
```

Expected: clean `main`, ancestry success, only `docs/*` differs, and the sole current remote named `origin` points to `petus/MeteoPlaneRadar`.

- [x] **Step 2: Rewire remotes without touching commits**

```powershell
git remote rename origin upstream
git remote add origin https://github.com/eBastlir1492/MeteoKolecko.git
git remote set-url --push upstream DISABLED
git config remote.pushDefault origin
git fetch --prune origin
git fetch --prune upstream
git remote -v
```

Expected URLs:

```text
origin    https://github.com/eBastlir1492/MeteoKolecko.git (fetch)
origin    https://github.com/eBastlir1492/MeteoKolecko.git (push)
upstream  https://github.com/petus/MeteoPlaneRadar.git (fetch)
upstream  DISABLED (push)
```

- [x] **Step 3: Prove fast-forward safety before the first push**

```powershell
$remoteBase = git rev-parse origin/main
if ($remoteBase -ne 'fcf0ced215632be0bc3ca42b0b0b8468a850b430') {
  throw "Unexpected fork main: $remoteBase"
}
git merge-base --is-ancestor origin/main main
if ($LASTEXITCODE -ne 0) { throw 'Push would not be fast-forward' }
$unexpected = git diff --name-only origin/main..main |
  Where-Object { $_ -notlike 'docs/*' }
if ($unexpected) { throw "Unexpected source changes before publication: $unexpected" }
git log --graph --decorate --oneline origin/main..main
```

Expected: remote base exact, ancestry success and only reviewed documentation commits ahead.

- [x] **Step 4: Publish main and original tags without force**

```powershell
git push -u origin main
git push origin refs/tags/v0.4 refs/tags/v0.5.2 refs/tags/v0.5.3 refs/tags/v0.5.4
git fetch --prune origin
```

Git Credential Manager may open GitHub login. Do not copy credentials into the terminal. Expected: normal fast-forward push and four original tags created or reported up-to-date.

- [x] **Step 5: Create the publication evidence and commit it**

Create `docs/verification/git-publication.md` with actual output values and these exact sections:

```markdown
# MeteoKolecko Git Publication Evidence

## Remotes
## Ancestry
## Main object IDs before evidence commit
## Original tag object IDs
## Force-push audit
```

Record fetch/push URLs, `git merge-base`, local/remote main IDs, `git show-ref --tags`, and the statement `Force-push used: no`. Phase A and GitHub protection sections se přidají až s naměřenými výsledky v Tasku 3; Task 2 nevytváří prázdné nebo dočasné sekce.

Then:

```powershell
git add docs/verification/git-publication.md `
        docs/superpowers/plans/2026-08-08-meteokolecko-git-publish-plan.md
git commit -m "chore: record MeteoKolecko fork publication"
git push origin main
git fetch origin
if ((git rev-parse main) -ne (git rev-parse origin/main)) {
  throw 'Published main does not match local main'
}
git status --short
```

Expected: evidence commit published, object IDs match, status empty.

### Task 3: Protect main and bootstrap the Phase A branch

**Files:**
- Modify: `docs/verification/git-publication.md`
- Modify: `docs/superpowers/plans/2026-08-08-meteokolecko-git-publish-plan.md`
- Modify: local/GitHub branch metadata through Git and GitHub Settings.

**Interfaces:**
- Consumes: published, matching `main` from Task 2.
- Produces: GitHub protection against force-push/deletion and `feature/level1-phase-a` tracking its origin counterpart at the final workflow checkpoint.

- [x] **Step 1: Configure the minimal GitHub main protection**

In `eBastlir1492/MeteoKolecko` open **Settings → Rules → Rulesets**, create an active branch ruleset targeting the default branch, enable only:

```text
Restrict deletions: enabled
Block force pushes: enabled
Require a pull request before merging: disabled until CI exists
Required status checks: none until CI exists
```

Do not enable a status check that the repository does not provide. Capture the ruleset name and active status for the evidence document.

- [x] **Step 2: Create and publish the Phase A branch from verified main**

```powershell
git switch main
git pull --ff-only origin main
if (git status --porcelain=v1) { throw 'Worktree must be clean' }
git switch -c feature/level1-phase-a
git push -u origin feature/level1-phase-a
git fetch origin
if ((git rev-parse feature/level1-phase-a) -ne
    (git rev-parse origin/feature/level1-phase-a)) {
  throw 'Phase A local and remote branch mismatch'
}
```

Expected: new branch is a direct child pointer of the verified `main`, publishes normally and tracks `origin/feature/level1-phase-a`.

- [x] **Step 3: Add the final branch and protection evidence**

Do `docs/verification/git-publication.md` přidat sekce `## Phase A branch` a `## GitHub main protection` s těmito skutečnými výsledky:

- actual local and remote Phase A object IDs;
- output proving the branch tracks `origin/feature/level1-phase-a`;
- GitHub ruleset name and active status;
- `Restrict deletions: enabled`, `Block force pushes: enabled`, PR requirement/status checks disabled.

Žádná prázdná evidence sekce ani neověřené tvrzení nesmí zůstat.

- [x] **Step 4: Commit the bootstrap record on Phase A and publish it**

```powershell
git add docs/verification/git-publication.md `
        docs/superpowers/plans/2026-08-08-meteokolecko-git-publish-plan.md
git commit -m "chore: bootstrap Level 1 Phase A branch"
git push origin feature/level1-phase-a
git fetch origin
```

Expected: commit exists only on the Phase A branch until its later phase merge.

- [x] **Step 5: Run the final topology and resume verification**

```powershell
if ((git branch --show-current) -ne 'feature/level1-phase-a') {
  throw 'Expected Phase A branch'
}
if (git status --porcelain=v1) { throw 'Worktree must be clean' }
if ((git rev-parse HEAD) -ne (git rev-parse origin/feature/level1-phase-a)) {
  throw 'Phase A branch not fully published'
}
git merge-base --is-ancestor origin/main HEAD
if ($LASTEXITCODE -ne 0) { throw 'Phase A does not descend from published main' }
$upstreamPush = git remote get-url --push upstream
if ($upstreamPush -ne 'DISABLED') { throw 'Upstream push is not disabled' }
git remote -v
git branch -vv
git log --graph --decorate --oneline --all -12
git status --short
```

Expected: current branch Phase A, clean status, origin tracking/object ID match, published main is an ancestor and upstream push remains disabled. The next implementation action is Phase A Task A1, not another Git restructuring step.
