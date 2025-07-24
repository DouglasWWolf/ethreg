#include <unistd.h>
#include <stdio.h>
#include <cstring>
#include <string>
#include "udpsock.h"
#include "endian_types.h"
#include "ethreg.h"

using std::string;

CEthReg device;

bool wide = false;;

enum display_mode_t
{
    MODE_DEC, MODE_HEX,  MODE_BOTH
};

string data_str = "";
string addr_str = "";
bool   addr_is_numeric;

display_mode_t display_mode = MODE_BOTH;

void   execute();
void   parse_command_line(char** argv);
string fetch_token(const char* in, bool* is_numeric = nullptr);

int main(int argc, char** argv)
{
    // Parse the command line
    parse_command_line(argv);

    if (!addr_is_numeric)
    {
        fprintf(stderr, "ethreg: %s is not a number\n", *argv);
        exit(1);
    }

    try
    {
        execute();
    }
    catch(const CEthRegEx& e)
    {
        fprintf(stderr, "%s\n", e.what().c_str());
    }
    
}


//=========================================================================================================
// fetch_token() - Fetches a single token, strippping out underscores if the token is numeric
//=========================================================================================================
string fetch_token(const char* in, bool* is_numeric)
{
    int count = 0;
    char token[1024], *out = token;
    bool temp;

    // Make sure that "is_numeric" points to somewhere valid
    if (is_numeric == nullptr) is_numeric = &temp;

    // Skip over any leading whitespace
    while (*in == ' ' || *in == 9) ++in;

    // Is this a number?
    *is_numeric = (*in >= '0' && *in <= '9');

    while (true)
    {
        int c = *in++;
        
        // If this token is a number, we ignore underscores
        if (is_numeric && c == '_') continue;

        // If this is the end of the token, break out of the loop
        if (c == ' ' || c == 9 || c == 0) break;

        // Store the character
        *out++ = c;

        // Don't overflow the buffer!
        if (++count == sizeof(token)-1) break;
    }

    // Nul terminate our string
    *out = 0;

    // And hand the token to the caller
    return token;
}
//=========================================================================================================


//=========================================================================================================
// parse_command_line() - Parse the command line parameters to extract these global variables:
//                          display_mode
//                          wide
//                          addr_str
//                          addr_is_numeric
//                          data_str
//=========================================================================================================
void parse_command_line(char** argv)
{
    bool is_numeric;

    while (*(++argv))
    {
        // Make a string out of this token
        string token = *argv;

        if (token == "-dec")
        {
            display_mode = MODE_DEC;
            continue;            
        }

        if (token == "-hex")
        {
            display_mode = MODE_HEX;
            continue;
        }

        if (token == "-wide")
        {
            wide = true;
            continue;
        }
        
        // If we don't have an address yet...
        if (addr_str == "")
        {
            addr_str = fetch_token(*argv, &addr_is_numeric);
            continue;                        
        }

        if (data_str == "")
        {
            data_str = fetch_token(*argv, &is_numeric);
            if (!is_numeric)
            {
                fprintf(stderr, "ethreg: %s is not a number\n", *argv);
                exit(1);                
            }
            continue;
        }

        fprintf(stderr, "ethreg: extraneous parameter %s", *argv);
        exit(1);
    }

}
//=========================================================================================================


//=========================================================================================================
// perform_read() - Reads the register at the specified address and displays the result
//=========================================================================================================
void perform_read(uint64_t addr)
{
    uint64_t result;

    if (wide)
        result = device.read64(addr);
    else
        result = device.read32(addr);
 
    // Divide the result into two halves
    uint32_t h = (result >> 32) & 0xFFFFFFFF;
    uint32_t l = (result      ) & 0xFFFFFFFF;
             
    switch(display_mode)
    {
        case MODE_BOTH:
            if (h == 0)
                printf("0x%08X", l);
            else
                printf("0x%08X%08X", h, l);
            printf(" (%lu)\n", result);
            break;

        case MODE_HEX:
            if (wide)
                printf("%08X%08X\n", h, l);
            else
                printf("%08X\n", l);
            break;

        case MODE_DEC:
            printf("%lu\n", result);
            break;            
    }

}
//=========================================================================================================


//=========================================================================================================
// perform_write() - Writes data to a register
//=========================================================================================================
void perform_write(uint64_t addr, uint64_t data)
{
    if (wide)
        device.write64(addr, data);
    else
        device.write32(addr, (uint32_t)data);
};
//=========================================================================================================


//=========================================================================================================
// execute() - performs the bulk of the work of this program
//=========================================================================================================
void execute()
{
    device.connect("12.12.12.7");

    // Parse the address and data
    uint64_t addr = strtoull(addr_str.c_str(), nullptr, 0);
    uint64_t data = strtoull(data_str.c_str(), nullptr, 0);

    // If we're reading data
    if (data_str == "")
        perform_read(addr);
    else
        perform_write(addr, data);
}
//=========================================================================================================
    

