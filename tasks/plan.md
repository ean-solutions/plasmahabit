# Implementation Plan: PlasmaHabit (`org.eansolutions.plasmahabit`)

## Overview

Build **PlasmaHabit** (`org.eansolutions.plasmahabit`), a single-user, offline Linux desktop habit tracker in **C++20**, **Qt6**, and **KDE Frameworks 6 (Kirigami)** backed by a local on-disk **SQLite** database (`QStandardPaths::AppLocalDataLocation`). We will start with the KDE/CMake base project and application shell, build and test the pure C++20 streak engine and SQLite persistence layer, and then deliver vertical slices connecting the Qt list/heatmap model to the Kirigami dashboard UI (per-habit binary heatmaps, retroactive toggling, global combined intensity heatmap, and habit creation/archiving).

## Dependency Graph

```text
Task 1: Root CMake + Desktop/AppStream Metadata
    │
    └──► Task 2: App Entry Point (main.cpp) + QRC + Kirigami Shell (Main.qml)
             │
             ├──► Task 3: StreakCalculator (Daily & Weekly Mon–Sun Rules + Unit Tests)
             │        │
             └──► Task 4: DatabaseManager (Local SQLite Schema/CRUD + Integration Tests)
                      │
                      └──► Task 5: HabitModel (C++ QAbstractListModel + Create/Archive API)
                               │   (depends on Task 3 + Task 4)
                               ▼
                           Task 6: Per-Habit Binary Heatmap & Retroactive Date Toggling
                               │
                               ▼
                           Task 7: Global Combined Intensity Heatmap, Create Dialog & Archive View
```

## Architecture Decisions

- **Static Core Library (`plasmahabit_core`) + Thin Executable (`plasmahabit`):** Compiling `StreakCalculator`, `DatabaseManager`, and `HabitModel` into an internal static library allows both the main executable (`src/main.cpp`) and `Qt6::Test` binaries (`tests/`) to link against the exact same domain and SQLite code without duplicating compilation units.
- **Pure Functional `StreakCalculator`:** Decoupling streak math from SQLite queries makes daily (`1x/day`) and weekly (`N/week`, Monday–Sunday) edge cases (such as mid-week mathematical impossibility and retroactive past-day toggles) 100% deterministic and fast to unit-test.
- **SQLite WAL Mode & Parameterized Queries:** `DatabaseManager` uses `QSQLITE` with `PRAGMA foreign_keys = ON` and `PRAGMA journal_mode = WAL`, resolving the default database path via `QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)` while accepting an explicit file path for isolated test databases.
- **Consistent Namespace (`org.eansolutions.plasmahabit`):** All CMake targets, desktop metadata, `KLocalizedString` domain (`plasmahabit`), and QRC paths (`/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml`) use the `eansolutions` / `plasmahabit` identifiers.

---

## Task List

### Phase 1: Foundation & KDE/Qt6 Base Scaffold

## Task 1: Create Root Build Configuration and FreeDesktop Metadata

**Description:** Create the root `CMakeLists.txt` with C++20, CMake ≥ 3.16, `ECM`, `Qt6` (`Core`, `Gui`, `Qml`, `QuickControls2`, `Sql`, `Test`), and `KF6` (`CoreAddons`, `I18n`, `Kirigami`), along with the KDE standard modules (`KDEInstallDirs`, `KDECompilerSettings`, `KDECMakeSettings`) and the XDG `.desktop` and AppStream `.metainfo.xml` files for `org.eansolutions.plasmahabit`.

**Acceptance criteria:**
- [ ] Root `CMakeLists.txt` enforces CMake ≥ 3.16 and C++20, locates `ECM`, `Qt6`, and `KF6` packages, and includes `KDEInstallDirs`, `KDECompilerSettings`, and `KDECMakeSettings`.
- [ ] `org.eansolutions.plasmahabit.desktop` defines a valid desktop entry (`Type=Application`, `Exec=plasmahabit`, `Categories=Qt;KDE;Utility;`).
- [ ] `org.eansolutions.plasmahabit.metainfo.xml` defines AppStream component metadata for `org.eansolutions.plasmahabit` under developer `eansolutions`.

