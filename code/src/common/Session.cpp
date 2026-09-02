#include "Session.h"

Session &Session::instance()
{
    static Session s;
    return s;
}

void Session::reset()
{
    *this = Session();
}
