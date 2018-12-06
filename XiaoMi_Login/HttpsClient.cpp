#include "HttpsClient.h"
#include "Transcode.h"
#include <list>
#include <vector>
#include <iostream>
#include <sstream>

#pragma comment( lib, "libeay32.lib" )  
#pragma comment( lib, "ssleay32.lib" )  
#ifdef WIN32
#pragma comment( lib, "ws2_32.lib" ) 
#endif 

HttpsClient::HttpsClient() :
m_ssl(NULL),
m_sslCtx(NULL),
m_sslMethod(NULL)
{
	m_nStatusCode = 0;
	m_socketClient = 0;
	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();

	m_vCookie["uLocale"] = "zh_CN";
	m_vCookie["pass_ua"] = "web";
}

HttpsClient::~HttpsClient(void)
{
	CloseServer();
}

BOOL HttpsClient::ConnectToServer(const CString strServerUrl, const int nPort)
{
	cstrServerUrl = strServerUrl;
	m_nServerPort = nPort;
	BOOL bRet = FALSE;
	do
	{
		if (!InitializeSocketContext())
			break;
		if (!SocketConnect())
			break;
		if (!InitializeSslContext())
			break;
		if (!SslConnect())
			break;
		bRet = TRUE;
	} while (FALSE);
	return bRet;
}

void HttpsClient::CloseServer()
{
	if (NULL != m_ssl)
	{
		SSL_shutdown(m_ssl);
		SSL_free(m_ssl);
		m_ssl = NULL;
	}
	if (NULL != m_sslCtx)
		SSL_CTX_free(m_sslCtx);

	if (m_socketClient)
		closesocket(m_socketClient);
	//WSACleanup();
}

BOOL HttpsClient::LogoutOfServer()
{
	return FALSE;
}

BOOL HttpsClient::InitializeSocketContext()
{
	BOOL bRet = FALSE;
	do
	{
#ifdef WIN321
		WSADATA wsaData;
		WORD wVersion = MAKEWORD(2, 2);
		if (0 != WSAStartup(wVersion, &wsaData))
		{
			break;
		}

		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
		{
			WSACleanup();
			break;
		}
#endif // WIN32
		LPHOSTENT lpHostTent;
		std::string strUrl;
		Transcode::Unicode_to_UTF8(cstrServerUrl, cstrServerUrl.GetLength(), strUrl);
		lpHostTent = gethostbyname(strUrl.c_str());
		if (NULL == lpHostTent)
		{
			break;
		}

		m_socketClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_socketClient == INVALID_SOCKET)
		{
			WSACleanup();
			break;
		}

		m_socketAddrClient.sin_family = AF_INET;
		m_socketAddrClient.sin_port = htons(m_nServerPort);
		m_socketAddrClient.sin_addr = *((LPIN_ADDR)*lpHostTent->h_addr_list);
		memset(m_socketAddrClient.sin_zero, 0, sizeof(m_socketAddrClient.sin_zero));

		bRet = TRUE;
	} while (FALSE);

	return bRet;
}

BOOL HttpsClient::SocketConnect()
{
	BOOL bRet = FALSE;
	do
	{
		if (SOCKET_ERROR == connect(m_socketClient, (LPSOCKADDR)&m_socketAddrClient, sizeof(SOCKADDR_IN)))
		{
			int nErrorCode = WSAGetLastError();
			closesocket(m_socketClient);
			break;
		}
		bRet = TRUE;
	} while (FALSE);

	return bRet;
}

BOOL HttpsClient::InitializeSslContext()
{
	BOOL bRet = FALSE;
	do
	{
		m_sslMethod = SSLv23_client_method();
		if (NULL == m_sslMethod)
			break;

		m_sslCtx = SSL_CTX_new(m_sslMethod);
		if (NULL == m_sslCtx)
			break;

		m_ssl = SSL_new(m_sslCtx);
		if (NULL == m_ssl)
			break;

		bRet = TRUE;
	} while (FALSE);
	return bRet;
}

BOOL HttpsClient::SslConnect()
{
	BOOL bRet = FALSE;
	do
	{
		SSL_set_fd(m_ssl, m_socketClient);
		int nRet = SSL_connect(m_ssl);
		if (-1 == nRet)
			break;
		bRet = TRUE;
	} while (FALSE);

	return bRet;
}

