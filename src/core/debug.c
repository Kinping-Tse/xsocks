/*
 * This file is part of xsocks, a lightweight proxy tool for science online.
 *
 * Copyright (C) 2019 XJP09_HK <jianping_xie@aliyun.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "common.h"
#include "redis/config.h"

#ifdef HAVE_BACKTRACE

#include "version.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>

#define TRACE_MAX_LEN 1024

static int bug_report_start = 0;

static void sigsegvHandler(int sig, siginfo_t *info, void *secret);
static void *getMcontextEip(ucontext_t *uc);

#endif // HAVE_BACKTRACE

void setupSigsegvHandlers() {
#ifdef HAVE_BACKTRACE
    struct sigaction act;

    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
    act.sa_sigaction = sigsegvHandler;

    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGBUS, &act, NULL);
    sigaction(SIGFPE, &act, NULL);
    sigaction(SIGILL, &act, NULL);
#endif // HAVE_BACKTRACE
}

#ifdef HAVE_BACKTRACE
void bugReportStart() {
    if (bug_report_start == 0) {
        LOGER("\n=== XSOCKS BUG REPORT START: Cut & paste starting from here ===\n\n");
        bug_report_start = 1;
    }
}

void logStackTrace(ucontext_t *uc) {
    void *trace[TRACE_MAX_LEN];
    int trace_size = 0;
    char **trace_strings;

    /* Generate the stack trace */
    trace_size = backtrace(trace+1, TRACE_MAX_LEN-1);

    if (getMcontextEip(uc) != NULL) {
        LOGER("EIP:\n");
        trace[0] = getMcontextEip(uc);
        trace_strings = backtrace_symbols(trace, 1);
        LOGER("%s\n", trace_strings[0]);
    }

    LOGER("\nBacktrace:\n");
    trace_strings = backtrace_symbols(trace+1, trace_size-1);
    for (int i = 0; i < trace_size-1; i++) {
        LOGER("%s\n", trace_strings[i]);
    }
}

void dumpX86Calls(void *addr, size_t len) {
    size_t j;
    unsigned char *p = addr;
    Dl_info info;
    /* Hash table to best-effort avoid printing the same symbol
     * multiple times. */
    unsigned long ht[256] = {0};

    if (len < 5) return;
    for (j = 0; j < len-4; j++) {
        if (p[j] != 0xE8) continue; /* Not an E8 CALL opcode. */
        unsigned long target = (unsigned long)addr+j+5;
        target += *((int32_t*)(p+j+1));
        if (dladdr((void*)target, &info) != 0 && info.dli_sname != NULL) {
            if (ht[target&0xff] != target) {
                printf("Function at 0x%lx is %s\n",target,info.dli_sname);
                ht[target&0xff] = target;
            }
            j += 4; /* Skip the 32 bit immediate. */
        }
    }
}

void logHexDump(char *descr, void *value, size_t len) {
    char buf[65], *b;
    unsigned char *v = value;
    char charset[] = "0123456789abcdef";

    LOGER("%s (hexdump of %zu bytes):", descr, len);
    b = buf;
    while(len) {
        b[0] = charset[(*v)>>4];
        b[1] = charset[(*v)&0xf];
        b[2] = '\0';
        b += 2;
        len--;
        v++;
        if (b-buf == 64 || len == 0) {
            LOGER(buf);
            b = buf;
        }
    }
    LOGER("\n");
}

void logCoreDump(void *eip) {
    Dl_info info;

    if (!eip || dladdr(eip, &info) == 0) return;

    LOGER(
        "\n------ DUMPING CODE AROUND EIP ------\n"
        "Symbol: %s (base: %p)\n"
        "Module: %s (base %p)\n"
        "$ xxd -r -p /tmp/dump.hex /tmp/dump.bin\n"
        "$ objdump --adjust-vma=%p -D -b binary -m i386:x86-64 /tmp/dump.bin\n"
        "------\n",
        info.dli_sname, info.dli_saddr, info.dli_fname, info.dli_fbase,
        info.dli_saddr);
    size_t len = (long)eip - (long)info.dli_saddr;
    unsigned long sz = sysconf(_SC_PAGESIZE);
    if (len < 1<<13) {
        unsigned long next = ((unsigned long)eip + sz) & ~(sz-1);
        unsigned long end = (unsigned long)eip + 128;
        if (end > next) end = next;
        len = end - (unsigned long)info.dli_saddr;
        logHexDump("dump of function", info.dli_saddr, len);
        dumpX86Calls(info.dli_saddr, len);
    }
}

static void sigsegvHandler(int sig, siginfo_t *info, void *secret) {
    ucontext_t *uc = (ucontext_t *)secret;
    void *eip = getMcontextEip(uc);

    bugReportStart();

    LOGE("XSOCKS %s crashed by signal: %d", XS_VERSION, sig);
    if (eip != NULL) LOGE("Crashed running the instruction at: %p", eip);
    if (sig == SIGSEGV || sig == SIGBUS) LOGE("Accessing address: %p", info->si_addr);

    LOGER("\n------ STACK TRACE ------\n");
    logStackTrace(uc);

    logCoreDump(eip);

    LOGER("\n=== XSOCKS BUG REPORT END. Make sure to include from START to END. ===\n\n");

    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND;
    act.sa_handler = SIG_DFL;
    sigaction(sig, &act, NULL);
    kill(getpid(), sig);
}

static void *getMcontextEip(ucontext_t *uc) {
#if defined(__APPLE__) && !defined(MAC_OS_X_VERSION_10_6)
    /* OSX < 10.6 */
    #if defined(__x86_64__)
    return (void*) uc->uc_mcontext->__ss.__rip;
    #elif defined(__i386__)
    return (void*) uc->uc_mcontext->__ss.__eip;
    #else
    return (void*) uc->uc_mcontext->__ss.__srr0;
    #endif
#elif defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_6)
    /* OSX >= 10.6 */
    #if defined(_STRUCT_X86_THREAD_STATE64) && !defined(__i386__)
    return (void*) uc->uc_mcontext->__ss.__rip;
    #else
    return (void*) uc->uc_mcontext->__ss.__eip;
    #endif
#elif defined(__linux__)
    /* Linux */
    #if defined(__i386__)
    return (void*) uc->uc_mcontext.gregs[14]; /* Linux 32 */
    #elif defined(__X86_64__) || defined(__x86_64__)
    return (void*) uc->uc_mcontext.gregs[16]; /* Linux 64 */
    #elif defined(__ia64__) /* Linux IA64 */
    return (void*) uc->uc_mcontext.sc_ip;
    #elif defined(__arm__) /* Linux ARM */
    return (void*) uc->uc_mcontext.arm_pc;
    #elif defined(__aarch64__) /* Linux AArch64 */
    return (void*) uc->uc_mcontext.pc;
    #endif
#elif defined(__FreeBSD__)
    /* FreeBSD */
    #if defined(__i386__)
    return (void*) uc->uc_mcontext.mc_eip;
    #elif defined(__x86_64__)
    return (void*) uc->uc_mcontext.mc_rip;
    #endif
#elif defined(__OpenBSD__)
    /* OpenBSD */
    #if defined(__i386__)
    return (void*) uc->sc_eip;
    #elif defined(__x86_64__)
    return (void*) uc->sc_rip;
    #endif
#elif defined(__DragonFly__)
    return (void*) uc->uc_mcontext.mc_rip;
#else
    return NULL;
#endif
}

#endif // HAVE_BACKTRACE
