# Implementation Plan: PlasmaHabit (`org.eansolutions.plasmahabit`)

## Overview

Extend **PlasmaHabit** (`org.eansolutions.plasmahabit`) with GitHub-style heatmap axis labels (3-letter month headers along the top and `Mon`, `Wed`, `Fri` labels along the left of both the global and per-habit heatmaps), update the Global Combined Heatmap and its *Less → More* legend to GitHub's 4-step dark-to-bright green color scale (`#0e4429` → `#006d32` → `#26a641` → `#39d353`), and add a General Settings dialog allowing the user to configure **"Start of the week"** (*Monday* or *Sunday*). Changing the start of the week persists locally in SQLite (`app_settings`) and dynamically updates heatmap row ordering, axis labels, weekly habit progress (`N× / week`), and weekly streak calculations.

## Dependency Graph

```text
Completed Foundation & Core (Tasks 1–7)
    │
    ├──► Task 8: StreakCalculator Week Start Generalization (Qt::Monday / Qt::Sunday + Unit Tests)
    │        │
    └──► Task 9: DatabaseManager app_settings Persistence (week_start_day + Integration Tests)
             │
             └──► Task 10: HabitModel Week Start, Month Labels & Day Labels API (+ Model Tests)
                      │   (depends on Task 8 + Task 9)
                      ▼
                  Task 11: Main.qml GitHub Heatmap Axes, Green Palette & General Settings Dialog (+ Startup Test)
```

## Architecture Decisions

- **Self-Contained SQLite Settings (`app_settings` table):** Storing `week_start_day` (`'monday'` or `'sunday'`, defaulting to `'monday'`) in the existing SQLite database keeps all user state in a single local file under `QStandardPaths::AppLocalDataLocation` and allows isolated `QTemporaryDir` database and model tests without polluting host `QSettings`.
- **Pure Functional `StreakCalculator` with `Qt::DayOfWeek`:** Extending `StreakCalculator::calculate` with `Qt::DayOfWeek weekStartDay = Qt::Monday` preserves backward compatibility for existing callers while making `Monday–Sunday` and `Sunday–Saturday` weekly streak rules 100% deterministic and unit-testable.
- **Precomputed Axis Labels in `HabitModel`:** Exposing `dayLabels` (`QStringList` of 7 strings) and `monthLabels` (`QVariantList` of 52 strings) directly from `HabitModel` keeps QML declarative, avoids date math duplication in QML delegates, and allows direct verification in `tst_habitmodel`.
- **GitHub Dark-to-Bright Green Progression:** Updating the Global Combined Heatmap intensity colors (`1..4`) and legend swatches in `Main.qml` to `#0e4429` → `#006d32` → `#26a641` → `#39d353` makes higher completion ratios visually brighter while preserving per-habit custom `colorHex` on individual binary heatmaps.

---

## Task List

### Completed Phases (Phases 1–3: Tasks 1–7)
- [x] **Task 1:** Create Root Build Configuration and FreeDesktop Metadata
- [x] **Task 2:** Create Application Entry Point, QRC Resources, and Kirigami Window Shell
- [x] **Task 3:** Implement and Unit-Test Daily & Weekly Streak Calculator
- [x] **Task 4:** Implement and Integration-Test Local SQLite Database Manager
- [x] **Task 5:** Expose Habit List Model with Create & Archive Actions
- [x] **Task 6:** Build Per-Habit Binary Heatmap & Retroactive Completion Toggling in QML
- [x] **Task 7:** Build Global Combined Intensity Heatmap, Create Habit Dialog, and Archive View

---

### Phase 4: Configurable Week Start in Domain & SQLite Persistence

## Task 8: Generalize `StreakCalculator` for Configurable Week Start (`Qt::Monday` / `Qt::Sunday`)

**Description:** Update `StreakCalculator::calculate` to accept an optional `Qt::DayOfWeek weekStartDay = Qt::Monday` parameter. Generalize weekly start-of-week calculation (`offset = d.dayOfWeek() - 1` for Monday start; `offset = d.dayOfWeek() % 7` for Sunday start) and remaining-days-in-week calculation (`r_cur`) so weekly streaks and mid-week reachability work accurately for both `Qt::Monday` and `Qt::Sunday`. Add unit tests in `tests/tst_streakcalculator.cpp` covering Sunday-start weekly streaks.

