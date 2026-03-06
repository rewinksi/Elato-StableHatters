# AGENTS.md

## Repository Workflow

### Branching Strategy
- `main` is reserved as a clean mirror of the upstream Elato project.
- Do **not** commit customizations, local experiments, or feature work directly to `main`.
- All custom work must be done on purpose-specific branches (for example: `feature/...`, `custom/...`, `fix/...`, `hotfix/...`).

### Remote Conventions
- `upstream` points to the original Elato project repository.
- `origin` points to this fork/custom repository.

If `upstream` is not set, add it once:
```bash
git remote add upstream <upstream-elato-repo-url>
```

## Daily Git Workflows

### 1) Sync `main` with upstream mirror
```bash
git checkout main
git fetch upstream
git reset --hard upstream/main
git push origin main --force-with-lease
```

Notes:
- This intentionally keeps `main` identical to `upstream/main`.
- `--force-with-lease` is expected for mirror updates to `origin/main`.

### 2) Start customization or feature work
Always branch from the latest mirrored `main`:
```bash
git checkout main
git fetch upstream
git reset --hard upstream/main
git checkout -b feature/<short-name>
```

Examples:
```bash
git checkout -b custom/stablehatters-ui
git checkout -b feature/telemetry-improvements
git checkout -b fix/connection-timeout
```

### 3) Keep a working branch updated with upstream changes
```bash
git fetch upstream
git checkout <your-branch>
git rebase upstream/main
```

If preferred for shared branches, merge instead of rebase:
```bash
git fetch upstream
git checkout <your-branch>
git merge upstream/main
```

### 4) Publish a new working branch
```bash
git push -u origin <your-branch>
```

### 5) Finish and integrate custom work
- Open a PR from your working branch into the destination custom branch (or into `main` only if intentionally updating mirror behavior).
- Keep commits scoped and descriptive.

## Guardrails
- Treat `main` as read-only for custom development.
- Never open customization PRs directly from `main`.
- Before starting work, confirm you are **not** on `main`:
```bash
git branch --show-current
```
- Before committing, review changes:
```bash
git status
git diff --stat
```

## Suggested Branch Naming
- `feature/<capability>` for net-new user-facing work
- `custom/<project-area>` for fork-specific behavior
- `fix/<bug-name>` for bug fixes
- `chore/<maintenance-task>` for tooling/docs/maintenance

## One-Time Setup Checklist
```bash
git remote -v
git remote add upstream <upstream-elato-repo-url>   # if missing
git fetch upstream
git checkout main
git reset --hard upstream/main
git push origin main --force-with-lease
```

## Quick Start (Most Common Path)
```bash
git checkout main
git fetch upstream
git reset --hard upstream/main
git checkout -b feature/<short-name>
```

## Work Habits
- After every "chunk" of work (every bug fix or feature addition) run `git status`, stage the changes, and create an atomic commit so history reflects each discrete step.
- At the start of each new daily session prompt for a fresh `git pull`/`git status` check before diving into changes to ensure we build on the latest state.
