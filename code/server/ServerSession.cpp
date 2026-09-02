#include "ServerSession.h"

ServerSession &ServerSession::instance()
{
    static ServerSession s;
    return s;
}

void ServerSession::reset()
{
    *this = ServerSession();
}
