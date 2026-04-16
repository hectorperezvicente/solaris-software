Create a git commit for the current repository using the user's git profile.

## Steps

1. Run `git status` to see staged and unstaged changes.
2. Run `git diff HEAD` to understand what changed.
3. Run `git log --oneline -5` to match the repo's commit message style.
4. Stage all modified tracked files with `git add -u`, then stage any new untracked files that are clearly part of the current work (use judgment — do not blindly `git add .` if there are build artifacts or unrelated files).
5. Write a short, clear commit message:
   - Imperative mood, no period at the end
   - First line: 50 chars max
   - If more context is needed, add a blank line then a short body (wrap at 72 chars)
   - No bullet lists, no verbose explanations, no headers
6. Commit using the user's configured git identity. Do NOT add any `Co-Authored-By`, `Signed-off-by`, or any other trailer lines. The commit message ends after the subject (and optional body).

## Commit command format

```bash
git commit -m "$(cat <<'EOF'
Subject line here

Optional body here if needed.
EOF
)"
```

## What NOT to do

- Do not add `Co-Authored-By: Claude` or any Anthropic attribution.
- Do not add `Signed-off-by` lines.
- Do not use `--author` to override the git identity.
- Do not amend existing commits unless the user explicitly asks.
- Do not push — only commit locally.
- Do not commit files that look like build artifacts, secrets, or IDE state.
