#define STRICT
#include <windows.h>
#include <tchar.h>

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR szStr, INT iCmdShow) {
    MessageBox(NULL, _T("‚Ä‚·‚Æ"), _T("‚Ù‚°‚Á‚Æ"), MB_OK | MB_ICONWARNING);
    return 0;
}