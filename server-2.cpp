//                              -*- Mode: C++ -*- 
// 
// Description: 
// 
// Author: 
// Created: 2016/6/24 ������ 16:06:05
// Last-Updated: 2016/6/27 ����һ 15:59:00
//           By: 10034491
// 
//     Update #: 92
// 

// Change Log:
// 
// IOCP_TCPIP_Socket_Server.cpp  
      
#include <WinSock2.h>  
#include <Windows.h>  
#include <vector>  
#include <iostream>  

#include "mswsock.h"

using namespace std;  
      
#pragma comment(lib, "Ws2_32.lib")      // Socket������õĶ�̬���ӿ�  
#pragma comment(lib, "Kernel32.lib")    // IOCP��Ҫ�õ��Ķ�̬���ӿ�  
      
/** 
 * �ṹ�����ƣ�PER_IO_DATA 
 * �ṹ�幦�ܣ��ص�I/O��Ҫ�õ��Ľṹ�壬��ʱ��¼IO���� 
 **/  
const int DataBuffSize  = 2 * 1024;  
typedef struct  
{  
    WSAOVERLAPPED overlapped;  
    WSABUF databuff;  
    char buffer[ DataBuffSize ];  
    int data_len;  
    int operationType;

    SOCKET op_socket;
}PER_IO_OPERATEION_DATA, *LPPER_IO_DATA, *LPPER_IO_DATA, PER_IO_DATA;  
      
/** 
 * �ṹ�����ƣ�PER_HANDLE_DATA 
 * �ṹ��洢����¼�����׽��ֵ����ݣ��������׽��ֵı������׽��ֵĶ�Ӧ�Ŀͻ��˵ĵ�ַ�� 
 * �ṹ�����ã��������������Ͽͻ���ʱ����Ϣ�洢���ýṹ���У�֪���ͻ��˵ĵ�ַ�Ա��ڻطá� 
 **/  
typedef struct  
{  
    SOCKET socket;  
    SOCKADDR_STORAGE ClientAddr;  
}PER_HANDLE_DATA, *LPPER_HANDLE_DATA;  
      
// ����ȫ�ֱ���  
const int DefaultPort = 6000;         
vector < PER_HANDLE_DATA* > clientGroup;      // ��¼�ͻ��˵�������  
      
HANDLE hMutex = CreateMutex(NULL, FALSE, NULL);  
DWORD WINAPI ServerWorkThread(LPVOID CompletionPortID);  
DWORD WINAPI ServerSendThread(LPVOID IpParam);  

HANDLE ghSemaphore; // connect ����

// ��ʼ������  
int main()  
{  
    // ����socket��̬���ӿ�  
    WORD wVersionRequested = MAKEWORD(2, 2); // ����2.2�汾��WinSock��  
    WSADATA wsaData;    // ����Windows Socket�Ľṹ��Ϣ  
    DWORD err = WSAStartup(wVersionRequested, &wsaData);  
      
    if (0 != err){  // ����׽��ֿ��Ƿ�����ɹ�  
        cerr << "Request Windows Socket Library Error!\n";  
        return -1;  
    }  
    if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2){// ����Ƿ�����������汾���׽��ֿ�  
        WSACleanup();  
        cerr << "Request Windows Socket Version 2.2 Error!\n";  
        return -1;  
    }  
      
    // ����IOCP���ں˶���  
    /** 
     * ��Ҫ�õ��ĺ�����ԭ�ͣ� 
     * HANDLE WINAPI CreateIoCompletionPort( 
     *    __in   HANDLE FileHandle,     // �Ѿ��򿪵��ļ�������߿վ����һ���ǿͻ��˵ľ�� 
     *    __in   HANDLE ExistingCompletionPort, // �Ѿ����ڵ�IOCP��� 
     *    __in   ULONG_PTR CompletionKey,   // ��ɼ���������ָ��I/O��ɰ���ָ���ļ� 
     *    __in   DWORD NumberOfConcurrentThreads // ��������ͬʱִ������߳�����һ���ƽ���CPU������*2 
     * ); 
     **/  
    HANDLE completionPort = CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, 0, 0);  
    if (NULL == completionPort){    // ����IO�ں˶���ʧ��  
        cerr << "CreateIoCompletionPort failed. Error:" << GetLastError() << endl;  
        return -1;  
    }  
      
    // ����IOCP�߳�--�߳����洴���̳߳�  
      
    // ȷ���������ĺ�������  
    SYSTEM_INFO mySysInfo;  
    GetSystemInfo(&mySysInfo);  
      
    // ���ڴ������ĺ������������߳�  
    for(DWORD i = 0; i < (mySysInfo.dwNumberOfProcessors * 2); ++i){  
        // �����������������̣߳�������ɶ˿ڴ��ݵ����߳�  
        HANDLE ThreadHandle = CreateThread(NULL, 0, ServerWorkThread, completionPort, 0, NULL);  
        if(NULL == ThreadHandle){  
            cerr << "Create Thread Handle failed. Error:" << GetLastError() << endl;  
            return -1;  
        }  
        CloseHandle(ThreadHandle);  
    }  
      
    // ������ʽ�׽���  
    //SOCKET srvSocket = socket(AF_INET, SOCK_STREAM, 0);
    SOCKET srvSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);  

    LPPER_HANDLE_DATA PerHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA));  // �ڶ���Ϊ���PerHandleData����ָ����С���ڴ�  
    PerHandleData->socket = srvSocket;
    CreateIoCompletionPort((HANDLE)srvSocket, completionPort, (ULONG_PTR)PerHandleData, 0);

    // ��SOCKET������  
    SOCKADDR_IN srvAddr;  
    srvAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    srvAddr.sin_family = AF_INET;  
    srvAddr.sin_port = htons(DefaultPort);  
    int bindResult = bind(srvSocket, (SOCKADDR*)&srvAddr, sizeof(SOCKADDR));  
    if(SOCKET_ERROR == bindResult){  
        cerr << "Bind failed. Error:" << GetLastError() << endl;  
        return -1;  
    }  
      
    // ��SOCKET����Ϊ����ģʽ  
    int listenResult = listen(srvSocket, SOMAXCONN);   // 10->SOMAXCONN
    if(SOCKET_ERROR == listenResult){  
        cerr << "Listen failed. Error: " << GetLastError() << endl;  
        return -1;  
    }  
          
    // ��ʼ����IO����  
    cout << "����������׼�����������ڵȴ��ͻ��˵Ľ���...\n";  
      
    // �������ڷ������ݵ��߳�  
    HANDLE sendThread = CreateThread(NULL, 0, ServerSendThread, 0, 0, NULL);

    // AcceptEx
    LPFN_ACCEPTEX lpfnAcceptEx;
    GUID GuidAcceptEx = WSAID_ACCEPTEX;
    DWORD dwBytes=0;
    WSAIoctl(srvSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx, sizeof(GuidAcceptEx), &lpfnAcceptEx, sizeof(lpfnAcceptEx),
             &dwBytes, NULL, NULL);

    ghSemaphore = ::CreateSemaphore(NULL, 10, 10, NULL);
    if (NULL==ghSemaphore)
    {
        cerr << " ::CreateSemaphore() failed. Error: " << GetLastError() << endl;
        return -1;
    }
    while(true)
    {
        ::WaitForSingleObject(ghSemaphore, INFINITE);

        SOCKET accept_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (INVALID_SOCKET==accept_socket)
        {
            cerr << "create accept socket failed. Error: " << GetLastError() << endl;
            WSACleanup();
            return -1;
        }

        LPPER_IO_DATA PerIoData = NULL;  
        PerIoData = (LPPER_IO_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_DATA));  
        ZeroMemory(&(PerIoData -> overlapped), sizeof(WSAOVERLAPPED));  
        PerIoData->databuff.len = 1024;  
        PerIoData->databuff.buf = PerIoData->buffer;  
        PerIoData->operationType = 0;    // connect
        PerIoData->op_socket = accept_socket;
    
        //char lpOutputBuf[1024];
        //int outBufLen = 1024;
        //lpfnAcceptEx(srvSocket, accept_socket, lpOutputBuf, outBufLen-((sizeof(sockaddr_in)+16)*2),
        lpfnAcceptEx(srvSocket, accept_socket, PerIoData->buffer, 1024-((sizeof(sockaddr_in)+16)*2),
                     sizeof(sockaddr_in)+16,
                     sizeof(sockaddr_in)+16, &dwBytes, &PerIoData->overlapped);
    
        //CreateIoCompletionPort((HANDLE)accept_socket, completionPort, (u_long)0, 0);
    }
      
    return 0;  
}  
      
