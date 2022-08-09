#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>

struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int sendSubType;	// Sending sub-type
	char buffer[1024];
	int len;
};

const int TIME_PORT = 27015;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;

/*Message Types*/
const int SEND_GET = 1;
const int SEND_HEAD = 2;
const int SEND_POST = 3;
const int SEND_PUT = 4;
const int SEND_DELETE = 5;
const int SEND_OPTIONS = 6;
const int SEND_TRACE = 7;

/*Language Constant values*/
const int EN_LANG = 1;
const int HE_LANG = 2;
const int FR_LANG = 3;

bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);
void setResponseHeaders(char sendBuff[], const char status[]);
void getPathOrData(char line[], int index, int& langType);
void setGetResponse(int index, char sendBuff[], char body[], char bodyLength[]);
void setDeleteResponse(int index, char sendBuff[], char body[], char bodyLength[]);
void setOptionsResponse(int index, char sendBuff[]);
void setHeadResponse(int index, char sendBuff[], char bodyLength[]);
void setPostResponse(int index, char sendBuff[], char body[], char bodyLength[]);
void setPutResponse(int index, char sendBuff[], char body[], char bodyLength[]);
void getFullPath(char line[], int index);
void setTraceResponse(int index, char sendBuff[], char body[]);

struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;


void main()
{
	// Initialize Winsock (Windows Sockets).

	// Create a WSADATA object called wsaData.
	// The WSADATA structure contains information about the Windows 
	// Sockets implementation.
	WSAData wsaData;

	// Call WSAStartup and return its value as an integer and check for errors.
	// The WSAStartup function initiates the use of WS2_32.DLL by a process.
	// First parameter is the version number 2.2.
	// The WSACleanup function destructs the use of WS2_32.DLL by a process.
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Time Server: Error at WSAStartup()\n";
		return;
	}

	// Server side:
	// Create and bind a socket to an internet address.
	// Listen through the socket for incoming connections.

	// After initialization, a SOCKET object is ready to be instantiated.

	// Create a SOCKET object called listenSocket. 
	// For this application:	use the Internet address family (AF_INET), 
	//							streaming sockets (SOCK_STREAM), 
	//							and the TCP/IP protocol (IPPROTO_TCP).
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	// Error detection is a key part of successful networking code. 
	// If the socket call fails, it returns INVALID_SOCKET. 
	// The if statement in the previous code is used to catch any errors that
	// may have occurred while creating the socket. WSAGetLastError returns an 
	// error number associated with the last error that occurred.
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Time Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	// For a server to communicate on a network, it must bind the socket to 
	// a network address.

	// Need to assemble the required data for connection in sockaddr structure.

	// Create a sockaddr_in object called serverService. 
	sockaddr_in serverService;
	// Address family (must be AF_INET - Internet address family).
	serverService.sin_family = AF_INET;
	// IP address. The sin_addr is a union (s_addr is a unsigned long 
	// (4 bytes) data type).
	// inet_addr (Iternet address) is used to convert a string (char *) 
	// into unsigned long.
	// The IP address is INADDR_ANY to accept connections on all interfaces.
	serverService.sin_addr.s_addr = INADDR_ANY;
	// IP Port. The htons (host to network - short) function converts an
	// unsigned short from host to TCP/IP network byte order 
	// (which is big-endian).
	serverService.sin_port = htons(8080);

	// Bind the socket for client's requests.

	// The bind function establishes a connection to a specified socket.
	// The function uses the socket handler, the sockaddr structure (which
	// defines properties of the desired connection) and the length of the
	// sockaddr structure (in bytes).
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Time Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	// Listen on the Socket for incoming connections.
	// This socket accepts only one connection (no pending connections 
	// from other clients). This sets the backlog parameter.
	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Time Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN);

	// Accept connections and handles them one by one.
	while (true)
	{
		// The select function determines the status of one or more sockets,
		// waiting if necessary, to perform asynchronous I/O. Use fd_sets for
		// sets of handles for reading, writing and exceptions. select gets "timeout" for waiting
		// and still performing other operations (Use NULL for blocking). Finally,
		// select returns the number of descriptors which are ready for use (use FD_ISSET
		// macro to check which descriptor in each set is ready to be used).
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}

		//
		// Wait for interesting event.
		// Note: First argument is ignored. The fourth is for exceptions.
		// And as written above the last is a timeout, hence we are blocked if nothing happens.
		//
		int nfd;
		const timeval max = {120,0};
		nfd = select(0, &waitRecv, &waitSend, NULL, &max);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Time Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:
					receiveMessage(i);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:
					sendMessage(i);
					break;
				}
			}
		}
	}

	// Closing connections and Winsock.
	cout << "Time Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SOCKET id, int what)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}

