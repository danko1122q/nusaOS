/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

/*
 * nsa — NSA language toolchain v2.4
 *
 * Commands:
 *   nsa build <file.nsa> [output.nbin]         compile .nsa → .nbin
 *   nsa build-nss <file.nss>                   validate a .nss module
 *   nsa run   <file.nbin>                      execute a .nbin program
 *   nsa help                                   show this help
 *   nsa version                                print version info
 *
 * Build options:
 *   --nss-path <dir[:dir...]>   extra directories to search for .nss modules
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <vector>

#include "nsa_compiler.h"
#include "nsa_vm.h"
#include "nsa_opcodes.h"

#define NSA_VERSION  "2.5.2"
#define NSA_CODENAME "NusaOS"

/* ── Helpers ─────────────────────────────────────────────────────────── */

static bool write_exact(int fd, const void* buf, size_t n) {
    size_t total=0;
    while (total<n) {
        ssize_t w=write(fd,(const char*)buf+total,n-total);
        if (w<=0) return false; total+=w;
    }
    return true;
}
static bool read_exact(int fd, void* buf, size_t n) {
    size_t total=0;
    while (total<n) {
        ssize_t r=read(fd,(char*)buf+total,n-total);
        if (r<=0) return false; total+=r;
    }
    return true;
}

static std::string resolve_path(const char* path) {
    if (path[0]=='/') return std::string(path);
    char cwd[4096];
    if (getcwd(cwd,sizeof(cwd))!=nullptr) {
        std::string r(cwd); r+='/'; r+=path; return r;
    }
    return std::string(path);
}
static std::string replace_ext(const std::string& path, const std::string& new_ext) {
    size_t dot=path.rfind('.'); size_t sep=path.rfind('/');
    if (dot!=std::string::npos&&(sep==std::string::npos||dot>sep))
        return path.substr(0,dot)+new_ext;
    return path+new_ext;
}

/* ── Help ────────────────────────────────────────────────────────────── */
static void print_help(const char* prog) {
    fprintf(stdout,
        "NSA Language Toolchain v" NSA_VERSION " (" NSA_CODENAME ")\n"
        "\n"
        "Usage:\n"
        "  %s build <file.nsa> [out.nbin] [--nss-path dir]   Compile source\n"
        "  %s build-nss <file.nss>                            Validate a module\n"
        "  %s run   <file.nbin>                               Run compiled program\n"
        "  %s help                                            Show this help\n"
        "  %s version                                         Print version\n"
        "\n"
        "Module system (.nss files):\n"
        "\n"
        "  A .nss file is a reusable module — like a .h + .c combined.\n"
        "  It exports global constants and functions to any .nsa that imports it.\n"
        "  Only the symbols actually used are included (tree-shaking).\n"
        "\n"
        "  .nss syntax:\n"
        "    global int   MAX_SIZE = 100     declare exported global constant\n"
        "    global str   APP_NAME = \"Nusa\"  (int, str, bool supported)\n"
        "    global bool  DEBUG    = false\n"
        "\n"
        "    func add a b -> result           exported function (same as .nsa)\n"
        "        let result = 0\n"
        "        add result a\n"
        "        add result b\n"
        "    endfunc\n"
        "\n"
        "  Importing in a .nsa file:\n"
        "    import \"math\"              loads math.nss from same directory\n"
        "    import \"libs/strings\"      sub-path also works\n"
        "\n"
        "  Using imported symbols:\n"
        "    call math.add x y -> result   call imported function\n"
        "    let n = math.MAX_SIZE         read imported global\n"
        "\n"
        "  The compiler automatically finds .nss files in the same directory\n"
        "  as the .nsa source, or in --nss-path directories.\n"
        "\n"
        "Language reference:\n"
        "\n"
        "  Variables:\n"
        "    let x = 42             integer variable\n"
        "    let s = \"hello\"        string variable\n"
        "    let b = true           bool variable\n"
        "    let y = x              copy variable\n"
        "    copy dst src           copy between variables\n"
        "\n"
        "  Output / Input:\n"
        "    print x                print variable (with newline)\n"
        "    print \"text\"           print string literal\n"
        "    println x              print without trailing newline\n"
        "    input x                read from stdin\n"
        "\n"
        "  Arithmetic:\n"
        "    add x 10  sub x y  mul x 3  div x y  mod x 7\n"
        "    inc x     dec x    neg x\n"
        "\n"
        "  Logic / Comparison:\n"
        "    not b\n"
        "    cmp result a == b      integers: all 6 ops; strings: == and != only\n"
        "    and result a b         or result a b\n"
        "\n"
        "  String operations:\n"
        "    concat s \" world\"      len n s    to_int n s    to_str s n\n"
        "\n"
        "  Arrays:\n"
        "    arr int  scores 5      arr str  names 3      arr bool flags 4\n"
        "    aset scores i 100      aget val scores i     alen n scores\n"
        "\n"
        "  Functions:\n"
        "    func name a b -> result\n"
        "        ...\n"
        "    endfunc\n"
        "    call name x y -> result    return\n"
        "\n"
        "  Control flow:\n"
        "    if x == 10 then  ...  else  ...  end\n"
        "    loop 5 times  ...  end\n"
        "    loop while x != 0  ...  end\n"
        "\n"
        "  Comments:\n"
        "    // line comment    # also a comment    /* block comment */\n",
        prog,prog,prog,prog,prog
    );
}

