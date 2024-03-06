#include <windows.h>
#include <iostream>

#define DLL_EXPORT extern "C" __declspec(dllexport)

DLL_EXPORT void printTest() {
    std::cout << "<< Test >>" << std::endl;
}

BOOL WINAPI DllMain(HINSTANCE hintDLL, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            std::cout << "Dll Process Attached! " << std::endl;
            break;
        case DLL_THREAD_ATTACH:
            std::cout << "Dll Thread Attached! " << std::endl;
            break;
        case DLL_THREAD_DETACH:
            std::cout << "Dll Detached! " << std::endl;
            break;
        case DLL_PROCESS_DETACH:
            std::cout << "Dll Process Detached! " << std::endl;
            break;
    }
    return TRUE;
}