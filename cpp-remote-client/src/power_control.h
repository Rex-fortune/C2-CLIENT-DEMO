#ifndef POWER_CONTROL_H
#define POWER_CONTROL_H

#include <string>

class PowerControl {
public:
    // Restart the remote machine
    static bool restart();

    // Shut down the remote machine
    static bool shutdown();

    // Log off the remote machine
    static bool logOff();

    // Sleep the remote machine
    static bool sleep();
};

#endif // POWER_CONTROL_H