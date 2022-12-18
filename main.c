#include "ftp.h"

int main(int argc, char* argv[]) {

    if (argc != 2) {
        fprintf(stderr, "usage: ftp://[<user>:<password>@]<host><url-path>\n");
        exit(-1);
    }

    return ftp_server(argv[1]);

}