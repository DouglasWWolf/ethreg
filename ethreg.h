#include <string>
#include "udpsock.h"



//=============================================================================
// These are the exceptions we throw
//=============================================================================
struct CEthRegEx
{
    enum err_t
    {
        CANT_CREATE_SERVER,
        CANT_MAKE_CLIENT,
        NO_CONNECT_RESPONSE,
        NO_RESPONSE,
        REMOTE_ERROR,
        MALFORMED_RESPONSE,
    };

    CEthRegEx(err_t err, uint64_t P1=0, uint64_t P2=0, uint64_t P3=0)
    {
        error = err;
        p1 = P1;
        p2 = P2;
        p3 = P3;
    }



    err_t       error;
    uint64_t    p1, p2, p3;
    
    std::string what() const;
};
//=============================================================================

class CEthReg
{
public:

    // Creates a connection to our board
    void     connect(std::string ip, std::string bind_to = "");

    // Read the value of a 32-bit or 64-bit register
    uint32_t read32(uint64_t address);   
    uint64_t read64(uint64_t address);

    // Write a value to a 32-bit or 64-bit register
    void     write32(uint64_t address, uint32_t data);   
    void     write64(uint64_t address, uint64_t data);


protected:

    // This performs a UDP transaction to perform a read or write
    void    transact(void* msg, void* rsp);

    // The server socket that we listen on
    UDPSock m_server;

    // This is the socket that we send messages on
    UDPSock m_sender;

    // This is the socket that we send broadcast message on
    UDPSock m_broadcast;

};