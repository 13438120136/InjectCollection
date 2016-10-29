// InjectDllByOEP.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>

using namespace std;


BOOL GrantPriviledge(WCHAR* PriviledgeName);

UINT32 GetLoadDllByOEPOffsetInFile(PVOID DllBuffer);

UINT32 RVAToOffset(UINT32 RVA, PIMAGE_NT_HEADERS NtHeader);

BOOL GetProcessIdByProcessImageName(IN WCHAR* wzProcessImageName, OUT UINT32* TargetProcessId);

HANDLE WINAPI LoadRemoteDll(HANDLE ProcessHandle, PVOID ModuleFileBaseAddress, UINT32 ModuleFileSize, LPVOID lParam);

CHAR DllFullPath[MAX_PATH] = { 0 };

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
	GetProcessIdByProcessImageName(L"explorer.exe", &ProcessId);
	strcat_s(DllFullPath, "\\x64LoadRemoteDll.dll");
#else
	GetProcessIdByProcessImageName(L"notepad++.exe", &ProcessId);
	strcat_s(DllFullPath, "\\x86LoadRemoteDll.dll");
#endif

	// ���dll���
	HANDLE FileHandle = CreateFileA(DllFullPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (FileHandle == INVALID_HANDLE_VALUE)
	{
		printf("Open File Error\r\n");
		return 0;
	}

	// ���dll�ļ�����
	UINT32	FileSize = GetFileSize(FileHandle, NULL);
	if (FileSize == INVALID_FILE_SIZE || FileSize == 0)
	{
		printf("Get File Size Error\r\n");
		CloseHandle(FileHandle);
		return 0;
	}

	// �����ڴ棬����
	PVOID	FileData = HeapAlloc(GetProcessHeap(), 0, FileSize);
	if (FileData == NULL)
	{
		printf("HeapAlloc Error\r\n");
		CloseHandle(FileHandle);
		return 0;
	}

	DWORD ReturnLength = 0;
	BOOL bOk = ReadFile(FileHandle, FileData, FileSize, &ReturnLength, NULL);
	CloseHandle(FileHandle);
	if (bOk == FALSE)
	{
		printf("ReadFile Error\r\n");
		HeapFree(GetProcessHeap(), 0, FileData);
		return 0;
	}

	HANDLE ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessId);
	if (ProcessHandle == NULL)
	{
		printf("OpenProcess Error\r\n");
		HeapFree(GetProcessHeap(), 0, FileData);
		return 0;
	}

	// ִ��Dll�еĵ�������LoadDllByOEP����Ŀ�����ʵ��LoadLibrary����
	HANDLE ThreadHandle = LoadRemoteDll(ProcessHandle, FileData, FileSize, NULL);
	if (ThreadHandle == NULL)
	{
		goto _Clear;
	}

	WaitForSingleObject(ThreadHandle, INFINITE);

_Clear:

	if (FileData)
	{
		HeapFree(GetProcessHeap(), 0, FileData);
	}

	if (ProcessHandle)
	{
		CloseHandle(ProcessHandle);
	}

	return 0;
}


/************************************************************************
*  Name : LoadRemoteDll
*  Param: ProcessHandle			���̾��	��IN��
*  Param: ModuleBaseAddress		ģ�����ַ
*  Param: ModuleLength			ģ�����ļ��еĴ�С
*  Param: lParam				ģ����
*  Ret  : HANDLE
*  ��Dll���ļ���ʽд��Ŀ������ڴ棬��ִ��Dll�ĵ�������LoadDllByOEP
************************************************************************/

