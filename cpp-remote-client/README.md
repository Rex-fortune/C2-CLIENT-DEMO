# C++ Remote Client

This project is a C++ application designed for remote control and management of a computer. It provides various features such as screen sharing, file management, process management, system information display, chat functionality, remote shell access, power control, clipboard synchronization, camera access, multi-session management, file transfer capabilities, screenshot functionality, session recording, help and FAQ support, and security features.

## Features

1. **Real-time Interactive Screen Sharing / Remote Control**
   - Live screen stream with continuous screenshot/video frame updates.
   - Mouse and keyboard control for remote interaction.

2. **File Manager**
   - Browse, upload, download, delete, rename, and move files/folders with a GUI.

3. **Process Manager**
   - View and manage running processes, including CPU and memory usage.

4. **System Info and Monitoring**
   - Display hardware and network information, including resource usage charts.

5. **Chat or Messaging**
   - Simple text chat functionality for communication during sessions.

6. **Remote Shell Terminal**
   - Interactive shell terminal for executing commands and viewing outputs.

7. **Power Control**
   - Options to restart, shut down, sleep, or log off the remote machine.

8. **Clipboard Sync**
   - Synchronize clipboard text between local and remote machines.

9. **Remote Camera Access**
   - Access and display the remote device's webcam.

10. **Multi-session Management**
    - Manage multiple clients with tabs or a list for easy switching.

11. **File Transfer Resume & Progress Bar**
    - Show progress of uploads/downloads and support resuming broken transfers.

12. **Screenshot Annotation and Saving**
    - Save and annotate screenshots locally from sessions.

13. **Session Recording**
    - Record remote sessions as video or screenshots for later playback.

14. **Help / FAQ Panel**
    - Provide simple help guides and instructions for users.

15. **Secure Access**
    - Implement user authentication, encryption, session timeout, and logging.

## Build Instructions

To build the project, you need to have CMake installed. Follow these steps:

1. Clone the repository:
   ```
   git clone <repository-url>
   cd cpp-remote-client
   ```

2. Create a build directory:
   ```
   mkdir build
   cd build
   ```

3. Run CMake to configure the project:
   ```
   cmake ..
   ```

4. Build the project:
   ```
   make
   ```

## Usage

After building the project, you can run the application. Ensure that the server is set up to accept connections from the client. Follow the on-screen instructions to utilize the various features of the application.

## License

This project is licensed under the MIT License. See the LICENSE file for more details.