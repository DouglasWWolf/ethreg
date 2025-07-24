#include <unistd.h>
#include <string>
#include <cstring>
#include "ethreg.h"
#include "endian_types.h"

const uint16_t LOCAL_PORT  = 32012;
const uint16_t REMOTE_PORT = 0x0122;

using std::string;

const uint8_t OP_WRITE32 = 1;
const uint8_t OP_WRITE64 = 2;
const uint8_t OP_READ32  = 3;
const uint8_t OP_READ64  = 4;


//==========================================================================================================
// Turns an error code into a message
//==========================================================================================================
string CEthRegEx::what() const
{
    char buffer[1024], addr[256];
    const char* opcode;
    const char* remote_err;

    // Turn the opcode into a string
    switch(p1)
    {
        case OP_READ32:
            opcode = "READ32"; break;  
        case OP_READ64:
            opcode = "READ64"; break;
        case OP_WRITE32:
            opcode = "WRITE32"; break;  
        case OP_WRITE64:
            opcode = "WRITE64"; break;
        default:
            opcode = "UNKNOWN"; break;
    }

    // Convert p2 into four 16-bit fields so we can display it
    // conveniently as an address
    uint16_t a1 = (p2 >> 48) & 0xFFFF;
    uint16_t a2 = (p2 >> 32) & 0xFFFF;
    uint16_t a3 = (p2 >> 16) & 0xFFFF;
    uint16_t a4 = (p2      ) & 0xFFFF;

    // Get an ASCII representation of the address
    if ((a1 == 0) && (a2 == 0))
        sprintf(addr, "0x%04X_%04X", a3, a4);
    else
        sprintf(addr, "0x%04X_%04X_%04X_%04X", a1, a2, a3, a4);    
   
    // Turn the returned error code into a string
    switch(p3)
    {
        case 0:
            remote_err = "OKAY";
            break;

        case 1:
            remote_err = "EXOKAY";
            break;
        
        case 2:
            remote_err = "SLVERR";
            break;

        case 3:
            remote_err = "DECERR";
            break;

        default:
            remote_err = "UNKNOWN error";
    }


    switch(error)
    {
        case CANT_CREATE_SERVER:
            return "Create server failed";
        
        case CANT_MAKE_CLIENT:
            return "Create client failed";
        
        case NO_CONNECT_RESPONSE:
            return "No response while connecting";
        
        case NO_RESPONSE:
            sprintf(buffer, "No response during %s at address %s", opcode, addr);
            return buffer;
       
        case REMOTE_ERROR:
            sprintf(buffer, "%s during %s at address %s", remote_err, opcode, addr);
            return buffer;

        default:
            return "Unknown error";
    };
}
//==========================================================================================================


//==========================================================================================================
// This is the 22-byte message we use to send and receive "register read/write" messages
//==========================================================================================================
struct rw_msg_t
{
    uint8_t     opcode;
    uint8_t     error;
    be_uint16_t respond_to;
    be_uint64_t address;
    be_uint64_t data;
    uint8_t     reserved[2];
};
//==========================================================================================================



//==========================================================================================================
// Connects to our board and complains if it cant
//==========================================================================================================
void CEthReg::connect(string remote_ip, string bind_to)
{

    rw_msg_t msg, response;

    // Create our UDP server socket
    if (!m_server.create_server(LOCAL_PORT, bind_to))
    {
        throw CEthRegEx(CEthRegEx::CANT_CREATE_SERVER);
    }

    // Create our sender socket
    if (!m_sender.create_sender(REMOTE_PORT, remote_ip))
    {
        throw CEthRegEx(CEthRegEx::CANT_MAKE_CLIENT);    
    }

    // Create a message that will read AXI address 0
    memset(&msg, 0, sizeof msg);
    msg.opcode     = OP_READ32;
    msg.respond_to = LOCAL_PORT;

    // Send it
    m_sender.send(&msg, sizeof msg);

    // Wait 1 second for a response
    if (!m_server.wait_for_data(1000))
    {
        throw CEthRegEx(CEthRegEx::NO_CONNECT_RESPONSE);
    }

    // Fetch the returned message
    m_server.receive(&response, sizeof response);

}
//==========================================================================================================



//==========================================================================================================
// transact() - Sends a message, waits for the response, and performs error checking
//==========================================================================================================
void  CEthReg::transact(void* p1, void* p2)
{
    // Turn those pointers into structured references
    rw_msg_t& msg = *(rw_msg_t*)p1;
    rw_msg_t& rsp = *(rw_msg_t*)p2;

    // Create a message 
    msg.respond_to = LOCAL_PORT;

    // Send it
    m_sender.send(&msg, sizeof msg);

    // Wait 1 second for a response
    if (!m_server.wait_for_data(1000))
    {
        throw CEthRegEx(CEthRegEx::NO_RESPONSE, msg.opcode, msg.address);
    }

    // Fetch the returned message
    m_server.receive(&rsp, sizeof rsp);

    // Did we receive back the same parameters that we sent?
    bool ok = (msg.opcode  == rsp.opcode )
            & (msg.address == rsp.address);

    // If this was a write, we must also compare the data we wrote 
    if (msg.opcode == OP_WRITE32 || msg.opcode == OP_WRITE64)
    {
        ok |= (msg.data == rsp.data);
    }            

    // If we didn't receive back the same parameters we sent, complain
    if (!ok)
    {
        throw CEthRegEx(CEthRegEx::MALFORMED_RESPONSE, msg.opcode, msg.address);
    }

    // If the remote side reported an error, complain
    if (rsp.error)
    {
        throw CEthRegEx(CEthRegEx::REMOTE_ERROR, msg.opcode, msg.address, rsp.error);        
    }

}
//==========================================================================================================



//==========================================================================================================
// read32() - Reads a 32-bit value from a register
//==========================================================================================================
uint32_t CEthReg::read32(uint64_t address)
{
    rw_msg_t msg, rsp;

    // Create a message 
    memset(&msg, 0, sizeof msg);
    msg.opcode  = OP_READ32;
    msg.address = address;

    // Perform the transaction
    transact(&msg, &rsp);

    // And hand the returned value to the caller
    return (uint32_t) rsp.data;
}
//==========================================================================================================



//==========================================================================================================
// read64() - Reads a 64-bit value from a register
//==========================================================================================================
uint64_t CEthReg::read64(uint64_t address)
{
    rw_msg_t msg, rsp;

    // Create a message 
    memset(&msg, 0, sizeof msg);
    msg.opcode  = OP_READ64;
    msg.address = address;

    // Perform the transaction
    transact(&msg, &rsp);

    // And hand the returned value to the caller
    return (uint64_t) rsp.data;
}
//==========================================================================================================


//==========================================================================================================
// write32() - Writes a 32-bit value to a register
//==========================================================================================================
void CEthReg::write32(uint64_t address, uint32_t data)
{
    rw_msg_t msg, rsp;

    // Create a message 
    memset(&msg, 0, sizeof msg);
    msg.opcode  = OP_WRITE32;
    msg.address = address;
    msg.data    = data;

    // Perform the transaction
    transact(&msg, &rsp);
}
//==========================================================================================================


//==========================================================================================================
// write64() - Writes a 64-bit value to a register
//==========================================================================================================
void CEthReg::write64(uint64_t address, uint64_t data)
{
    rw_msg_t msg, rsp;

    // Create a message 
    memset(&msg, 0, sizeof msg);
    msg.opcode  = OP_WRITE64;
    msg.address = address;
    msg.data    = data;

    // Perform the transaction
    transact(&msg, &rsp);
}
//==========================================================================================================


