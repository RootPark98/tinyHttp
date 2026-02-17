#include "server.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv){
    int port = 8080;
    if(argc >= 2) port = atoi(argv[1]);

    if(port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %d\n", port);
        return 1;
    }

    return run_server("0.0.0.0", port);
}