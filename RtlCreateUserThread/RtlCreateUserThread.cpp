// RtlCreateUserThread.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>

using namespace std;

#define NT_SUCCESS(x) ((x) >= 0)

typedef struct _CLIENT_ID {
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

typedef NTSTATUS(NTAPI * pfnRtlCreateUserThread)(
	IN HANDLE ProcessHandle,
	IN PSECURITY_DESCRIPTOR SecurityDescriptor OPTIONAL,
	IN BOOLEAN CreateSuspended,
	IN ULONG StackZeroBits OPTIONAL,
	IN SIZE_T StackReserve OPTIONAL,
	IN SIZE_T StackCommit OPTIONAL,
	IN PTHREAD_START_ROUTINE StartAddress,
	IN PVOID Parameter OPTIONAL,
	OUT PHANDLE ThreadHandle OPTIONAL,
	OUT PCLIENT_ID ClientId OPTIONAL);

BOOL GrantPriviledge(WCHAR* PriviledgeName);
BOOL GetProcessIdByProcessImageName(IN WCHAR* wzProcessImageName, OUT UINT32* TargetProcessId);
BOOL InjectDll(UINT32 ProcessId);

CHAR	DllFullPath[MAX_PATH] = { 0 };

int main()
{
	printf("This Injection used RtlCreateUserThread\r\n");
	// ������Ȩһ��
	if (GrantPriviledge(SE_DEBUG_NAME) == FALSE)
	{
		printf("GrantPriviledge Error\r\n");
	}

	// ����ͨ���������õ�����id
	UINT32	ProcessId = 0;

	GetCurrentDirectoryA(MAX_PATH, DllFullPath);

#ifdef _WIN64
	GetProcessIdByProcessImageName(L"Taskmgr.exe", &ProcessId);
	strcat_s(DllFullPath, "\\x64Dll.dll");
#else
	GetProcessIdByProcessImageName(L"notepad++.exe", &ProcessId);
	strcat_s(DllFullPath, "\\x86Dll.dll");
#endif

	if (ProcessId == 0)
	{
		printf("Can't Find Target Process\r\n");
		return 0;
	}

	printf("DllFullPath is :%s\r\n", DllFullPath);
	printf("Target ProcessId is :%d\r\n", ProcessId);

	BOOL bOk = InjectDll(ProcessId);
	if (bOk == FALSE)
	{
		printf("Inject Error\r\n");
		return 0;
	}

	printf("Inject Success\r\nInput Any Key To Exit\r\n");
	getchar();
	getchar();

	return 0;
}

/************************************************************************
*  Name : InjectDll
*  Param: ProcessId		����Id
*  Ret  : BOOLEAN
*  ʹ��CreateRemoteThread����Զ���߳�ʵ��ע��
************************************************************************/

BOOL InjectDll(UINT32 ProcessId)
{
	HANDLE ProcessHandle = NULL;

	ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessId);

	// �ڶԷ����̿ռ������ڴ�,�洢Dll����·��
	UINT32	DllFullPathLength = (strlen(DllFullPath) + 1);
	PVOID DllFullPathBufferData = VirtualAllocEx(ProcessHandle, NULL, DllFullPathLength, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (DllFullPathBufferData == NULL)
	{
		CloseHandle(ProcessHandle);
		return FALSE;
	}

	// ��DllFullPathд���ո�������ڴ���
	SIZE_T	ReturnLength;
	BOOL bOk = WriteProcessMemory(ProcessHandle, DllFullPathBufferData, DllFullPath, strlen(DllFullPath) + 1, &ReturnLength);

	LPTHREAD_START_ROUTINE	LoadLibraryAddress = NULL;
	HMODULE					Kernel32Module = GetModuleHandle(L"Kernel32");

	LoadLibraryAddress = (LPTHREAD_START_ROUTINE)GetProcAddress(Kernel32Module, "LoadLibraryA");

	pfnRtlCreateUserThread RtlCreateUserThread = (pfnRtlCreateUserThread)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "RtlCreateUserThread");


	HANDLE ThreadHandle = NULL;
	NTSTATUS Status = RtlCreateUserThread(ProcessHandle, NULL, FALSE, 0, 0, 0, LoadLibraryAddress, DllFullPathBufferData, &ThreadHandle, NULL);
	if (!NT_SUCCESS(Status) || ThreadHandle == NULL)
	{
		CloseHandle(ProcessHandle);
		return FALSE;
	}

	if (WaitForSingleObject(ThreadHandle, INFINITE) == WAIT_FAILED)
	{
		return FALSE;
	}

	CloseHandle(ProcessHandle);
	CloseHandle(ThreadHandle);

	return TRUE;
}

