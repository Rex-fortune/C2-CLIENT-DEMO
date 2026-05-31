# C2-CLIENT-DEMO
FOR DEMONSTRATION PURPUSES ONLY. This shows the in-depth on how command to server outbound connection works. the mainpurpose is for security pesonnel to know the patterns in which a c2 Virus might work in the background and steal infomations from users in the backgound pesistently. 


**MORE INFO** <br>
In these pogram, i used a pulling method  hence it affects cpu usage. this is done deliberately so that no one build up this project and use it seemlessly.
But in a real world  usage, streamlining or socketted connection will be used.

This program can gets inf from the victims system, also operate the system remotely from any where, accessing all features in the victim's system.

This programm is built using c++, which caqn interact with the any BIOS of the machine

My goal is to understand oe of the still useful way a red hacker steal info so as security measures to protect against it will be discovered.

A wel tailored malware, when compiled will be strip off and obsulatedand signed in, so antivirus alone, windows defender, wont be able to detect it.

As shown in this program, when the victim clicks on it, a pdf document alone is shown, deceiving the client that it is just an invoice pdf instead of a malicious attack.


**USAGE** <br>
BUILDING FROM MACOS

x86_64-w64-mingw32-g++ *.cpp *.h resource.o sqlite3.o \\n  -o ../build/invoice-viewer.exe \\n  -std=gnu++17 \\n  -I. \\n  -static -static-libgcc -static-libstdc++ -pthread \\n  -lws2_32 \\n  -liphlpapi \\n  -lgdiplus -lgdi32 -lole32 -loleaut32 -luuid -lstrmiids \\n  -lwtsapi32 -lnetapi32 \\n  -lMfplat -lMf -lMfreadwrite -lMfuuid -lPowrProf \\n  -lcrypt32 -lbcrypt -lshlwapi -mwindows

cd ../build
 click .exe

 BUILDING FROM WINDOWS
 g++ *.cpp *.h resource.o sqlite3.o \\n  -o ../build/invoice-viewer.pdf.exe \\n  -std=gnu++17 \\n  -I. \\n  -static -static-libgcc -static-libstdc++ -pthread \\n  -lws2_32 \\n  -liphlpapi \\n  -lgdiplus -lgdi32 -lole32 -loleaut32 -luuid -lstrmiids \\n  -lwtsapi32 -lnetapi32 \\n  -lMfplat -lMf -lMfreadwrite -lMfuuid -lPowrProf \\n  -lcrypt32 -lbcrypt -lshlwapi -mwindows

 cd ../build
 click .exe

 