**Verification:**
- [ ] Tests pass: `ctest --test-dir build --output-on-failure` (after Task 2 creates `src/CMakeLists.txt`)
- [ ] Build succeeds: `cmake -B build -DCMAKE_BUILD_TYPE=Debug` validates package discovery
- [ ] Manual check: Inspect `CMakeLists.txt`, `.desktop`, and `.metainfo.xml` against KDE Frameworks 6 conventions

**Dependencies:** None

**Files likely touched:**
- `CMakeLists.txt`
- `org.eansolutions.plasmahabit.desktop`
- `org.eansolutions.plasmahabit.metainfo.xml`

**Estimated scope:** Medium: 3 files

---

## Task 2: Create Application Entry Point, QRC Resources, and Kirigami Window Shell

**Description:** Create `src/CMakeLists.txt`, `src/main.cpp`, `src/resources.qrc`, and the initial `src/ui/Main.qml`. Initialize `QGuiApplication`, `KLocalizedString::setApplicationDomain("plasmahabit")`, organization metadata (`eansolutions`, `eansolutions.org`), and `QQmlApplicationEngine`, loading `qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml` and safely verifying that the root object loaded.

**Acceptance criteria:**
- [ ] `src/main.cpp` sets organization name `eansolutions`, domain `eansolutions.org`, app name `plasmahabit`, configures `KLocalizedString`, and connects `QQmlApplicationEngine::objectCreationFailed` / checks `engine.rootObjects().isEmpty()`.
- [ ] `src/resources.qrc` registers `ui/Main.qml` under prefix `/qt/qml/org/eansolutions/plasmahabit`.
- [ ] The project compiles cleanly and starts in offscreen mode with `Main.qml` instantiating a valid `Kirigami.ApplicationWindow`.

**Verification:**
- [ ] Tests pass: `QT_QPA_PLATFORM=offscreen timeout 2s ./build/bin/plasmahabit || [ $? -eq 124 ]`
- [ ] Build succeeds: `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel`
- [ ] Manual check: Launch `./build/bin/plasmahabit` and confirm the Kirigami window opens without QML warnings

**Dependencies:** Task 1

**Files likely touched:**
- `src/CMakeLists.txt`
- `src/main.cpp`
- `src/resources.qrc`
- `src/ui/Main.qml`

**Estimated scope:** Medium: 4 files

---

### Checkpoint: Foundation (After Tasks 1–2)
- [ ] Root and `src/` CMake targets configure and build without errors or warnings
- [ ] `plasmahabit` binary loads `qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml` and verifies root object creation
- [ ] Review with human if any build environment issues arise

---

### Phase 2: Domain Streak Engine & Local SQLite Persistence

## Task 3: Implement and Unit-Test Daily & Weekly Streak Calculator

**Description:** Implement `StreakCalculator` in pure C++20 to compute `currentStreak` and `longestStreak` for both `daily` (`1x/day`) and `weekly` (`N times per week`, Monday–Sunday) habits according to the rules in `SPEC.md`, accompanied by a comprehensive `Qt6::Test` unit test suite.

**Acceptance criteria:**
- [ ] Daily habits keep their streak alive through `today` if `today - 1` was completed, increment on `today` completion, and strictly reset `currentStreak` to `0` if `today - 1` was missed and `today` is uncompleted.
- [ ] Weekly habits (`1..7` per Mon–Sun week) count consecutive weeks meeting `>= N` completions, keep prior streak alive mid-week while `c_cur + r_cur >= N`, and strictly reset `currentStreak` to `0` immediately when `c_cur + r_cur < N` or when the prior week finished `< N`.
- [ ] Retroactive date additions/removals in the completion set accurately recompute both `currentStreak` and `longestStreak`.

**Verification:**
- [ ] Tests pass: `ctest --test-dir build -R tst_streakcalculator --output-on-failure`
- [ ] Build succeeds: `cmake --build build --parallel`
- [ ] Manual check: Review test cases in `tests/tst_streakcalculator.cpp` covering Monday–Sunday boundaries and mid-week impossibility

**Dependencies:** Task 2

**Files likely touched:**
- `src/domain/StreakCalculator.h`
- `src/domain/StreakCalculator.cpp`
- `tests/CMakeLists.txt`
- `tests/tst_streakcalculator.cpp`

**Estimated scope:** Medium: 4 files

---

## Task 4: Implement and Integration-Test Local SQLite Database Manager