/************************************************************************
*  Name : GetProcessIdByProcessImageName
*  Param: wzProcessImageName		����ӳ������	��IN��
*  Param: TargetProcessId			����Id			��OUT��
*  Ret  : BOOLEAN
*  ʹ��ToolHelpϵ�к���ͨ������ӳ�����ƻ�ý���Id
************************************************************************/

BOOL GetProcessIdByProcessImageName(IN WCHAR* wzProcessImageName, OUT UINT32* TargetProcessId)
{
	HANDLE			ProcessSnapshotHandle = NULL;
	PROCESSENTRY32	ProcessEntry32 = { 0 };

	ProcessEntry32.dwSize = sizeof(PROCESSENTRY32);		// ��ʼ��PROCESSENTRY32�ṹ

	ProcessSnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);	// ��ϵͳ���еĽ��̿���

	if (ProcessSnapshotHandle == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	Process32First(ProcessSnapshotHandle, &ProcessEntry32);		// �ҵ���һ��
	do
	{
		if (lstrcmpi(ProcessEntry32.szExeFile, wzProcessImageName) == 0)		// �����ִ�Сд
		{
			*TargetProcessId = ProcessEntry32.th32ProcessID;
			break;
		}
	} while (Process32Next(ProcessSnapshotHandle, &ProcessEntry32));

	CloseHandle(ProcessSnapshotHandle);
	ProcessSnapshotHandle = NULL;
	return TRUE;
}

/************************************************************************
*  Name : GrantPriviledge
*  Param: PriviledgeName		��Ҫ������Ȩ��
*  Ret  : BOOLEAN
*  �����Լ���Ҫ��Ȩ��
************************************************************************/

BOOL GrantPriviledge(WCHAR* PriviledgeName)
{
	TOKEN_PRIVILEGES TokenPrivileges, OldPrivileges;
	DWORD			 dwReturnLength = sizeof(OldPrivileges);
	HANDLE			 TokenHandle = NULL;
	LUID			 uID;

	// ��Ȩ������
	if (!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &TokenHandle))
	{
		if (GetLastError() != ERROR_NO_TOKEN)
		{
			return FALSE;
		}
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &TokenHandle))
		{
			return FALSE;
		}
	}

	if (!LookupPrivilegeValue(NULL, PriviledgeName, &uID))		// ͨ��Ȩ�����Ʋ���uID
	{
		CloseHandle(TokenHandle);
		return FALSE;
	}

	TokenPrivileges.PrivilegeCount = 1;		// Ҫ������Ȩ�޸���
	TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;    // ��̬���飬�����С����Count����Ŀ
	TokenPrivileges.Privileges[0].Luid = uID;

	// ���������ǽ��е���Ȩ��
	if (!AdjustTokenPrivileges(TokenHandle, FALSE, &TokenPrivileges, sizeof(TOKEN_PRIVILEGES), &OldPrivileges, &dwReturnLength))
	{
		CloseHandle(TokenHandle);
		return FALSE;
	}

	// �ɹ���
	CloseHandle(TokenHandle);
	return TRUE;
}