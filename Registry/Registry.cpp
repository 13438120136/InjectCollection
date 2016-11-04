// Registry.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <Windows.h>
#include <iostream>

using namespace std;

int main()
{
	LSTATUS Status = 0;
#ifdef _WIN64
	WCHAR*	wzSubKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows";
#else
	WCHAR*	wzSubKey = L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Windows";
#endif // _WIN64
	HKEY	hKey = NULL;

	// ��ע���
	Status = RegOpenKeyExW(HKEY_LOCAL_MACHINE,		// Ҫ�򿪵�����
		wzSubKey,			// Ҫ�򿪵��Ӽ����ֵ�ַ
		0,					// ��������0
		KEY_ALL_ACCESS,		// �򿪵ķ�ʽ
		&hKey);				// ���ص��Ӽ����
	if (Status != ERROR_SUCCESS)
	{
		return 0;
	}

	WCHAR*	wzValueName = L"AppInit_DLLs";
	DWORD	dwValueType = 0;
	UINT8	ValueData[MAX_PATH] = { 0 };
	DWORD	dwReturnLength = 0;

	// ��ѯע���
	Status = RegQueryValueExW(hKey,		// �Ӽ����
		wzValueName,		// ����ѯ��ֵ������
		NULL,				// ����
		&dwValueType,		// ��������
		ValueData,			// ��ֵ
		&dwReturnLength);

	// ׼������Ҫд���Dll·��
	WCHAR	wzDllFullPath[MAX_PATH] = { 0 };
	GetCurrentDirectoryW(MAX_PATH, wzDllFullPath);

#ifdef _WIN64
	wcscat_s(wzDllFullPath, L"\\x64NormalDll.dll");
#else
	wcscat_s(wzDllFullPath, L"\\x86NormalDll.dll");
#endif

	// ���ü�ֵ
	Status = RegSetValueExW(hKey,
		wzValueName,
		NULL,
		dwValueType,
		(CONST BYTE*)wzDllFullPath,
		(lstrlen(wzDllFullPath) + 1) * sizeof(WCHAR));
	if (Status != ERROR_SUCCESS)
	{
		return 0;
	}

	wzValueName = L"LoadAppInit_DLLs";
	DWORD	dwLoadAppInit = 1;

	// ��ѯע���
	Status = RegQueryValueExW(hKey, wzValueName, NULL, &dwValueType, ValueData, &dwReturnLength);

	// ���ü�ֵ
	Status = RegSetValueExW(hKey, wzValueName, NULL, dwValueType, (CONST BYTE*)&dwLoadAppInit, sizeof(DWORD));
	if (Status != ERROR_SUCCESS)
	{
		return 0;
	}

	printf("Input Any Key To Resume\r\n");

	getchar();
	getchar();

	// �ָ���ֵ
	dwLoadAppInit = 0;
	Status = RegQueryValueExW(hKey, wzValueName, NULL, &dwValueType, ValueData, &dwReturnLength);
	Status = RegSetValueExW(hKey, wzValueName, NULL, dwValueType, (CONST BYTE*)&dwLoadAppInit, sizeof(DWORD));

	wzValueName = L"AppInit_DLLs";
	ZeroMemory(wzDllFullPath, (lstrlen(wzDllFullPath) + 1) * sizeof(WCHAR));
	Status = RegQueryValueExW(hKey, wzValueName, NULL, &dwValueType, ValueData, &dwReturnLength);
	Status = RegSetValueExW(hKey, wzValueName, NULL, dwValueType, (CONST BYTE*)wzDllFullPath, 0);

	RegCloseKey(hKey);

	return 0;
}