/* ── build command ───────────────────────────────────────────────────── */
static int cmd_build(int argc, char** argv) {
    if (argc<2) {
        fprintf(stderr,"nsa build: missing source file\n"
                       "usage: nsa build <file.nsa> [output.nbin] [--nss-path <dirs>]\n");
        return 1;
    }

    std::string src_path=resolve_path(argv[1]);
    std::string out_path;
    std::string nss_path;

    /* Parse remaining args */
    for (int i=2;i<argc;i++) {
        if (strcmp(argv[i],"--nss-path")==0) {
            if (i+1>=argc) { fprintf(stderr,"nsa build: --nss-path requires argument\n"); return 1; }
            nss_path=argv[++i];
        } else if (out_path.empty() && argv[i][0]!='-') {
            out_path=resolve_path(argv[i]);
        } else {
            fprintf(stderr,"nsa build: unknown option '%s'\n",argv[i]); return 1;
        }
    }
    if (out_path.empty())
        out_path=replace_ext(src_path,".nbin");

    /* Read source */
    int src_fd=open(src_path.c_str(),O_RDONLY);
    if (src_fd<0) {
        fprintf(stderr,"nsa build: couldn't open '%s': %s\n",src_path.c_str(),strerror(errno));
        return 1;
    }
    std::string source; char chunk[4096]; ssize_t n;
    while ((n=read(src_fd,chunk,sizeof(chunk)))>0) source.append(chunk,(size_t)n);
    close(src_fd);

    /* Compile */
    NsaCompiler::CompileResult result=NsaCompiler::compile(
        source,src_path.c_str(),nss_path.empty()?nullptr:nss_path.c_str());

    if (result.warning_count>0)
        fprintf(stderr,"nsa build: %d warning(s)\n",result.warning_count);
    if (!result.ok) {
        fprintf(stderr,"nsa build: compilation failed (%d error%s)\n",
                result.error_count,result.error_count==1?"":"s");
        return 1;
    }

    /* Write .nbin */
    int out_fd=open(out_path.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0755);
    if (out_fd<0) {
        fprintf(stderr,"nsa build: couldn't create '%s': %s\n",out_path.c_str(),strerror(errno));
        return 1;
    }
    uint8_t  sym_count=result.sym_count;
    uint16_t bc_size  =(uint16_t)result.bytecode.size();
    bool ok=true;
    ok=ok&&write_exact(out_fd,NSA_MAGIC,sizeof(NSA_MAGIC));
    ok=ok&&write_exact(out_fd,&sym_count,1);
    ok=ok&&write_exact(out_fd,&bc_size,2);
    ok=ok&&write_exact(out_fd,result.bytecode.data(),bc_size);
    close(out_fd);
    if (!ok) { fprintf(stderr,"nsa build: write error on '%s'\n",out_path.c_str()); return 1; }

    fprintf(stdout,"nsa build: OK  %u byte(s) bytecode, %d var(s)  →  %s\n",
            (unsigned)bc_size,(int)sym_count,out_path.c_str());
    return 0;
}