**Description:** Implement `DatabaseManager` using `Qt6::Sql` (`QSQLITE`) to initialize the on-disk SQLite schema (`habits` and `habit_completions` tables with `CHECK` constraints, foreign keys, and indexes) in `QStandardPaths::AppLocalDataLocation`, and provide parameterized CRUD operations for creating habits, soft-archiving/unarchiving, deleting, and toggling date completions on or before `today`.

**Acceptance criteria:**
- [ ] `DatabaseManager` creates the SQLite file and schema automatically with `PRAGMA foreign_keys = ON` and `PRAGMA journal_mode = WAL`, using parameterized `QSqlQuery` statements exclusively.
- [ ] Toggling a date (`<= today`) inserts a completion if absent or deletes it if present, while rejecting future dates (`> today`).
- [ ] Archiving sets `is_archived = 1` without deleting any `habit_completions` records for that habit.

**Verification:**
- [ ] Tests pass: `ctest --test-dir build -R tst_databasemanager --output-on-failure`
- [ ] Build succeeds: `cmake --build build --parallel`
- [ ] Manual check: Verify temporary SQLite file creation and persistence across close/reopen in `tst_databasemanager`

**Dependencies:** Task 2

**Files likely touched:**
- `src/db/DatabaseManager.h`
- `src/db/DatabaseManager.cpp`
- `tests/CMakeLists.txt`
- `tests/tst_databasemanager.cpp`

**Estimated scope:** Medium: 4 files

---

### Checkpoint: Domain & Persistence (After Tasks 3–4)
- [ ] All unit and integration tests (`tst_streakcalculator` and `tst_databasemanager`) pass via `ctest`
- [ ] Application builds cleanly with zero SQL injection vectors or unparameterized queries
- [ ] Review with human if schema or streak rules need adjustments

---

### Phase 3: Vertical Feature Slices (Qt Model + Kirigami UI)

## Task 5: Expose Habit List Model with Create & Archive Actions

**Description:** Create `HabitModel` (`QAbstractListModel`) bridging `DatabaseManager` and `StreakCalculator` to QML. Expose roles for habit metadata (`id`, `name`, `frequencyType`, `targetCount`, `colorHex`, `isArchived`, `completedToday`, `currentStreak`, `longestStreak`, `weeklyProgress`) and `Q_INVOKABLE` methods to create, archive, unarchive, and delete habits, registering the model with QML in `src/main.cpp`.

**Acceptance criteria:**
- [ ] `HabitModel` exposes habit rows filtered by `showArchived` state and computes live `currentStreak` and `longestStreak` via `StreakCalculator`.
- [ ] Calling `addHabit(name, frequencyType, targetCount, colorHex)` and `setHabitArchived(id, archived)` persists changes in SQLite and emits model update signals immediately.
- [ ] `src/main.cpp` initializes `DatabaseManager` in `QStandardPaths::AppLocalDataLocation` and exposes `HabitModel` to `Main.qml`.

**Verification:**
- [ ] Tests pass: `ctest --test-dir build --output-on-failure`
- [ ] Build succeeds: `cmake --build build --parallel`
- [ ] Manual check: Run headless startup check confirming `HabitModel` initializes SQLite and binds cleanly in QML

**Dependencies:** Task 3, Task 4

**Files likely touched:**
- `src/models/HabitModel.h`
- `src/models/HabitModel.cpp`
- `src/CMakeLists.txt`
- `src/main.cpp`

**Estimated scope:** Medium: 4 files

---

## Task 6: Build Per-Habit Binary Heatmap & Retroactive Completion Toggling in QML

**Description:** Extend `HabitModel` to provide the 52-week Monday–Sunday binary activity grid per habit and a `Q_INVOKABLE bool toggleDate(int habitId, const QString &isoDate)` action. Update `src/ui/Main.qml` to render active habit cards with streak badges, quick "Done Today" toggle, and an interactive 52-week GitHub-style binary heatmap where clicking today or any past day toggles completion in SQLite and recalculates streaks on the spot.

**Acceptance criteria:**
- [ ] Each habit card displays its 52-week Monday–Sunday binary heatmap colored with the habit's accent color for completed days and muted cells for uncompleted/future days.
- [ ] Clicking today or any past cell on a habit's heatmap (or the "Done Today" button) toggles the SQLite record and immediately updates the cell color, `currentStreak`, and `longestStreak`.
- [ ] Clicking future cells (`> today`) in the current week is disabled and does not record a completion.

