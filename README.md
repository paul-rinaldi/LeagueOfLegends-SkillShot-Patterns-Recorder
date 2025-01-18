# Skillshot Analyzer (Win32 Hooking Tool)

This project is a standalone Win32 console application that demonstrates how to use low-level system hooks (mouse and keyboard) on Windows. It logs your mouse movements/clicks and key presses in real time, with the eventual goal of providing post-game analysis for precision-based interactions (such as “skillshots” in games).

## 1. Description of the Problem

Many players (particularly in competitive games like League of Legends) want to improve their aiming and dodging of skillshots. However, it’s hard to understand your own micro-movement patterns and input tendencies just by rewatching replays or scanning logs.

Problem:

- Standard replays only show broad champion positions or actions; they don’t track exact mouse positions or every keystroke in detail.
- Players lack a detailed record of where they clicked, when they used certain abilities, or how often they “telegraph” movement patterns before firing skillshots.
- Reviewing in-game replays is time-consuming and rote

## 2. Goal

1. Capture precise input data (mouse position, clicks, keyboard events) in near real-time.
2. Store this data for later analysis or visualization (e.g., heatmaps, reaction-time studies, etc.).
3. Eventually, develop an analyzer that can help players identify consistent movement or aiming habits and optimize their gameplay.

## 3. What the Program Does

- Hooks into low-level mouse and keyboard events on Windows (via SetWindowsHookEx(WH_MOUSE_LL) and SetWindowsHookEx(WH_KEYBOARD_LL)).
- Logs:
  - Mouse position on a configurable interval (default 20 ms).
  - Mouse clicks (left/right down/up).
  - Key presses (key down/up) for specified keys.
- Outputs logs to the console (in this minimal version), or to a CSV file (in the advanced version).
- Provides a simple CLI in the console:
  - `start [pollIntervalMs]` – begin capturing events.
  - `stop` – stop capturing events.
  - `exit` – quit the program.

## 4. How to Use the Program

1. Launch the application (e.g., main.exe).
2. In the console, enter commands:
   start (optionally with a poll interval, e.g. start 10) to begin logging.

- Interact with your mouse and keyboard as normal—each event is captured in the background.
- `stop` to end the logging session.
- `exit` to close the program.

Example Session

```
> start 10
Logging started (poll interval = 10 ms).
[MOUSE_POS] X=500 Y=300
[MOUSE_POS] X=501 Y=300
[KEY] Down: 81  (Q key)
[KEY] Up: 81
[MOUSE] Left Button Down
...
> stop
Logging stopped.
> exit
Exiting.
```

## 5. How to Build from Source

Below is a basic approach using Microsoft’s cl.exe (the Visual C++ compiler). You can also create a Visual Studio Win32 Console Application project and add these files, then link `User32.lib`.

### Prerequisites

- Windows with Visual Studio installed (or the Build Tools for Visual Studio).
- The Developer Command Prompt (or any environment where cl.exe is available).

### Steps

1. Clone or copy the project files into a folder:

- `main.cpp`
- `input_tracker.cpp`
- (Optional) `input_tracker.h`

2. Open the Developer Command Prompt for VS.
3. Navigate to the folder containing the `.cpp` files:

```
cd C:\Path\To\Project
```

4. Compile:

```
cl /EHsc main.cpp input_tracker.cpp /link user32.lib
```

- This produces `main.exe` (the name may differ if you specify /`Fe:myprogram.exe`).

5. Run:

```
main.exe
```

Type `start` to begin logging, `stop` to stop, `exit` to quit.

## 6. Future of the Project: Analyzer

While the current version simply logs inputs, the next step is to create an analyzer that can:

- Correlate skill usage with precise mouse positions and times.
- Identify patterns in micro-movements or predictive “tells” before casting skillshots.
- Possibly incorporate machine learning or advanced analytics to highlight suboptimal behavior or missed opportunities.

### Planned Features:

- A post-game GUI to visualize movement heatmaps and chart skill usage.
- Automatic detection of “panic clicks” or repeated patterns in approach or retreat.
- Potential export to external data science tools (e.g., CSV for Python analysis).

## Thank You for Checking Out Skillshot Analyzer!

Feel free to fork or clone the repo and contribute. We welcome feedback and pull requests that push this project closer to an in-depth analysis tool for improving mechanical skills in competitive games.
