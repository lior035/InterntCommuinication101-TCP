#include <iostream>
using namespace std;

#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#pragma warning (disable: 4996)

struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int sendSubType;	// Sending sub-type
	char buffer[1000];
	int len;
	string currentRequestFullInfo;
	time_t timer;
};

const int SERVER_PORT = 8080;
const int MAX_SOCKETS = 60;

const int EMPTY = 0;
const int LISTEN  = 1;
const int CONNECTED = 2;
const int IDLE = 3;
const int SEND = 4;

const int SEND_TYPE_GET = 3;
const int SEND_TYPE_HEAD = 4;
const int SEND_TYPE_PUT = 5;
const int SEND_TYPE_DELETE = 6;
const int SEND_TYPE_ERROR_BAD_REQUEST = 7;
const int SEND_TYPE_TRACE = 8;

const int CONTENT_LENGTH_PHRASE_SIZE = 16; // The length of "Content-Length: " 
const int MAX_TIME_TO_BE_STUCKED = 120;


bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);
// added
string& replaceAll(string& context, const string& from, const string& to);

struct SocketState sockets[MAX_SOCKETS]={0};
int socketsCount = 0;


void main() 
{
	WSAData wsaData; 

	if (NO_ERROR != WSAStartup(MAKEWORD(2,2), &wsaData))
	{
        cout<<"Server: Error at WSAStartup()\n";
		return;
	}
    
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (INVALID_SOCKET == listenSocket)
	{
        cout<<"Server: Error at socket(): "<<WSAGetLastError()<<endl;
        WSACleanup();
        return;
	}

	sockaddr_in serverService;

    serverService.sin_family = AF_INET; 

	serverService.sin_addr.s_addr = INADDR_ANY;
	
	serverService.sin_port = htons(SERVER_PORT);

    if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR *) &serverService, sizeof(serverService))) 
	{
		cout<<"Server: Error at bind(): "<<WSAGetLastError()<<endl;
        closesocket(listenSocket);
		WSACleanup();
        return;
    }

    if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Server: Error at listen(): " << WSAGetLastError() << endl;
        closesocket(listenSocket);
		WSACleanup();
        return;
	}
	
	addSocket(listenSocket, LISTEN);

	
	while (true)
	{
		fd_set waitRecv;
		FD_ZERO(&waitRecv);

		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			time_t timerForCurrentTime; 
			time(&timerForCurrentTime); // update for current time

			if (((timerForCurrentTime - sockets[i].timer) > MAX_TIME_TO_BE_STUCKED) && (sockets[i].recv != LISTEN))
			{
				closesocket(sockets[i].id);
				removeSocket(i);
			}
			else if ((sockets[i].recv == LISTEN) || (sockets[i].recv == CONNECTED))
			{
				FD_SET(sockets[i].id, &waitRecv);
			}
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);

		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
			{
				FD_SET(sockets[i].id, &waitSend);
			}
		}

		int nfd;
		struct timeval periodOfTimeToWaitForConnention;
		
		periodOfTimeToWaitForConnention.tv_sec = MAX_TIME_TO_BE_STUCKED; // define seconds
		periodOfTimeToWaitForConnention.tv_usec = 0; // define microseconds

		nfd = select(0, &waitRecv, &waitSend, NULL, &periodOfTimeToWaitForConnention);
		if (nfd == SOCKET_ERROR)
		{
			cout <<"Server: Error at select(): " << WSAGetLastError() << endl;
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

				case CONNECTED:
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
	cout << "Server: Closing Connection.\n";
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
			time(&sockets[i].timer);
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
	struct sockaddr_in from;		// hold address of Client
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr *)&from, &fromLen);

	if (INVALID_SOCKET == msgSocket)
	{ 
		 cout << "Server: Error at accept(): " << WSAGetLastError() << endl; 		 
		 return;
	}
	cout << "Server: Client "<<inet_ntoa(from.sin_addr)<<":"<<ntohs(from.sin_port)<<" is connected." << endl;

	//
	// Set the socket to be in non-blocking mode.
	//
	unsigned long flag=1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout<<"Server: Error at ioctlsocket(): "<<WSAGetLastError()<<endl;
		closesocket(id);
	}
	else if (addSocket(msgSocket, CONNECTED) == false)
	{
		cout<<"\t\tToo many connections, dropped!\n";
		closesocket(id);
	}		
	
	return;
}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;
	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, sockets[index].buffer + len, sizeof(sockets[index].buffer) - len, 0);
	
	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Server: Error at recv(): " << WSAGetLastError() << endl;
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
		cout<<"Server: Recieved: "<<bytesRecv<<" bytes of \n\""<<sockets[index].buffer + len<<"\" message.\n";
		
		sockets[index].len += bytesRecv;

		if (sockets[index].len > 0)
		{
			if (strncmp(sockets[index].buffer, "GET", 3) == 0)
			{
				sockets[index].send  = SEND;
				sockets[index].sendSubType = SEND_TYPE_GET;
			}
			else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0)
			{
				sockets[index].send  = SEND;
				sockets[index].sendSubType = SEND_TYPE_HEAD;
			}
			else if (strncmp(sockets[index].buffer, "PUT", 3) == 0)
			{
				sockets[index].send  = SEND;
				sockets[index].sendSubType = SEND_TYPE_PUT;
			}
			else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0)
			{
				sockets[index].send  = SEND;
				sockets[index].sendSubType = SEND_TYPE_DELETE;
			}
			else if (strncmp(sockets[index].buffer, "TRACE", 5) == 0)
			{
				sockets[index].send  = SEND;
				sockets[index].sendSubType = SEND_TYPE_TRACE;
			}
			else
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_TYPE_ERROR_BAD_REQUEST;
			} 

			sockets[index].currentRequestFullInfo = sockets[index].buffer;

			int lenghtOfInfo = sockets[index].currentRequestFullInfo.length();

			memcpy(sockets[index].buffer, sockets[index].buffer + lenghtOfInfo, sockets[index].len - lenghtOfInfo);

			sockets[index].len -= lenghtOfInfo;
		}
	}
}

