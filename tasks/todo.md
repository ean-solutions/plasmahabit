# Task List: PlasmaHabit (`org.eansolutions.plasmahabit`)

See [`tasks/plan.md`](file:///home/Eze/projects/plasmahabit/tasks/plan.md) for architecture decisions, dependency graph, and risk mitigations.

## Phase 1: Foundation & KDE/Qt6 Base Scaffold

- [x] **Task 1: Create Root Build Configuration and FreeDesktop Metadata**
  - **Acceptance:**
    - [x] Root `CMakeLists.txt` enforces CMake ≥ 3.16 and C++20, locates `ECM`, `Qt6`, and `KF6` packages, and includes `KDEInstallDirs`, `KDECompilerSettings`, and `KDECMakeSettings`.
    - [x] `org.eansolutions.plasmahabit.desktop` defines a valid desktop entry (`Type=Application`, `Exec=plasmahabit`, `Categories=Qt;KDE;Utility;`).
    - [x] `org.eansolutions.plasmahabit.metainfo.xml` defines AppStream component metadata for `org.eansolutions.plasmahabit` under developer `eansolutions`.
  - **Verify:** `cmake -B build -DCMAKE_BUILD_TYPE=Debug`
  - **Dependencies:** None
  - **Files:** `CMakeLists.txt`, `org.eansolutions.plasmahabit.desktop`, `org.eansolutions.plasmahabit.metainfo.xml` (Medium: 3 files)

- [ ] **Task 2: Create Application Entry Point, QRC Resources, and Kirigami Window Shell**
  - **Acceptance:**
    - [ ] `src/main.cpp` sets organization name `eansolutions`, domain `eansolutions.org`, app name `plasmahabit`, configures `KLocalizedString`, and safely loads `qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml`.
    - [ ] `src/resources.qrc` registers `ui/Main.qml` under prefix `/qt/qml/org/eansolutions/plasmahabit`.
    - [ ] Project compiles cleanly and starts in offscreen mode with `Main.qml` instantiating a valid `Kirigami.ApplicationWindow`.
  - **Verify:** `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel && (QT_QPA_PLATFORM=offscreen timeout 2s ./build/bin/plasmahabit || [ $? -eq 124 ])`
  - **Dependencies:** Task 1
  - **Files:** `src/CMakeLists.txt`, `src/main.cpp`, `src/resources.qrc`, `src/ui/Main.qml` (Medium: 4 files)

### Checkpoint: Foundation (After Tasks 1–2)
- [ ] Root and `src/` CMake targets configure and build without errors or warnings
- [ ] `plasmahabit` binary loads `qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml` and verifies root object creation
- [ ] Review with human before proceeding if needed

---

## Phase 2: Domain Streak Engine & Local SQLite Persistence

- [ ] **Task 3: Implement and Unit-Test Daily & Weekly Streak Calculator**
  - **Acceptance:**
    - [ ] Daily habits keep their streak alive through `today` if `today - 1` was completed, increment on `today` completion, and strictly reset `currentStreak` to `0` if `today - 1` was missed and `today` is uncompleted.
    - [ ] Weekly habits (`1..7` per Mon–Sun week) count consecutive weeks meeting `>= N` completions, keep prior streak alive mid-week while `c_cur + r_cur >= N`, and strictly reset `currentStreak` to `0` immediately when `c_cur + r_cur < N` or when the prior week finished `< N`.
    - [ ] Retroactive date additions/removals in the completion set accurately recompute both `currentStreak` and `longestStreak`.
  - **Verify:** `cmake --build build --parallel && ctest --test-dir build -R tst_streakcalculator --output-on-failure`
  - **Dependencies:** Task 2
  - **Files:** `src/domain/StreakCalculator.h`, `src/domain/StreakCalculator.cpp`, `tests/CMakeLists.txt`, `tests/tst_streakcalculator.cpp` (Medium: 4 files)

- [ ] **Task 4: Implement and Integration-Test Local SQLite Database Manager**
  - **Acceptance:**
    - [ ] `DatabaseManager` creates the SQLite file and schema automatically with `PRAGMA foreign_keys = ON` and `PRAGMA journal_mode = WAL`, using parameterized `QSqlQuery` statements exclusively.
    - [ ] Toggling a date (`<= today`) inserts a completion if absent or deletes it if present, while rejecting future dates (`> today`).
    - [ ] Archiving sets `is_archived = 1` without deleting any `habit_completions` records for that habit.
  - **Verify:** `cmake --build build --parallel && ctest --test-dir build -R tst_databasemanager --output-on-failure`
  - **Dependencies:** Task 2
  - **Files:** `src/db/DatabaseManager.h`, `src/db/DatabaseManager.cpp`, `tests/CMakeLists.txt`, `tests/tst_databasemanager.cpp` (Medium: 4 files)

### Checkpoint: Domain & Persistence (After Tasks 3–4)
- [ ] All unit and integration tests (`tst_streakcalculator` and `tst_databasemanager`) pass via `ctest`
- [ ] Application builds cleanly with zero SQL injection vectors or unparameterized queries
- [ ] Review with human before proceeding if needed

---

## Phase 3: Vertical Feature Slices (Qt Model + Kirigami UI)

- [ ] **Task 5: Expose Habit List Model with Create & Archive Actions**
  - **Acceptance:**
    - [ ] `HabitModel` exposes habit rows filtered by `showArchived` state and computes live `currentStreak` and `longestStreak` via `StreakCalculator`.
    - [ ] Calling `addHabit(name, frequencyType, targetCount, colorHex)` and `setHabitArchived(id, archived)` persists changes in SQLite and emits model update signals immediately.
    - [ ] `src/main.cpp` initializes `DatabaseManager` in `QStandardPaths::AppLocalDataLocation` and exposes `HabitModel` to `Main.qml`.
  - **Verify:** `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
  - **Dependencies:** Task 3, Task 4
  - **Files:** `src/models/HabitModel.h`, `src/models/HabitModel.cpp`, `src/CMakeLists.txt`, `src/main.cpp` (Medium: 4 files)

- [ ] **Task 6: Build Per-Habit Binary Heatmap & Retroactive Completion Toggling in QML**
  - **Acceptance:**
    - [ ] Each habit card displays its 52-week Monday–Sunday binary heatmap colored with the habit's accent color for completed days and muted cells for uncompleted/future days.
    - [ ] Clicking today or any past cell on a habit's heatmap (or the "Done Today" button) toggles the SQLite record and immediately updates the cell color, `currentStreak`, and `longestStreak`.
    - [ ] Clicking future cells (`> today`) in the current week is disabled and does not record a completion.
  - **Verify:** `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
  - **Dependencies:** Task 5
  - **Files:** `src/models/HabitModel.h`, `src/models/HabitModel.cpp`, `src/ui/Main.qml` (Medium: 3 files)

- [ ] **Task 7: Build Global Combined Intensity Heatmap, Create Habit Dialog, and Archive View**
  - **Acceptance:**
    - [ ] The top Global Combined Heatmap renders a 52-week Monday–Sunday grid whose cell color intensity (`0..4`) updates immediately whenever any active habit's completion is toggled.
    - [ ] The "+ New Habit" dialog validates non-empty habit names, allows configuring `Daily (1x/day)` or `Weekly (1..7/week)` and color, and adds the habit to the dashboard immediately.
    - [ ] Users can switch between Active and Archived habits, archive/unarchive habits without losing completion history, and see a helpful `Kirigami.PlaceholderMessage` when no habits exist.
  - **Verify:** `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
  - **Dependencies:** Task 6
  - **Files:** `src/models/HabitModel.h`, `src/models/HabitModel.cpp`, `src/ui/Main.qml` (Medium: 3 files)

### Checkpoint: Complete (After Tasks 5–7)
- [ ] All unit and integration tests pass (`ctest --test-dir build --output-on-failure`)
- [ ] Application builds cleanly and starts without QML or SQLite errors
- [ ] All acceptance criteria in `SPEC.md` are met
- [ ] Ready for human review
