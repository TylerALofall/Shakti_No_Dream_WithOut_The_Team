# Global-Hasher

shakti-hasher — the global hasher and logger. Seal edition.

## THE DRAWING — what it does at a glance

```
            the tree                     the machine                    the record
        ┌──────────────┐          ┌─────────────────────┐
        │  dir/        │          │  1. SELF-TEST       │   sha-256 vectors must pass
        │  ├─ sub/     │          │     or nothing runs │   — a broken hasher never
        │  │  └─ f.c   │   walk   │                     │     touches a tree
        │  └─ g.h      │ ───────► │  2. CHILDREN FIRST  │
        └──────────────┘          │     subdirs sealed  │      file hash ──┐
                                  │     before parents  │                  │ manifest
                                  └─────────────────────┘      dir hash ──┤ lines:
                                                                  ...     │ D child <seal>
                                                                          ▼ F file  <hash>
                          seal(dir) = sha256(manifest)          sha256 ───┴──────────

        check law:  MATCH  -> silent, move on
                    STALE  -> flag loud, both hashes shown       ──►  00-repair_log.txt
                    NEW    -> unsealed, counted, not flagged          (append-only,
                                                                       one event per line)

        ready law:  dirty tree (registry FAIL / MISSING / STALE) -> exit 2, NOT sealed.
                    "shall not be posted until ready."

        writes only:  <every dir>/00-seal.sha256   (one 64-hex line)
                      <root>/00-master_seal.sha256 (the fingerprint of the tree)
                      <root>/00-repair_log.txt     (append-only events)
        never hashes: its own three artifacts, .git
```

## USAGE

```
shakti-hasher ROOT_DIR REGISTRY                 check only — writes nothing
shakti-hasher ROOT_DIR REGISTRY --date D --write    seal the ready tree
shakti-hasher --self-test
```

Build: `cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Global.Hasher.c -o shakti-hasher`

Exit 0: tree matches its seals and registry. Exit 2: FAIL / MISSING / STALE / bench.

## PROOF (clean-room, 2026-10-02, verbatim runs)

- seal 5 dirs: root seal `432a2ee7ae08dd3b6d38a8dc29baacf0cb0f373ea580798bc2d58ef8493773f1`
- re-check: 5/5 MATCH, exit 0, nothing rewritten
- tamper one deep file (`a/b/c/f3.txt`): STALE fired on `a/b/c`, `a/b`, `a`, and root — the break climbs the whole chain
- repair log: append-only, old seal and new seal on every event line
- -O0 and -O2 builds produce byte-identical root seals (`84ed9c39…60fd5dc` after reseal)
- check mode writes nothing; `--write` refuses without `--date` (no clock reads)

## KNOWN LIMITS

Caps (BENCH loud, never truncate): 4096 files per dir, 512 subdirs per dir, 32 levels deep,
8192 registry lines, 1024-byte paths, 256-byte names. Date is supplied, never read from a clock.
Seal file is one line; the manifest that produced it is recorded in the master seal.
