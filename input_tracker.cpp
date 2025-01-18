#include <windows.h>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <mutex>
#include <string>
#include <sstream>
#include <cctype>
#include <fstream>
#include <queue>

//----------------------------------------------------//
//                  Data Structures
//----------------------------------------------------//

// Event structure to store any input event
struct InputEvent
{
    std::string eventType;  // e.g. "MOUSE_LEFT_DOWN", "KEY_UP", "MOUSE_POS", etc.
    DWORD       timestamp;  // in ms, from GetTickCount()
    POINT       mousePos;   // relevant for mouse or for reference on keyboard
    UINT        keyCode;    // relevant for keyboard events
};

// Returns a string like "20250118_162453"
static std::string getTimestampString()
{
    std::time_t now = std::time(nullptr);
    std::tm localTime;
#if defined(_WIN32) || defined(_MSC_VER)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y%m%d_%H%M%S");
    return oss.str();
}

//----------------------------------------------------//
//              CSVLogger Class Declaration
//----------------------------------------------------//

class CSVLogger {
public:
    CSVLogger(const std::string& filename = "input_log.csv",
        int flushIntervalSeconds = 60);
    ~CSVLogger();

    void start();           // Starts the background flush thread
    void stop();            // Stops the background flush thread (flushes remaining events)

    // Thread-safe method to queue an event
    void logEvent(const InputEvent& evt);

private:
    void flushThreadFunc(); // Thread loop that periodically flushes
    void flushToDisk();     // Writes buffered events to file

private:
    std::string                     m_filename;
    int                             m_flushIntervalSec;
    std::atomic<bool>               m_running{ false };

    // A thread-safe queue for events
    std::mutex                      m_queueMutex;
    std::queue<InputEvent>          m_eventQueue;

    // Background flush thread
    std::thread                     m_flushThread;
};

//----------------------------------------------------//
//             CSVLogger Implementation
//----------------------------------------------------//

CSVLogger::CSVLogger(const std::string& filename, int flushIntervalSeconds)
    : m_flushIntervalSec(flushIntervalSeconds)
{
    if (filename.empty()) {
        // Generate a unique filename based on timestamp
        std::string ts = getTimestampString();  // e.g. "20250118_162453"
        m_filename = "input_log_" + ts + ".csv";
    }
    else {
        m_filename = filename;
    }
}


CSVLogger::~CSVLogger()
{
    // Make sure to stop and flush if the user forgot
    if (m_running.load()) {
        stop();
    }
}

void CSVLogger::start()
{
    if (m_running.load()) {
        return; // already running
    }
    m_running.store(true);

    // Optionally, create or truncate the CSV file if you want a clean start:
    {
        std::ofstream ofs(m_filename, std::ios::trunc);
        // Write header if desired
        ofs << "timestamp_ms,event_type,x,y,key_code\n";
    }

    // Launch background flush thread
    m_flushThread = std::thread(&CSVLogger::flushThreadFunc, this);
}

void CSVLogger::stop()
{
    if (!m_running.load()) {
        return; // not running
    }
    m_running.store(false);

    // Wait for the flush thread to exit
    if (m_flushThread.joinable()) {
        m_flushThread.join();
    }

    // Final flush in case there are leftover events
    flushToDisk();
}

void CSVLogger::logEvent(const InputEvent& evt)
{
    // Thread-safe insertion
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_eventQueue.push(evt);
}

void CSVLogger::flushThreadFunc()
{
    // Loop until m_running is set to false
    while (m_running.load()) {
        // Sleep for flush interval
        std::this_thread::sleep_for(std::chrono::seconds(m_flushIntervalSec));
        if (!m_running.load()) {
            break;
        }
        flushToDisk();
    }
}

void CSVLogger::flushToDisk()
{
    // Move events from the queue into a local vector (so we don't hold the lock while writing)
    std::vector<InputEvent> localBuffer;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        // Move all events from queue to localBuffer
        while (!m_eventQueue.empty()) {
            localBuffer.push_back(m_eventQueue.front());
            m_eventQueue.pop();
        }
    }

    if (localBuffer.empty()) {
        return; // nothing to write
    }

    // Open file in append mode
    std::ofstream ofs(m_filename, std::ios::app);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open CSV file for appending: " << m_filename << "\n";
        return;
    }

    // Write events
    // CSV format: timestamp_ms,event_type,x,y,key_code
    for (const auto& evt : localBuffer) {
        ofs << evt.timestamp << ","
            << evt.eventType << ","
            << evt.mousePos.x << ","
            << evt.mousePos.y << ","
            << evt.keyCode << "\n";
    }
    ofs.close();
}

//----------------------------------------------------//
//   Global Config & Original Tracker Functionality
//----------------------------------------------------//

