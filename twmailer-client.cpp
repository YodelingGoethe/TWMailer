#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"

#define  speak std::cout<<
#define error std::cerr <<
#define clear <<std::flush

bool askinput(const char* promt, std::string &out)
{
    speak promt clear;
    return static_cast<bool>(std::getline(std::cin, out)); //read stuff is written directly into out, return is true if the function read a line.
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        speak "This ain't how it works... Usage: " << argv[0] << " <ip> <port>\n";
        //argv[0] is always the program name, so this way we can change it later without rewrites.
        return 1;
    }
    int port = atoi(argv[2]); //returned 0 bei inkompatiblem input, das wird beim range check rausgefiltert.
    sockaddr_in ipadress{}; //{} zero initializes the struct.
    ipadress.sin_family = AF_INET; //AF_INET has to be used for TCP/UDP connections.
    ipadress.sin_port = htons(static_cast<uint16_t>(port));
    //^htons converts from little endian to bit endian.
    //it then has to get cast to 16 bits which is the width of a TCP port-field,
    //otherwise the compiler would complain about "lost information" when narrowing to int.
    if (port< 1|| port > 65535 || inet_pton(AF_INET, argv[1], &ipadress.sin_addr) != 1)
    //inet_pton from arpa/inet.h parses the ip adress (xxx.xxx.xxx.xxx) with the dots
    //into the ip field in the sockaddr_in struct
    {
        error "invalid ip or port. check your inputs and try again.\n";
        return 1;
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0); //SOCK_STREAM is TCP, protocol 0 means take the default
    if (fd<0)   //socket return 0 on failure
    {
        perror("socket");
        return 1;
    }
    if (connect(fd, reinterpret_cast<sockaddr*>(&ipadress), sizeof(ipadress)) <0)
    //ip is an IPv4 specific struct, but for connect it has to be cast to a generic sockaddr*.
    {
        perror("connect");
        return 1;
    }
    LineReader in(fd); //this reads the server replies from the socket.
    speak "Connected successfully.\n";
    speak "Possible Commands: SEND, LIST, READ, DEL, QUIT (q)\n";

    std::string cmd;
    std::string field1;
    std::string field2;
    std::string field3;
    std::string line;
    std::string msg;
    while (askinput("", cmd))
    {
        if (cmd == "SEND")
        {
            if (!askinput("Sender: ", field1) || !askinput("Reciever: ", field2) || !askinput("Subject (max. 80 chars): ", field3))
            {
                break;
            }
            msg = "SEND\n" + field1 + "\n" + field2 + "\n" + field3 + "\n"; //looks kinda ugly but task pdf wants fields seperated by line.
            speak "Message (end with single line containing .\".\"): ";
            while (std::getline(std::cin, line))
            {
                msg+=line+"\n";
                if (line==".")
                {
                    break;
                }
            }
            if (!sendAll(fd, msg) || !in.readLine(line))
            //this sends the whole request and reads the server's reply. if it fails the connection failed.
            {
                error "something went wrong, check connection to server.\n";
                break;
            }
            speak "Message sent." << "\n";

        }else if (cmd =="LIST")
        {
            if (!askinput("Username: ", field1))
            {
                break;
            }
            if (!sendAll(fd, "LIST\n" + field1 + "\n")|| !in.readLine(line))
            {
                error "Connection lost\n";
                break;
            }
            speak line << " messages\n";
            int n = std::atoi(line.c_str());
            for (int i = 1; i<=n && in.readLine(line); i++)
            {
                speak i << ": " << line << "\n";
            }
        }else if (cmd =="READ")
        {
            //do reading stuff
        }else if (cmd =="DEL")
        {
            //do deleting stuff
        }else if (cmd == "QUIT" || cmd == "q")
        {
            //do quitting stuff
        }else
        {
            speak "Something went wrong. Maybe your command does not exist?\n";
        }


    }



}