int PositionToEndOfFilePath(const string& str)
{
	int endOfLine = str.find_first_of("\r\n");
	int i = endOfLine;

	while (str[i] != ' ')
	{
		i--;
	}

	return i - 1;
}

void sendMessage(int index)
{
	int bytesSent = 0;
	SOCKET msgSocket = sockets[index].id;
	string goodResponseMessage = "HTTP/1.1 200 OK\r\n";
	string badResponeMessage400 = "HTTP/1.1 400 Bad Request\r\n";
	string badResponeMessage404 = "HTTP/1.1 404 Not Found\r\n";

	char* responseMessageSendInPractice;

	if (sockets[index].sendSubType == SEND_TYPE_GET)
	{
		int positionOfBeginingOfFilePath = sockets[index].currentRequestFullInfo.find_first_of(" ") + 2;
		int positionToEndOfCurrentRequestedFilePath = PositionToEndOfFilePath(sockets[index].currentRequestFullInfo);
		string requestedFilePath = sockets[index].currentRequestFullInfo.substr(positionOfBeginingOfFilePath, positionToEndOfCurrentRequestedFilePath - (positionOfBeginingOfFilePath - 1));
		
		replaceAll(requestedFilePath, "/", "\\");
		replaceAll(requestedFilePath, "%20", " "); // to fix file path name for cases " " chars were involved

		FILE* requestedFile = fopen(requestedFilePath.data(), "rb");
		if (requestedFile == NULL)
		{
			responseMessageSendInPractice = strdup(badResponeMessage404.c_str());
			responseMessageSendInPractice = (char*)realloc(responseMessageSendInPractice, strlen(responseMessageSendInPractice) + strlen("Content-Length: 13\r\n\r\n404 Not Found") + 1);
			strcat(responseMessageSendInPractice, "Content-Length: 13\r\n\r\n404 Not Found");
		}
		else
		{
			// calculating file size
			fseek(requestedFile, 0, SEEK_END);
			int fileSize = ftell(requestedFile);
			fseek(requestedFile, 0, SEEK_SET);
			
			char* strFileContent = (char*)malloc(fileSize * sizeof(char) + 1);
			fread(strFileContent, sizeof(char), fileSize, requestedFile);
			strFileContent[fileSize] = '\0';

			fclose (requestedFile);
			char* temp = (char*)malloc(sizeof(char)* 1000);
			string strFileSize = itoa(fileSize, temp, 10);
			string contentLength = "Content-Length: ";
			contentLength += strFileSize;
			goodResponseMessage += contentLength;
			goodResponseMessage += "\r\n\r\n";
			goodResponseMessage += strFileContent;
			free((void*)temp);
			free((void*)strFileContent);

			responseMessageSendInPractice = strdup(goodResponseMessage.c_str());
		}
	}



	else if (sockets[index].sendSubType == SEND_TYPE_HEAD)
	{
		int positionOfBeginingOfFilePath = sockets[index].currentRequestFullInfo.find_first_of(" ") + 2;



		int positionToEndOfCurrentRequestedFilePath = PositionToEndOfFilePath(sockets[index].currentRequestFullInfo);

		string requestedFilePath = sockets[index].currentRequestFullInfo.substr(positionOfBeginingOfFilePath, positionToEndOfCurrentRequestedFilePath - (positionOfBeginingOfFilePath - 1));

		
		replaceAll(requestedFilePath, "/", "\\");
		replaceAll(requestedFilePath, "%20", " "); // to fix file path name for cases " " chars were involved

		FILE* requestedFile = fopen(requestedFilePath.data(), "rb");
		if (requestedFile == NULL)
		{
			responseMessageSendInPractice = strdup(badResponeMessage404.c_str());	
			responseMessageSendInPractice = (char*)realloc(responseMessageSendInPractice, strlen(responseMessageSendInPractice) + strlen("Content-Length: 13\r\n\r\n404 Not Found") + 1);
			strcat(responseMessageSendInPractice, "Content-Length: 13\r\n\r\n404 Not Found");
		}
		else
		{
			// calculating file size
			fseek(requestedFile, 0, SEEK_END);
			int fileSize = ftell(requestedFile);
			fseek(requestedFile, 0, SEEK_SET);
			
			fclose (requestedFile);
			char* temp = (char*)malloc(sizeof(char)* 1000);
			string strFileSize = itoa(fileSize, temp, 10);
			string contentLength = "Content-Length: ";
			contentLength += strFileSize;
			goodResponseMessage += contentLength;
			goodResponseMessage += "\r\n\r\n";
			free((void*)temp);

			responseMessageSendInPractice = strdup(goodResponseMessage.c_str());
		}
	}
	else if (sockets[index].sendSubType == SEND_TYPE_DELETE)
	{
		int positionOfBeginingOfFilePath = sockets[index].currentRequestFullInfo.find_first_of(" ") + 2;

		int positionToEndOfCurrentRequestedFilePath = PositionToEndOfFilePath(sockets[index].currentRequestFullInfo);

		string requestedFilePath = sockets[index].currentRequestFullInfo.substr(positionOfBeginingOfFilePath, positionToEndOfCurrentRequestedFilePath - (positionOfBeginingOfFilePath - 1));

		
		replaceAll(requestedFilePath, "/", "\\");
		replaceAll(requestedFilePath, "%20", " "); // to fix file path name for cases " " chars were involved

		if (remove(requestedFilePath.data()) != 0)
		{
			responseMessageSendInPractice = strdup(badResponeMessage404.c_str());	
			responseMessageSendInPractice = (char*)realloc(responseMessageSendInPractice, strlen(responseMessageSendInPractice) + strlen("Content-Length: 13\r\n\r\n404 Not Found") + 1);
			strcat(responseMessageSendInPractice, "Content-Length: 13\r\n\r\n404 Not Found");
		}
		else
		{
			responseMessageSendInPractice = strdup(goodResponseMessage.c_str());
			responseMessageSendInPractice = (char*)realloc(responseMessageSendInPractice, strlen(responseMessageSendInPractice) + strlen("Content-Length: 0\r\n\r\n") + 1);
			strcat(responseMessageSendInPractice, "Content-Length: 0\r\n\r\n");
		}
	}


	else if (sockets[index].sendSubType == SEND_TYPE_PUT)
	{
		int positionOfBeginingOfFilePath = sockets[index].currentRequestFullInfo.find_first_of(" ") + 2;
		int positionToEndOfCurrentRequestedFilePath = PositionToEndOfFilePath(sockets[index].currentRequestFullInfo);
		string requestedFilePath = sockets[index].currentRequestFullInfo.substr(positionOfBeginingOfFilePath, positionToEndOfCurrentRequestedFilePath - (positionOfBeginingOfFilePath - 1));
		
		replaceAll(requestedFilePath, "/", "\\");
		replaceAll(requestedFilePath, "%20", " "); // to fix file path name for cases " " chars were involved

		FILE* fileToBeCreated = fopen(requestedFilePath.data(), "wb");
		if (fileToBeCreated == NULL)
		{
			responseMessageSendInPractice = strdup(badResponeMessage400.c_str());
			responseMessageSendInPractice = (char*)realloc(responseMessageSendInPractice, strlen(responseMessageSendInPractice) + strlen("Content-Length: 15\r\n\r\n400 Bad Request") + 1);
			strcat(responseMessageSendInPractice, "Content-Length: 15\r\n\r\n400 Bad Request");
		}
		else
		{
			int positionOfContentLengthPhrase = sockets[index].currentRequestFullInfo.find("Content-Length:");
			int positionOfEndOfContentLengthVal = sockets[index].currentRequestFullInfo.find("\r\n", positionOfContentLengthPhrase); 

			string conentLengthValInStr = sockets[index].currentRequestFullInfo.substr(positionOfContentLengthPhrase + CONTENT_LENGTH_PHRASE_SIZE, positionOfEndOfContentLengthVal - (CONTENT_LENGTH_PHRASE_SIZE + positionOfContentLengthPhrase));
			int conentLengthVal = atoi(conentLengthValInStr.c_str());

			int dataStartingPoint = sockets[index].currentRequestFullInfo.find("\r\n\r\n");

			fwrite(sockets[index].currentRequestFullInfo.c_str() + dataStartingPoint + 4, sizeof(char), conentLengthVal, fileToBeCreated);
				
			fclose (fileToBeCreated);

			responseMessageSendInPractice = strdup(goodResponseMessage.c_str());
			responseMessageSendInPractice = (char*)realloc(responseMessageSendInPractice, strlen(responseMessageSendInPractice) + strlen("Content-Length: 0\r\n\r\n") + 1);
			strcat(responseMessageSendInPractice, "Content-Length: 0\r\n\r\n");
		}
	}

	else if (sockets[index].sendSubType == SEND_TYPE_TRACE)
	{
		string contentLength = "Content-Length: ";
		char* temp = (char*)malloc(sizeof(char)* 1000);
		string strContentLength = itoa(sockets[index].currentRequestFullInfo.length(), temp, 10);
		contentLength += strContentLength;
		goodResponseMessage += contentLength;
		goodResponseMessage += "\r\n\r\n";
		goodResponseMessage += sockets[index].currentRequestFullInfo;

		responseMessageSendInPractice = strdup(goodResponseMessage.c_str());
	}

	else //sockets[index].sendSubType == SEND_TYPE_ERROR_BAD_REQUEST
	{
		responseMessageSendInPractice = strdup(badResponeMessage400.c_str());
		responseMessageSendInPractice = (char*)realloc(responseMessageSendInPractice, strlen(responseMessageSendInPractice) + strlen("Content-Length: 15\r\n\r\n400 Bad Request") + 1);
		strcat(responseMessageSendInPractice, "Content-Length: 15\r\n\r\n400 Bad Request");
	}

	bytesSent = send(msgSocket, responseMessageSendInPractice, (int)strlen(responseMessageSendInPractice), 0);
	
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Server: Error at send(): " << WSAGetLastError() << endl;	
		return;
	}

	cout<<"Server: Sent: "<<bytesSent<<"\\"<<strlen(responseMessageSendInPractice)<<" bytes of \n\""<<responseMessageSendInPractice<<"\""<<endl<<"message\n\n";
		
	free((void*)responseMessageSendInPractice);

	sockets[index].send = IDLE;
}

string& replaceAll(string& context, const string& from, const string& to) // this function was found on the web through google
{
    size_t lookHere = 0;
    size_t foundHere;
    while((foundHere = context.find(from, lookHere)) != string::npos)
    {
          context.replace(foundHere, from.size(), to);
          lookHere = foundHere + to.size();
    }

    return context;
}