void acceptConnection(int index)
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Time Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Time Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	//
	// Set the socket to be in non-blocking mode.
	//
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Time Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Time Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "Time Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].len += bytesRecv;

		if (sockets[index].len > 0)
		{
			if (strncmp(sockets[index].buffer, "GET", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_GET;
				memcpy(sockets[index].buffer, &sockets[index].buffer[3], sockets[index].len - 3);
				sockets[index].len -= 3;
				sockets[index].buffer[sockets[index].len] = '\0';
			}
			else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_HEAD;
				memcpy(sockets[index].buffer, &sockets[index].buffer[4], sockets[index].len - 4);
				sockets[index].len -= 4;
				sockets[index].buffer[sockets[index].len] = '\0';
			}
			else if (strncmp(sockets[index].buffer, "POST", 4) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_POST;
				memcpy(sockets[index].buffer, &sockets[index].buffer[4], sockets[index].len - 4);
				sockets[index].len -= 4;
				sockets[index].buffer[sockets[index].len] = '\0';
			}
			else if (strncmp(sockets[index].buffer, "PUT", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_PUT;
				memcpy(sockets[index].buffer, &sockets[index].buffer[3], sockets[index].len - 3);
				sockets[index].len -= 3;
				sockets[index].buffer[sockets[index].len] = '\0';
			}
			else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_DELETE;
				memcpy(sockets[index].buffer, &sockets[index].buffer[6], sockets[index].len - 6);
				sockets[index].len -= 6;
				sockets[index].buffer[sockets[index].len] = '\0';
			}
			else if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_OPTIONS;
				memcpy(sockets[index].buffer, &sockets[index].buffer[7], sockets[index].len - 7);
				sockets[index].len -= 7;
				sockets[index].buffer[sockets[index].len] = '\0';
			}
			else if (strncmp(sockets[index].buffer, "TRACE", 5) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_TRACE;
				memcpy(sockets[index].buffer, &sockets[index].buffer[5], sockets[index].len - 5);
				sockets[index].len -= 5;
				sockets[index].buffer[sockets[index].len] = '\0';
			}
			else if (strncmp(sockets[index].buffer, "Exit", 4) == 0)
			{
				closesocket(msgSocket);
				removeSocket(index);
				return;
			}
		}
	}

}

void setResponseHeaders(char sendBuff[], const char status[])
{
	time_t timer;
	time(&timer);
	strcpy(sendBuff, "HTTP/1.1 ");
	strcat(sendBuff, status);
	strcat(sendBuff, " OK\r\n");
	strcat(sendBuff, "Server: HTTP Web Server\r\n");
	strcat(sendBuff, "Content-Type: text/html\r\n");
	strcat(sendBuff, "Date: ");
	strcat(sendBuff, ctime(&timer));
	sendBuff[strlen(sendBuff) - 1] = '\r';
	sendBuff[strlen(sendBuff)] = '\0';
	sendBuff[strlen(sendBuff) + 1] = '\0';
	sendBuff[strlen(sendBuff)] = '\n';
	sendBuff[strlen(sendBuff) + 1] = '\0';
}

