#define STRICT
#include <windows.h>
#include <tchar.h>

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR szStr, INT iCmdShow) {
    MessageBox(NULL, _T("�Ă���"), _T("�ق�����"), MB_OK | MB_ICONWARNING);
    return 0;
}