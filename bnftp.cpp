/*-
 * Copyright (c) 2009 Ryan Kwolek
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice, this list of
 *     conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, this list
 *     of conditions and the following disclaimer in the documentation and/or other materials
 *     provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//bnftp [-cftspqhvV] <filename>

////////////////////////[Preprocessor Directives]////////////////////////////

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment(lib, "WS2_32.lib")

#define HELPTEXT \
	"BNFTP Downloader v1.0\n" \
	" Syntax: bnftp [-cftspqhvV] <filename>\n\n" \
	" -c - client used in download request. default = STAR\n" \
	" -f - name of file to download.\n" \
	" -t - target filename to write to. default = %%USERPROFILE%%\\Desktop\\<filename>\n" \
	" -s - battle.net server to download from. default = useast.battle.net\n" \
	" -p - port of bnftp daemon. default = 6112\n" \
	" -q - subtracts 1 level of verbosity.\n" \
	" -h - displays this help text and quits.\n" \
	" -v - adds one level of verbosity.\n" \
	" -V - displays version information and quits.\n\n"

////////////////////////////////[Globals]////////////////////////////////////

int volume;

char server[32];
unsigned short port;

char filename[64];

unsigned long client;
unsigned long platform;

char target[MAX_PATH];


///////////////////////////////[Functions]///////////////////////////////////

inline void fastswap32(unsigned long *num) {									
	__asm {
		mov ecx, dword ptr [num]
		mov eax, dword ptr [ecx]
		bswap eax
		mov dword ptr [ecx], eax
	}
}
			
		  
void ParseCmdLine(int argc, char *argv[]) {
	for (int i = 1; i != argc; i++) {
		if (*argv[i] == '-') {
			switch (argv[i][1]) {
				case 'c': //client
					client = *(unsigned long *)(argv[i + 1]);
					fastswap32(&client);
					break;
				case 'm': //platform
					platform = *(unsigned long *)(argv[i + 1]);
					fastswap32(&platform);
					break;
				case 'f': //file
					strncpy(filename, argv[i + 1], sizeof(filename));
					i++;
					break;
				case 't': //target
					strncpy(target, argv[i + 1], sizeof(target));
					break;
				case 's': //server
					strncpy(server, argv[i + 1], sizeof(server));
					i++;
					break;
				case 'p': //port
					port = atoi(argv[i + 1]);
					i++;
					break;
				case 'q': //quiet
					volume--;
					break;
				case 'v': //verbose
					volume++;
					break;
				case 'h': //help
					printf(HELPTEXT);
					exit(0);
					break;
				case 'V': //version
					printf("BNFTP downloader\n version 1.0\n by Ryan Kwolek\n\n");
					exit(0);
					break;
				default:
					printf("WARNING: Invalid parameter %c\n", argv[i][1]);
			}
		} else if (i == (argc - 1) && !*filename) {
			strncpy(filename, argv[i], sizeof(filename));
		}
	}
}


void FillDefaults() {
	if (!client)
		client = 'STAR';
	if (!platform)
		platform = 'IX86';
	if (!*filename) {
		printf("please specify a file to download!\n");
		exit(0);
	}
	if (!*target) {
		GetEnvironmentVariable("userprofile", target, sizeof(target));	  
		strncat(target, "\\Desktop\\", sizeof(target));
		strncat(target, filename, sizeof(target));
	}
	if (!*server)
		strcpy(server, "useast.battle.net");
	if (!port)
		port = 6112;
}


bool ConnectSocket(SOCKET *sck, char *server, unsigned short port) {
	struct sockaddr_in sName;
	*sck = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*sck == INVALID_SOCKET)
		return false;
	sName.sin_family = AF_INET;
	sName.sin_port = htons(port);
	char *p = server;
	while (*p && (isdigit(*p) || (*p == '.')))
		p++;
	if (*p) {
		struct hostent *hstEnt = gethostbyname(server);
		memcpy(&sName.sin_addr, hstEnt->h_addr, hstEnt->h_length);
	} else {
		sName.sin_addr.s_addr = inet_addr(server);
	}
	if (connect(*sck, (struct sockaddr *)&sName, sizeof(sName)))
		return false;
	return true;
}


bool DownloadFileFromBNFTP(char *filename) {
	char buf[512];
	unsigned int totallen;
	SOCKET sckFTP;
	SYSTEMTIME systime;
	FILETIME filetime;
	WSADATA wsadata;

	if (!filename || !*filename)
		return false;

	WSAStartup(0x0202, &wsadata);
	if (wsadata.wVersion != 0x0202)
		return false;

	if (!ConnectSocket(&sckFTP, server, port)) {
		if (volume >= 0)
			printf("Error %d creating/connecting socket.\n", WSAGetLastError());
		return false;
	}

	send(sckFTP, "\x02", 1, 0);

	*(short *)(buf + 2) = 0x0100;
	*(int *)(buf + 4)   = platform;
	*(int *)(buf + 8)   = client;
	*(int *)(buf + 12)  = 0;
	*(int *)(buf + 16)  = 0;
	*(int *)(buf + 20)  = 0;
	*(int *)(buf + 24)  = 0;
	*(int *)(buf + 28)  = 0;
	strncpy(buf + 32, filename, sizeof(buf) - 32);
	totallen = 33 + strlen(filename);
	*(short *)(buf) = totallen;
	send(sckFTP, buf, totallen, 0);
	if (volume >= 0)
		printf("Sending request...\n");
	totallen = 0;

	int len = recv(sckFTP, buf, sizeof(buf), 0);
	if (len == 0 || len == -1) {
		if (volume >= 0)
			printf(!len ? "Server disconnected gracefully, file not found.\n" :
				"Server disconnected forcefully.\n");
		shutdown(sckFTP, 2);
		closesocket(sckFTP);
		return false;
	}

	char *dlfilename = buf + 24;
	unsigned int filesize = *(unsigned long *)(buf + 4);		  
	filetime.dwLowDateTime  = *(int *)(buf + 16);
	filetime.dwHighDateTime = *(int *)(buf + 20);

	char *asdf = (char *)malloc(filesize);
	HANDLE hFile = CreateFile(target, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	do {
		DWORD byteswritten;
		len = recv(sckFTP, asdf, filesize, 0);
		if (!len || len == SOCKET_ERROR)
			return false;
		totallen += len;
		WriteFile(hFile, asdf, len, &byteswritten, NULL);
		if (volume > 0)
			printf("%d/%d bytes (%%%d)\n", totallen, filesize, (int)(((double)totallen / (double)filesize) * 100));
	} while (totallen < filesize);

	free(asdf);
	shutdown(sckFTP, 2);
	closesocket(sckFTP);

	if (!SetFileTime(hFile, &filetime, NULL, &filetime))
		return false;

	CloseHandle(hFile);

	FileTimeToSystemTime(&filetime, &systime);
	if (volume >= 0)
		printf("Saved %s (%d bytes, last modified %d/%d/%d %d:%d:%d.%d)\n",
			dlfilename, filesize, systime.wMonth, systime.wDay, systime.wYear,
			systime.wHour, systime.wMinute, systime.wSecond, systime.wMilliseconds);

	return true;
}


int main(int argc, char *argv[]) {
	ParseCmdLine(argc, argv);
	FillDefaults();
	if (volume >= 0)
		printf(DownloadFileFromBNFTP(filename) ?
			"Successfully downloaded file.\n" :
			"Failed to download file.\n");
	return 0;
}