BOOL HttpsClient::SslGetCipherAndCertification()
{
	BOOL bRet = FALSE;
	do
	{
		const char * cstrSslCipher = SSL_get_cipher(m_ssl);
		X509 *pserverCertification = SSL_get_certificate(m_ssl);

		if (NULL == pserverCertification)
			break;

		m_cstrSslSubject = X509_NAME_oneline(X509_get_subject_name(pserverCertification), 0, 0);
		m_cstrSslIssuer = X509_NAME_oneline(X509_get_issuer_name(pserverCertification), 0, 0);

		X509_free(pserverCertification);

		bRet = TRUE;
	} while (FALSE);

	return bRet;
}

bool HttpsClient::socketHttps(std::string host, std::string request)
{
	std::string strRecv;
	int nRet = SSL_write(m_ssl, request.c_str(), request.length());

	bool bHttpHeadHead = false;
	bool bHttpHeadTail = false;
	char recvData[1025] = { 0 };
	do
	{
		int nErr = SSL_read(m_ssl, recvData, 1024);
		if (nErr > 0)
		{ 
			recvData[nErr] = 0;
			int nLen = strRecv.length() - 15;
			nLen < 0 ? nLen = 0 : 1;
			strRecv.append(recvData, nErr);
			// ����httpͷ��
			if (bHttpHeadHead == false)
			{
				const char * pHttp = strstr(strRecv.c_str() + nLen, "HTTP/1.");
				if (pHttp)
					bHttpHeadHead = true;
			}
			// �ҵ�httpͷ������httpͷ����β
			if (bHttpHeadHead == true)
			{
				// Content-Length: 294 ��һ���У�����\r\n\r\nһ������
				const char * pContent = strstr(strRecv.c_str() + nLen, "\r\n\r\n");
				if (pContent)
				{
					// ��һ��Copntent���ݾͽ��������
					if (nErr < 1024)
						break;
					bHttpHeadTail = true;
				}
				// Httpβ���ҵ��ˣ�
				if (bHttpHeadTail)
				{
					if (pContent == NULL && nErr < 1023)
						break;
				}
			}	
		}
		if (nErr <= 0) // �����յ�С��1024�м������ʱ
			break;
	} while (1);

	// �������� 
	m_strHeader.clear();
	m_strGetResult.clear();
	const char* pBufRecv = strRecv.c_str();
	const char* pHttp = strstr(pBufRecv, "HTTP/1.");
	if ((pHttp + 9) != NULL && (pHttp + 12) != NULL)
	{
		std::string strRespone;
		strRespone.append(pHttp + 9, 3);
		m_nStatusCode = atoi(strRespone.c_str());
	}
	int nRecvSize = strRecv.length();
	const char *pFind = strstr(pHttp, "\r\n\r\n");
	if (pFind)
	{
		int nHeaderSize = pFind - pHttp;
		m_strHeader.append(pHttp, nHeaderSize); // ����Ҫ��Ҫ���ϻ��е�4���ֽ�
		m_strGetResult.append(strRecv.c_str() + nHeaderSize + 4, nRecvSize - nHeaderSize - 4);
		const char* pData = m_strGetResult.c_str();
		ExtractCookie();
		return true;
	}
	return false;
}

int HttpsClient::GetHttpStatusCode()
{
	return m_nStatusCode;
}

bool HttpsClient::postData(std::string host, std::string path, std::string post_content)
{
	//POST����ʽ
	std::stringstream stream;
	stream << "POST " << path;
	stream << " HTTP/1.1\r\n";
	stream << "Host: " << host << "\r\n";	
	stream << "Connection: keep-alive\r\n";
	stream << "User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; zh-CN; rv:1.9.2.3) Gecko/20100401 Firefox/3.6.3\r\n";
	stream << "Content-Type: application/x-www-form-urlencoded\r\n";

	std::string strCookie;
	SetCookie(strCookie);
	if (strCookie.length())
		stream << "Cookie: " << strCookie.c_str() << "\r\n";

	stream << "Content-Length:" << post_content.length() << "\r\n\r\n";
	stream << post_content.c_str();
	return socketHttps(host, stream.str());
}

