// main.cpp
#include <windows.h>
#include <iostream>
#include <string>
#include "input_tracker.h"  // optional if you have declarations

int main()
{
    std::cout << "Welcome to the standalone Win32 skillshot analyzer!\n";
    std::cout << "Type 'start [intervalMs]' to begin logging\n";
    std::cout << "Type 'stop' to stop logging\n";
    std::cout << "Type 'exit' to quit\n";

    while (true) {
        std::cout << "> ";
        std::string command;
        if (!std::getline(std::cin, command)) {
            // End of file or error
            break;
        }

        if (command.rfind("start", 0) == 0) {
            // e.g. "start 10"
            int interval = 20; // default
            size_t spacePos = command.find(' ');
            if (spacePos != std::string::npos) {
                try {
                    interval = std::stoi(command.substr(spacePos));
                } catch(...) {}
            }
            startLogging(interval);
        }
        else if (command == "stop") {
            stopLogging();
        }
        else if (command == "exit") {
            break;
        }
        else {
            std::cout << "Unknown command.\n";
        }
    }

    // If we started logging, ensure we stop
    stopLogging();

    std::cout << "Exiting.\n";
    return 0;
}
