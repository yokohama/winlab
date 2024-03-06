#include <windows.h>
#include <iostream>

typedef void (*PRINTTEST)();
typedef DWORD (WINAPI *PFNGETVERSION)(void);

int main() {
    /*
    HINSTANCE hDLL = LoadLibrary(TEXT("print_test.dll"));
    if (hDLL == NULL) {
        std::cerr << "Err" << std::endl;
        return -1;
    }

    PRINTTEST printTest = (PRINTTEST)GetProcAddress(hDLL, "printTest");
    if (printTest == NULL) {
        std::cerr << "ErrPrintTest" << std::endl;
        return -1;
    }

    printTest();
    FreeLibrary(hDLL);
    */

    HMODULE hModule = GetModuleHandle(TEXT("kernel32.dll"));
    PFNGETVERSION pfnGetVersion = (PFNGETVERSION)GetProcAddress(hModule, "GetVersion");

    DWORD version = pfnGetVersion();
    std::cout << "Windows Version: " << (version & 0xFF) << "." << ((version >> 8) & 0xFF) << std::endl;


    return 0;
}