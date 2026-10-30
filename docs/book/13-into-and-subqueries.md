# Lesson 13: Into-Temp Tables and Subqueries

## Goal

Let one query feed the next without touching disk:

- `seek ... into <tmp>` stashes the answer as a session-temp table
- temp tables query exactly like real ones (`seek`, `join`, `change`, `export`)
- temp tables never reach snapshots: saving skips them, reloading forgets them
- `seek (seek ...) ...` runs a parenthesized subquery as the FROM source
- subquery columns are addressed bare; join keys use `(subquery).col`

This lesson answers the oldest ExdeusQL gap: results used to be
transient — printed, then gone. Now they can be named and reused.

## Syntax

```text
seek customers show name, balance into rich;
seek rich where balance above 10000 order by balance descending;

seek (seek customers show name) show name order by name;

seek (seek customers show id, name)
  inner join accounts on (subquery).id equals accounts.customer_id
  show name into holders;
seek holders;
```

## Line-by-line connections

`SeekRows` gained two members: `into_table` (`std::optional<std::string>`)
and `from_query` (`std::shared_ptr<SeekRows>`). The parser fills `into`
only at the end of a seek (`... order by a into tmp;`), and `from_query`
only from `( seek ... )` in the FROM slot. Nesting caps at 8 deep in both
parser and interpreter — `((((((((((` fails fast instead of blowing the
call stack.

`Table` gained a `bool temp_` flag (default false, so every existing
construction is unchanged). `Engine::create_temp_table` forges then flags;
`Engine::is_temp` reports it; `rename_table` preserves it. The flag lives
on the table, not in a side registry, so copies and renames cannot lose it.

`run_seek` split into three: `run_seek` (public, returns the `Result`),
`run_seek_depth` (resolves the FROM source — catalog table or a recursive
subquery materialized in memory), and `run_seek_against` (the old seek body:
columns map, join, where, order, projection). Outer queries read a
materialized subquery through the exact same code path as catalog tables,
which is why filters, joins, ordering, and even `into` compose freely.

`materialize_temp` builds the scratch table: bare display names (qualifiers
stripped: `customers.name` -> `name`), exact source types per column (so an
empty answer still forges correct types), everything nullable, no checks,
no keys. Two display columns stripping to the same bare name fail loudly
(`column 'id' is ambiguous: narrow the projection`) instead of guessing —
this is the one place a wide `show *` join cannot go straight into temp.

Temp tables skip snapshots: `SnapshotStore::save` ignores them, and saving
a database that holds *only* temps throws (`no persistent tables to save`)
instead of writing a file that can never reload. Export allows temps —
dumping scratch to CSV is harmless, persisting it silently is not. Permanent
tables cannot be forged with `into` twice (duplicate-name error, no
clobber), and `into` itself is rejected as a target (`seek t into t` fails
the same way).

Joining against a subquery spells its side `(subquery).col`:
`on (subquery).id equals customers.id`. Bare names in `where`, `show`, and
`order by` resolve against the materialized columns left-first, exactly like
a real table. Only Equals is allowed on join keys — the Lesson-6 rule, now
shared by both source kinds.

## Build and test

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

New checks: `into` lexes; `into` + subqueries parse (nesting, missing name,
unclosed paren, bare-FROM errors); `into` stashes, re-queries, refuses to
clobber, and survives empty answers; subqueries filter, order, join, and
stack into temps; temps vanish from snapshots while data survives; temp-only
save fails loudly.

## Completion checkpoint

Do not continue until you can explain:

- why temp tables are flagged on `Table` instead of tracked in a side list
- why `into` refuses to overwrite instead of replacing
- why materialization strips qualifiers — and when that fails
- why saving skips temps but exporting allows them
- why `(subquery).col` exists for join keys but bare names work elsewhere
- why nesting caps at 8 in two separate places