bool HttpsClient::getData(std::string host, std::string pathAndparameter)
{
	//GET����ʽ
	std::stringstream stream;
	stream << "GET " << pathAndparameter;
	stream << " HTTP/1.1\r\n";
	stream << "Host: " << host << "\r\n";	
	stream << "Connection: keep-alive\r\n";
	stream << "User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; zh-CN; rv:1.9.2.3) Gecko/20100401 Firefox/3.6.3\r\n";
	//stream << "Referer: https ://account.xiaomi.com/pass/serviceLogin\r\n";
	stream << "Accept-Language: zh-CN,zh;q=0.9\r\n";
	std::string strCookie;
	SetCookie(strCookie);
	if (strCookie.length())
		stream << "Cookie: " << strCookie.c_str();
	stream << "\r\n\r\n";
	return socketHttps(host, stream.str());
}

bool HttpsClient::getDataWithParam(std::string host, std::string path, std::string get_content)
{
	std::string strPath = path + "?" + get_content;
	return getData(host,strPath);
}

// ��ȡ����Ľ��
std::string HttpsClient::GetLastRequestResult()
{
	return m_strGetResult;
}

//////////////////////////////////////////////////////////////////////////
//
//	Set-Cookie: device-uid=dce82970-bbb4-11e8-9f16-15bf44f4da8e; path=/; expires=Mon, 19 Sep 2118 02:36:50 GMT; domain=.36kr.com; httponly
//
//	Cookie����Ϊdevice-uid=dce82970-bbb4-11e8-9f16-15bf44f4da8e;
//////////////////////////////////////////////////////////////////////////

bool HttpsClient::ExtractCookie()
{
	if (m_strHeader.empty())
		return false;
	const char* pHeader = m_strHeader.c_str();
	long nPos = m_strHeader.find("Set-Cookie:");
	const char* pBegin = strstr(pHeader, "Set-Cookie: ");
	int nSetCookieLen = strlen("Set-Cookie: ");

	do 
	{
		if (pBegin == NULL)
			break;
		const char* pDengHao = strstr(pBegin + nSetCookieLen, "=");
		const char* pFenHao = strstr(pDengHao, ";");
		const char* pLineEnd = strstr(pFenHao, "\n");
		const char* pExpires = strstr(pBegin, "expires");
		//if (pExpires < pLineEnd) // �й��ڵ�Ϊ��Ҫ���ֵ�cookie������������ˣ�Ӧ���Ǳ���Ϊ�ļ���ʱ��
		if (1)
		{
			std::string name;
			name.append(pBegin + nSetCookieLen, pDengHao - pBegin - nSetCookieLen);
			std::string value;
			value.append(pDengHao + 1, pFenHao - pDengHao - 1);
			if (value == "EXPIRED")
			{
				CookieIt it = m_vCookie.find(name);
				if (it != m_vCookie.end())
					m_vCookie.erase(it);
			}
			else
			{
				m_vCookie[name] = value;
			}	
		}
		else  // session ���͵�cookie
		{
			std::string name;
			name.append(pBegin + nSetCookieLen, pDengHao - pBegin - nSetCookieLen);
			std::string value;
			value.append(pDengHao + 1, pFenHao - pDengHao - 1);
			if (value == "EXPIRED")
			{
				CookieIt it = m_vCookie.find(name);
				if (it != m_vCookie.end())
					m_vCookie.erase(it);
			}
			else// if (name != "JSESSIONID")
				m_vCookie[name] = value;
		}
		pBegin = strstr(pLineEnd + 1, "Set-Cookie: ");
	} while (1);
	return true;
}

void HttpsClient::SetCookie(std::string &strCookie)
{
	if (m_vCookie.size())
	{
		for (CookieIt it = m_vCookie.begin();;)
		{
			strCookie.append(it->first.c_str());
			strCookie.append("=");
			strCookie.append(it->second.c_str());
			it++;
			if (it != m_vCookie.end())
				strCookie.append("; "); // Cookie�ļ����
			else
				break;
		}
	}
	if (strCookie.empty())
	{
		strCookie = "uLocale=zh_CN";
	}
}


