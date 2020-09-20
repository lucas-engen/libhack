/**
 * @file process.c
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Operações com processos
 * @version 0.1
 * @date 2019-08-10
 * 
 * @copyright Copyright (c) 2019 Lucas Vieira
 * 
 */

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <assert.h>
#include <io.h>
#ifndef bool
#include <stdbool.h>
#endif
#include "process.h"

/**
 * @brief Checking types
 * 
 */
enum CHECK_TYPES {
	WRITE_CHECK,
	READ_CHECK
};

/**
 * @brief Checks if the specified handle can be used to specified 'type' access
 * 
 * @param handle Handle to libhack
 * @param type Check to be performed
 * @return true On success
 * @return false On error
 */
static bool libhack_perform_check(struct libhack_handle *handle, enum CHECK_TYPES type)
{
	switch(type)
	{
		case WRITE_CHECK:
		case READ_CHECK:

			// Checks if the process can be opened for read or write
			if((!handle) || !(handle->bProcessIsOpen))
				return false;

		break;
	}

	return true;
}

BOOL libhack_open_process(struct libhack_handle *handle)
{
	char v1[8], v2[8];

	if(!handle)
		return FALSE;

	// We need to check if loaded version is compatible with .dll version
	memset(v1, 0, sizeof(v1));
	memset(v2, 0, sizeof(v2));

	snprintf(v1, arraySize(v1), "%s", libhack_getversion());
	snprintf(v2, arraySize(v2), "%d.%d.%d", MAJOR, MINOR, PATCH);
	
	if(strncmp(v1, v2, strlen(v1)) != 0) {
		libhack_debug("libhack version mismatch: %s != %s\n", v1, v2);
		return FALSE;
	}
	
	/* Check if the process is already open */
	if(!handle->bProcessIsOpen)
	{
		DWORD pid = libhack_get_process_id(handle);
		if(pid)
		{
			handle->hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

			if(!handle->hProcess) {
				libhack_debug("Failed to open process with pid %lu: %lu\n", pid, GetLastError());
				return FALSE;
			}

			/* Update flag */
			handle->bProcessIsOpen = TRUE;

			return TRUE;
		}

		return FALSE;
	}

	/* Handle already opened */	
	SetLastError(ERROR_ALREADY_INITIALIZED);

	return TRUE;
}

DWORD libhack_get_process_id(struct libhack_handle *handle)
{
	HANDLE hSnapshot;
	PROCESSENTRY32 *entry = NULL;
	DWORD pid = 0;
	size_t max_count = 0;

	/* Check if the process is already open */
	if(handle->bProcessIsOpen)
	{
		if(!handle->pid)
			return GetProcessId(handle->hProcess);
		
		return handle->pid;
	}

	/* Allocate memory */
	entry = (PROCESSENTRY32*)malloc(sizeof(PROCESSENTRY32));
	if(!entry)
	{
		libhack_debug("Failed to allocate memory\n");
		return 0;
	}
	
	/* Create a snapshot */
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(!hSnapshot)
	{
		libhack_debug("Failed to create snapshot\n");
		return 0;
	}

	if(!Process32First(hSnapshot, entry)) {
		libhack_debug("Failed to initialize process list: %lu\n", GetLastError());
		return 0;
	}

	/* Get process exe name length */
	max_count = strlen(handle->process_name);

	do 
	{
		if(strnicmp(entry->szExeFile, handle->process_name, max_count) == 0)
		{
			pid = entry->th32ProcessID;
			break;
		}
	} while(Process32Next(hSnapshot, entry));

	/* Close process handle */
	CloseHandle(hSnapshot);

	return pid;
}

int libhack_read_int_from_addr64(struct libhack_handle *handle, DWORD64 addr)
{
	int value = -1;
	SIZE_T readed;

	/* Validate handle */
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("check failed! Either process is not opened or handle is invalid\n");
		return -1;
	}

	/* Read memory at the specified address */
	if(!ReadProcessMemory(handle->hProcess, (const void*)addr, (void*)&value, sizeof(int), &readed)) {
		libhack_debug("Failed to read memory: %lu\n", GetLastError());
		return -1;
	}

	return readed ? value : -1;
}

