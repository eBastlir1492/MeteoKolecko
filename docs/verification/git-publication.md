# MeteoKolecko Git Publication Evidence

## Remotes

```text
origin fetch: https://github.com/eBastlir1492/MeteoKolecko.git
origin push:  https://github.com/eBastlir1492/MeteoKolecko.git
upstream fetch: https://github.com/petus/MeteoPlaneRadar.git
upstream push:  DISABLED
remote.pushDefault: origin
```

## Ancestry

The immutable preflight before the first push returned this merge base:

```text
git merge-base origin/main main
fcf0ced215632be0bc3ca42b0b0b8468a850b430
```

`git merge-base --is-ancestor origin/main main` exited with status 0 before the
first push. After the initial publication and before this evidence commit,
`git merge-base main origin/main` returned:

```text
339bc818f00ee0ae4511dc54e1f48757bae0cce8
```

## Main object IDs before evidence commit

```text
local main:  339bc818f00ee0ae4511dc54e1f48757bae0cce8
origin/main: 339bc818f00ee0ae4511dc54e1f48757bae0cce8
```

## Original tag object IDs

`git show-ref --tags` returned:

```text
c0621ab56dd1a2408939fc8d5b6e19331a69daf6 refs/tags/v0.4
ada551ab33b56974e06fa1d7757deb5610e93ded refs/tags/v0.5.2
4ff3d8afadd59ab37ba7f7084f3888ac5701c53f refs/tags/v0.5.3
980151f3612a46e2efe36ca228293020d4229e49 refs/tags/v0.5.4
```

## Force-push audit

Force-push used: no

## Phase A branch

Immediately after branch creation and before the bootstrap evidence commit, the
local and remote Phase A branch object IDs were identical:

```text
local feature/level1-phase-a:         e2077a33808eb80da5baf0a6fce8efa3bba85481
origin/feature/level1-phase-a:        e2077a33808eb80da5baf0a6fce8efa3bba85481
```

`git branch -vv` proved the upstream tracking relationship:

```text
* feature/level1-phase-a e2077a3 [origin/feature/level1-phase-a] chore: record MeteoKolecko fork publication
```

## GitHub main protection

The full repository ruleset detail returned by the GitHub REST API measured:

```text
Ruleset name: MeteoKolecko main protection
Ruleset ID: 20575432
Source: eBastlir1492/MeteoKolecko
Source type: Repository
Target: branch
Enforcement: active
Included refs: ~DEFAULT_BRANCH
Excluded refs: none
Bypass actors: none
```

The ruleset contains exactly these two rule types:

```text
Restrict deletions: enabled (deletion)
Block force pushes: enabled (non_fast_forward)
Require a pull request before merging: disabled (pull_request rule absent)
Required status checks: none (required_status_checks rule absent)
```
