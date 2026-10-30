# Spec: PlasmaHabit (`org.eansolutions.plasmahabit`)

## Objective

Build **PlasmaHabit** (`org.eansolutions.plasmahabit`), a fast, privacy-first, single-user native Linux desktop habit tracker for Fedora KDE Plasma written in **C++20**, **Qt6**, and **KDE Frameworks 6 (Kirigami)**.

The application persists all habit definitions, daily completion records, and user preferences exclusively in a local on-disk **SQLite** database (`QStandardPaths::AppLocalDataLocation`), renders a global combined contribution heatmap alongside per-habit binary GitHub-style heatmaps with month and day-of-week axis labels, allows creating and archiving habits on the fly with either a **daily** (`1x/day`) or **flexible weekly** (`N times per week`) target, supports configuring the **Start of the week** (*Monday* or *Sunday*) via a General Settings dialog, supports toggling today's or past days' completions directly on each habit's activity grid, and calculates strict, honest streaks that reset to `0` whenever a required day or week is missed.

### User Stories
- **US-1 (Create Habit):** As a user, I can create a new habit with a name, frequency type (`daily` at `1x/day` or `weekly` at `1..7` times per week), and optional accent color so I can start tracking it immediately.
- **US-2 (Toggle Completion on Heatmap):** As a user, I can click today's cell or any past date's cell on a habit's binary heatmap (or a quick "Complete Today" button) to toggle completion on/off, immediately updating the SQLite database, the habit's streak, and the global heatmap.
- **US-3 (Per-Habit & Global Heatmaps with GitHub Axes & Color Scale):** As a user, I can view a 52-week GitHub-style contribution grid for each active habit and a global combined heatmap at the top, featuring:
  - **Month Header Labels:** English abbreviated month names (`Jan`..`Dec`) positioned above the week column where a new month begins.
  - **Day-of-Week Labels:** Abbreviated day labels (`Mon`, `Wed`, `Fri`) on the left axis aligned with their exact day rows (`rows 0, 2, 4` when week starts on Monday; `rows 1, 3, 5` when week starts on Sunday).
  - **GitHub Global Heatmap Palette:** The global heatmap and its *Less → More* legend render positive completion intensities from a muted/dark green to a bright/light green as completion ratio increases:
    - Intensity `0` (0 completions): `Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)`
    - Intensity `1` (1–25%): `#0e4429`
    - Intensity `2` (26–50%): `#006d32`
    - Intensity `3` (51–75%): `#26a641`
    - Intensity `4` (76–100%): `#39d353`
  - Per-habit heatmaps remain binary and use each habit's chosen `colorHex`.
- **US-4 (Honest Streaks):** As a user, I can see both the **current streak** and **longest streak** for each habit, computed strictly from completion history with zero grace passes or streak freezes.
- **US-5 (Archive / Unarchive Habit):** As a user, I can archive a habit so it leaves my active workspace without deleting its SQLite completion history, and optionally view or restore archived habits later.
- **US-6 (General Settings — Start of the Week):** As a user, I can click a **Settings** button in the main toolbar to open a General Settings dialog and choose **"Start of the week"** (*Monday* or *Sunday*, default *Monday*). Changing this setting persists in SQLite and immediately updates both the heatmap row/column layout (`Mon–Sun` vs `Sun–Sat`) and all weekly habit progress and streak calculations.

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