class TrackerConfig {
public:
    std::atomic<bool> isRunning{ false };   // Are we actively logging?
    std::atomic<int>  pollIntervalMs{ 20 }; // How often we poll cursor position

    // Default tracked keys: Q, W, E, R, 1, 2, 3, 4, CTRL
    std::vector<UINT> trackedKeys = {
        'Q', 'W', 'E', 'R',
        '1', '2', '3', '4',
        VK_CONTROL
    };

    // Hooks
    HHOOK mouseHook = nullptr;
    HHOOK keyboardHook = nullptr;

    // CSV logger to reduce memory usage
    // By default, flush every 60 seconds
    CSVLogger csvLogger{ "input_log.csv", 60 };
};

// We keep a single global instance:
static TrackerConfig g_config;

//----------------------------------------------------//
//                 Utility Functions
//----------------------------------------------------//

DWORD getCurrentTimeMs()
{
    return GetTickCount();
}

// Convert a VK code to a debug string (basic)
std::string vkCodeToString(UINT vkCode)
{
    switch (vkCode) {
    case VK_CONTROL: return "CTRL";
    case VK_SHIFT:   return "SHIFT";
    case VK_MENU:    return "ALT";
    default:
        if ((vkCode >= '0' && vkCode <= '9') ||
            (vkCode >= 'A' && vkCode <= 'Z')) {
            return std::string(1, static_cast<char>(vkCode));
        }
        {
            std::ostringstream oss;
            oss << "VK(" << vkCode << ")";
            return oss.str();
        }
    }
}

//----------------------------------------------------//
//            Low-Level Hook Callbacks
//----------------------------------------------------//

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && g_config.isRunning.load()) {
        MSLLHOOKSTRUCT* pMouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        POINT pt = pMouse->pt;
        DWORD time = getCurrentTimeMs();

        std::string eventType;
        switch (wParam) {
        case WM_LBUTTONDOWN: eventType = "MOUSE_LEFT_DOWN";  break;
        case WM_LBUTTONUP:   eventType = "MOUSE_LEFT_UP";    break;
        case WM_RBUTTONDOWN: eventType = "MOUSE_RIGHT_DOWN"; break;
        case WM_RBUTTONUP:   eventType = "MOUSE_RIGHT_UP";   break;
        default:
            // not interested in other mouse events here
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }

        InputEvent evt{
            eventType,
            time,
            pt,
            0 // no keyCode for mouse events
        };
        g_config.csvLogger.logEvent(evt);
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && g_config.isRunning.load()) {
        KBDLLHOOKSTRUCT* pKeyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        DWORD time = getCurrentTimeMs();
        UINT vkCode = pKeyboard->vkCode;

        // Check if the key is in our tracked list
        bool isTracked = false;
        for (auto code : g_config.trackedKeys) {
            if (code == vkCode) {
                isTracked = true;
                break;
            }
        }
        if (!isTracked) {
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }

        // Determine if it's key-down or key-up
        std::string eventType;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            eventType = "KEY_DOWN";
        }
        else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            eventType = "KEY_UP";
        }
        else {
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }

        POINT pt;
        GetCursorPos(&pt);

        InputEvent evt{
            eventType,
            time,
            pt,
            vkCode
        };
        g_config.csvLogger.logEvent(evt);
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

//----------------------------------------------------//
//               Cursor Polling Thread
//----------------------------------------------------//

void cursorPollingThread()
{
    while (g_config.isRunning.load()) {
        POINT pt;
        if (GetCursorPos(&pt)) {
            DWORD time = getCurrentTimeMs();
            InputEvent evt{
                "MOUSE_POS",
                time,
                pt,
                0
            };
            g_config.csvLogger.logEvent(evt);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(g_config.pollIntervalMs.load()));
    }
}

//----------------------------------------------------//
//             Hook/Unhook & Start/Stop
//----------------------------------------------------//

void installHooks()
{
    g_config.mouseHook = SetWindowsHookEx(
        WH_MOUSE_LL,
        LowLevelMouseProc,
        GetModuleHandle(NULL),
        0
    );
    if (!g_config.mouseHook) {
        std::cerr << "Failed to install mouse hook.\n";
    }

    g_config.keyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandle(NULL),
        0
    );
    if (!g_config.keyboardHook) {
        std::cerr << "Failed to install keyboard hook.\n";
    }
}

void removeHooks()
{
    if (g_config.mouseHook) {
        UnhookWindowsHookEx(g_config.mouseHook);
        g_config.mouseHook = nullptr;
    }
    if (g_config.keyboardHook) {
        UnhookWindowsHookEx(g_config.keyboardHook);
        g_config.keyboardHook = nullptr;
    }
}

