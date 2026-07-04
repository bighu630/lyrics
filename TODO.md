# Windows Compatibility Phase 2

## Task List

### Wave 1 — Parallel Independent Tasks

- [x] **Task A: Cross-platform core utilities**
  - Update `config.cpp` with Windows paths (%APPDATA%, %LOCALAPPDATA%)
  - Update `util/exec.h` with `_popen`/`_pclose` for Windows
  - Update `ui/console_tui.cpp` with Windows Console API terminal width
  - Guard `i3block/controller.h` and `controller.cpp` with `#ifndef _WIN32`
  - Guard `app.cpp` i3block calls with `#ifndef _WIN32`
  - Guard `app.h` i3block member with `#ifndef _WIN32`

- [x] **Task B: SMTC Helper C# project**
  - Create `smtc-helper/smtc-helper.csproj`
  - Create `smtc-helper/Program.cs` with full SMTC monitoring

- [x] **Task C: Windows tray (`tray_windows.cpp`)**
  - Create `src/tray/tray_windows.cpp` using Win32 `Shell_NotifyIconW`

### Wave 2 — Depends on Task B

- [x] **Task D: Windows player detection (`player_windows.cpp`)**
  - Create `src/player_windows.cpp` using SMTC helper process
  - Implements `player.h` interface

### Wave 3 — Integration

- [x] **Task E: Build system + main integration**
  - Update `CMakeLists.txt` for Windows conditional compilation
  - Update `main.cpp` for Windows mode handling