**Verification:**
- [ ] Tests pass: `ctest --test-dir build --output-on-failure`
- [ ] Build succeeds: `cmake --build build --parallel`
- [ ] Manual check: Toggle today and past days on a daily and weekly habit card and verify immediate streak recalculation

**Dependencies:** Task 5

**Files likely touched:**
- `src/models/HabitModel.h`
- `src/models/HabitModel.cpp`
- `src/ui/Main.qml`

**Estimated scope:** Medium: 3 files

---

## Task 7: Build Global Combined Intensity Heatmap, Create Habit Dialog, and Archive View

**Description:** Add global 52-week combined heatmap aggregation (`0..4` intensity levels across active habits per day) to `HabitModel`, and complete `src/ui/Main.qml` with the top Global Combined Heatmap card, the Create Habit `Kirigami.Dialog` (supporting `Daily 1x/day` and `Weekly N/week` with color swatches), the Active/Archived view switcher, empty-state `Kirigami.PlaceholderMessage`, and error banner.

**Acceptance criteria:**
- [ ] The top Global Combined Heatmap renders a 52-week Monday–Sunday grid whose cell color intensity (`0..4`) updates immediately whenever any active habit's completion is toggled.
- [ ] The "+ New Habit" dialog validates non-empty habit names, allows configuring `Daily (1x/day)` or `Weekly (1..7/week)` and color, and adds the habit to the dashboard immediately.
- [ ] Users can switch between Active and Archived habits, archive/unarchive habits without losing completion history, and see a helpful `Kirigami.PlaceholderMessage` when no habits exist.

**Verification:**
- [ ] Tests pass: `ctest --test-dir build --output-on-failure`
- [ ] Build succeeds: `cmake --build build --parallel`
- [ ] Manual check: Verify end-to-end flow (create daily & weekly habits, toggle completions on grid, verify global heatmap intensity, archive & restore habit)

**Dependencies:** Task 6

**Files likely touched:**
- `src/models/HabitModel.h`
- `src/models/HabitModel.cpp`
- `src/ui/Main.qml`

**Estimated scope:** Medium: 3 files

---

### Checkpoint: Complete (After Tasks 5–7)
- [ ] All unit and integration tests pass (`ctest --test-dir build --output-on-failure`)
- [ ] Application builds cleanly and starts without QML or SQLite errors
- [ ] All acceptance criteria in `SPEC.md` are met
- [ ] Ready for human review

---

## Parallelization Opportunities

- **Parallelizable after Task 2:** **Task 3** (`StreakCalculator` + unit tests) and **Task 4** (`DatabaseManager` + integration tests) have no mutual dependency and can be developed/tested independently before converging in **Task 5** (`HabitModel`).
- **Sequential chain:** Tasks 1 → 2 must run first to establish the CMake build system; Tasks 5 → 6 → 7 iterate on `HabitModel` and `Main.qml` and must run sequentially.

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| ISO week boundary edge cases (Sunday = day 7 vs Monday = day 1) in weekly streaks | High | Use `QDate::dayOfWeek()` (which follows ISO 8601: `1 = Monday .. 7 = Sunday`) consistently in `StreakCalculator` and cover Monday/Sunday transitions with unit tests in Task 3. |
| Missing QSQLITE driver or uncreated XDG data directory on first launch | High | Ensure `QDir().mkpath(dataDir)` runs before `QSqlDatabase::open()`, verify `QSqlDatabase::isDriverAvailable("QSQLITE")`, and surface errors via `HabitModel::lastError`. |
| QML resource path mismatch preventing window load | High | Enforce `/qt/qml/org/eansolutions/plasmahabit` in `src/resources.qrc` and `qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml` in `src/main.cpp`, verified by headless launch test. |
| Heatmap rendering overhead for 365 cells per habit | Med | Precompute the 52-week cell vector (`QVariantList`) in C++ (`HabitModel`) and only refresh affected roles when a completion is toggled. |

## Open Questions

- None—all requirements and task boundaries are aligned with [SPEC.md](file:///home/Eze/projects/plasmahabit/SPEC.md).
