#include <iostream>
#include "httpserver/http_server.h"

using namespace std;

int main()
{
    HttpServer server;
    server.start();
    return 0;
}