int libhack_write_int_to_addr64(struct libhack_handle *handle, DWORD64 addr, int value)
{
	SIZE_T written = 0;

	/* Validate parameters */
	if(!libhack_perform_check(handle, WRITE_CHECK)) {
		libhack_debug("check failed! Either process is not opened or handle is invalid\n");
		return -1;
	}

	/* Write memory at the specified address */
	if(!WriteProcessMemory(handle->hProcess, (void*)addr, (const void*)&value, sizeof(int), &written)) {
		libhack_debug("Failed to write memory: %lu\n", GetLastError());
		return -1;
	}

	return written ? value : 0;
}

DWORD64 libhack_get_base_addr64(struct libhack_handle *handle)
{
	HMODULE module;
    DWORD needed;
	char procName[BUFLEN];

	/* Validate parameters */
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("check failed! Either process is not opened or handle is invalid\n");
		return -1;
	}

	/* Initialize memory */
    RtlSecureZeroMemory(procName, sizeof(procName));

    /* Check if we have a base address already */
    if(handle->base_addr)
        return handle->base_addr;

    /* Enumerate process modules */
    if(K32EnumProcessModulesEx(handle->hProcess, &module, sizeof(HMODULE), &needed, LIST_MODULES_ALL))
    {
        K32GetModuleBaseNameA(handle->hProcess, module, procName, BUFLEN);

		libhack_debug("Name: %s (%s)\n", procName, handle->process_name);

        if(strnicmp(procName, handle->process_name, strlen(handle->process_name)) == 0)
        {
            handle->hModule = module;
			return (DWORD64)module;
        }
    }
	
	libhack_debug("we failed to get process base address: %lu\n", GetLastError());

	return 0;
}

LIBHACK_API int libhack_read_int_from_addr(struct libhack_handle *handle, DWORD addr)
{
	int value = -1;
	SIZE_T readed;

	/* Validate handle */
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("check failed! Either process is not opened or handle is invalid\n");
		return -1;
	}

	/* Read memory at the specified address */
	if(!ReadProcessMemory(handle->hProcess, (const void*)addr, (void*)&value, sizeof(DWORD), &readed)) {
		libhack_debug("Failed to read memory: %lu\n", GetLastError());
		return -1;
	}

	return readed ? value : -1;
}

LIBHACK_API int libhack_write_int_to_addr(struct libhack_handle *handle, DWORD addr, int value)
{
	SIZE_T written = 0;

	/* Validate parameters */
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("check failed! Either process is not opened or handle is invalid\n");
		return -1;
	}

	/* Write memory at the specified address */
	if(!WriteProcessMemory(handle->hProcess, (void*)addr, (const void*)&value, sizeof(int), &written)) {
		libhack_debug("Failed to write memory: %lu\n", GetLastError());
		return -1;
	}

	return written ? value : 0;
}

LIBHACK_API DWORD libhack_get_base_addr(struct libhack_handle *handle)
{
	HMODULE module;
    DWORD needed;
	char procName[BUFLEN];

	/* Validate parameters */
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("check failed! Either process is not opened or handle is invalid\n");
		return -1;
	}

	/* Initialize memory */
    RtlSecureZeroMemory(procName, sizeof(procName));

    /* Checks if we have a base address already */
    if(handle->base_addr)
        return handle->base_addr;

    /* Enumerate process modules */
    if(K32EnumProcessModulesEx(handle->hProcess, &module, sizeof(HMODULE), &needed, LIST_MODULES_ALL))
    {
        K32GetModuleBaseNameA(handle->hProcess, module, procName, BUFLEN);

        if(strnicmp(procName, handle->process_name, strlen(handle->process_name)) == 0)
        {
            handle->hModule = module;
			return (DWORD)module;
        }
    }
	
	return 0;   
}