/* ── build-nss command (validate a .nss module) ──────────────────────── */
static int cmd_build_nss(int argc, char** argv) {
    if (argc<2) {
        fprintf(stderr,"nsa build-nss: missing module file\n"
                       "usage: nsa build-nss <file.nss>\n");
        return 1;
    }
    std::string path=resolve_path(argv[1]);
    int fd=open(path.c_str(),O_RDONLY);
    if (fd<0) {
        fprintf(stderr,"nsa build-nss: couldn't open '%s': %s\n",path.c_str(),strerror(errno));
        return 1;
    }
    std::string source; char chunk[4096]; ssize_t n;
    while ((n=read(fd,chunk,sizeof(chunk)))>0) source.append(chunk,(size_t)n);
    close(fd);

    NsaCompiler::NssModule mod;
    if (!NsaCompiler::compile_nss(source,path.c_str(),mod)) {
        fprintf(stderr,"nsa build-nss: module has errors\n"); return 1;
    }
    fprintf(stdout,"nsa build-nss: OK  module '%s'  "
                   "%d function(s)  %d global(s)\n",
            path.c_str(),(int)mod.funcs.size(),(int)mod.globals.size());
    for (auto& kv:mod.funcs)
        fprintf(stdout,"  func  %s  (%d param(s)%s)\n",
                kv.first.c_str(),(int)kv.second.params.size(),
                kv.second.has_ret?" → return":"");
    for (auto& kv:mod.globals) {
        const char* ts="int";
        if (kv.second.type==NsaCompiler::SYM_STR)  ts="str";
        if (kv.second.type==NsaCompiler::SYM_BOOL) ts="bool";
        fprintf(stdout,"  global %s  %s\n",ts,kv.first.c_str());
    }
    return 0;
}

/* ── run command ─────────────────────────────────────────────────────── */
static int cmd_run(int argc, char** argv) {
    if (argc<2) {
        fprintf(stderr,"nsa run: missing program file\n"
                       "usage: nsa run <file.nbin>\n");
        return 1;
    }
    std::string path=resolve_path(argv[1]);
    int fd=open(path.c_str(),O_RDONLY);
    if (fd<0) {
        fprintf(stderr,"nsa run: couldn't open '%s': %s\n",path.c_str(),strerror(errno));
        return 1;
    }
    char header[9];
    if (!read_exact(fd,header,9)) {
        fprintf(stderr,"nsa run: '%s': file too small\n",path.c_str());
        close(fd); return 1;
    }
    if (memcmp(header,NSA_MAGIC,sizeof(NSA_MAGIC))!=0) {
        fprintf(stderr,"nsa run: '%s': not a valid .nbin file\n",path.c_str());
        close(fd); return 1;
    }
    uint8_t  sym_count=(uint8_t)header[6];
    uint16_t bc_size  =(uint8_t)header[7]|((uint16_t)(uint8_t)header[8]<<8);
    if (sym_count>NSA_MAX_VARS) {
        fprintf(stderr,"nsa run: '%s': corrupt header\n",path.c_str());
        close(fd); return 1;
    }
    std::vector<uint8_t> bytecode(bc_size);
    if (bc_size>0&&!read_exact(fd,bytecode.data(),bc_size)) {
        fprintf(stderr,"nsa run: '%s': truncated bytecode\n",path.c_str());
        close(fd); return 1;
    }
    close(fd);
    return NsaVM::run(bytecode,(int)sym_count,argv[1]);
}

/* ── Entry point ─────────────────────────────────────────────────────── */
int main(int argc, char** argv) {
    if (argc<2) {
        fprintf(stderr,"NSA Language Toolchain v" NSA_VERSION "\n"
                       "usage: nsa <command> [args]\n"
                       "       nsa help   for full documentation\n");
        return 1;
    }
    const char* cmd=argv[1];
    if (!strcmp(cmd,"help")||!strcmp(cmd,"--help")||!strcmp(cmd,"-h")) {
        print_help(argv[0]); return 0;
    }
    if (!strcmp(cmd,"version")||!strcmp(cmd,"--version")||!strcmp(cmd,"-v")) {
        fprintf(stdout,"NSA Language Toolchain v" NSA_VERSION " (" NSA_CODENAME ")\n"
                       "Built for nusaOS — GPL-3.0-or-later\n");
        return 0;
    }
    if (!strcmp(cmd,"build"))     return cmd_build(argc-1,argv+1);
    if (!strcmp(cmd,"build-nss")) return cmd_build_nss(argc-1,argv+1);
    if (!strcmp(cmd,"run"))       return cmd_run(argc-1,argv+1);

    fprintf(stderr,"nsa: unknown command '%s'\n"
                   "     run 'nsa help' for usage\n",cmd);
    return 1;
}