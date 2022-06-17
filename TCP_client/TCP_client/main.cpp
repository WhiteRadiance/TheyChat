#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <WinSock2.h>					// Windows下的select()只能监控socket描述符,不能监控stdin或文件描述符
#include <stringapiset.h>				// MultiByteToWideChar(), WideCharToMultiByte()
#include <string>
using namespace std;

#pragma comment(lib, "ws2_32.lib")		// 指明需要用到 Win32 相关的库


constexpr int BUFFER_MAXSIZE = 4096;


char obuf[BUFFER_MAXSIZE] = { 0 };
char ibuf[BUFFER_MAXSIZE] = { 0 };


#ifdef _WIN32
/*  --------   Windows版本 utf8 <--> gbk, 必须引用<stringapiset.h>   -------  */
/* 用法:
 *		string str_utf8 = u8"中文";
 *		printf_s("origin str: %s \n", str_utf8.c_str());
 *		string cvt_str = UTF8_to_GBK(str_utf8.c_str());
 *		printf_s("convert str: %s \n", cvt_str.c_str());
 */
static string UTF8_to_GBK(const char *str_utf8);
static string GBK_to_UTF8(const char* str_gbk);
#endif


int main()
{
	int			err;
	int			tail;
	SOCKET		ClientSock;
	SOCKADDR_IN	ServerAddr;
	WSADATA		wsaData;

	system("chcp");

	err = WSAStartup(0x0202, &wsaData);		//启动 Windows-Sockets-Asynchronous 异步套接字
	if (err)		return 0;

	ClientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == ClientSock) {
		printf_s("[Error] 创建Client套接字失败.\n");
		return 0;
	}

	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.S_un.S_addr = inet_addr("1.15.3.15");//192.168.137.24 / 1.15.3.15
	ServerAddr.sin_port = htons(6666);
	printf_s("[info]  connecting to ip=%s, port=%d\n", inet_ntoa(ServerAddr.sin_addr), ntohs(ServerAddr.sin_port));
	err = connect(ClientSock, (sockaddr*)&ServerAddr, sizeof(SOCKADDR_IN));
	if (SOCKET_ERROR == err) {
		printf_s("[Error] 无法连接到服务器\n");
		goto ERR_CLEAN;
	}
	printf_s("[info]  成功连接至服务器\n");
	printf_s("[warn]  本程序未使用'多线程/IO复用',因此不支持实时聊天.\n\n");
	printf_s("    >>  输入想要发送的信息:\n");

	while (true)
	{
		/* 获取输入的数据 */
		fgets(obuf, BUFFER_MAXSIZE, stdin);
		tail = (int)strlen(obuf);
		if (obuf[0] == '\n' && obuf[1] == '\0')
			continue;
		else if (obuf[tail - 1] == '\n')		// 不要给server发送换行符
			obuf[tail - 1] = '\0';
		else;

		/* 输入close则退出 */
		if (strcmp(obuf, "close") == 0)
			goto ERR_CLEAN;

		/* 发送 utf8 */
		string obuf_utf8 = GBK_to_UTF8(obuf);
		send(ClientSock, obuf_utf8.c_str(), (int)obuf_utf8.size() + 1, 0);	// 注意发送长度不再是tail
		
		/* 接收 utf8 */
		recv(ClientSock, ibuf, BUFFER_MAXSIZE, 0);
		string ibuf_gbk = UTF8_to_GBK(ibuf);
		strcpy_s(ibuf, ibuf_gbk.size() + 1, ibuf_gbk.c_str());
		printf_s("%s", ibuf);
		memset(ibuf, 0, BUFFER_MAXSIZE);
		fflush(stdout);
	}

ERR_CLEAN:
	Sleep(1300);
	printf_s("\n");
	closesocket(ClientSock);
	WSACleanup();
	return 0;
}




#ifdef _WIN32

static string UTF8_to_GBK(const char *str_utf8)
{
	size_t len = MultiByteToWideChar(CP_UTF8, 0, str_utf8, -1, NULL, 0);		// CP->Unicode所需长度
	wchar_t* wstrUni = new wchar_t[len + 1];
	memset(wstrUni, 0, len * 2 + 2);
	MultiByteToWideChar(CP_UTF8, 0, str_utf8, -1, wstrUni, (int)len);			// 将utf8映射到Unicode
	len = WideCharToMultiByte(CP_ACP, 0, wstrUni, -1, NULL, 0, NULL, NULL);		// Unicode->CP所需长度
	char* strGBK = new char[len + 1];
	memset(strGBK, 0, len + 1);
	WideCharToMultiByte(CP_ACP, 0, wstrUni, -1, strGBK, (int)len, NULL, NULL);	// 将Unicode映射到ANSI(gbk)
	string str_gbk(strGBK);
	if (wstrUni)		delete[] wstrUni;
	if (strGBK)			delete[] strGBK;
	return str_gbk;
}



static string GBK_to_UTF8(const char* str_gbk)
{
	size_t len = MultiByteToWideChar(CP_ACP, 0, str_gbk, -1, NULL, 0);			// CP->Unicode所需长度
	wchar_t* wstrUni = new wchar_t[len + 1];
	memset(wstrUni, 0, len + 1);
	MultiByteToWideChar(CP_ACP, 0, str_gbk, -1, wstrUni, (int)len);				// 将gbk映射到Unicode
	len = WideCharToMultiByte(CP_UTF8, 0, wstrUni, -1, NULL, 0, NULL, NULL);	// Unicode->CP所需长度
	char* strUTF8 = new char[len + 1];
	WideCharToMultiByte(CP_UTF8, 0, wstrUni, -1, strUTF8, (int)len, NULL, NULL);// 将Unicode映射到utf8
	string str_utf8(strUTF8);
	if (wstrUni)		delete[] wstrUni;
	if (strUTF8)		delete[] strUTF8;
	return str_utf8;
}

#endif
