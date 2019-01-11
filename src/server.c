
#include "common.h"
#include "event/event.h"

#include "anet.h"

static void initConfig() {}

int main(int argc, char const *argv[]) {
    initConfig();
    char err[256];
    int fd = anetTcpServer(err, 11888, NULL, 256);
    printf("%d\n", fd);

    return EXIT_SUCCESS;
}