// ��ʼ�������̺߳���  
DWORD WINAPI ServerWorkThread(LPVOID IpParam)  
{  
    HANDLE CompletionPort = (HANDLE)IpParam;  
    DWORD BytesTransferred;  
    LPWSAOVERLAPPED IpOverlapped;  
    LPPER_HANDLE_DATA PerHandleData = NULL;  
    LPPER_IO_DATA PerIoData = NULL;  
    DWORD RecvBytes;  
    DWORD Flags = 0;  
    BOOL bRet = false;  
      
    while(true){  
        bRet = GetQueuedCompletionStatus(CompletionPort, &BytesTransferred, (PULONG_PTR)&PerHandleData, (LPWSAOVERLAPPED*)&IpOverlapped, INFINITE);  
        if(bRet == 0){  
            cerr << "GetQueuedCompletionStatus Error: " << GetLastError() << endl;  
            return -1;  
        }  
        PerIoData = (LPPER_IO_DATA)CONTAINING_RECORD(IpOverlapped, PER_IO_DATA, overlapped);  

        cout << "quened msg length=" << BytesTransferred << endl;  

        // ������׽������Ƿ��д�����  
        if(0 == BytesTransferred){
            if (NULL!=PerHandleData)
            {
                closesocket(PerHandleData->socket);  
                GlobalFree(PerHandleData);
            }
            GlobalFree(PerIoData);  
            continue;  
        }  

        if (PerIoData->operationType == 0) // connect
        {
            PER_HANDLE_DATA * new_PerHandleData = NULL;  
            SOCKADDR_IN saRemote;
            int RemoteLen;
            
            SOCKADDR_IN saLocal;

            SOCKET acceptSocket;  

            RemoteLen = sizeof(saRemote);  

            acceptSocket = PerIoData->op_socket;
            SOCKET listen_socket = PerHandleData->socket;

            memset(&saRemote, 0, sizeof(SOCKADDR_IN));
            memset(&saLocal, 0, sizeof(SOCKADDR_IN));

            //*
            if (SOCKET_ERROR == setsockopt(acceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&listen_socket, sizeof(listen_socket)))
            {
                cerr << "setsockopt() failed. Error: " << GetLastError() << endl;
                WSACleanup();
                return -1;
            }

            int len = sizeof(sockaddr);;
            getsockname(acceptSocket, (sockaddr*)&saLocal, &len);
            getpeername(acceptSocket, (sockaddr*)&saRemote, &len);
            //*/

            /*
            // GetAcceptExSockaddrs
            LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs;
            GUID GuidAcceptEx = WSAID_GETACCEPTEXSOCKADDRS;
            DWORD dwBytes=0;
            WSAIoctl(acceptSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx, sizeof(GuidAcceptEx), &lpfnGetAcceptExSockaddrs, sizeof(lpfnGetAcceptExSockaddrs), &dwBytes, NULL, NULL);

            char lpOutputBuf[1024];
            int outBufLen = 1024;
            LPSOCKADDR pLocalAddr, pRemoteAddr;
            int local_len;
            int remote_len;
            lpfnGetAcceptExSockaddrs(PerIoData->buffer, 1024-(sizeof(sockaddr_in)+16)*2,
                                     sizeof(sockaddr_in)+16, sizeof(sockaddr_in)+16,
                                     (SOCKADDR**)&pLocalAddr, &local_len,
                                     (SOCKADDR**)&pRemoteAddr, &remote_len);
            memcpy(&saLocal, pLocalAddr, sizeof(saLocal));
            memcpy(&saRemote, pRemoteAddr, sizeof(saRemote));
            //*/
              
            // �����������׽��ֹ����ĵ����������Ϣ�ṹ  
            new_PerHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA));  // �ڶ���Ϊ���PerHandleData����ָ����С���ڴ�  
            new_PerHandleData -> socket = acceptSocket;  
            memcpy (&new_PerHandleData->ClientAddr, &saRemote, RemoteLen);  
            clientGroup.push_back(new_PerHandleData);       // �������ͻ�������ָ��ŵ��ͻ�������  
      
            // �������׽��ֺ���ɶ˿ڹ���  
            CreateIoCompletionPort((HANDLE)(new_PerHandleData->socket), CompletionPort, (DWORD)new_PerHandleData, 0);  


            WaitForSingleObject(hMutex,INFINITE);  
            cout << "A Client(" << inet_ntoa(saRemote.sin_addr) << ") says: " << PerIoData->databuff.buf << endl;  
            ReleaseMutex(hMutex);  

            // ��ʼ�ڽ����׽����ϴ���I/Oʹ���ص�I/O����  
            // ���½����׽�����Ͷ��һ�������첽  
            // WSARecv��WSASend������ЩI/O������ɺ󣬹������̻߳�ΪI/O�����ṩ����      
            // ��I/O��������(I/O�ص�)  
            LPPER_IO_DATA PerIoData = NULL;  
            PerIoData = (LPPER_IO_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_DATA));  
            ZeroMemory(&(PerIoData -> overlapped), sizeof(WSAOVERLAPPED));  
            PerIoData->databuff.len = 1024;  
            PerIoData->databuff.buf = PerIoData->buffer;  
            PerIoData->operationType = 1;    // read  
      
            DWORD RecvBytes;  
            DWORD Flags = 0;  
            WSARecv(new_PerHandleData->socket, &(PerIoData->databuff), 1, &RecvBytes, &Flags, &(PerIoData->overlapped), NULL);

            ::ReleaseSemaphore(ghSemaphore, 1, NULL);
        }
        else if (PerIoData->operationType == 1) // read
        {
            // ��ʼ���ݴ����������Կͻ��˵�����  
            WaitForSingleObject(hMutex,INFINITE);

            cout << "A Client(" << inet_ntoa(((sockaddr_in*)&PerHandleData->ClientAddr)->sin_addr) <<") says: " << PerIoData->databuff.buf << endl;  
            ReleaseMutex(hMutex);  
      
            // Ϊ��һ���ص����ý�����I/O��������  
            ZeroMemory(&(PerIoData->overlapped), sizeof(WSAOVERLAPPED)); // ����ڴ�  
            PerIoData->databuff.len = 1024;  
            PerIoData->databuff.buf = PerIoData->buffer;  
            PerIoData->operationType = 1;    // read  
            WSARecv(PerHandleData->socket, &(PerIoData->databuff), 1, &RecvBytes, &Flags, &(PerIoData->overlapped), NULL);
        }
        else if (PerIoData->operationType == 2) // write
        {
            int len = PerIoData->data_len;
            char *talk = new char[len+1];
            memcpy(talk, PerIoData->buffer, len);
            talk[len] = '\0';
            cout << "send to " << inet_ntoa(((sockaddr_in*)&PerHandleData->ClientAddr)->sin_addr) << ": " << talk << endl;
            delete talk;
            talk = NULL;
        }
    }  
      
    return 0;  
}  
      
      
// ������Ϣ���߳�ִ�к���  
DWORD WINAPI ServerSendThread(LPVOID IpParam)  
{  
    while(1){  
        char talk[200];  
        gets(talk);  
        int len;  
        for (len = 0; talk[len] != '\0'; ++len){  
            // �ҳ�����ַ���ĳ���  
        }  
        talk[len] = '\n';  
        talk[++len] = '\0';

        //printf("I Say:");  
        //cout << talk;  
        WaitForSingleObject(hMutex,INFINITE);  
        for(int i = 0; i < clientGroup.size(); ++i){  
            //send(clientGroup[i]->socket, talk, 200, 0);  // ������Ϣ
            //cout << "send to " << inet_ntoa(((sockaddr_in*)&clientGroup[i]->ClientAddr)->sin_addr) << ": " << talk << endl;
            LPPER_IO_DATA PerIoData = NULL;  
            PerIoData = (LPPER_IO_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_DATA));  
            ZeroMemory(&(PerIoData -> overlapped), sizeof(WSAOVERLAPPED));
            memcpy(PerIoData->buffer, talk, len-1);
            PerIoData->databuff.len = len-1;
            PerIoData->databuff.buf = PerIoData->buffer;
            PerIoData->data_len = len-1;
            PerIoData->operationType = 2;    // write
            DWORD Flags = 0;  
            WSASend(clientGroup[i]->socket, &(PerIoData->databuff), 1, NULL, Flags, &(PerIoData->overlapped), NULL);
        }  
        ReleaseMutex(hMutex);   
    }  
    return 0;  
}