CREATE TABLE IF NOT EXISTS app_settings (
    key             TEXT    PRIMARY KEY CHECK (length(trim(key)) > 0),
    value           TEXT    NOT NULL
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
4. **Week Start Setting Persistence:** `app_settings` stores `key = 'week_start_day'` with allowed values `'monday'` (`Qt::Monday = 1`, default when absent or invalid) or `'sunday'` (`Qt::Sunday = 7`).

---

## Streak & Heatmap Alignment Rules

All streaks are deterministic pure functions of `(frequency_type, target_count, set<QDate> completions, QDate today, Qt::DayOfWeek weekStartDay)` and are recomputed whenever any completion is toggled or when `weekStartDay` changes.

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
- **Week Boundary:** Determined by `weekStartDay` (`Qt::Monday` = Monday–Sunday, or `Qt::Sunday` = Sunday–Saturday).
  - For any date `d`, its 0-based offset within the week (`0..6`) is:
    - When `weekStartDay == Qt::Monday`: `offset(d) = d.dayOfWeek() - 1` (`Mon=0 .. Sun=6`).
    - When `weekStartDay == Qt::Sunday`: `offset(d) = d.dayOfWeek() % 7` (`Sun=0, Mon=1 .. Sat=6`).
  - The start of the week containing `d` is `startOfWeek(d) = d.addDays(-offset(d))`.
  - Let `W(today)` be the current week starting at `startOfWeek(today)` and `W - 1, W - 2, ...` be preceding 7-day weeks.
- **Unit:** Consecutive successful weeks (`w`), where week `W_k` is **successful** iff `completions_in(W_k) >= N`.
- **Current Week Viability Check (Mid-Week Missed Reset):**
  - Let `c_cur = completions_in(W(today))` (completions on dates in `W(today)` up to `today`).
  - Let `r_cur` be the number of remaining days in `W(today)` including `today` if `today` is not yet completed:
    - If `today ∈ completions`: `r_cur = 6 - offset(today)` (days strictly after `today` through the end of the current week).
    - If `today ∉ completions`: `r_cur = 7 - offset(today)` (`today` through the end of the current week).
  - **Rule A — Current week already met (`c_cur >= N`):**
    - Current week counts as `+1` toward the current streak, plus all consecutive immediately preceding weeks `W - 1, W - 2, ...` that satisfied `completions_in(W_k) >= N`.
  - **Rule B — Current week still mathematically possible (`c_cur < N` and `c_cur + r_cur >= N`):**
    - The streak from prior weeks remains **alive**: current streak equals the number of consecutive immediately preceding weeks `W - 1, W - 2, ...` that satisfied `completions_in(W_k) >= N`.
  - **Rule C — Current week mathematically impossible (`c_cur + r_cur < N`):**
    - Because there are not enough days left in the current week to reach `N`, the current streak strictly resets to **`0`** immediately.
- **Missed Prior Week Reset:** If `W - 1` had `< N` completions, no prior week can carry into `W(today)` (current streak is `1` if `c_cur >= N`, else `0`).
- **Longest Weekly Streak:** Maximum number of consecutive 7-day weeks (aligned to `weekStartDay`) in history where `completions_in(W_k) >= N`.

### 3. Heatmap Grid & Axis Labels Rules
- **Grid Dimensions:** 52 week columns (`weekIndex = 0..51`) × 7 day rows (`dayIndex = 0..6`), where column `51` starts at `startOfWeek(today)` and column `0` starts at `startOfWeek(today).addDays(-51 * 7)`.
- **Day-of-Week Labels (Left Axis):**
  - 7 row slots matching the height and spacing of the 7 day cells per column.
  - Only `Mon`, `Wed`, and `Fri` are displayed (just like GitHub):
    - When `weekStartDay == Qt::Monday`: row `0` = `"Mon"`, row `2` = `"Wed"`, row `4` = `"Fri"`, other rows empty `""`.
    - When `weekStartDay == Qt::Sunday`: row `1` = `"Mon"`, row `3` = `"Wed"`, row `5` = `"Fri"`, other rows empty `""`.
- **Month Labels (Top Axis):**
  - For each week column `w` (`0..51`), let `weekStartDate = startWeek.addDays(w * 7)`.
  - A column displays a 3-letter English month abbreviation (`Jan`, `Feb`, `Mar`, `Apr`, `May`, `Jun`, `Jul`, `Aug`, `Sep`, `Oct`, `Nov`, `Dec`) if:
    - `w == 0` (unless column `1` or `2` transitions into a new month, to avoid overlapping header text), or
    - `weekStartDate.month() != prevWeekStartDate.month()` (i.e. the first week column whose start date falls in a new month).

---

## Core Screens and UI States

The UI is a single-window Kirigami application (`Kirigami.ApplicationWindow` + `Kirigami.ScrollablePage`) located in [`src/ui/Main.qml`](file:///home/Eze/projects/plasmahabit/src/ui/Main.qml). All code and UI text are strictly in English using `i18n(...)`.

### Screen 1: Main Dashboard (Active View)
1. **Header / Global Toolbar:**
   - Application title (`Habit Tracker` / `Archived Habits`), **"+ New Habit"** primary action button, **"Show Archived"** / **"Active Habits"** filter toggle, and **"Settings"** (`configure` icon) action button.
2. **Global Combined Heatmap Card:**
   - Displays the last 52 weeks aggregated across all active habits, with **month labels** along the top and **`Mon` / `Wed` / `Fri` day labels** along the left.
   - Cell intensity level (`0` = 0 completions, `1` = 1–25% `#0e4429`, `2` = 26–50% `#006d32`, `3` = 51–75% `#26a641`, `4` = 76–100% `#39d353` of active habits completed on that date).
   - Hover tooltip showing `YYYY-MM-DD: X completed`.
   - *Less → More* legend using the same 5 color swatches (level `0` + `#0e4429`, `#006d32`, `#26a641`, `#39d353`).
3. **Active Habits List (`Repeater` of Kirigami Cards):**
   - Each habit card displays:
     - **Header row:** Habit color dot, title, frequency badge (`Daily` or `N× / week (X/N)`), current streak pill (`🔥 Xd streak` or `🔥 Xw streak`), longest streak label (`(Best: Yd/Yw)`), quick **"Done Today ✓" / "Mark Today"** check button, and **"Archive"** action button.
     - **Subtitle:** Dynamic label reflecting current week start setting: `Contribution Grid (Mon–Sun, click any past or current cell to toggle)` or `Contribution Grid (Sun–Sat, click any past or current cell to toggle)`.
     - **Per-Habit Binary Heatmap:** 52-week interactive grid with top month labels and left `Mon`/`Wed`/`Fri` day labels, where each cell represents one local calendar date (`<= today`).
       - Clicking any cell on or before `today` toggles that date's completion in SQLite and immediately updates both heatmaps and streak counters.
       - Future cells in the current week (after `today`) are rendered muted/disabled.

### Screen 2: Create Habit Dialog (`Kirigami.Dialog`)
- **Fields:**
  - Habit Name (`QQC2.TextField`, required, non-empty).
  - Frequency Type (`QQC2.ComboBox`: `Daily (1x/day)` vs `Flexible Weekly (N/week)`).
  - Weekly Target SpinBox (`1..7`, visible/enabled only when `Weekly` is selected).
  - Accent Color Picker (preset palette swatches).
- **Validation:** Save button is disabled when `name.trim()` is empty.

### Screen 3: General Settings Dialog (`Kirigami.Dialog`)
- Opened via the **"Settings"** toolbar action.
- **Fields:**
  - **Start of the week:** `QQC2.ComboBox` with options `Monday` and `Sunday`, bound to `habitModel.weekStartDay` (`"monday"` or `"sunday"`).
- **Behavior:** Selecting/saving updates the setting via `habitModel.setWeekStartDay(...)`, persists it in SQLite (`app_settings`), and immediately refreshes heatmaps, day labels, and weekly habit calculations.

### Screen 4: Archived Habits View / Mode
- Toggling "Show Archived" lists archived habits with their read-only historical heatmap (including month and day labels), final/longest streak stats, a **"Restore Habit"** action, and a **"Delete Permanently"** confirmation action.

### UI States
- **Empty State:** When 0 habits exist in the current view, displays `Kirigami.PlaceholderMessage` with an action button to create the first habit.
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
│   │   ├── DatabaseManager.h                # SQLite connection, schema init, habits/completions/settings CRUD
│   │   └── DatabaseManager.cpp
│   ├── domain/
│   │   ├── HabitValidation.h                # Input sanitization & validation
│   │   ├── HabitValidation.cpp
│   │   ├── StreakCalculator.h               # Pure C++20 daily & weekly streak calculation (Mon or Sun start)
│   │   └── StreakCalculator.cpp
│   ├── models/
│   │   ├── HabitModel.h                     # QAbstractListModel + heatmap & settings properties for QML
│   │   └── HabitModel.cpp
│   └── ui/
│       └── Main.qml                         # Kirigami dashboard, global + per-habit heatmaps, dialogs
└── tests/
    ├── CMakeLists.txt                       # Qt6::Test executables registered with CTest
    ├── tst_streakcalculator.cpp             # Unit tests for daily & weekly streak edge cases (Mon & Sun start)
    ├── tst_databasemanager.cpp              # Integration tests for SQLite CRUD, toggling, and settings
    ├── tst_habitmodel.cpp                   # Model tests for heatmaps, month/day labels, and week start toggling
    ├── tst_dbvalidation.cpp                 # DB validation tests
    └── tst_startup.cpp                      # Headless QML startup tests
```

---

## Code Style

- **Standard:** Modern C++20 (`std::optional`, `std::span`, `constexpr`, range algorithms where clear).
- **Language:** All source code, comments, identifiers, and UI strings in English (`i18n(...)`).
- **Naming Conventions:**
  - Classes / Structs / QML files: `PascalCase` (`StreakCalculator`, `HabitModel`, `Main.qml`)
  - Methods / Functions / Variables / QML properties: `camelCase` (`toggleCompletion`, `currentStreak`, `weekStartDay`)
  - Member variables: `m_camelCase` (`m_dbManager`, `m_items`)
  - SQLite identifiers: `snake_case` (`habit_completions`, `app_settings`, `week_start_day`)
- **Formatting:** 4-space indentation, braces on same line for control blocks/functions, `#pragma once` in headers.

```cpp
#pragma once

#include <QDate>
#include <QSet>

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
        const QDate &today = QDate::currentDate(),
        Qt::DayOfWeek weekStartDay = Qt::Monday);
};

} // namespace PlasmaHabit
```

---

## Testing Strategy

- **Framework:** `Qt6::Test` (`QTest`) integrated with CMake `enable_testing()` and `ctest`.
- **Unit Tests (`tests/tst_streakcalculator.cpp`):**
  - Daily streak: consecutive days ending today, alive today when yesterday completed, reset to `0` when yesterday missed, retroactive toggle bridges two runs.
  - Weekly streak (`N/week`, both `Qt::Monday` and `Qt::Sunday` week starts): streak increments on reaching `N` in current week, stays alive mid-week while `c_cur + r_cur >= N`, resets to `0` mid-week when `c_cur + r_cur < N`, and verifies boundary dates (e.g. a Sunday completion belongs to the ending week when `Qt::Monday` is active, but starts the new week when `Qt::Sunday` is active).
- **Database Integration Tests (`tests/tst_databasemanager.cpp`):**
  - Verifies schema creation including `app_settings`, default `weekStartDay` (`Qt::Monday`), persisting `"sunday"` and `"monday"` across database reloads, and rejecting invalid setting values safely.
- **Model Tests (`tests/tst_habitmodel.cpp`):**
  - Verifies `weekStartDay` property, `dayLabels` (`["Mon", "", "Wed", "", "Fri", "", ""]` vs `["", "Mon", "", "Wed", "", "Fri", ""]`), `monthLabels` (52 entries with month abbreviations at month transitions), heatmap cell ordering (`dayOfWeek` 0..6 aligned to Monday or Sunday), and reactive recalculation of weekly progress/streaks when `weekStartDay` is toggled.
- **QML / Startup Verification (`tests/tst_startup.cpp`):**
  - Headless (`QT_QPA_PLATFORM=offscreen`) startup check verifying `QQmlApplicationEngine` loads `qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml` without QML errors.

---

## Boundaries

- **Always do:**
  - Parameterize all SQL queries with `QSqlQuery::prepare` / `bindValue` (never concatenate user strings into SQL).
  - Keep all habit and settings data strictly local in SQLite under the `eansolutions` / `plasmahabit` namespace.
  - Keep all code, comments, and UI strings in English (`i18n(...)`).
  - Run `cmake --build build && ctest --test-dir build --output-on-failure` before considering any task complete.
- **Ask first:**
  - Changing the SQLite schema beyond the specified `habits`, `habit_completions`, and `app_settings` tables.
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
4. **Arbitrary Start-of-Week Days Beyond Monday/Sunday:** Only `Monday` and `Sunday` are supported in General Settings.
5. **Changing Per-Habit Heatmap Binary Color Model:** Individual habit heatmaps continue using their single selected accent color (`colorHex`) for completed days.

---

## Success Criteria

- [ ] Both the Global Combined Heatmap and every Per-Habit Heatmap display 3-letter English month headers (`Jan`–`Dec`) aligned to the week columns where each month begins, and `Mon`, `Wed`, `Fri` labels along the left axis aligned with their respective day rows.
- [ ] The Global Combined Heatmap and its *Less → More* legend render intensity levels `1..4` using GitHub's dark-to-bright green scale (`#0e4429`, `#006d32`, `#26a641`, `#39d353`), while keeping level `0` as the subtle background color.
- [ ] A **Settings** button in the main toolbar opens a General Settings dialog (in English) allowing the user to select **"Start of the week"** (`Monday` or `Sunday`).
- [ ] Selecting `Monday` or `Sunday` persists in SQLite (`app_settings` table) across application restarts and immediately updates:
  - Heatmap week columns and day rows (`Mon–Sun` with `Mon`/`Wed`/`Fri` on rows `0, 2, 4` vs `Sun–Sat` with `Mon`/`Wed`/`Fri` on rows `1, 3, 5`).
  - Weekly habit progress (`weeklyProgress`) and weekly streak calculations (`StreakCalculator::calculate`).
- [ ] All existing and new unit, model, database, and headless QML startup tests in `ctest` pass 100%.

---

## Open Questions

None — all requirements and design decisions have been confirmed during the intent interview.
