#include <iostream>
using namespace std;
// Don't forget to include "Ws2_32.lib" in the library list.
#include <winsock2.h> 
#include <string.h>
#include <iomanip>
#pragma warning (disable: 4996)

const int TIME_PORT = 8080;


void main() 
{
	WSAData wsaData; 
	if (NO_ERROR != WSAStartup(MAKEWORD(2,2), &wsaData))
	{
		cout << "Client: Error at WSAStartup()\n";
		return;
	}

    SOCKET connSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == connSocket)
	{
        cout << "Client: Error at socket(): " << WSAGetLastError() << endl;
        WSACleanup();
        return;
	}

	sockaddr_in server;
    server.sin_family = AF_INET; 
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_port = htons(TIME_PORT);

	if (SOCKET_ERROR == connect(connSocket, (SOCKADDR *) &server, sizeof(server)))
	{
		cout << "Client: Error at connect(): " << WSAGetLastError() << endl;
        closesocket(connSocket);
		WSACleanup();
        return;
    }

    int bytesSent = 0;
    int bytesRecv = 0;
	char sendBuff[1000] = "HEAD C:\\SWDL.txt HTTP/1.1\r\nDate: Today\r\nContent-Type: text/html\r\n\r\n";
    char recvBuff[1000];

	//"PUT C:\\goa.txt HTTP/1.1\r\nDate: Today\r\nContent-Length: 5\r\nContent-Type: text/html\r\n\r\nHELLO";

	while(true)
	{
		cout<<"Enter string to send to server:\n";

	//	gets(sendBuff);  
		
		bytesSent = send(connSocket, sendBuff, (int)strlen(sendBuff), 0);
		
		if (SOCKET_ERROR == bytesSent)
		{
			cout << "Client: Error at send(): " << WSAGetLastError() << endl;
			closesocket(connSocket);
			WSACleanup();
			return;
		}	
		cout<<"Client: Sent: "<<bytesSent<<"/"<<strlen(sendBuff)<<" bytes of \""<<sendBuff<<"\" message.\n";

		bytesRecv = recv(connSocket, recvBuff, 255, 0);
		if (SOCKET_ERROR == bytesRecv)
		{
			cout << "Client: Error at recv(): " << WSAGetLastError() << endl;
			closesocket(connSocket);
			WSACleanup();
			return;
		}
		if (bytesRecv == 0)
		{
			cout<<"Server closed the connection\n";
			return;
		}
			
		recvBuff[bytesRecv]='\0'; //add the null-terminating to make it a string
		cout<<recvBuff<<endl;

		cout<<"Client: Recieved: "<<bytesRecv<<" bytes of \n\""<<recvBuff<<"\" message.\n";
		break;

	}
}