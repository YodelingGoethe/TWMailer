#pragma once

#include <string>
#include <sys/socket.h>
#include <sys/types.h>

class LineReader {
private:
    int _fd; //copy of the socket file descriptor
    char _buf[1024]{};
    size_t _pos = 0, _len = 0;
public:
    explicit LineReader(const int fd) : _fd(fd) {}
    bool readLine(std::string &line)
    {
        line.clear();
        while (true)
        {
            if (_pos == _len) //if the buffer is fully comsumed...
            {
                const ssize_t n = recv(_fd, _buf, sizeof(_buf), 0); //ssize_t == signed, size_t is unsigned.
                if (n<=0) //n<0 means error, 0 means peer closed the connection.
                {
                    return false;
                }
                _pos=0;
                _len=static_cast<size_t>(n); //this cast works bc of the n<=0 check earlier
            }
            const char c = _buf[_pos++];
            if (c=='\n')
            {
                return true;
            }
            if (c!='\r')
            {
                line+=c; //drop \r so that \r\n endings are permissible
            }
        }
    }
};
