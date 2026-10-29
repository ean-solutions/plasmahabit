# Spec: PlasmaHabit (`org.eansolutions.plasmahabit`)

## Objective

Build **PlasmaHabit** (`org.eansolutions.plasmahabit`), a fast, privacy-first, single-user native Linux desktop habit tracker for Fedora KDE Plasma written in **C++20**, **Qt6**, and **KDE Frameworks 6 (Kirigami)**.

The application persists all habit definitions and daily completion records exclusively in a local on-disk **SQLite** database (`QStandardPaths::AppLocalDataLocation`), renders a global combined contribution heatmap alongside per-habit binary GitHub-style heatmaps, allows creating and archiving habits on the fly with either a **daily** (`1x/day`) or **flexible weekly** (`N times per week`, Monday–Sunday) target, supports toggling today's or past days' completions directly on each habit's activity grid, and calculates strict, honest streaks that reset to `0` whenever a required day or week is missed.

### User Stories
- **US-1 (Create Habit):** As a user, I can create a new habit with a name, frequency type (`daily` at `1x/day` or `weekly` at `1..7` times per Monday–Sunday week), and optional accent color so I can start tracking it immediately.
- **US-2 (Toggle Completion on Heatmap):** As a user, I can click today's cell or any past date's cell on a habit's binary heatmap (or a quick "Complete Today" button) to toggle completion on/off, immediately updating the SQLite database, the habit's streak, and the global heatmap.
- **US-3 (Per-Habit & Global Heatmaps):** As a user, I can view a binary 52-week GitHub-style contribution grid (Monday–Sunday rows, weekly columns) for each active habit, plus a global combined heatmap at the top whose cell color intensity (`0..4` levels) reflects the ratio/count of active habits completed on each date.
- **US-4 (Honest Streaks):** As a user, I can see both the **current streak** and **longest streak** for each habit, computed strictly from completion history with zero grace passes or streak freezes.
- **US-5 (Archive / Unarchive Habit):** As a user, I can archive a habit so it leaves my active workspace without deleting its SQLite completion history, and optionally view or restore archived habits later.

---

## Tech Stack

- **Language:** C++20 (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`)
- **Build System:** CMake ≥ 3.16 + Extra CMake Modules (`ECM`) with `KDEInstallDirs`, `KDECompilerSettings`, `KDECMakeSettings`
- **Frameworks & Libraries:**
  - **Qt6:** `Core`, `Gui`, `Qml`, `QuickControls2`, `Sql`, `Test`
  - **KDE Frameworks 6 (KF6):** `CoreAddons`, `I18n`, `Kirigami`
- **Database:** Local SQLite 3 via `QSqlDatabase` (`QSQLITE` driver)
- **Application Metadata:**
  - Organization Name: `eansolutions`
  - Organization Domain: `eansolutions.org`
  - Application Name / Desktop ID: `plasmahabit` / `org.eansolutions.plasmahabit`
  - QML Resource Prefix: `qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml`

---

## Local Storage & DB Data Model

All data stays strictly on the local device. Zero network sockets or telemetry libraries are linked or invoked.

### Database File Location
- Resolved at startup via `QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)` (typically `~/.local/share/eansolutions/plasmahabit/plasmahabit.sqlite3`).
- Parent directory is created automatically via `QDir().mkpath(...)` before opening the connection.
- Connection executes `PRAGMA foreign_keys = ON;` and `PRAGMA journal_mode = WAL;` on open.

### Schema Definition

```sql
CREATE TABLE IF NOT EXISTS habits (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT    NOT NULL CHECK (length(trim(name)) > 0),
    frequency_type  TEXT    NOT NULL CHECK (frequency_type IN ('daily', 'weekly')),
    target_count    INTEGER NOT NULL CHECK (
                        (frequency_type = 'daily'  AND target_count = 1) OR
                        (frequency_type = 'weekly' AND target_count BETWEEN 1 AND 7)
                    ),
    color_hex       TEXT    NOT NULL DEFAULT '#2ecc71',
    is_archived     INTEGER NOT NULL DEFAULT 0 CHECK (is_archived IN (0, 1)),
    created_at      TEXT    NOT NULL DEFAULT (date('now', 'localtime'))
);

