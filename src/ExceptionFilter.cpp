#if defined(XUO_WINDOWS)

#include <psapi.h>
#include <tlhelp32.h>

string GetMemStorageText(int storage)
{
    switch (storage)
    {
        case MEM_FREE:
            return "Free";
        case MEM_RESERVE:
            return "Reserve";
        case MEM_IMAGE:
            return "Image";
        case MEM_MAPPED:
            return "Mapped";
        case MEM_PRIVATE:
            return "Private";
        default:
            break;
    }

    return "Unknown";
}

inline BOOL ModuleFind(const HANDLE &snapshot, void *pvBaseAddr, PMODULEENTRY32 pme)
{
    for (BOOL ok = Module32First(snapshot, pme); ok; ok = Module32Next(snapshot, pme))
    {
        if (pme->modBaseAddr == pvBaseAddr)
            return TRUE;
    }

    return FALSE;
}

void DumpRegionInfo(const HANDLE &snapshot, HANDLE hProcess, VMQUERY &vmq)
{
    if (vmq.dwRgnStorage == MEM_FREE)
        return;

    string filePath = "";

    if (vmq.pvRgnBaseAddress != nullptr)
    {
        MODULEENTRY32 me = { sizeof(me) };
        if (ModuleFind(snapshot, vmq.pvRgnBaseAddress, &me))
            filePath = ToString(me.szExePath);
        else
        {
            wchar_t filename[MAX_PATH + 1];
            uint32_t d = GetMappedFileName(hProcess, vmq.pvRgnBaseAddress, filename, MAX_PATH);

            if (d)
                filePath = ToString(filename);
        }
    }

    if (vmq.fRgnIsAStack)
        filePath = "Thread Stack";

    INFO(
        Client,
        "Address: 0x{:0>8x} => 0x{:0>8x} Storage: '{}' size: {} blocks: {} path: {}",
        vmq.pvBlkBaseAddress,
        (uint8_t *)vmq.pvBlkBaseAddress + vmq.RgnSize,
        GetMemStorageText(vmq.dwBlkStorage),
        vmq.RgnSize,
        vmq.dwRgnBlocks,
        filePath);
}

void DumpLibraryInformation()
{
#if USE_DEBUG_FUNCTION_NAMES == 1
    INFO(Client, "trace functions:");
    for (const string &str : g_DebugFuncStack)
        INFO(Client, str);

    static bool libraryInfoPrinted = false;

    if (!libraryInfoPrinted)
    {
        libraryInfoPrinted = true;

        INFO(Client, "library information:");

        uint32_t processId = 0;
        GetWindowThreadProcessId(g_GameWindow.Handle, &processId);

        HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

        if (process == nullptr)
        {
            INFO(Client, "::OpenProcess failed!");
            return;
        }

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, processId);

        if (snapshot == nullptr)
        {
            INFO(Client, "::CreateToolhelp32Snapshot failed!");
            CloseHandle(process);
            return;
        }

        void *address = nullptr;

        while (true)
        {
            VMQUERY vmq = { 0 };

            if (!VMQuery(process, address, &vmq))
                break;

            DumpRegionInfo(snapshot, process, vmq);

            address = ((uint8_t *)vmq.pvRgnBaseAddress + vmq.RgnSize);
        }

        CloseHandle(snapshot);
        CloseHandle(process);
    }
#endif
}

void DumpCurrentRegistersInformation(CONTEXT *CR)
{
#if defined(_WIN64)
    INFO(
        Client,
        "EAX=0x{:0>16x}, EBX=0x{:0>16x}, ECX=0x{:0>16x}, EDX=0x{:0>16x}",
        CR->Rax,
        CR->Rbx,
        CR->Rcx,
        CR->Rdx);
    INFO(
        Client,
        "ESI=0x{:0>16x}, EDI=0x{:0>16x}, ESP=0x{:0>16x}, EBP=0x{:0>16x}",
        CR->Rsi,
        CR->Rdi,
        CR->Rsp,
        CR->Rbp);
    INFO(Client, "EIP=0x{:0>16x}, EFLAGS=0x{:0>16x}", CR->Rip, CR->EFlags);
    INFO_DUMP(Client, "Bytes at EIP:", (uint8_t *)CR->Rip, 16);
    INFO_DUMP(Client, "Bytes at ESP:", (uint8_t *)CR->Rsp, 64);
#else
    INFO(
        Client,
        "EAX=0x{:0>8x}, EBX=0x{:0>8x}, ECX=0x{:0>8x}, EDX=0x{:0>8x}",
        CR->Eax,
        CR->Ebx,
        CR->Ecx,
        CR->Edx);
    INFO(
        Client,
        "ESI=0x{:0>8x}, EDI=0x{:0>8x}, ESP=0x{:0>8x}, EBP=0x{:0>8x}",
        CR->Esi,
        CR->Edi,
        CR->Esp,
        CR->Ebp);
    INFO(Client, "EIP=0x{:0>8x}, EFLAGS=0x{:0>8x}", CR->Eip, CR->EFlags);
    INFO_DUMP(Client, "Bytes at EIP:", (uint8_t *)CR->Eip, 16);
    INFO_DUMP(Client, "Bytes at ESP:", (uint8_t *)CR->Esp, 64);
#endif
}

