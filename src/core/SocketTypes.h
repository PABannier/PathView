#pragma once

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    using socket_t = SOCKET;
    #ifndef INVALID_SOCKET_VALUE
        #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #endif
#else
    using socket_t = int;
    #ifndef INVALID_SOCKET_VALUE
        #define INVALID_SOCKET_VALUE (-1)
    #endif
#endif