LIBHACK_API BOOL libhack_process_is_running(struct libhack_handle *handle)
{
	DWORD state;

	// Validate parameters
	if(!libhack_perform_check(handle, READ_CHECK)) {
		libhack_debug("check failed! Either process is not opened or handle is invalid\n");
		return FALSE;
	}

	// Try to get exit code of the process if any
	if(!GetExitCodeProcess(handle->hProcess, &state))
	{
		libhack_debug("Failed to get process exit code\n");
		return FALSE;
	}
	
	return state == STILL_ACTIVE ? TRUE : FALSE;
}

LIBHACK_API int libhack_write_string_to_addr(struct libhack_handle *handle, DWORD addr, const char *string, size_t string_len)
{
	SIZE_T written = 0;

	/* Validate parameters */
	if(!libhack_perform_check(handle, WRITE_CHECK)) {
		libhack_debug("check failed! Either process is not opened or handle is invalid\n");
		return -1;
	}

	libhack_debug("Address: %#llx\n", addr);

	/* Write memory at the specified address */
	if(!WriteProcessMemory(handle->hProcess, (void*)addr, string, string_len, &written)) {
		libhack_debug("Failed to write memory: %lu\n", GetLastError());
		return -1;
	}

	return written ? (int)written : 0;	
}

int libhack_write_string_to_addr64(struct libhack_handle *handle, DWORD64 addr, const char *string, size_t string_len)
{
	SIZE_T written = 0;

	/* Validate parameters */
	if(!libhack_perform_check(handle, WRITE_CHECK)) {
		libhack_debug("check failed! Either process is not opened or handle is invalid\n");
		return -1;
	}

	/* Write memory at the specified address */
	if(!WriteProcessMemory(handle->hProcess, (void*)addr, string, string_len, &written)) {
		libhack_debug("Failed to write memory: %lu\n", GetLastError());
		return -1;
	}

	return written ? (int)written : 0;
}

LIBHACK_API BOOL libhack_inject_dll(struct libhack_handle *handle, const char *dll_path)
{
	void *pDllPath = NULL;
	DWORD threadId;
	HANDLE hRemoteThread = NULL;
	DWORD waitStatus;
	HANDLE hKernel32 = NULL;
	size_t dll_path_len = 0;

	libhack_assert_or_return(handle, FALSE);
	libhack_assert_or_return(dll_path, FALSE);
	
	if(!handle->bProcessIsOpen) {
		libhack_debug("you need to call libhack_open_process() before trying to inject dll on it\n");
		return FALSE;
	}

	if(!(access(dll_path, 0) != -1)) {
		libhack_debug("%s could not be found. Don't forget to specify a full path to dll\n", dll_path);
		return FALSE;
	}

	hKernel32 = LoadLibraryA("kernel32.dll");
	libhack_assert_or_return(hKernel32, FALSE);

	dll_path_len = strlen(dll_path);

	// Allocate memory to store full path of dll to be loaded into target process memory
	pDllPath = VirtualAllocEx(handle->hProcess, NULL, dll_path_len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if(!pDllPath) {
		libhack_debug("virtual alloc failed: %lu\n", GetLastError());
		return FALSE;
	}

	if(!WriteProcessMemory(handle->hProcess, pDllPath, dll_path, dll_path_len, NULL)) {
		libhack_debug("failed to write process memory: %lu\n", GetLastError());
		VirtualFreeEx(handle->hProcess, pDllPath, dll_path_len, MEM_RELEASE);
		return FALSE;
	}

	// Creates the remote thread on target process that will load library
	hRemoteThread = CreateRemoteThread(handle->hProcess, NULL, 0, 
	(LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA"),
	pDllPath, 0, &threadId);

	// Checks if dll injection was completed
	if(!hRemoteThread) {
		libhack_debug("failed to inject dll: %lu\n", GetLastError());
		return FALSE;
	}

	// Once library is loaded we can release all resources
	if(!VirtualFreeEx(handle->hProcess, pDllPath, dll_path_len, MEM_RELEASE)) {
		libhack_debug("warn: dll injection was successfull but we failed to free virtual memory: %lu\n", GetLastError());
	}

	libhack_assert_or_warn(CloseHandle(hRemoteThread));

	// TRUE because dll was injected on target process
	return TRUE;
}