std::vector<uint32_t> FindPattern(uint8_t *ptr, int size, const vector<uint8_t> &pattern)
{
    vector<uint32_t> result;
    int patternSize = (int)pattern.size();
    int count = size - patternSize - 1;
    for (int i = 0; i < count; i++)
    {
        if (!memcmp(&ptr[0], &pattern[0], patternSize))
            result.push_back(0x00400000 + (int)i);

        ptr++;
    }
    return result;
}

LONG __stdcall GameUnhandledExceptionFilter(struct _EXCEPTION_POINTERS *exceptionInfo)
{
    static int errorCount = 0;
    static uint32_t lastErrorTime = 0;

    uint32_t ticks = GetTickCount();
    errorCount++;

    if (exceptionInfo->ExceptionRecord)
    {
#if defined(_WIN64)
        INFO(
            Client,
            "Unhandled exception #{}: 0x{:0>16x} at {:0>16x}",
            errorCount,
            exceptionInfo->ExceptionRecord->ExceptionCode,
            exceptionInfo->ExceptionRecord->ExceptionAddress);
#else
        INFO(
            Client,
            "Unhandled exception #{}: 0x{:0>8x} at {:0>8x}",
            errorCount,
            exceptionInfo->ExceptionRecord->ExceptionCode,
            exceptionInfo->ExceptionRecord->ExceptionAddress);
#endif

        if (errorCount > 10 && (ticks - lastErrorTime) < 5000)
        {
            GameStackWalker sw;
            sw.ShowCallstack(GetCurrentThread(), exceptionInfo->ContextRecord);

            DumpLibraryInformation();

            DumpCurrentRegistersInformation(exceptionInfo->ContextRecord);

            Wisp::CMappedFile file;

            wchar_t fileName[MAX_PATH] = { 0 };
            GetModuleFileNameW(0, fileName, MAX_PATH);
            bool crashlog = false;
            if (file.Load(fileName))
            {
                vector<uint8_t> pattern;
#if defined(_WIN64)
                uint8_t *eipBytes = (uint8_t *)exceptionInfo->ContextRecord->Rip;
#else
                uint8_t *eipBytes = (uint8_t *)exceptionInfo->ContextRecord->Eip;
#endif

                for (int i = 0; i < 16; i++)
                    pattern.push_back(eipBytes[i]);

                auto list = FindPattern(file.Start, (int)file.Size, pattern);
                for (int item : list)
                {
                    INFO(Client, "Address in exe (by EIP): 0x{:0>8x}", item);
                }

                file.Unload();
                if (g_PacketManager.m_PacketsStack.empty())
                {
                    INFO(Client, "Packets stack is empty.");
                }
                else
                {
                    INFO(Client, "Packets in stack:");
                    for (deque<vector<uint8_t>>::iterator i =
                             g_PacketManager.m_PacketsStack.begin();
                         i != g_PacketManager.m_PacketsStack.end();
                         ++i)
                    {
                        INFO_DUMP(Client, "Packet data:", (uint8_t *)i->data(), int(i->size()));
                    }
                }
                crashlog = true;
            }
            /*
            g_Logger.Close();
            os_path crashlogPath = L"\"" + g_Logger.FileName + L"\"";
            STARTUPINFOW si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            bool reportSent = CreateProcessW(
                L"CrashReporter.exe",
                &crashlogPath[0], // Command line
                nullptr,          // Process handle not inheritable
                nullptr,          // Thread handle not inheritable
                FALSE,            // Set handle inheritance to FALSE
                0,                // No creation flags
                nullptr,          // Use parent's environment block
                nullptr,          // Use parent's starting directory
                &si,              // Pointer to STARTUPINFO structure
                &pi);
*/
            g_Game.Uninstall();

            //if (!reportSent)
            {
                string msg = "CrossUO client performed an unrecoverable operation.";
                if (crashlog)
                    msg += "\nCrashlog has been created in crashlogs folder.";
                msg += "\nTerminating...";

                MessageBoxA(0, msg.c_str(), 0, MB_ICONSTOP | MB_OK);
            }

            ExitProcess(1);
        }

        if (ticks - lastErrorTime > 5000)
        {
            errorCount = 0;
            lastErrorTime = ticks;
        }

        if (errorCount == 1)
            DumpCurrentRegistersInformation(exceptionInfo->ContextRecord);
    }

    //	return EXCEPTION_CONTINUE_SEARCH;
    return EXCEPTION_CONTINUE_EXECUTION;
}
#endif // XUO_WINDOWS
