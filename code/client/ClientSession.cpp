#include "ClientSession.h"

ClientSession &ClientSession::instance()
{
    static ClientSession s;
    return s;
}

void ClientSession::reset()
{
    *this = ClientSession();
}
