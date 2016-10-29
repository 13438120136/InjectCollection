// SetWindowsHookEx.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <Windows.h>
#include <iostream>
#include <vector>
#include <TlHelp32.h>

using namespace std;


BOOL GrantPriviledge(IN PWCHAR PriviledgeName);
BOOL GetProcessIdByProcessImageName(IN WCHAR* wzProcessImageName, OUT UINT32* TargetProcessId);
BOOL GetThreadIdByProcessId(UINT32 ProcessId, vector<UINT32>& ThreadIdVector);
BOOL Inject(IN UINT32 ThreadId, OUT HHOOK& HookHandle);



CHAR	DllFullPath[MAX_PATH] = { 0 };



int main()
{
	// ������Ȩһ��
	if (GrantPriviledge(SE_DEBUG_NAME) == FALSE)
	{
		printf("GrantPriviledge Error\r\n");
	}

	// ����ͨ���������õ�����id
	UINT32	ProcessId = 0;

	GetCurrentDirectoryA(MAX_PATH, DllFullPath);

#ifdef _WIN64
	//	GetProcessIdByProcessImageName(L"Taskmgr.exe", &ProcessId);
	//	GetProcessIdByProcessImageName(L"calculator.exe", &ProcessId);
	GetProcessIdByProcessImageName(L"explorer.exe", &ProcessId);
	strcat_s(DllFullPath, "\\x64WindowHookDll.dll");
#else
	GetProcessIdByProcessImageName(L"notepad++.exe", &ProcessId);
	strcat_s(DllFullPath, "\\x86WindowHookDll.dll");
#endif



	// Ȼ��ͨ������idö�ٵ������߳�id
	vector<UINT32> ThreadIdVector;
	GetThreadIdByProcessId(ProcessId, ThreadIdVector);

	HHOOK HookHandle = NULL;

	for (UINT32 ThreadId : ThreadIdVector)
	{
		Inject(ThreadId, HookHandle);
		break;
	}

	printf("Input Any Key To UnHook\r\n");
	getchar();
	getchar();

	UnhookWindowsHookEx(HookHandle);

	return 0;
}

/************************************************************************
*  Name : Inject
*  Param: ThreadId			�߳�Id			��IN��
*  Param: HookHandle		��Ϣ���Ӿ��	��OUT��
*  Ret  : BOOL
*  ��Ŀ���̵߳�ָ����Ϣ���¹����߽�Dll��������
************************************************************************/

BOOL Inject(IN UINT32 ThreadId, OUT HHOOK& HookHandle)
{
	HMODULE	DllModule = LoadLibraryA(DllFullPath);
	FARPROC FunctionAddress = GetProcAddress(DllModule, "Sub_1");

	HookHandle = SetWindowsHookEx(WH_KEYBOARD, (HOOKPROC)FunctionAddress, DllModule, ThreadId);
	if (HookHandle == NULL)
	{
		return FALSE;
	}
	return TRUE;
}

/************************************************************************
*  Name : GetProcessIdByProcessImageName
*  Param: ProcessId				����Id		��IN��
*  Param: ThreadIdVector		�߳�Idģ��	��OUT��
*  Ret  : BOOL
*  ö���ƶ�����Id�������̣߳�ѹ��ģ���У������߳�ģ�弯�ϣ�TlHelp32��
************************************************************************/

BOOL GetThreadIdByProcessId(UINT32 ProcessId, vector<UINT32>& ThreadIdVector)
{
	HANDLE			ThreadSnapshotHandle = NULL;
	THREADENTRY32	ThreadEntry32 = { 0 };

	ThreadEntry32.dwSize = sizeof(THREADENTRY32);

	ThreadSnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (ThreadSnapshotHandle == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	Thread32First(ThreadSnapshotHandle, &ThreadEntry32);
	do
	{
		if (ThreadEntry32.th32OwnerProcessID == ProcessId)
		{
			ThreadIdVector.emplace_back(ThreadEntry32.th32ThreadID);		// �Ѹý��̵������߳�idѹ��ģ��
		}
	} while (Thread32Next(ThreadSnapshotHandle, &ThreadEntry32));

	CloseHandle(ThreadSnapshotHandle);
	ThreadSnapshotHandle = NULL;
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

	if (*TargetProcessId == 0)
	{
		return FALSE;
	}

	return TRUE;
}

/************************************************************************
*  Name : GrantPriviledge
*  Param: PriviledgeName		��Ҫ������Ȩ��
*  Ret  : BOOLEAN
*  �����Լ���Ҫ��Ȩ��
************************************************************************/

BOOL GrantPriviledge(IN PWCHAR PriviledgeName)
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