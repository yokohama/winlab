#define STRICT
#include <windows.h>
#include <tchar.h>

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR szStr, INT iCmdShow) {
    MessageBox(NULL, _T("てすと"), _T("ほげっと"), MB_OK | MB_ICONWARNING);
    return 0;
}