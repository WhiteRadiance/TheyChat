#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <WinSock2.h>					// Windows�µ�select()ֻ�ܼ��socket������,���ܼ��stdin���ļ�������
#include <stringapiset.h>				// MultiByteToWideChar(), WideCharToMultiByte()
#include <string>
using namespace std;

#pragma comment(lib, "ws2_32.lib")		// ָ����Ҫ�õ� Win32 ��صĿ�


constexpr int BUFFER_MAXSIZE = 4096;


char obuf[BUFFER_MAXSIZE] = { 0 };
char ibuf[BUFFER_MAXSIZE] = { 0 };


#ifdef _WIN32
/*  --------   Windows�汾 utf8 <--> gbk, ��������<stringapiset.h>   -------  */
/* �÷�:
 *		string str_utf8 = u8"����";
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

	err = WSAStartup(0x0202, &wsaData);		//���� Windows-Sockets-Asynchronous �첽�׽���
	if (err)		return 0;

	ClientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == ClientSock) {
		printf_s("[Error] ����Client�׽���ʧ��.\n");
		return 0;
	}

	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.S_un.S_addr = inet_addr("1.15.3.15");//192.168.137.24 / 1.15.3.15
	ServerAddr.sin_port = htons(6666);
	printf_s("[info]  connecting to ip=%s, port=%d\n", inet_ntoa(ServerAddr.sin_addr), ntohs(ServerAddr.sin_port));
	err = connect(ClientSock, (sockaddr*)&ServerAddr, sizeof(SOCKADDR_IN));
	if (SOCKET_ERROR == err) {
		printf_s("[Error] �޷����ӵ�������\n");
		goto ERR_CLEAN;
	}
	printf_s("[info]  �ɹ�������������\n");
	printf_s("[warn]  ������δʹ��'���߳�/IO����',��˲�֧��ʵʱ����.\n\n");
	printf_s("    >>  ������Ҫ���͵���Ϣ:\n");

	while (true)
	{
		/* ��ȡ��������� */
		fgets(obuf, BUFFER_MAXSIZE, stdin);
		tail = (int)strlen(obuf);
		if (obuf[0] == '\n' && obuf[1] == '\0')
			continue;
		else if (obuf[tail - 1] == '\n')		// ��Ҫ��server���ͻ��з�
			obuf[tail - 1] = '\0';
		else;

		/* ����close���˳� */
		if (strcmp(obuf, "close") == 0)
			goto ERR_CLEAN;

		/* ���� utf8 */
		string obuf_utf8 = GBK_to_UTF8(obuf);
		send(ClientSock, obuf_utf8.c_str(), (int)obuf_utf8.size() + 1, 0);	// ע�ⷢ�ͳ��Ȳ�����tail
		
		/* ���� utf8 */
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
	size_t len = MultiByteToWideChar(CP_UTF8, 0, str_utf8, -1, NULL, 0);		// CP->Unicode���賤��
	wchar_t* wstrUni = new wchar_t[len + 1];
	memset(wstrUni, 0, len * 2 + 2);
	MultiByteToWideChar(CP_UTF8, 0, str_utf8, -1, wstrUni, (int)len);			// ��utf8ӳ�䵽Unicode
	len = WideCharToMultiByte(CP_ACP, 0, wstrUni, -1, NULL, 0, NULL, NULL);		// Unicode->CP���賤��
	char* strGBK = new char[len + 1];
	memset(strGBK, 0, len + 1);
	WideCharToMultiByte(CP_ACP, 0, wstrUni, -1, strGBK, (int)len, NULL, NULL);	// ��Unicodeӳ�䵽ANSI(gbk)
	string str_gbk(strGBK);
	if (wstrUni)		delete[] wstrUni;
	if (strGBK)			delete[] strGBK;
	return str_gbk;
}



static string GBK_to_UTF8(const char* str_gbk)
{
	size_t len = MultiByteToWideChar(CP_ACP, 0, str_gbk, -1, NULL, 0);			// CP->Unicode���賤��
	wchar_t* wstrUni = new wchar_t[len + 1];
	memset(wstrUni, 0, len + 1);
	MultiByteToWideChar(CP_ACP, 0, str_gbk, -1, wstrUni, (int)len);				// ��gbkӳ�䵽Unicode
	len = WideCharToMultiByte(CP_UTF8, 0, wstrUni, -1, NULL, 0, NULL, NULL);	// Unicode->CP���賤��
	char* strUTF8 = new char[len + 1];
	WideCharToMultiByte(CP_UTF8, 0, wstrUni, -1, strUTF8, (int)len, NULL, NULL);// ��Unicodeӳ�䵽utf8
	string str_utf8(strUTF8);
	if (wstrUni)		delete[] wstrUni;
	if (strUTF8)		delete[] strUTF8;
	return str_utf8;
}

#endif
