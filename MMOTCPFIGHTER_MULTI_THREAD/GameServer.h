#pragma once
class GameServer : public IHandler
{
public:
	BOOL Start(DWORD dwMaxSession);
	void SendPacket(ID id, Packet* pPacket);
	virtual BOOL OnConnectionRequest();
	virtual void* OnAccept(ID id);
	virtual void OnRecv(void* pClient, Packet* pPacket);
	virtual void OnRelease(void* pPlayer);
	static unsigned __stdcall AcceptThread(LPVOID arg);
	static unsigned __stdcall IOCPWorkerThread(LPVOID arg);
	Session* pSessionArr_;
	LONG lSessionNum_ = 0;
	LONG lMaxSession_;
	Stack DisconnectStack_;
	CRITICAL_SECTION stackLock_;
	HANDLE hcp_;
	HANDLE hAcceptThread_;
	HANDLE* hIOCPWorkerThreadArr_;
	SOCKET hListenSock_;
	BOOL RecvPost(Session* pSession);
	BOOL SendPost(Session* pSession);
	void ReleaseSession(Session* pSession);
	void RecvProc(Session* pSession, DWORD dwNumberOfBytesTransferred);
	void SendProc(Session* pSession, DWORD dwNumberOfBytesTransferred);

	// Monitoring º¯¼ö
	// Accept
	LONG lAcceptTPS_ = 0;

	// Disconnect
	LONG lDisconnectTPS_ = 0;
};
