#include "governor/loop.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Main CLI entrypoint with the on/off logic. No more, no less*/

void start_powergov(void)
{
    setsid();    
    powergov_loop();
}

void stop_powergov(void)
{
    system("pkill powergov");
    printf("Powergov stopped.\n");
}

int main(int argc, char *argv[]) 
{
    if(argc != 2) 
    {
        printf("Usage: powergov [on/off]\n", argv[0]);
        return -1;
    } 
    
    if(strcmp(argv[1], "on") == 0)
    {
        /* Call to start_powergov(); */
        start_powergov();
    }

    else if(strcmp(argv[1], "off") == 0)
    {
        /* Call to stop_powergov(); */
        stop_powergov();
    }

    else if(strcmp(argv[1], "--battery-safe") == 0)
    {
       
    }

    else
    {
        printf("Invalid argument. Use --help.\n");
        return 1;
    }

    return 0;

}