CREATE TABLE IF NOT EXISTS habit_completions (
    habit_id        INTEGER NOT NULL,
    completed_date  TEXT    NOT NULL CHECK (completed_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),
    PRIMARY KEY (habit_id, completed_date),
    FOREIGN KEY (habit_id) REFERENCES habits(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_completions_date
    ON habit_completions(completed_date);

CREATE INDEX IF NOT EXISTS idx_habits_archived
    ON habits(is_archived);
```

### Data Invariants
1. **Binary Daily Completion per Habit:** A habit is either completed or not completed on a given `YYYY-MM-DD` local calendar date (`PRIMARY KEY (habit_id, completed_date)`). Toggling inserts a row if absent or deletes the row if present.
2. **Future Dates Disallowed:** `completed_date > today` (in local system time) is rejected by the C++ domain layer.
3. **Soft Archiving:** Setting `is_archived = 1` hides the habit from the active habit list and active global heatmap view while preserving all rows in `habit_completions`.

---

## Streak Rules

All streaks are deterministic pure functions of `(frequency_type, target_count, set<QDate> completions, QDate today)` and are recomputed whenever any completion is toggled (including retroactive edits to past dates).

### 1. Daily Habits (`frequency_type = 'daily'`, `target_count = 1`)
- **Unit:** Days (`d`).
- **Today Completed (`today ∈ completions`):**
  - Walk backward day-by-day starting from `today` (`today, today - 1, today - 2, ...`) and count consecutive dates present in `completions`.
- **Today Not Yet Completed (`today ∉ completions`):**
  - The streak stays **alive** through the end of `today` if `today - 1 ∈ completions`.
  - Walk backward day-by-day starting from `today - 1` (`today - 1, today - 2, ...`) and count consecutive dates present in `completions`.
  - If `today - 1 ∉ completions`, then yesterday was missed and the current streak is strictly `0`.
- **Missed Day Reset:** As soon as any calendar day `< today` has no completion record, no streak can span across that date; if `today - 1` is missed and `today` is not completed, current streak is `0`.
- **Longest Streak:** Maximum length of any contiguous run of consecutive calendar dates in `completions`.

### 2. Flexible Weekly Habits (`frequency_type = 'weekly'`, `target_count = N` where `1 ≤ N ≤ 7`)
- **Week Boundary:** ISO Monday–Sunday week (`Monday = day 1`, `Sunday = day 7`). Let `W(today)` be the current Monday–Sunday week and `W - 1, W - 2, ...` be preceding weeks.
- **Unit:** Consecutive successful weeks (`w`), where week `W_k` is **successful** iff `completions_in(W_k) >= N`.
- **Current Week Viability Check (Mid-Week Missed Reset):**
  - Let `c_cur = completions_in(W(today))` (completions on dates in `W(today)` up to `today`).
  - Let `r_cur` be the number of remaining days in `W(today)` including `today` if `today` is not yet completed:
    - If `today ∈ completions`: `r_cur = 7 - dayOfWeek(today)` (days strictly after `today` through Sunday).
    - If `today ∉ completions`: `r_cur = 7 - dayOfWeek(today) + 1` (today through Sunday).
  - **Rule A — Current week already met (`c_cur >= N`):**
    - Current week counts as `+1` toward the current streak, plus all consecutive immediately preceding weeks `W - 1, W - 2, ...` that satisfied `completions_in(W_k) >= N`.
  - **Rule B — Current week still mathematically possible (`c_cur < N` and `c_cur + r_cur >= N`):**
    - The streak from prior weeks remains **alive**: current streak equals the number of consecutive immediately preceding weeks `W - 1, W - 2, ...` that satisfied `completions_in(W_k) >= N`.
  - **Rule C — Current week mathematically impossible (`c_cur + r_cur < N`):**
    - Because there are not enough days left in the Monday–Sunday week to reach `N`, the current streak strictly resets to **`0`** immediately.
- **Missed Prior Week Reset:** If `W - 1` had `< N` completions, no prior week can carry into `W(today)` (current streak is `1` if `c_cur >= N`, else `0`).
- **Longest Weekly Streak:** Maximum number of consecutive ISO Monday–Sunday weeks in history where `completions_in(W_k) >= N`.

---

## Core Screens and UI States

The UI is a single-window Kirigami application (`Kirigami.ApplicationWindow` + `Kirigami.ScrollablePage`) located in [`src/ui/Main.qml`](file:///home/Eze/projects/plasmahabit/src/ui/Main.qml).

### Screen 1: Main Dashboard (Active View)
1. **Header / Global Toolbar:**
   - Application title (`PlasmaHabit`), total active habits badge, **"+ New Habit"** primary action button, and **"Archived Habits"** filter toggle.
2. **Global Combined Heatmap Card:**
   - Displays the last 52 weeks (Monday–Sunday rows × 52/53 week columns) aggregated across all active habits.
   - Cell intensity level (`0` = 0 completions, `1` = 1–25%, `2` = 26–50%, `3` = 51–75%, `4` = 76–100% of active habits completed on that date).
   - Hover tooltip showing `YYYY-MM-DD: X completed`.
3. **Active Habits List (`ListView` / `Repeater` of Kirigami Cards):**
   - Each habit card displays:
     - **Header row:** Habit title, frequency badge (`Daily` or `N× / week`), current streak pill (`🔥 X day/week streak`), longest streak label (`Best: Y`), quick **"Done Today"** check button, and **"Archive"** action button.
     - **Per-Habit Binary Heatmap:** 52-week interactive Monday–Sunday grid where each cell represents one local calendar date (`<= today`).
       - Clicking any cell on or before `today` toggles that date's completion in SQLite and immediately updates both heatmaps and streak counters.
       - Future cells in the current week (after `today`) are rendered muted/disabled.

### Screen 2: Create Habit Dialog (`Kirigami.Dialog` / `OverlaySheet`)
- **Fields:**
  - Habit Name (`QQC2.TextField`, required, non-empty).
  - Frequency Type (`QQC2.ComboBox` or segmented buttons: `Daily (1x/day)` vs `Weekly (N/week)`).
  - Weekly Target SpinBox (`1..7`, visible/enabled only when `Weekly` is selected).
  - Accent Color Picker (preset KDE Breeze-harmonious palette swatches).
- **Validation:** Save button is disabled when `name.trim()` is empty.

### Screen 3: Archived Habits View / Mode
- Toggling "Show Archived" lists archived habits with their read-only historical heatmap, final/longest streak stats, an **"Unarchive"** action, and a **"Delete Permanently"** confirmation action.

### UI States
- **Empty State:** When 0 active habits exist, displays `Kirigami.PlaceholderMessage` with an action button to create the first habit.
- **Database Error Banner:** `Kirigami.InlineMessage` (type `Error`) at the top of the page if a SQLite write or constraint check fails.

---

## Commands

```bash
# Configure (Debug + Tests enabled)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON

# Build all targets
cmake --build build --parallel

# Run automated test suite via CTest
ctest --test-dir build --output-on-failure

# Run the application locally
./build/bin/plasmahabit
```

---

## Project Structure

```text
plasmahabit/
├── CMakeLists.txt                           # Root CMake (C++20, Qt6, ECM, KF6, tests)
├── SPEC.md                                  # Living specification (this file)
├── org.eansolutions.plasmahabit.desktop     # XDG desktop entry
├── org.eansolutions.plasmahabit.metainfo.xml# AppStream metadata
├── src/
│   ├── CMakeLists.txt                       # Core static lib + executable target
│   ├── main.cpp                             # QGuiApplication, KLocalizedString, QQmlApplicationEngine
│   ├── resources.qrc                        # QRC registering /qt/qml/org/eansolutions/plasmahabit/ui/Main.qml
│   ├── db/
│   │   ├── DatabaseManager.h                # SQLite connection, schema init, CRUD queries
│   │   └── DatabaseManager.cpp
│   ├── domain/
│   │   ├── StreakCalculator.h               # Pure C++20 daily & weekly streak calculation
│   │   └── StreakCalculator.cpp
│   ├── models/
│   │   ├── HabitModel.h                     # QAbstractListModel + Q_INVOKABLE actions for QML
│   │   └── HabitModel.cpp
│   └── ui/
│       └── Main.qml                         # Kirigami dashboard, global + per-habit heatmaps, dialogs
└── tests/
    ├── CMakeLists.txt                       # Qt6::Test executables registered with CTest
    ├── tst_streakcalculator.cpp             # Unit tests for daily & weekly streak edge cases
    └── tst_databasemanager.cpp              # Integration tests for SQLite CRUD & toggling
```

---

## Code Style

- **Standard:** Modern C++20 (`std::optional`, `std::span`, `constexpr`, range algorithms where clear).
- **Naming Conventions:**
  - Classes / Structs / QML files: `PascalCase` (`StreakCalculator`, `HabitModel`, `Main.qml`)
  - Methods / Functions / Variables / QML properties: `camelCase` (`toggleCompletion`, `currentStreak`)
  - Member variables: `m_camelCase` (`m_db`, `m_habits`)
  - SQLite identifiers: `snake_case` (`habit_completions`, `completed_date`)
- **Formatting:** 4-space indentation, braces on same line for control blocks/functions, `#pragma once` in headers.

```cpp
#pragma once

#include <QDate>
#include <QSet>
#include <QString>

namespace PlasmaHabit {

enum class FrequencyType {
    Daily,
    Weekly
};

struct StreakStats {
    int currentStreak = 0;
    int longestStreak = 0;
};

class StreakCalculator {
public:
    [[nodiscard]] static StreakStats calculate(
        FrequencyType type,
        int targetCount,
        const QSet<QDate> &completions,
        const QDate &today = QDate::currentDate());
};

} // namespace PlasmaHabit
```

---

## Testing Strategy

- **Framework:** `Qt6::Test` (`QTest`) integrated with CMake `enable_testing()` and `ctest`.
- **Unit Tests (`tests/tst_streakcalculator.cpp`):**
  - Daily streak: consecutive days ending today, alive today when yesterday completed, reset to `0` when yesterday missed, retroactive toggle of a missed past day bridges two runs and updates both current and longest streak.
  - Weekly streak (`N/week`, Mon–Sun): streak increments on reaching `N` in current week, stays alive mid-week while `c_cur + r_cur >= N`, resets to `0` mid-week as soon as `c_cur + r_cur < N` (e.g., `N = 4` with `0` completions on Friday where only 3 days remain), and resets to `0` when prior week `< N`.
- **Database Integration Tests (`tests/tst_databasemanager.cpp`):**
  - Uses isolated temporary SQLite databases (`QTemporaryDir`) per test case.
  - Verifies schema creation, creating daily and weekly habits, toggling completions on/off idempotently, rejecting future dates, and archiving/unarchiving without losing completion rows.
- **QML / Startup Verification:**
  - Headless (`QT_QPA_PLATFORM=offscreen`) startup check verifying `QQmlApplicationEngine` loads `qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml` with a non-empty `rootObjects()` list.

---

## Boundaries

- **Always do:**
  - Parameterize all SQL queries with `QSqlQuery::prepare` / `bindValue` (never concatenate user strings into SQL).
  - Keep all habit data strictly local in SQLite under the `eansolutions` / `plasmahabit` namespace.
  - Run `cmake --build build && ctest --test-dir build --output-on-failure` before considering any task complete.
  - Use `KLocalizedString` (`i18n(...)`) for user-facing strings.
- **Ask first:**
  - Changing the SQLite schema or adding new tables/columns.
  - Adding any new external CMake/KF6/Qt6 package dependencies beyond those listed in Tech Stack.
  - Changing the streak calculation rules for daily or weekly habits.
- **Never do:**
  - Add network access (`Qt6::Network`), telemetry, analytics, or external API calls.
  - Implement streak freezes, grace days, or automatic streak forgiveness.
  - Delete historical `habit_completions` rows when archiving a habit.

---

## Explicit Non-Goals (Out of Scope)

1. **Cloud Sync / Multi-Device / User Accounts:** Strictly single-user on the local Linux machine.
2. **Sub-Daily Quantified Targets:** Habits are binary per calendar day (`1x/day` or `N days/week`); counting glasses of water or minutes per day is out of scope.
3. **Streak Freezes / Forgiveness Mechanics:** Missing a required day or week strictly resets the current streak to `0`.
4. **Push Notifications / Background Daemons:** PlasmaHabit runs as a foreground desktop application without a background systemd service or tray daemon.
5. **Mobile / Web / Non-Linux Packaging:** Targeted specifically at Linux (Fedora KDE Plasma) with C++20 / Qt6 / KF6.

---

## Success Criteria

- [ ] Root `CMakeLists.txt` configures C++20, CMake ≥ 3.16, `Qt6` (`Core`, `Gui`, `Qml`, `QuickControls2`, `Sql`, `Test`), `ECM`, and `KF6` (`CoreAddons`, `I18n`, `Kirigami`) with `KDEInstallDirs`, `KDECompilerSettings`, and `KDECMakeSettings`.
- [ ] `src/main.cpp` initializes `QGuiApplication`, `KLocalizedString::setApplicationDomain("plasmahabit")`, sets organization `eansolutions` (`eansolutions.org`) and app name `plasmahabit`, and safely loads `qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml` verifying `engine.rootObjects()` is non-empty.
- [ ] SQLite database is automatically created on disk and persists habits and date completions across restarts.
- [ ] User can create and archive daily (`1x/day`) and weekly (`N/week`, Mon–Sun) habits from the UI.
- [ ] Per-habit binary heatmaps and the global combined intensity heatmap render the 52-week Monday–Sunday grid and update immediately when today or any past day is toggled.
- [ ] Unit and integration tests in `ctest` pass 100%, covering all daily and weekly streak reset and survival scenarios.

---

## Open Questions

None at this stage—all domain, storage, and framework conventions have been confirmed.
