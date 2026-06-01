#ifndef SCREEN_SHARE_H
#define SCREEN_SHARE_H

#include <vector>
#include <string>
#include "sockets.h"

// Function to capture the screen and return the image data as a byte vector
bool capture_screenshot_png(std::vector<unsigned char>& outBuffer);

// Function to send the captured screenshot to the server
bool send_screenshot_to_server(int socket, const std::vector<unsigned char>& imageData);

// Function to start the screen sharing process
void start_screen_sharing(sock_t socket);

// Function to stop the screen sharing process
void stop_screen_sharing();

#endif // SCREEN_SHARE_H