**Acceptance criteria:**
- [ ] `StreakCalculator::calculate` accepts `Qt::DayOfWeek weekStartDay = Qt::Monday` and produces identical results to existing behavior when `Qt::Monday` is passed.
- [ ] When `Qt::Sunday` is passed, 7-day weeks span Sunday (`offset 0`) through Saturday (`offset 6`), and a completion on Sunday counts toward the new Sunday–Saturday week rather than the preceding week.
- [ ] Unit tests in `tests/tst_streakcalculator.cpp` verify Sunday-start consecutive weekly streaks, mid-week reachability/impossibility, and Monday vs. Sunday boundary differences.

**Verification:**
- [ ] Tests pass: `ctest --test-dir build -R tst_streakcalculator --output-on-failure`
- [ ] Build succeeds: `cmake --build build --parallel`
- [ ] Manual check: Inspect `StreakCalculator.cpp` week offset logic for `Qt::Monday` and `Qt::Sunday`

**Dependencies:** Tasks 1–7

**Files likely touched:**
- `src/domain/StreakCalculator.h`
- `src/domain/StreakCalculator.cpp`
- `tests/tst_streakcalculator.cpp`

**Estimated scope:** Small: 3 files

---

## Task 9: Add `app_settings` Table and `weekStartDay` Persistence to `DatabaseManager`

**Description:** Extend `DatabaseManager::initialize` to create the `app_settings` key-value table (`CREATE TABLE IF NOT EXISTS app_settings (key TEXT PRIMARY KEY CHECK (length(trim(key)) > 0), value TEXT NOT NULL);`). Add `weekStartDay() const` (returning `Qt::DayOfWeek`, defaulting to `Qt::Monday`) and `setWeekStartDay(Qt::DayOfWeek day)` using parameterized `INSERT OR REPLACE` queries to persist `'monday'` or `'sunday'`. Add integration tests in `tests/tst_databasemanager.cpp`.

**Acceptance criteria:**
- [ ] `DatabaseManager::initialize` creates the `app_settings` table automatically on new and existing SQLite databases.
- [ ] `DatabaseManager::weekStartDay()` returns `Qt::Monday` by default when no setting is stored or if an invalid value exists in the table.
- [ ] `DatabaseManager::setWeekStartDay(Qt::Sunday)` and `setWeekStartDay(Qt::Monday)` persist across database close/reopen and reject unsupported `Qt::DayOfWeek` values safely.

**Verification:**
- [ ] Tests pass: `ctest --test-dir build -R tst_databasemanager --output-on-failure`
- [ ] Build succeeds: `cmake --build build --parallel`
- [ ] Manual check: Verify parameterized SQL in `DatabaseManager.cpp` and test coverage in `tests/tst_databasemanager.cpp`

**Dependencies:** Tasks 1–7

**Files likely touched:**
- `src/db/DatabaseManager.h`
- `src/db/DatabaseManager.cpp`
- `tests/tst_databasemanager.cpp`

**Estimated scope:** Small: 3 files

---

### Checkpoint: Domain & Persistence Week Start (After Tasks 8–9)
- [ ] `tst_streakcalculator` and `tst_databasemanager` pass 100% via `ctest --test-dir build --output-on-failure`
- [ ] Application builds cleanly with zero warnings
- [ ] Review with human if needed

---

### Phase 5: Heatmap Axes, Color Scale & General Settings UI

## Task 10: Expose `weekStartDay`, `dayLabels`, and `monthLabels` in `HabitModel`

**Description:** Extend `HabitModel` with QML properties `weekStartDay` (`QString`: `"monday"` or `"sunday"`, with setter `setWeekStartDay`), `dayLabels` (`QStringList` of 7 strings: `["Mon", "", "Wed", "", "Fri", "", ""]` for Monday start or `["", "Mon", "", "Wed", "", "Fri", ""]` for Sunday start), and `monthLabels` (`QVariantList` of 52 strings containing 3-letter English month names `Jan`..`Dec` at the first week column of each month, avoiding col-0 overlap when col 1 or 2 transitions). Update `reload()`, `computeHeatmapCells()`, and `globalHeatmap()` to use the active `weekStartDay` for week alignment, weekly progress (`weeklyProgress`), and `StreakCalculator::calculate`.

**Acceptance criteria:**
- [ ] `HabitModel` exposes `weekStartDay`, `dayLabels`, and `monthLabels` with change notifications (`weekStartDayChanged`, `globalHeatmapChanged`) and persists `weekStartDay` changes via `DatabaseManager`.
- [ ] Changing `weekStartDay` between `"monday"` and `"sunday"` immediately shifts heatmap cell dates (`dayOfWeek` 0..6), updates `dayLabels` (`Mon`/`Wed`/`Fri` at indices `0, 2, 4` vs `1, 3, 5`), updates `monthLabels`, and recomputes weekly progress and streaks.
- [ ] `tests/tst_habitmodel.cpp` verifies `dayLabels`, `monthLabels` length/content, heatmap cell start-of-week alignment, and reactive weekly streak/progress updates when `weekStartDay` changes.

