#include "core.h"
#include "error.h"
#include <string.h>

#if IS_WINDOWS
/*
	WINDOWS MEMORY FUNCTIONS
*/

void* Memory_Alloc(size_t num, size_t size) {
	void* ptr;
	if((ptr = calloc(num, size)) == NULL) {
		Error_Set(ET_SYS, GetLastError());
		return NULL;
	}
	return ptr;
}

void Memory_Copy(void* dst, const void* src, size_t count) {
	memcpy(dst, src, count);
}

void Memory_Fill(void* dst, size_t count, int val) {
	memset(dst, val, count);
}

/*
	WINDOWS FILE FUNCTIONS
*/

FILE* File_Open(const char* path, const char* mode) {
	FILE* fp;
	if((fp = fopen(path, mode)) == NULL) {
		Error_Set(ET_SYS, GetLastError());
	}
	return fp;
}

size_t File_Read(void* ptr, size_t size, size_t count, FILE* fp) {
	if(!fp) {
		Error_Set(ET_SYS, ERROR_INVALID_HANDLE);
		return 0;
	}

	int ncount;
	if(count != (ncount = fread(ptr, size, count, fp))) {
		Error_Set(ET_SYS, GetLastError());
		return ncount;
	}
	return count;
}

bool File_Write(const void* ptr, size_t size, size_t count, FILE* fp) {
	if(!fp) {
		Error_Set(ET_SYS, ERROR_INVALID_HANDLE);
		return false;
	}
	if(count != fwrite(ptr, size, count, fp)) {
		Error_Set(ET_SYS, GetLastError());
		return false;
	}
	return true;
}

bool File_WriteFormat(FILE* fp, const char* fmt, ...) {
	if(!fp) {
		Error_Set(ET_SYS, ERROR_INVALID_HANDLE);
		return false;
	}
	va_list args;
	va_start(args, fmt);
	vfprintf(fp, fmt, args);
	va_end(args);
	if(ferror(fp)) {
		Error_Set(ET_SYS, GetLastError());
		return false;
	}
	return true;
}

bool File_Close(FILE* fp) {
	if(!fp) {
		Error_Set(ET_SYS, ERROR_INVALID_HANDLE);
		return false;
	}
	if(fclose(fp) != 0) {
		Error_Set(ET_SYS, GetLastError());
		return false;
	}
	return true;
}

/*
	WINDOWS SOCKET FUNCTIONS
*/
bool Socket_Init() {
	WSADATA ws;
	if(WSAStartup(MAKEWORD(1, 1), &ws) == SOCKET_ERROR) {
		Error_Set(ET_SYS, WSAGetLastError());
		return false;
	}
	return true;
}

SOCKET Socket_Bind(const char* ip, ushort port) {
	SOCKET fd;

	if(INVALID_SOCKET == (fd = socket(AF_INET, SOCK_STREAM, 0))) {
		Error_Set(ET_SYS, WSAGetLastError());
		return INVALID_SOCKET;
	}

	struct sockaddr_in ssa;
	ssa.sin_family = AF_INET;
	ssa.sin_port = htons(port);
	if(inet_pton(AF_INET, ip, &ssa.sin_addr.s_addr) <= 0) {
		Error_Set(ET_SYS, WSAGetLastError());
		return INVALID_SOCKET;
	}

	if(bind(fd, (const struct sockaddr*)&ssa, sizeof ssa) == -1) {
		Error_Set(ET_SYS, WSAGetLastError());
		return INVALID_SOCKET;
	}

	if(listen(fd, SOMAXCONN) == -1) {
		Error_Set(ET_SYS, WSAGetLastError());
		return INVALID_SOCKET;
	}

	return fd;
}

void Socket_Close(SOCKET sock) {
	closesocket(sock);
}

/*
	WINDOWS THREAD FUNCTIONS
*/
THREAD Thread_Create(TFUNC func, const TARG lpParam) {
	return CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)func,
		lpParam,
		0,
		NULL
	);
}

bool Thread_IsValid(THREAD th) {
	return th != (THREAD)NULL;
}

bool Thread_SetName(const char* name) {
	return false; //????
}

void Thread_Close(THREAD th) {
	if(th)
		CloseHandle(th);
}

/*
	WINDOWS STRING FUNCTIONS
*/
bool String_CaselessCompare(const char* str1, const char* str2) {
	return _stricmp(str1, str2) == 0;
}

bool String_Compare(const char* str1, const char* str2) {
	return strcmp(str1, str2) == 0;
}

size_t String_Length(const char* str) {
	return strlen(str);
}

size_t String_Copy(char* dst, size_t len, const char* src) {
	if(!dst) return 0;
	if(!src) return 0;

	if(String_Length(src) < len)
		strcpy(dst, src);
	else
		return 0;

	return len;
}

char* String_CopyUnsafe(char* dst, const char* src) {
	return strcpy(dst, src);
}

uint String_FormatError(uint code, char* buf, uint buflen) {
	uint len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, 0, buf, buflen, NULL);
	if(len > 0) {
		Error_WinBuf[len - 1] = 0;
		Error_WinBuf[len - 2] = 0;
	} else {
		Error_Set(ET_SYS, GetLastError());
	}

	return len;
}

void String_FormatBufVararg(char* buf, size_t len, const char* str, va_list* args) {
	vsprintf_s(buf, len, str, *args);
}