void sendMessage(int index)
{
	int bytesSent = 0;
	char sendBuff[1024];
	char body[1024] = "";
	char bodyLength[1024] = "";

	SOCKET msgSocket = sockets[index].id;
	/*Yes*/
	if (sockets[index].sendSubType == SEND_GET)
	{
		setGetResponse(index, sendBuff, body, bodyLength); //to remove the new-line from the created string
	}
	/*Yes*/
	else if (sockets[index].sendSubType == SEND_HEAD)
	{
		setHeadResponse(index, sendBuff, bodyLength);
	}
	else if (sockets[index].sendSubType == SEND_POST)
	{
		strcpy(body, sockets[index].buffer);
		setPostResponse(index, sendBuff, body, bodyLength);
	}
	/*Yes*/
	else if (sockets[index].sendSubType == SEND_PUT)
	{
		strcpy(body, sockets[index].buffer);
		setPutResponse(index, sendBuff, body, bodyLength);
	}
	/*Yes*/
	else if (sockets[index].sendSubType == SEND_DELETE)
	{
		setDeleteResponse(index, sendBuff, body, bodyLength);
	}
	/*Yes*/
	else if (sockets[index].sendSubType == SEND_OPTIONS)
	{
		setOptionsResponse(index, sendBuff);
	}
	/*Yes*/
	else if (sockets[index].sendSubType == SEND_TRACE)
	{
		strcpy(body, sockets[index].buffer);
		setTraceResponse(index, sendBuff, body);
	}
	strcat(sendBuff, body);
	strcat(sendBuff, "\r\n\r");
	bytesSent = send(msgSocket, sendBuff,(int)(strlen(sendBuff)), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Time Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "Time Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n";
	memset(sockets[index].buffer, '\0', 1024);
	memset(sendBuff, '\0', 1024);
	sockets[index].len = 0;
	sockets[index].send = IDLE;
}

void getFullPath(char line[], int index)
{
	char c;
	int j = 2;
	int i = 2;
	while (c = sockets[index].buffer[i])
	{
		if (c == '\t' || c == ' ')
			break;
		line[i - j] = c;
		i++;
	}
	line[strlen(line)] = '\0';
}

void getPathOrData(char line[], int index, int& langType)
{
	char c;
	int j = 2;
	int i = 2;
	int t = 0;
	int k = 0;
	char temp[7] = "";
	char langstr[] = "?lang";
	char langTypeStr[3] = "";
	int lang_query = EN_LANG;
	while (c = sockets[index].buffer[i])
	{
		if (c == '\t' || c == ' ' || c == '?')
			break;
		line[i - j] = c;
		i++;
	}
	if(c == '?')
	{
		while (c = sockets[index].buffer[i])
		{
			if (c == '\t' || c == ' ' || c == '=')
				break;
			temp[t] = c;
			i++;
			t++;
		}
	}
	if(strcmp(temp, langstr) == 0)
	{
		while (c = sockets[index].buffer[i])
		{
			if (c == '\t' || c == ' ' || k > 2)
				break;
			langTypeStr[k] = c;
			i++;
			k++;
		}
	}
	if (c == ' ' || c == '\t' || c == '\0')
	{
		if (strncmp(langTypeStr, "=he", 3) == 0)
		{
			langType = HE_LANG;
		}
		else if (strncmp(langTypeStr, "=fr", 3) == 0)
		{
			langType = FR_LANG;
		}
	}
	line[strlen(line)] = '\0';
}

/*Get Method*/
void setGetResponse(int index, char sendBuff[], char body[], char bodyLength[])
{
	time_t timer;
	time(&timer);
	ifstream ifs;
	string buff;
	char path[255] = "pages/";
	int lang_type = EN_LANG;
	char const str_content_len[] = "Content-Length: ";
	char line[255] = { 0 };
	getPathOrData(line, index, lang_type);
	if (lang_type == HE_LANG)
		strcat(path, "he/");
	else if (lang_type == FR_LANG)
		strcat(path, "fr/");
	else
		strcat(path, "en/");
	strcat(path, line);
	ifs.open(path);
	strcpy(sendBuff, "HTTP/1.1 ");
	if (!ifs.is_open()) {

		strcat(sendBuff, "404 Not Found\r\n");
		strcat(sendBuff, "Server: HTTP Web Server\r\n");
		strcat(sendBuff, "Content-Type: text/html\r\n");
		strcat(sendBuff, "Date: ");
		strcat(sendBuff, ctime(&timer));
		sendBuff[strlen(sendBuff) - 1] = '\r';
		sendBuff[strlen(sendBuff)] = '\0';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		sendBuff[strlen(sendBuff)] = '\n';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		strcat(sendBuff, "Content-Length: 98\r\n\r\n");
		strcat(body, "<html><head><title>Error!</title><h2>Error!</h2></head><body><h1>404 Not Found</h1></body></html>");
	}
	else {
			strcat(sendBuff, "200 OK\r\n");
			strcat(sendBuff, "Server: HTTP Web Server\r\n");
			strcat(sendBuff, "Content-Type: text/html\r\n");
			strcat(sendBuff, "Date: ");
			strcat(sendBuff, ctime(&timer));
			sendBuff[strlen(sendBuff) - 1] = '\r';
			sendBuff[strlen(sendBuff)] = '\0';
			sendBuff[strlen(sendBuff) + 1] = '\0';
			sendBuff[strlen(sendBuff)] = '\n';
			sendBuff[strlen(sendBuff) + 1] = '\0';
			int size = 0;
			while (getline(ifs, buff))
			{
				buff[buff.length()] = '\0';
				size += buff.length();
				strcat(body, buff.c_str());
			}
			strcat(sendBuff, str_content_len);
			_itoa(size, bodyLength, 10);
			strcat(sendBuff, bodyLength);
			strcat(sendBuff, "\r\n\r\n");
			ifs.close();
	}
}

/*Delete Response Method*/
void setDeleteResponse(int index, char sendBuff[], char body[], char bodyLength[])
{
	time_t timer;
	time(&timer);
	ifstream ifs;
	string buff;
	char line[255] = { 0 };
	getFullPath(line, index);
	int status;
	status = remove(line);
	strcpy(sendBuff, "HTTP/1.1 ");
	if (status == 0)
	{
		strcat(sendBuff, "200 OK\r\n");
		strcat(sendBuff, "Server: HTTP Web Server\r\n");
		strcat(sendBuff, "Content-Type: text/html\r\n");
		strcat(sendBuff, "Date: ");
		strcat(sendBuff, ctime(&timer));
		sendBuff[strlen(sendBuff) - 1] = '\r';
		sendBuff[strlen(sendBuff)] = '\0';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		sendBuff[strlen(sendBuff)] = '\n';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		strcat(sendBuff, "Connection: close\r\n");
		strcat(sendBuff, "Content-Length: 62\r\n\r\n");
		strcat(body, "<html><body><h1>File Deleted Successfully!</h1></body></html>");
	}
	else
	{
		strcat(sendBuff, "404 Not Found\r\n");
		strcat(sendBuff, "Server: HTTP Web Server\r\n");
		strcat(sendBuff, "Content-Type: text/html\r\n");
		strcat(sendBuff, "Date: ");
		strcat(sendBuff, ctime(&timer));
		sendBuff[strlen(sendBuff) - 1] = '\r';
		sendBuff[strlen(sendBuff)] = '\0';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		sendBuff[strlen(sendBuff)] = '\n';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		strcat(sendBuff, "Content-Length: 60\r\n\r\n");
		strcat(body, "<html><body><h1>The file did not exists!</h1></body></html>");
	}
}

/*Put Response Method*/
void setPutResponse(int index, char sendBuff[], char body[], char bodyLength[])
{
	time_t timer;
	time(&timer);
	ofstream ofs;
	ifstream ifs;
	string buff;
	string str;
	boolean flag = false;
	int size = 0;
	strcpy(sendBuff, "HTTP/1.1 ");
	char line[255] = { 0 };
	getFullPath(line, index);
	ofs.open(line);
	if(!ofs.is_open())
	{
		strcat(sendBuff, "404 Not Found\r\n");
	}
	else 
		strcat(sendBuff, "200 OK\r\n");
	strcat(sendBuff, "Server: HTTP Web Server\r\n");
	strcat(sendBuff, "Content-Type: text/html\r\n");
	strcat(sendBuff, "Date: ");
	strcat(sendBuff, ctime(&timer));
	sendBuff[strlen(sendBuff) - 1] = '\r';
	sendBuff[strlen(sendBuff)] = '\0';
	sendBuff[strlen(sendBuff) + 1] = '\0';
	sendBuff[strlen(sendBuff)] = '\n';
	sendBuff[strlen(sendBuff) + 1] = '\0';
	if (ofs.is_open())
	{
		str.append(body);
		string current_str;
		stringstream body_stream(str);
		
		while (getline(body_stream, current_str, '\n'))
		{
			if (current_str == "\r")
			{
				flag = true;
			}
			else if (flag)
			{
				ofs << current_str;
			}
		}
		ofs.close();
		ifs.open(line);
		strcpy(body, "");
		while (getline(ifs, buff))
		{
			buff[buff.length()] = '\0';
			size += buff.length();
			strcat(body, buff.c_str());
		}
	}
	if (!flag)
		strcpy(body, "");
	strcat(sendBuff, "Content-Length:");
	_itoa(size, bodyLength, 10);
	strcat(sendBuff, bodyLength);
	strcat(sendBuff, "\r\n\r");
	ifs.close();
}

/*Trace Response Method*/
void setTraceResponse(int index, char sendBuff[], char body[])
{
	time_t timer;
	time(&timer);
	char line[255] = { 0 };
	char size[255] = { 0 };
	getFullPath(line, index);
	strcpy(sendBuff, "HTTP/1.1 ");
	strcat(sendBuff, "200 OK\r\n");
	strcat(sendBuff, "Server: HTTP Web Server\r\n");
	strcat(sendBuff, "Content-Type: message/html\r\n");
	strcat(sendBuff, "Date: ");
	strcat(sendBuff, ctime(&timer));
	sendBuff[strlen(sendBuff) - 1] = '\r';
	sendBuff[strlen(sendBuff)] = '\0';
	sendBuff[strlen(sendBuff) + 1] = '\0';
	sendBuff[strlen(sendBuff)] = '\n';
	sendBuff[strlen(sendBuff) + 1] = '\0';
	strcat(sendBuff, "Content-Length: ");
	_itoa(strlen(body)+5, size, 10);
	strcat(sendBuff, size);
	strcat(sendBuff, "\r\n\r\n");
	strcat(sendBuff, "TRACE");
}

/*Options Method*/
void setOptionsResponse(int index, char sendBuff[])
{
	time_t timer;
	time(&timer);
	ifstream ifs;
	char line_path[255] = { 0 };
	getFullPath(line_path, index);
	if (strcmp(line_path, "*") == 0)
	{
		strcpy(sendBuff, "HTTP/1.1 ");
		strcat(sendBuff, "200 OK\r\n");
		strcat(sendBuff, "Allow: OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE\r\n");
		strcat(sendBuff, "Accept: text/html\r\n");
		strcat(sendBuff, "Accept-Language: en, fr, heb\r\n");
		strcat(sendBuff, "Date: ");
		strcat(sendBuff, ctime(&timer));
		sendBuff[strlen(sendBuff) - 1] = '\r';
		sendBuff[strlen(sendBuff)] = '\0';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		sendBuff[strlen(sendBuff)] = '\n';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		strcat(sendBuff, "Content-Length: 0\r\n\r\n");
	}
	else
	{
		ifs.open(line_path);
		strcpy(sendBuff, "HTTP/1.1 ");
		if(ifs.is_open())
		{
			strcat(sendBuff, "200 OK\r\n");
			strcat(sendBuff, "Allow: OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE\r\n");
			strcat(sendBuff, "Accept-Language: en, fr, he\r\n");
		}
		else
		{
			strcat(sendBuff, "204 No Content\r\n");
			strcat(sendBuff, "Allow: OPTIONS, PUT, TRACE\r\n");
		}
		strcat(sendBuff, "Accept: text/html\r\n");
		strcat(sendBuff, "Date: ");
		strcat(sendBuff, ctime(&timer));
		sendBuff[strlen(sendBuff) - 1] = '\r';
		sendBuff[strlen(sendBuff)] = '\0';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		sendBuff[strlen(sendBuff)] = '\n';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		strcat(sendBuff, "Content-Length: 0\r\n\r\n");
		ifs.close();
	}
}

/*Head Response Method*/
void setHeadResponse(int index, char sendBuff[], char bodyLength[])
{
	time_t timer;
	time(&timer);
	ifstream ifs;
	string buff;
	char line[255] = { 0 };
	char path[255] = "pages/";
	int lang_type = EN_LANG;
	getPathOrData(line, index, lang_type);
	if (lang_type == HE_LANG)
		strcat(path, "he/");
	else if (lang_type == FR_LANG)
		strcat(path, "fr/");
	else
		strcat(path, "en/");
	strcat(path, line);
	ifs.open(path);
	strcpy(sendBuff, "HTTP/1.1 ");
	if (!ifs.is_open()) 
	{
		strcat(sendBuff, "404 Not Found\r\n");
		strcat(sendBuff, "Server: HTTP Web Server\r\n");
		strcat(sendBuff, "Content-Type: text/html\r\n");
		
		strcat(sendBuff, ctime(&timer));
		sendBuff[strlen(sendBuff) - 1] = '\r';
		sendBuff[strlen(sendBuff)] = '\0';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		sendBuff[strlen(sendBuff)] = '\n';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		strcat(sendBuff, "Content-Length: 98\r\n\r\n");
	}
	else
	{
		strcat(sendBuff, "200 OK\r\n");
		strcat(sendBuff, "Server: HTTP Web Server\r\n");
		strcat(sendBuff, "Content-Type: text/html\r\n");
		strcat(sendBuff, "Date: ");
		strcat(sendBuff, ctime(&timer));
		sendBuff[strlen(sendBuff) - 1] = '\r';
		sendBuff[strlen(sendBuff)] = '\0';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		sendBuff[strlen(sendBuff)] = '\n';
		sendBuff[strlen(sendBuff) + 1] = '\0';
		int size = 0;
		while (getline(ifs, buff))
		{
			buff[buff.length()] = '\0';
			size += buff.length();
		}
		strcat(sendBuff,  "Content-Length:");
		_itoa(size, bodyLength, 10);
		strcat(sendBuff, bodyLength);
		strcat(sendBuff, "\r\n\r\n");
		ifs.close();
	}
}

void setPostResponse(int index, char sendBuff[], char body[], char bodyLength[])
{
	time_t timer;
	time(&timer);
	string buff;
	string str;
	boolean flag = false;
	int size = 0;
	strcpy(sendBuff, "HTTP/1.1 ");
	strcat(sendBuff, "200 OK\r\n");
	strcat(sendBuff, "Server: HTTP Web Server\r\n");
	strcat(sendBuff, "Content-Type: text/html\r\n");
	strcat(sendBuff, "Date: ");
	strcat(sendBuff, ctime(&timer));
	sendBuff[strlen(sendBuff) - 1] = '\r';
	sendBuff[strlen(sendBuff)] = '\0';
	sendBuff[strlen(sendBuff) + 1] = '\0';
	sendBuff[strlen(sendBuff)] = '\n';
	sendBuff[strlen(sendBuff) + 1] = '\0';
	char line[255] = { 0 };
	str.append(body);
	strcpy(body, "");
	string current_str;
	string tomp;
	stringstream body_stream(str);
	while (getline(body_stream, current_str))
	{
		if (current_str == "\r")
		{
			flag = true;
		}
		else if (flag)
		{
			buff.append(current_str);
			buff.append("\n\0");
		}
	}
	buff[buff.length()-1] = '\0';
	strcat(body, buff.c_str());
	strcat(sendBuff, "Content-Length:");
	_itoa(buff.length(), bodyLength, 10);
	strcat(sendBuff, bodyLength);
	strcat(sendBuff, "\r\n\r\n");
}