void startLogging(int intervalMs)
{
    if (g_config.isRunning.load()) {
        std::cout << "Logging is already running.\n";
        return;
    }

    if (intervalMs > 0) {
        g_config.pollIntervalMs.store(intervalMs);
    }

    g_config.isRunning.store(true);

    // Start the CSV logger (opens file, starts flush thread)
    g_config.csvLogger.start();

    // Install hooks
    installHooks();

    // Launch the polling thread
    std::thread polling(cursorPollingThread);
    polling.detach();

    std::cout << "Logging started (poll interval = "
        << g_config.pollIntervalMs.load() << " ms).\n";
}

void stopLogging()
{
    if (!g_config.isRunning.load()) {
        std::cout << "Logging is not currently running.\n";
        return;
    }

    g_config.isRunning.store(false);

    // Remove hooks
    removeHooks();

    // Stop the CSV logger (flushes any remaining events)
    g_config.csvLogger.stop();

    std::cout << "Logging stopped.\n";
}

//----------------------------------------------------//
//      Dedicated Message Loop for Hooks Thread
//----------------------------------------------------//

static std::atomic<bool> g_hookThreadActive{ true };

DWORD WINAPI hookThreadProc(LPVOID)
{
    MSG msg;
    while (g_hookThreadActive.load()) {
        BOOL res = GetMessage(&msg, NULL, 0, 0);
        if (res <= 0) {
            break; // WM_QUIT or error
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

//----------------------------------------------------//
//               CLI Command Handling
//----------------------------------------------------//

std::vector<std::string> splitTokens(const std::string& line)
{
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Convert a single key string (like "Q", "CTRL") to a virtual-key code
UINT keyStringToVk(const std::string& keyStr)
{
    std::string upper;
    for (char c : keyStr) upper.push_back(static_cast<char>(toupper(c)));

    if (upper == "CTRL")  return VK_CONTROL;
    if (upper == "SHIFT") return VK_SHIFT;
    if (upper == "ALT")   return VK_MENU;

    if (upper.size() == 1) {
        char c = upper[0];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) {
            return static_cast<UINT>(c);
        }
    }
    return 0; // not recognized
}

void setTrackedKeys(const std::vector<std::string>& keys)
{
    std::vector<UINT> newVk;
    for (auto& k : keys) {
        UINT vk = keyStringToVk(k);
        if (vk != 0) {
            newVk.push_back(vk);
        }
    }

    g_config.trackedKeys = newVk;
    std::cout << "Tracked keys updated. Count = " << newVk.size() << "\n";
}

//----------------------------------------------------//
//                      main()
//----------------------------------------------------//

int main()
{
    // 1) Start a dedicated thread with a message loop for hooking
    HANDLE hHookThread = CreateThread(NULL, 0, hookThreadProc, NULL, 0, NULL);
    if (!hHookThread) {
        std::cerr << "Failed to create hook thread.\n";
        return 1;
    }

    std::cout << "Welcome to Input Tracker CLI (with CSV logging)!\n";
    std::cout << "Commands:\n"
        << "  start [intervalMs]\n"
        << "  stop\n"
        << "  setkeys [key1 key2 ...]\n"
        << "  exit\n";

    // 2) Main command loop
    while (true) {
        std::cout << "> ";
        std::string line;
        if (!std::getline(std::cin, line)) {
            break; // EOF or error
        }

        auto tokens = splitTokens(line);
        if (tokens.empty()) {
            continue;
        }

        std::string cmd = tokens[0];

        if (cmd == "start") {
            int interval = 20; // default
            if (tokens.size() > 1) {
                try {
                    interval = std::stoi(tokens[1]);
                }
                catch (...) {}
            }
            startLogging(interval);
        }
        else if (cmd == "stop") {
            stopLogging();
        }
        else if (cmd == "setkeys") {
            if (tokens.size() > 1) {
                std::vector<std::string> keys(tokens.begin() + 1, tokens.end());
                setTrackedKeys(keys);
            }
            else {
                std::cout << "Usage: setkeys [key1 key2 ...]\n";
            }
        }
        else if (cmd == "exit") {
            break;
        }
        else {
            std::cout << "Unknown command: " << cmd << "\n";
        }
    }

    // 3) Cleanup
    if (g_config.isRunning.load()) {
        stopLogging();
    }

    // Signal hook thread to exit
    g_hookThreadActive.store(false);
    PostThreadMessage(GetThreadId(hHookThread), WM_QUIT, 0, 0);

    WaitForSingleObject(hHookThread, INFINITE);
    CloseHandle(hHookThread);

    std::cout << "Exiting. Goodbye.\n";
    return 0;
}
