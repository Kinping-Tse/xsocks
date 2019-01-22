
#include "module.h"

/*
c: client lo: local r: remote
ls: localServer rs: remoteServer
ss: shadowsocks

    local                       server                            remote

tcp:
1. ss req:  enc(addr+raw)---> ls dec(addr+raw) -> rs (raw)----------->
2. ss stream: <-----(enc_buf) ls <- enc(buf) rs <--------------(raw)
3. ss stream: enc_buf ------> lc dec(raw) -> rs  (raw)-------->
4. ss stream: <-----(enc_buf) ls <- enc(buf) rs <--------------(raw)
5. (3.4 loop).....

udp:
1. udp: enc(addr+raw) ----> ls dec(addr+raw)->rs (raw) --------->
2. udp: <---------(enc_buf) ls <- enc(addr+raw) rs <--------------(raw)
3. (1,2 loop)...

*/

static module server;

static void initServer() {
    getLogger()->syslog_ident = "xs-server";
}

int main(int argc, char *argv[]) {
    moduleHook hook = {initServer, NULL, NULL};

    moduleInit(MODULE_SERVER, hook, &server, argc, argv);
    moduleRun();
    moduleExit();

    return EXIT_SUCCESS;
}
