# Task List: PlasmaHabit (`org.eansolutions.plasmahabit`)

See [`tasks/plan.md`](file:///home/Eze/projects/plasmahabit/tasks/plan.md) for architecture decisions, dependency graph, and risk mitigations.

## Completed Phases (Phases 1–3: Tasks 1–7)

- [x] **Task 1: Create Root Build Configuration and FreeDesktop Metadata**
- [x] **Task 2: Create Application Entry Point, QRC Resources, and Kirigami Window Shell**
- [x] **Task 3: Implement and Unit-Test Daily & Weekly Streak Calculator**
- [x] **Task 4: Implement and Integration-Test Local SQLite Database Manager**
- [x] **Task 5: Expose Habit List Model with Create & Archive Actions**
- [x] **Task 6: Build Per-Habit Binary Heatmap & Retroactive Completion Toggling in QML**
- [x] **Task 7: Build Global Combined Intensity Heatmap, Create Habit Dialog, and Archive View**

---

## Phase 4: Configurable Week Start in Domain & SQLite Persistence

- [x] **Task 8: Generalize `StreakCalculator` for Configurable Week Start (`Qt::Monday` / `Qt::Sunday`)**
  - **Acceptance:**
    - [x] `StreakCalculator::calculate` accepts `Qt::DayOfWeek weekStartDay = Qt::Monday` and produces identical results to existing behavior when `Qt::Monday` is passed.
    - [x] When `Qt::Sunday` is passed, 7-day weeks span Sunday (`offset 0`) through Saturday (`offset 6`), and a completion on Sunday counts toward the new Sunday–Saturday week rather than the preceding week.
    - [x] Unit tests in `tests/tst_streakcalculator.cpp` verify Sunday-start consecutive weekly streaks, mid-week reachability/impossibility, and Monday vs. Sunday boundary differences.
  - **Verify:** `cmake --build build --parallel && ctest --test-dir build -R tst_streakcalculator --output-on-failure`
  - **Dependencies:** Tasks 1–7
  - **Files:** `src/domain/StreakCalculator.h`, `src/domain/StreakCalculator.cpp`, `tests/tst_streakcalculator.cpp` (Small: 3 files)

- [x] **Task 9: Add `app_settings` Table and `weekStartDay` Persistence to `DatabaseManager`**
  - **Acceptance:**
    - [x] `DatabaseManager::initialize` creates the `app_settings` table automatically on new and existing SQLite databases.
    - [x] `DatabaseManager::weekStartDay()` returns `Qt::Monday` by default when no setting is stored or if an invalid value exists in the table.
    - [x] `DatabaseManager::setWeekStartDay(Qt::Sunday)` and `setWeekStartDay(Qt::Monday)` persist across database close/reopen and reject unsupported `Qt::DayOfWeek` values safely.
  - **Verify:** `cmake --build build --parallel && ctest --test-dir build -R tst_databasemanager --output-on-failure`
  - **Dependencies:** Tasks 1–7
  - **Files:** `src/db/DatabaseManager.h`, `src/db/DatabaseManager.cpp`, `tests/tst_databasemanager.cpp` (Small: 3 files)

### Checkpoint: Domain & Persistence Week Start (After Tasks 8–9)
- [ ] `tst_streakcalculator` and `tst_databasemanager` pass 100% via `ctest --test-dir build --output-on-failure`
- [ ] Application builds cleanly with zero warnings
- [ ] Review with human before proceeding if needed

---

## Phase 5: Heatmap Axes, Color Scale & General Settings UI

- [x] **Task 10: Expose `weekStartDay`, `dayLabels`, and `monthLabels` in `HabitModel`**
  - **Acceptance:**
    - [x] `HabitModel` exposes `weekStartDay`, `dayLabels`, and `monthLabels` with change notifications (`weekStartDayChanged`, `globalHeatmapChanged`) and persists `weekStartDay` changes via `DatabaseManager`.
    - [x] Changing `weekStartDay` between `"monday"` and `"sunday"` immediately shifts heatmap cell dates (`dayOfWeek` 0..6), updates `dayLabels` (`Mon`/`Wed`/`Fri` at indices `0, 2, 4` vs `1, 3, 5`), updates `monthLabels`, and recomputes weekly progress and streaks.
    - [x] `tests/tst_habitmodel.cpp` verifies `dayLabels`, `monthLabels` length/content, heatmap cell start-of-week alignment, and reactive weekly streak/progress updates when `weekStartDay` changes.
  - **Verify:** `cmake --build build --parallel && ctest --test-dir build -R tst_habitmodel --output-on-failure`
  - **Dependencies:** Task 8, Task 9
  - **Files:** `src/models/HabitModel.h`, `src/models/HabitModel.cpp`, `tests/tst_habitmodel.cpp` (Small: 3 files)

- [ ] **Task 11: Update `Main.qml` with GitHub Heatmap Axes, Global Green Color Scale, and General Settings Dialog**
  - **Acceptance:**
    - [ ] Both the Global Combined Heatmap and each Per-Habit Binary Heatmap render aligned top month labels (`habitModel.monthLabels`) and left `Mon`/`Wed`/`Fri` day labels (`habitModel.dayLabels`).
    - [ ] The Global Combined Heatmap and its *Less → More* legend use `#0e4429` (intensity 1), `#006d32` (intensity 2), `#26a641` (intensity 3), and `#39d353` (intensity 4), while individual habit heatmaps keep their binary `colorHex`.
    - [ ] Clicking the **"Settings"** toolbar action opens the General Settings dialog (in English) with the **"Start of the week"** (`Monday` / `Sunday`) selector, updating `habitModel.weekStartDay` and all UI elements immediately.
  - **Verify:** `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
  - **Dependencies:** Task 10
  - **Files:** `src/ui/Main.qml`, `tests/tst_startup.cpp` (Small: 2 files)

### Checkpoint: Complete (After Tasks 10–11)
- [ ] All unit, database, model, and startup tests pass (`ctest --test-dir build --output-on-failure`)
- [ ] Application builds cleanly with zero warnings or QML errors
- [ ] All acceptance criteria in [SPEC.md](file:///home/Eze/projects/plasmahabit/SPEC.md) are met
- [ ] Ready for human review