**Verification:**
- [ ] Tests pass: `ctest --test-dir build -R tst_habitmodel --output-on-failure`
- [ ] Build succeeds: `cmake --build build --parallel`
- [ ] Manual check: Inspect `HabitModel.cpp` month transition logic and model test assertions

**Dependencies:** Task 8, Task 9

**Files likely touched:**
- `src/models/HabitModel.h`
- `src/models/HabitModel.cpp`
- `tests/tst_habitmodel.cpp`

**Estimated scope:** Small: 3 files

---

## Task 11: Update `Main.qml` with GitHub Heatmap Axes, Global Green Color Scale, and General Settings Dialog

**Description:** Update `src/ui/Main.qml` to: (1) render the 52-column month header row (`Jan`..`Dec`) and 7-row left day-of-week axis (`Mon`, `Wed`, `Fri`) on both the Global Combined Heatmap and every Per-Habit Binary Heatmap; (2) update the Global Combined Heatmap cell colors and *Less → More* legend to GitHub's 4-level dark-to-bright green scale (`#0e4429`, `#006d32`, `#26a641`, `#39d353`); (3) update the per-habit grid subtitle to reflect `Mon–Sun` vs `Sun–Sat`; and (4) add a `"Settings"` toolbar action (`configure` icon) and `settingsDialog` (`Kirigami.Dialog`) allowing the user to select **"Start of the week"** (`Monday` / `Sunday`). Extend `tests/tst_startup.cpp` to verify headless QML loading and week-start switching without QML warnings.

**Acceptance criteria:**
- [ ] Both the Global Combined Heatmap and each Per-Habit Binary Heatmap render aligned top month labels (`habitModel.monthLabels`) and left `Mon`/`Wed`/`Fri` day labels (`habitModel.dayLabels`).
- [ ] The Global Combined Heatmap and its *Less → More* legend use `#0e4429` (intensity 1), `#006d32` (intensity 2), `#26a641` (intensity 3), and `#39d353` (intensity 4), while individual habit heatmaps keep their binary `colorHex`.
- [ ] Clicking the **"Settings"** toolbar action opens the General Settings dialog (in English) with the **"Start of the week"** (`Monday` / `Sunday`) selector, updating `habitModel.weekStartDay` and all UI elements immediately.

**Verification:**
- [ ] Tests pass: `ctest --test-dir build --output-on-failure`
- [ ] Build succeeds: `cmake --build build --parallel`
- [ ] Manual check: Run headless startup test (`tst_startup`) and verify zero QML errors when toggling `weekStartDay`

**Dependencies:** Task 10

**Files likely touched:**
- `src/ui/Main.qml`
- `tests/tst_startup.cpp`

**Estimated scope:** Small: 2 files

---

### Checkpoint: Complete (After Tasks 10–11)
- [ ] All unit, database, model, and startup tests pass (`ctest --test-dir build --output-on-failure`)
- [ ] Application builds cleanly with zero warnings or QML errors
- [ ] All acceptance criteria in [SPEC.md](file:///home/Eze/projects/plasmahabit/SPEC.md) are met
- [ ] Ready for human review

---

## Parallelization Opportunities

- **Parallelizable right away:** **Task 8** (`StreakCalculator` + unit tests) and **Task 9** (`DatabaseManager` + integration tests) touch completely disjoint files and can be implemented in parallel before **Task 10** (`HabitModel`).
- **Sequential chain:** **Task 10** depends on Tasks 8 & 9, and **Task 11** (`Main.qml` + `tst_startup.cpp`) depends on Task 10.

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Month header labels overlapping when column 0 starts at the end of a month and column 1 starts a new month | Med | In `HabitModel::monthLabels`, only emit column 0's month label if neither column 1 nor column 2 transitions to a new month; always emit on `weekStartDate.month() != prevWeekStartDate.month()`. |
| Misalignment between left `Mon`/`Wed`/`Fri` labels and the 7 heatmap cells | Med | Match the day-label column height (`11px`) and spacing (`3px`) to the heatmap cell column, offset vertically by the month header row height + spacing. |
| Locale-dependent month/day abbreviations breaking English UI constraint | Low | Use `QLocale::c().monthName(month, QLocale::ShortFormat)` (with `i18n(...)` where appropriate) so month and day abbreviations are consistently English (`Jan`..`Dec`, `Mon`, `Wed`, `Fri`). |

## Open Questions

- None — all requirements and task boundaries are aligned with [SPEC.md](file:///home/Eze/projects/plasmahabit/SPEC.md).