HANDLE WINAPI LoadRemoteDll(HANDLE ProcessHandle, PVOID ModuleFileBaseAddress, UINT32 ModuleFileSize, LPVOID lParam)
{

	HANDLE	ThreadHandle = NULL;

	__try
	{
		if (ProcessHandle == NULL || ModuleFileBaseAddress == NULL || ModuleFileSize == 0)
		{
			return NULL;
		}

		// ������������� ModuelBaseAddress �� Offset
		UINT32	FunctionOffset = GetLoadDllByOEPOffsetInFile(ModuleFileBaseAddress);
		if (FunctionOffset == 0)
		{
			return NULL;
		}

		// ��Ŀ����������ڴ�
		PVOID	RemoteBufferData = VirtualAllocEx(ProcessHandle, NULL, ModuleFileSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (RemoteBufferData == NULL)
		{
			return NULL;
		}
		
		// ��Dll�ļ�д��Ŀ������ڴ�ռ�
		BOOL	bOk = WriteProcessMemory(ProcessHandle, RemoteBufferData, ModuleFileBaseAddress, ModuleFileSize, NULL);
		if (bOk == FALSE)
		{
			return NULL;
		}

		// ���ļ���ʽȥִ�� Dll �е� LoadDllByOEP
		LPTHREAD_START_ROUTINE	RemoteThreadCallBack = (LPTHREAD_START_ROUTINE)((PUINT8)RemoteBufferData + FunctionOffset);

		ThreadHandle = CreateRemoteThread(ProcessHandle, NULL, 1024 * 1024, RemoteThreadCallBack, lParam, 0, NULL);

	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ThreadHandle = NULL;
	}

	return ThreadHandle;
}


/************************************************************************
*  Name : LoadRemoteDll
*  Param: ProcessHandle			���̾��
*  Ret  : HANDLE
*  ���LoadDllByOEP��Dll�ļ��е�ƫ����
************************************************************************/

UINT32 GetLoadDllByOEPOffsetInFile(PVOID DllBuffer)
{
	UINT_PTR			BaseAddress = (UINT_PTR)DllBuffer;
	PIMAGE_DOS_HEADER	DosHeader = NULL;
	PIMAGE_NT_HEADERS	NtHeader = NULL;

	DosHeader = (PIMAGE_DOS_HEADER)BaseAddress;
	NtHeader = (PIMAGE_NT_HEADERS)((PUINT8)BaseAddress + DosHeader->e_lfanew);

	/*
	#define IMAGE_NT_OPTIONAL_HDR32_MAGIC      0x10b
	#define IMAGE_NT_OPTIONAL_HDR64_MAGIC      0x20b
	#define IMAGE_ROM_OPTIONAL_HDR_MAGIC       0x107
	*/

	if (NtHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)	// pe32
	{
	}
	else if (NtHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)	// pe64
	{
	}
	else
	{
		return 0;
	}

	UINT32					ExportDirectoryRVA = NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	PIMAGE_EXPORT_DIRECTORY	ExportDirectory = (PIMAGE_EXPORT_DIRECTORY)((PUINT8)BaseAddress + RVAToOffset(ExportDirectoryRVA, NtHeader));

	UINT32					AddressOfNamesRVA = ExportDirectory->AddressOfNames;
	PUINT32					AddressOfNames = (PUINT32)((PUINT8)BaseAddress + RVAToOffset(AddressOfNamesRVA, NtHeader));

	UINT32					AddressOfFunctionsRVA = ExportDirectory->AddressOfFunctions;
	PUINT32					AddressOfFunctions = (PUINT32)((PUINT8)BaseAddress + RVAToOffset(AddressOfFunctionsRVA, NtHeader));

	UINT32					AddressOfNameOrdinalsRVA = ExportDirectory->AddressOfNameOrdinals;
	PUINT16					AddressOfNameOrdinals = (PUINT16)((PUINT8)BaseAddress + RVAToOffset(AddressOfNameOrdinalsRVA, NtHeader));

	for (UINT32 i = 0; i < ExportDirectory->NumberOfFunctions; i++)
	{
		CHAR*	ExportFunctionName = (CHAR*)((PUINT8)BaseAddress + RVAToOffset(*AddressOfNames, NtHeader));

		if (strstr(ExportFunctionName, "LoadDllByOEP") != NULL)
		{
			UINT16	ExportFunctionOrdinals = AddressOfNameOrdinals[i];

			return RVAToOffset(AddressOfFunctions[ExportFunctionOrdinals], NtHeader);
		}
	}
	return 0;
}

/************************************************************************
*  Name : RVAToOffset
*  Param: RVA				�ڴ���ƫ��
*  Param: NtHeader			Ntͷ
*  Ret  : UINT32
*  �ڴ���ƫ��ת�����ļ���ƫ��
************************************************************************/

UINT32 RVAToOffset(UINT32 RVA, PIMAGE_NT_HEADERS NtHeader)
{
	UINT32					i = 0;
	PIMAGE_SECTION_HEADER	SectionHeader = NULL;

	SectionHeader = IMAGE_FIRST_SECTION(NtHeader);

	if (RVA < SectionHeader[0].PointerToRawData)
	{
		return RVA;
	}

	for (i = 0; i < NtHeader->FileHeader.NumberOfSections; i++)
	{
		if (RVA >= SectionHeader[i].VirtualAddress && RVA < (SectionHeader[i].VirtualAddress + SectionHeader[i].SizeOfRawData))
		{
			return (RVA - SectionHeader[i].VirtualAddress + SectionHeader[i].PointerToRawData);
		}
	}

	return 0;
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