/*
	WINDOWS TIME FUNCTIONS
*/
void Time_Format(char* buf, size_t buflen) {
	SYSTEMTIME time;
	GetSystemTime(&time);
	sprintf_s(buf, buflen, "%02d:%02d:%02d.%03d",
		time.wHour,
		time.wMinute,
		time.wSecond,
		time.wMilliseconds
	);
}
#elif IS_POSIX
/*
	POSIX MEMORY FUNCTIONS
*/

void* Memory_Alloc(size_t num, size_t size) {
	void* ptr;
	if((ptr = calloc(num, size)) == NULL) {
		Error_Set(ET_SYS, errno);
		return NULL;
	}
	return ptr;
}

void Memory_Copy(void* dst, const void* src, size_t count) {
	memcpy(dst, src, count);
}

void Memory_Fill(void* dst, size_t count, int val) {
	memset(dst, val, count);
}

/*
	POSIX FILE FUNCTIONS
*/

FILE* File_Open(const char* path, const char* mode) {
	FILE* fp;
	if((fp = fopen(path, mode)) == NULL) {
		Error_Set(ET_SYS, errno);
	}
	return fp;
}

size_t File_Read(void* ptr, size_t size, size_t count, FILE* fp) {
	if(!fp) {
		Error_Set(ET_SYS, EBADF);
		return 0;
	}

	int ncount;
	if(count != (ncount = fread(ptr, size, count, fp))) {
		Error_Set(ET_SYS, errno);
		return ncount;
	}
	return count;
}

bool File_Write(const void* ptr, size_t size, size_t count, FILE* fp) {
	if(!fp) {
		Error_Set(ET_SYS, EBADF);
		return false;
	}
	if(count != fwrite(ptr, size, count, fp)) {
		Error_Set(ET_SYS, errno);
		return false;
	}
	return true;
}

bool File_WriteFormat(FILE* fp, const char* fmt, ...) {
	if(!fp) {
		Error_Set(ET_SYS, EBADF);
		return false;
	}

	va_list args;
	va_start(args, fmt);
	vfprintf(fp, fmt, args);
	va_end(args);
	if(ferror(fp)) {
		Error_Set(ET_SYS, errno);
		return false;
	}
	return true;
}

bool File_Close(FILE* fp) {
	if(!fp) {
		Error_Set(ET_SYS, EBADF);
		return false;
	}
	if(fclose(fp) != 0) {
		Error_Set(ET_SYS, errno);
		return false;
	}
	return true;
}

/*
	POSIX SOCKET FUNCTIONS
*/
bool Socket_Init() {
	return true;
}

SOCKET Socket_Bind(const char* ip, ushort port) {
	SOCKET fd;

	if((fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		Error_Set(ET_SYS, errno);
		return INVALID_SOCKET;
	}

	struct sockaddr_in ssa;
	ssa.sin_family = AF_INET;
	ssa.sin_port = htons(port);
	ssa.sin_addr.s_addr = inet_addr(ip);

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
		Error_Set(ET_SYS, errno);
		return INVALID_SOCKET;
	}

	if(bind(fd, (const struct sockaddr*)&ssa, sizeof(ssa)) == -1) {
		Error_Set(ET_SYS, errno);
		return INVALID_SOCKET;
	}

	if(listen(fd, SOMAXCONN) == -1) {
		Error_Set(ET_SYS, errno);
		return INVALID_SOCKET;
	}

	return fd;
}

void Socket_Close(SOCKET fd) {
	close(fd);
}

/*
	POSIX THREAD FUNCTIONS
*/
THREAD Thread_Create(TFUNC func, const TARG arg) {
	pthread_t thread;
	if(pthread_create(&thread, NULL, func, arg) != 0) {
		Error_Set(ET_SYS, errno);
		return -1;
	}
	return (THREAD)thread;
}

bool Thread_IsValid(THREAD th) {
	return th != (THREAD)-1;
}

bool Thread_SetName(const char* thName) {
	return pthread_setname_np(pthread_self(), thName) == 0;
}

void Thread_Close(THREAD th) {}

/*
	POSIX STRING FUNCTIONS
*/
bool String_CaselessCompare(const char* str1, const char* str2) {
	return strcasecmp(str1, str2) == 0;
}

bool String_Compare(const char* str1, const char* str2) {
	return strcmp(str1, str2) == 0;
}

size_t String_Length(const char* str) {
	return strlen(str);
}

size_t String_Copy(char* dst, size_t len, const char* src) {
	if(!dst) return 0;
	if(!src) return 0;

	if(String_Length(src) < len)
		strcpy(dst, src);
	else
		return 0;

	return len;
}

char* String_CopyUnsafe(char* dst, const char* src) {
	return strcpy(dst, src);
}

uint String_FormatError(uint code, char* buf, uint buflen) {
	char* errstr = strerror(code);
	return String_Copy(buf, buflen, errstr);
}

void String_FormatBufVararg(char* buf, size_t len, const char* str, va_list* args) {
	vsnprintf(buf, len, str, *args);
}

/*
	POSIX TIME FUNCTIONS
*/

void Time_Format(char* buf, size_t buflen) {
	struct timeval tv;
	struct tm* tm;
	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);

	if(buflen > 12) {
		sprintf(buf, "%02d:%02d:%02d.%03d",
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec,
			(int) (tv.tv_usec / 1000)
		);
	}
}
#endif
