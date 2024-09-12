#include <WinSock2.h>
#include "Session.h"

BOOL Session::Init(SOCKET clientSock, DWORD dwClientID, SHORT shIdx)
{
    sock = clientSock;
    bSendingInProgress = FALSE;
    _InterlockedExchange((LONG*)&bUsing, TRUE);
    id = MAKE_SESSION_ID((ULONGLONG)dwClientID, shIdx);
    IoCnt = 0;
    lSendBufNum = 0;
    recvRB.ClearBuffer();
    sendRB.ClearBuffer();
    return TRUE;
}
