/**
 * @file hack.h
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Libhack core
 * @version 0.1
 * @date 2020-07-18
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#ifndef LIBHACK_H
#define LIBHACK_H

#include "consts.h"
#include "platform.h"
#include <stdio.h>
#include <windows.h>

#ifndef LIBHACK_API
#ifdef DLL_EXPORT
#define LIBHACK_API __declspec(dllexport)
#else
#define LIBHACK_API
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Gets the number of elements in a vector
 * 
 */
#define arraySize(x) (sizeof(x)/sizeof(x[0]))

#define libhack_assert_or_exit(condition, exit_code) \
	if(!(condition)) { \
		fprintf(stdout, "[LIBHACK] warn: assert failure on %s line %d\n", __FILE__, __LINE__); \
		exit(exit_code); \
	}

#define libhack_assert_or_warn(condition) \
	if(!(condition)) { \
		fprintf(stdout, "[LIBHACK] warn: assert failure on %s line %d\n", __FILE__, __LINE__); \
	}

#define libhack_assert_or_return(condition, retval) \
	if(!(condition)) { \
		fprintf(stdout, "[LIBHACK] warn: assert failure on %s line %d\n", __FILE__, __LINE__); \
		return retval; \
	}

/**
 * @brief Handle to libhack
 * 
 */
struct libhack_handle
{
	/**
	 * @brief Process name
	 * 
	 */
	char process_name[BUFLEN];

	/**
	 * @brief Process identifier
	 * 
	 */
	DWORD pid;
#if defined(__x64__)
	/**
	 * @brief Process base address
	 * 
	 */
	DWORD64 base_addr;
#else
	/**
	 * @brief Process base address
	 * 
	 */
	DWORD base_addr;
#endif

	/**
	 * @brief Process handle
	 * 
	 */
	HANDLE hProcess;

	/**
	 * @brief Process module handle
	 * 
	 */
	HMODULE hModule;

	/**
	 * @brief Flag to check if process is open
	 * 
	 */
	BOOL bProcessIsOpen;

	/**
	 * @brief Flag to check if process is a x64 process
	 * 
	 */
	BOOL b64BitProcess;
};

/**
 * @brief Gets the target platform of library
 * 
 * @return LIBHACK_API const* platform string
 */
LIBHACK_API const char *libhack_get_platform();

/**
 * @brief Gets the program version and return a string
 * 
 * @return const char* Program version
 */
LIBHACK_API const char *libhack_getversion();

/**
 * @brief Gets the commit UUID
 * 
 * @return LIBHACK_API const* UUID corresponding to commit
 */
LIBHACK_API const char *libhack_getuuid();

/**
 * @brief Gets the program build date on UTC
 * 
 * @return LIBHACK_API const* string containing build date
 */
LIBHACK_API const char *libhack_get_utc_build_date();

/**
 * @brief Initialize libhack
 * 
 * @param process_name Name of process to be accessed by library
 * @return struct libhack_handle* Handle to libhack or NULL on error
 */
LIBHACK_API struct libhack_handle *libhack_init(const char *process_name);

/**
 * @brief Cleanup resources used by libhack
 * 
 * @param handle Handle to libhack previously opened by libhack_init
 */
LIBHACK_API void libhack_free(struct libhack_handle *handle);

/**
 * @brief Cleanup resources used by libhack
 * 
 */
#define libhack_cleanup(handle) libhack_free(handle)

#ifdef __cplusplus
}
#endif

#endif