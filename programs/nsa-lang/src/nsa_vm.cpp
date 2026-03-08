/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

#include "nsa_vm.h"
#include "nsa_opcodes.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <vector>

/* ── Syscall interface ──────────────────────────────────────────────────
 * On i386 nusaOS: int $0x80, eax=num, ebx=arg1, ecx=arg2, edx=arg3
 * We emit the trap inline so the VM itself (a nusaOS ELF) can use it.
 * On any other host (Linux dev build) the syscall block is a no-op stub
 * so the rest of the VM still compiles and runs for testing.
 * ---------------------------------------------------------------------- */
#if defined(__i386__) && !defined(NUSAOS_KERNEL)
static inline int nsa_syscall(int num, int a, int b, int c) {
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a), "c"(b), "d"(c)
        : "memory"
    );
    return ret;
}
#define NSA_SYSCALL_SUPPORTED 1
#else
/* Stub for non-i386 / kernel builds */
static inline int nsa_syscall(int num, int a, int b, int c) {
    (void)num; (void)a; (void)b; (void)c;
    return -1;
}
#define NSA_SYSCALL_SUPPORTED 0
#endif
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>


/* float_to_str — portable double→string without relying on %f/%g in printf.
   Produces up to 6 significant digits, trims trailing zeros.             */
static void float_to_str(double d, char* buf, size_t bufsz) {
    if (bufsz < 2) { buf[0]=0; return; }
    if (d != d) { /* NaN */ buf[0]='n';buf[1]='a';buf[2]='n';buf[3]=0; return; }
    /* Handle sign */
    char* p = buf; size_t rem = bufsz-1;
    if (d < 0.0) { *p++='-'; rem--; d=-d; }
    /* Infinity */
    if (d > 1e38) { 
        *p++='i';*p++='n';*p++='f';*p=0; return; 
    }
    /* Zero special case */
    if (d == 0.0) { *p++='0'; *p=0; return; }
    /* Scale to get 6 significant digits as integer */
    int exp = 0;
    while (d >= 1000000.0) { d /= 10.0; exp++; }
    while (d < 100000.0) { d *= 10.0; exp--; }
    long long mantissa = (long long)(d + 0.5);
    if (mantissa >= 1000000LL) { mantissa /= 10; exp++; }
    /* exp: decimal point is after digit 1, shifted by exp */
    /* Actual value = mantissa * 10^(exp - 5) */
    int dot_pos = 6 + exp; /* position of decimal point from left of 6-digit string */
    /* Build 6-digit string */
    char digits[8]; int nd=6;
    for (int i=5;i>=0;i--) { digits[i]='0'+(int)(mantissa%10); mantissa/=10; }
    digits[6]=0;
    /* Trim trailing zeros from fractional part */
    int last = 5;
    if (dot_pos < 6) { while(last>dot_pos && digits[last]=='0') last--; }
    /* Write output */
    if (dot_pos <= 0) {
        /* 0.000...digits */
        if(rem>1){*p++='0';rem--;} if(rem>1){*p++='.';rem--;}
        for(int z=0;z<-dot_pos&&rem>1;z++){*p++='0';rem--;}
        for(int i=0;i<=last&&rem>1;i++){*p++=digits[i];rem--;}
    } else if (dot_pos >= nd) {
        /* integer, possibly with trailing zeros */
        for(int i=0;i<nd&&rem>1;i++){*p++=digits[i];rem--;}
        for(int z=nd;z<dot_pos&&rem>1;z++){*p++='0';rem--;}
    } else {
        /* mixed */
        for(int i=0;i<dot_pos&&rem>1;i++){*p++=digits[i];rem--;}
        if(dot_pos<=last){
            if(rem>1){*p++='.';rem--;}
            for(int i=dot_pos;i<=last&&rem>1;i++){*p++=digits[i];rem--;}
        }
    }
    *p=0;
}

namespace NsaVM {

enum VarType : uint8_t { VAR_UNSET=0, VAR_INT, VAR_STR, VAR_BOOL, VAR_FILE, VAR_FLOAT };

struct VarSlot {
    VarType type;
    int32_t ival;
    double  dval;
    char    sval[NSA_MAX_STR_LEN+1];
};

struct CallFrame {
    size_t  ret_ip;
    VarSlot locals[NSA_MAX_LOCALS];
};

/* ── Decode helpers ──────────────────────────────────────────────── */
static bool read_u8(const std::vector<uint8_t>& bc, size_t& ip, uint8_t& out) {
    if (ip >= bc.size()) return false;
    out = bc[ip++]; return true;
}
static bool read_u16(const std::vector<uint8_t>& bc, size_t& ip, uint16_t& out) {
    if (ip+2 > bc.size()) return false;
    out = (uint16_t)(bc[ip] | ((uint16_t)bc[ip+1]<<8)); ip+=2; return true;
}
static bool read_i32(const std::vector<uint8_t>& bc, size_t& ip, int32_t& out) {
    if (ip+4 > bc.size()) return false;
    out = (int32_t)((uint32_t)bc[ip] | ((uint32_t)bc[ip+1]<<8) |
                    ((uint32_t)bc[ip+2]<<16) | ((uint32_t)bc[ip+3]<<24));
    ip+=4; return true;
}
static bool read_f64(const std::vector<uint8_t>& bc, size_t& ip, double& out) {
    if (ip+8 > bc.size()) return false;
    uint64_t bits=0;
    for (int i=0;i<8;i++) bits|=((uint64_t)bc[ip+i])<<(i*8);
    memcpy(&out,&bits,8); ip+=8; return true;
}
static bool read_str(const std::vector<uint8_t>& bc, size_t& ip,
                     char* buf, size_t bufsz) {
    uint8_t len; if (!read_u8(bc,ip,len)) return false;
    if (ip+len > bc.size() || len > bufsz-1) return false;
    memcpy(buf, &bc[ip], len); buf[len]='\0'; ip+=len; return true;
}
static bool is_truthy(const VarSlot& v) {
    switch (v.type) {
        case VAR_INT:  return v.ival != 0;
        case VAR_BOOL: return v.ival != 0;
        case VAR_STR:  return v.sval[0] != '\0';
        default:       return false;
    }
}

#ifndef NSA_NO_LOOP_LIMIT
static const uint32_t MAX_BACK_JUMPS = 10000000UL;
#endif

#define RT_ERR(msg) \
    do{fprintf(stderr,"%s: runtime error at offset %zu: %s\n",prog,ip-1,msg);return 1;}while(0)
#define RT_ERR1(fmt,a) \
    do{fprintf(stderr,"%s: runtime error at offset %zu: " fmt "\n",prog,ip-1,a);return 1;}while(0)
#define RT_ERR2(fmt,a,b) \
    do{fprintf(stderr,"%s: runtime error at offset %zu: " fmt "\n",prog,ip-1,a,b);return 1;}while(0)

/* VAR(id): resolve to local (in func) or global (top level) */
#define VAR(id) (call_depth>0 ? call_stack[call_depth-1].locals[(id)] : vars[(id)])

#define CHECK_ID(id,who) do{ \
    if(call_depth>0){ if((id)>=NSA_MAX_LOCALS) RT_ERR1(who ": local id %u out of range",(unsigned)(id)); } \
    else{ if((int)(id)>=sym_count) RT_ERR1(who ": var id %u out of range",(unsigned)(id)); } \
}while(0)
#define NEED_INT(id,who)   if(VAR(id).type!=VAR_INT)   RT_ERR1(who ": var %u is not an integer",(unsigned)(id))
#define NEED_STR(id,who)   if(VAR(id).type!=VAR_STR)   RT_ERR1(who ": var %u is not a string",(unsigned)(id))
#define NEED_FLOAT(id,who) if(VAR(id).type!=VAR_FLOAT) RT_ERR1(who ": var %u is not a float",(unsigned)(id))

/* Array bounds check — base_id is always in global vars[] regardless of call depth.
   Element slots are base_id+1 .. base_id+size.
   The descriptor slot (base_id) holds ival = declared size.                         */
#define ARR_CHECK(base,idx,who) do{ \
    if((int)(base)>=sym_count) RT_ERR1(who ": array base id %u out of range",(unsigned)(base)); \
    if(vars[(base)].type!=VAR_INT) RT_ERR(who ": array slot not initialized"); \
    int32_t _sz=vars[(base)].ival; \
    if((idx)<0||(idx)>=_sz) { \
        fprintf(stderr,"%s: runtime error at offset %zu: " who ": index %d out of bounds (size %d)\n",\
                prog,ip-1,(int)(idx),(int)_sz); return 1; } \
    if((int)(base)+1+(idx)>=sym_count) RT_ERR(who ": element slot out of range"); \
}while(0)

int run(const std::vector<uint8_t>& bc, int sym_count, const char* prog) {
    if (sym_count<0 || sym_count>(int)NSA_MAX_VARS) {
        fprintf(stderr,"%s: runtime error: invalid sym_count %d\n",prog,sym_count); return 1;
    }

    /* Static to avoid stack overflow on NusaOS (1.1 MB total).
       Safe because nsa is single-threaded and run() is not recursive. */
    static VarSlot   vars[NSA_MAX_VARS];
    /* File handle table — maps fd_slot index → POSIX file descriptor */
    static int       open_fds[NSA_MAX_VARS];
    static bool      fd_init_done;
    if (!fd_init_done) {
        for (int i=0;i<NSA_MAX_VARS;i++) open_fds[i]=-1;
        fd_init_done=true;
    }
    static CallFrame call_stack[NSA_MAX_CALL_DEPTH];
    memset(vars,       0, sizeof(vars));
    memset(call_stack, 0, sizeof(call_stack));
    for (int i=0;i<sym_count;i++) vars[i].type=VAR_UNSET;

    /* ── Syscall buffer pool ─────────────────────────────────────────────
     * sysbuf allocates raw heap buffers tracked here; freed on exit.
     * Max NSA_MAX_SYSBUFS live buffers at once.                        */
    static const int NSA_MAX_SYSBUFS = 32;
    static void*  sysbuf_ptr[NSA_MAX_SYSBUFS];
    static size_t sysbuf_len[NSA_MAX_SYSBUFS];
    static int    sysbuf_count;
    /* reset pool each run */
    for (int i=0;i<sysbuf_count;i++) { free(sysbuf_ptr[i]); sysbuf_ptr[i]=nullptr; }
    sysbuf_count = 0;

    int    call_depth = 0;
    size_t ip         = 0;
#ifndef NSA_NO_LOOP_LIMIT
    uint32_t back_jump_count = 0;
#endif

    while (ip < bc.size()) {
        uint8_t op = bc[ip++];

/* SC_ARG: decode syscall-style argument (flags byte + data).
 * flags: 0x00=var_id(1B)  0x01=i32_imm(4B)  0x02=literal_zero
 * Available to all opcodes in this switch.                     */
#define SC_ARG(out_val, label) do { \
    uint8_t _fl; \
    if (!read_u8(bc,ip,_fl)) RT_ERR(label ": truncated flags"); \
    if (_fl == 0x02) { (out_val) = 0; } \
    else if (_fl == 0x01) { \
        int32_t _v; \
        if (!read_i32(bc,ip,_v)) RT_ERR(label ": truncated imm"); \
        (out_val) = (int)_v; \
    } else { \
        uint8_t _id; \
        if (!read_u8(bc,ip,_id)) RT_ERR(label ": truncated var id"); \
        CHECK_ID(_id, label); \
        (out_val) = (int)VAR(_id).ival; \
    } \
} while(0)

        switch ((NsaOpcode)op) {

        case OP_NOP:  break;
        case OP_HALT: return 0;

        /* ── I/O ─────────────────────────────────────────────────────── */
        case OP_PRINT_STR: {
            char buf[NSA_MAX_STR_LEN+1];
            if (!read_str(bc,ip,buf,sizeof(buf))) RT_ERR("PRINT_STR: truncated");
            printf("%s\n",buf); break;
        }
        case OP_PRINT_STR_NL: {
            char buf[NSA_MAX_STR_LEN+1];
            if (!read_str(bc,ip,buf,sizeof(buf))) RT_ERR("PRINT_STR_NL: truncated");
            printf("%s",buf); break;
        }
        case OP_PRINT_VAR: {
            uint8_t id; if (!read_u8(bc,ip,id)) RT_ERR("PRINT_VAR: truncated");
            CHECK_ID(id,"PRINT_VAR");
            switch (VAR(id).type) {
                case VAR_INT:  printf("%d\n", VAR(id).ival); break;
                case VAR_STR:  printf("%s\n", VAR(id).sval); break;
                case VAR_BOOL: printf("%s\n", VAR(id).ival?"true":"false"); break;
                default:       printf("(unset)\n"); break;
            }
            break;
        }
        case OP_PRINT_VAR_NL: {
            uint8_t id; if (!read_u8(bc,ip,id)) RT_ERR("PRINT_VAR_NL: truncated");
            CHECK_ID(id,"PRINT_VAR_NL");
            switch (VAR(id).type) {
                case VAR_INT:  printf("%d", VAR(id).ival); break;
                case VAR_STR:  printf("%s", VAR(id).sval); break;
                case VAR_BOOL: printf("%s", VAR(id).ival?"true":"false"); break;
                default:       printf("(unset)"); break;
            }
            break;
        }
        case OP_INPUT_INT: {
            uint8_t id; if (!read_u8(bc,ip,id)) RT_ERR("INPUT_INT: truncated");
            CHECK_ID(id,"INPUT_INT"); fflush(stdout);
            long v=0; char line[64];
            if (fgets(line,sizeof(line),stdin)) { char* e; v=strtol(line,&e,10); if(e==line) v=0; }
            VAR(id).type=VAR_INT; VAR(id).ival=(int32_t)v; break;
        }
        case OP_INPUT_STR: {
            uint8_t id; if (!read_u8(bc,ip,id)) RT_ERR("INPUT_STR: truncated");
            CHECK_ID(id,"INPUT_STR"); fflush(stdout);
            VAR(id).type=VAR_STR; memset(VAR(id).sval,0,sizeof(VAR(id).sval));
            if (fgets(VAR(id).sval,(int)sizeof(VAR(id).sval),stdin)) {
                size_t l=strlen(VAR(id).sval);
                if (l>0 && VAR(id).sval[l-1]=='\n') VAR(id).sval[l-1]='\0';
            }
            break;
        }

        /* ── Variables ───────────────────────────────────────────────── */
        case OP_SET_STR: {
            uint8_t id; if (!read_u8(bc,ip,id)) RT_ERR("SET_STR: truncated id");
            CHECK_ID(id,"SET_STR");
            char buf[NSA_MAX_STR_LEN+1];
            if (!read_str(bc,ip,buf,sizeof(buf))) RT_ERR("SET_STR: truncated data");
            VAR(id).type=VAR_STR; memset(VAR(id).sval,0,sizeof(VAR(id).sval));
            strncpy(VAR(id).sval,buf,NSA_MAX_STR_LEN); break;
        }
        case OP_SET_INT: {
            uint8_t id; int32_t val;
            if (!read_u8(bc,ip,id))  RT_ERR("SET_INT: truncated id");
            CHECK_ID(id,"SET_INT");
            if (!read_i32(bc,ip,val)) RT_ERR("SET_INT: truncated value");
            VAR(id).type=VAR_INT; VAR(id).ival=val; break;
        }
        case OP_SET_BOOL: {
            uint8_t id,val;
            if (!read_u8(bc,ip,id))  RT_ERR("SET_BOOL: truncated id");
            CHECK_ID(id,"SET_BOOL");
            if (!read_u8(bc,ip,val)) RT_ERR("SET_BOOL: truncated value");
            VAR(id).type=VAR_BOOL; VAR(id).ival=val?1:0; break;
        }
        case OP_COPY: {
            uint8_t dst,src;
            if (!read_u8(bc,ip,dst)) RT_ERR("COPY: truncated dst");
            if (!read_u8(bc,ip,src)) RT_ERR("COPY: truncated src");
            CHECK_ID(dst,"COPY dst"); CHECK_ID(src,"COPY src");
            VAR(dst)=VAR(src); break;
        }

        /* ── Arithmetic IMM ──────────────────────────────────────────── */
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
        case OP_DIV_IMM: case OP_MOD_IMM: {
            uint8_t id; int32_t imm;
            if (!read_u8(bc,ip,id))   RT_ERR("ARITH_IMM: truncated id");
            CHECK_ID(id,"ARITH_IMM"); NEED_INT(id,"ARITH_IMM");
            if (!read_i32(bc,ip,imm)) RT_ERR("ARITH_IMM: truncated imm");
            if ((op==OP_DIV_IMM||op==OP_MOD_IMM) && imm==0) RT_ERR("division/modulo by zero");
            switch ((NsaOpcode)op) {
                case OP_ADD_IMM: VAR(id).ival+=imm; break;
                case OP_SUB_IMM: VAR(id).ival-=imm; break;
                case OP_MUL_IMM: VAR(id).ival*=imm; break;
                case OP_DIV_IMM: VAR(id).ival/=imm; break;
                case OP_MOD_IMM: VAR(id).ival%=imm; break;
                default: break;
            }
            break;
        }

        /* ── Arithmetic VAR ──────────────────────────────────────────── */
        case OP_ADD_VAR: case OP_SUB_VAR: case OP_MUL_VAR:
        case OP_DIV_VAR: case OP_MOD_VAR: {
            uint8_t dst,src;
            if (!read_u8(bc,ip,dst)) RT_ERR("ARITH_VAR: truncated dst");
            if (!read_u8(bc,ip,src)) RT_ERR("ARITH_VAR: truncated src");
            CHECK_ID(dst,"ARITH_VAR dst"); NEED_INT(dst,"ARITH_VAR dst");
            CHECK_ID(src,"ARITH_VAR src"); NEED_INT(src,"ARITH_VAR src");
            if ((op==OP_DIV_VAR||op==OP_MOD_VAR) && VAR(src).ival==0) RT_ERR("division/modulo by zero");
            switch ((NsaOpcode)op) {
                case OP_ADD_VAR: VAR(dst).ival+=VAR(src).ival; break;
                case OP_SUB_VAR: VAR(dst).ival-=VAR(src).ival; break;
                case OP_MUL_VAR: VAR(dst).ival*=VAR(src).ival; break;
                case OP_DIV_VAR: VAR(dst).ival/=VAR(src).ival; break;
                case OP_MOD_VAR: VAR(dst).ival%=VAR(src).ival; break;
                default: break;
            }
            break;
        }

        /* ── Unary ───────────────────────────────────────────────────── */
        case OP_INC: { uint8_t id; if(!read_u8(bc,ip,id))RT_ERR("INC: truncated");
            CHECK_ID(id,"INC"); NEED_INT(id,"INC"); VAR(id).ival++; break; }
        case OP_DEC: { uint8_t id; if(!read_u8(bc,ip,id))RT_ERR("DEC: truncated");
            CHECK_ID(id,"DEC"); NEED_INT(id,"DEC"); VAR(id).ival--; break; }
        case OP_NOT: { uint8_t id; if(!read_u8(bc,ip,id))RT_ERR("NOT: truncated");
            CHECK_ID(id,"NOT");
            if (VAR(id).type==VAR_INT||VAR(id).type==VAR_BOOL) VAR(id).ival=VAR(id).ival?0:1;
            else RT_ERR("NOT: not int or bool");
            break; }
        case OP_NEG: { uint8_t id; if(!read_u8(bc,ip,id))RT_ERR("NEG: truncated");
            CHECK_ID(id,"NEG"); NEED_INT(id,"NEG"); VAR(id).ival=-VAR(id).ival; break; }

        /* ── String ops ──────────────────────────────────────────────── */
        case OP_CONCAT: {
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst))RT_ERR("CONCAT: truncated dst");
            if(!read_u8(bc,ip,src))RT_ERR("CONCAT: truncated src");
            CHECK_ID(dst,"CONCAT dst"); NEED_STR(dst,"CONCAT dst");
            CHECK_ID(src,"CONCAT src"); NEED_STR(src,"CONCAT src");
            size_t dl=strlen(VAR(dst).sval), sl=strlen(VAR(src).sval);
            if (dl+sl>NSA_MAX_STR_LEN) sl=NSA_MAX_STR_LEN-dl;
            strncat(VAR(dst).sval,VAR(src).sval,sl); break;
        }
        case OP_CONCAT_LIT: {
            uint8_t dst; if(!read_u8(bc,ip,dst))RT_ERR("CONCAT_LIT: truncated dst");
            CHECK_ID(dst,"CONCAT_LIT"); NEED_STR(dst,"CONCAT_LIT");
            char buf[NSA_MAX_STR_LEN+1];
            if(!read_str(bc,ip,buf,sizeof(buf)))RT_ERR("CONCAT_LIT: truncated literal");
            size_t dl=strlen(VAR(dst).sval), sl=strlen(buf);
            if (dl+sl>NSA_MAX_STR_LEN) sl=NSA_MAX_STR_LEN-dl;
            strncat(VAR(dst).sval,buf,sl); break;
        }
        case OP_LEN: {
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst))RT_ERR("LEN: truncated dst");
            if(!read_u8(bc,ip,src))RT_ERR("LEN: truncated src");
            CHECK_ID(dst,"LEN dst"); CHECK_ID(src,"LEN src"); NEED_STR(src,"LEN src");
            VAR(dst).type=VAR_INT; VAR(dst).ival=(int32_t)strlen(VAR(src).sval); break;
        }
        case OP_STR_TO_INT: {
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst))RT_ERR("STR_TO_INT: truncated dst");
            if(!read_u8(bc,ip,src))RT_ERR("STR_TO_INT: truncated src");
            CHECK_ID(dst,"STR_TO_INT dst"); CHECK_ID(src,"STR_TO_INT src"); NEED_STR(src,"STR_TO_INT src");
            char* e; long v=strtol(VAR(src).sval,&e,10);
            VAR(dst).type=VAR_INT; VAR(dst).ival=(int32_t)v; break;
        }
        case OP_INT_TO_STR: {
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst))RT_ERR("INT_TO_STR: truncated dst");
            if(!read_u8(bc,ip,src))RT_ERR("INT_TO_STR: truncated src");
            CHECK_ID(dst,"INT_TO_STR dst"); CHECK_ID(src,"INT_TO_STR src"); NEED_INT(src,"INT_TO_STR src");
            VAR(dst).type=VAR_STR;
            snprintf(VAR(dst).sval,sizeof(VAR(dst).sval),"%d",VAR(src).ival); break;
        }

        /* ── String comparison (v2.2) ────────────────────────────────── */
        case OP_SCMP_EQ: case OP_SCMP_NE: {
            uint8_t dst,va,vb;
            if(!read_u8(bc,ip,dst))RT_ERR("SCMP: truncated dst");
            if(!read_u8(bc,ip,va)) RT_ERR("SCMP: truncated a");
            if(!read_u8(bc,ip,vb)) RT_ERR("SCMP: truncated b");
            CHECK_ID(dst,"SCMP dst"); CHECK_ID(va,"SCMP a"); CHECK_ID(vb,"SCMP b");
            NEED_STR(va,"SCMP a"); NEED_STR(vb,"SCMP b");
            bool eq = (strcmp(VAR(va).sval, VAR(vb).sval)==0);
            VAR(dst).type=VAR_BOOL;
            VAR(dst).ival = (op==OP_SCMP_EQ) ? (eq?1:0) : (eq?0:1);
            break;
        }

        /* ── Integer comparison ──────────────────────────────────────── */
        case OP_CMP_EQ: case OP_CMP_NE: case OP_CMP_LT:
        case OP_CMP_GT: case OP_CMP_LE: case OP_CMP_GE: {
            uint8_t dst,va,vb;
            if(!read_u8(bc,ip,dst))RT_ERR("CMP: truncated dst");
            if(!read_u8(bc,ip,va)) RT_ERR("CMP: truncated a");
            if(!read_u8(bc,ip,vb)) RT_ERR("CMP: truncated b");
            CHECK_ID(dst,"CMP dst"); CHECK_ID(va,"CMP a"); CHECK_ID(vb,"CMP b");
            NEED_INT(va,"CMP a"); NEED_INT(vb,"CMP b");
            int32_t a=VAR(va).ival, b=VAR(vb).ival; bool res;
            switch ((NsaOpcode)op) {
                case OP_CMP_EQ: res=(a==b); break; case OP_CMP_NE: res=(a!=b); break;
                case OP_CMP_LT: res=(a<b);  break; case OP_CMP_GT: res=(a>b);  break;
                case OP_CMP_LE: res=(a<=b); break; case OP_CMP_GE: res=(a>=b); break;
                default: res=false; break;
            }
            VAR(dst).type=VAR_BOOL; VAR(dst).ival=res?1:0; break;
        }

        /* ── Logical ─────────────────────────────────────────────────── */
        case OP_AND: case OP_OR: {
            uint8_t dst,va,vb;
            if(!read_u8(bc,ip,dst))RT_ERR("LOGICAL: truncated dst");
            if(!read_u8(bc,ip,va)) RT_ERR("LOGICAL: truncated a");
            if(!read_u8(bc,ip,vb)) RT_ERR("LOGICAL: truncated b");
            CHECK_ID(dst,"LOGICAL dst"); CHECK_ID(va,"LOGICAL a"); CHECK_ID(vb,"LOGICAL b");
            bool a=is_truthy(VAR(va)), b=is_truthy(VAR(vb));
            VAR(dst).type=VAR_BOOL;
            VAR(dst).ival=(op==OP_AND)?(a&&b?1:0):(a||b?1:0); break;
        }

        /* ── Forward jumps ───────────────────────────────────────────── */
        case OP_JMP_FWD: {
            uint16_t off; if(!read_u16(bc,ip,off))RT_ERR("JMP_FWD: truncated");
            if(ip+off>bc.size())RT_ERR("JMP_FWD: past end");
            ip+=off; break;
        }
        case OP_JMP_IF_TRUE: case OP_JMP_IF_FALSE: {
            uint8_t id; uint16_t off;
            if(!read_u8(bc,ip,id))  RT_ERR("JMP_IF: truncated id");
            CHECK_ID(id,"JMP_IF");
            if(!read_u16(bc,ip,off))RT_ERR("JMP_IF: truncated offset");
            bool take=(op==OP_JMP_IF_TRUE)?is_truthy(VAR(id)):!is_truthy(VAR(id));
            if(take){ if(ip+off>bc.size())RT_ERR("JMP_IF: past end"); ip+=off; }
            break;
        }
        case OP_JMP_IF_EQ: case OP_JMP_IF_NE:
        case OP_JMP_IF_LT: case OP_JMP_IF_GT:
        case OP_JMP_IF_LE: case OP_JMP_IF_GE: {
            uint8_t id; int32_t cmp; uint16_t off;
            if(!read_u8(bc,ip,id))   RT_ERR("JMP_CMP: truncated id");
            CHECK_ID(id,"JMP_CMP");
            if(!read_i32(bc,ip,cmp)) RT_ERR("JMP_CMP: truncated cmp");
            if(!read_u16(bc,ip,off)) RT_ERR("JMP_CMP: truncated offset");
            int32_t val=(VAR(id).type==VAR_INT)?VAR(id).ival:0; bool take;
            switch((NsaOpcode)op){
                case OP_JMP_IF_EQ:take=(val==cmp);break; case OP_JMP_IF_NE:take=(val!=cmp);break;
                case OP_JMP_IF_LT:take=(val<cmp); break; case OP_JMP_IF_GT:take=(val>cmp); break;
                case OP_JMP_IF_LE:take=(val<=cmp);break; case OP_JMP_IF_GE:take=(val>=cmp);break;
                default:take=false;break;
            }
            if(take){ if(ip+off>bc.size())RT_ERR("JMP_CMP: past end"); ip+=off; }
            break;
        }

        /* ── Backward jumps ──────────────────────────────────────────── */
        case OP_JMP_BACK: {
            uint16_t off; if(!read_u16(bc,ip,off))RT_ERR("JMP_BACK: truncated");
#ifndef NSA_NO_LOOP_LIMIT
            if(++back_jump_count>MAX_BACK_JUMPS)RT_ERR("infinite loop detected (limit 10M)");
#endif
            if(off>ip)RT_ERR("JMP_BACK: underflow"); ip-=off; break;
        }
        case OP_JMP_BACK_TRUE: case OP_JMP_BACK_FALSE: {
            uint8_t id; uint16_t off;
            if(!read_u8(bc,ip,id))  RT_ERR("JMP_BACK_T/F: truncated id");
            CHECK_ID(id,"JMP_BACK_T/F");
            if(!read_u16(bc,ip,off))RT_ERR("JMP_BACK_T/F: truncated offset");
            bool take=(op==OP_JMP_BACK_TRUE)?is_truthy(VAR(id)):!is_truthy(VAR(id));
            if(take){
#ifndef NSA_NO_LOOP_LIMIT
                if(++back_jump_count>MAX_BACK_JUMPS)RT_ERR("infinite loop detected");
#endif
                if(off>ip)RT_ERR("JMP_BACK_T/F: underflow"); ip-=off;
            }
            break;
        }
        case OP_JMP_BACK_NZ: case OP_JMP_BACK_Z: {
            uint8_t id; uint16_t off;
            if(!read_u8(bc,ip,id))  RT_ERR("JMP_BACK_NZ/Z: truncated id");
            CHECK_ID(id,"JMP_BACK_NZ/Z");
            if(!read_u16(bc,ip,off))RT_ERR("JMP_BACK_NZ/Z: truncated offset");
            int32_t val=(VAR(id).type==VAR_INT||VAR(id).type==VAR_BOOL)?VAR(id).ival:0;
            bool take=(op==OP_JMP_BACK_NZ)?(val!=0):(val==0);
            if(take){
#ifndef NSA_NO_LOOP_LIMIT
                if(++back_jump_count>MAX_BACK_JUMPS)RT_ERR("infinite loop detected");
#endif
                if(off>ip)RT_ERR("JMP_BACK_NZ/Z: underflow"); ip-=off;
            }
            break;
        }

        /* ── Functions ───────────────────────────────────────────────── */
        case OP_LOAD_ARG: {
            uint8_t local_id, global_id;
            if(!read_u8(bc,ip,local_id)) RT_ERR("LOAD_ARG: truncated local_id");
            if(!read_u8(bc,ip,global_id))RT_ERR("LOAD_ARG: truncated global_id");
            if(local_id>=NSA_MAX_LOCALS)        RT_ERR("LOAD_ARG: local_id out of range");
            if((int)global_id>=sym_count)       RT_ERR("LOAD_ARG: global_id out of range");
            if(call_depth>=NSA_MAX_CALL_DEPTH)  RT_ERR("LOAD_ARG: call stack overflow");
            call_stack[call_depth].locals[local_id]=vars[global_id]; break;
        }
        case OP_CALL: {
            uint16_t addr; if(!read_u16(bc,ip,addr))RT_ERR("CALL: truncated address");
            if(call_depth>=NSA_MAX_CALL_DEPTH)RT_ERR("call stack overflow (max depth 64)");
            call_stack[call_depth].ret_ip=ip;
            call_depth++; ip=addr; break;
        }
        case OP_RET: {
            if(call_depth<=0)RT_ERR("RET outside of function");
            call_depth--;
            ip=call_stack[call_depth].ret_ip; break;
        }
        case OP_STORE_RET: {
            /* STORE_RET  dst_id  callee_local_id
             *
             * Copies the return value from the callee's frame into the
             * destination slot of the CURRENT (caller) scope.
             *
             * call_stack[call_depth] still holds the callee's locals
             * (they are not zeroed on RET — only call_depth was decremented).
             *
             * The destination slot must be written into the caller's scope:
             *   - if inside a function (call_depth > 0): write to
             *     call_stack[call_depth-1].locals[dst_id]
             *   - if at top level (call_depth == 0):    write to vars[dst_id]
             *
             * This fixes the nested NSS call bug where outer() calls inner():
             * outer runs at call_depth=1, so its locals live in
             * call_stack[0].locals — vars[] is NOT outer's frame.
             */
            uint8_t dst_id, callee_local_id;
            if(!read_u8(bc,ip,dst_id))         RT_ERR("STORE_RET: truncated dst_id");
            if(!read_u8(bc,ip,callee_local_id)) RT_ERR("STORE_RET: truncated callee_local_id");
            if(callee_local_id>=NSA_MAX_LOCALS) RT_ERR("STORE_RET: callee_local_id out of range");
            VarSlot ret_val = call_stack[call_depth].locals[callee_local_id];
            if (call_depth > 0) {
                if(dst_id>=NSA_MAX_LOCALS) RT_ERR("STORE_RET: dst_id out of range (local)");
                call_stack[call_depth-1].locals[dst_id] = ret_val;
            } else {
                if((int)dst_id>=sym_count) RT_ERR("STORE_RET: dst_id out of range (global)");
                vars[dst_id] = ret_val;
            }
            break;
        }

        /* ── Arrays (v2.2) ───────────────────────────────────────────── */

        /* OP_ARR_GET  dst  base_id  idx_var
           Loads vars[base_id + 1 + idx] into VAR(dst).                */
        case OP_ARR_GET: {
            uint8_t dst, base, idx_id;
            if(!read_u8(bc,ip,dst))   RT_ERR("ARR_GET: truncated dst");
            if(!read_u8(bc,ip,base))  RT_ERR("ARR_GET: truncated base");
            if(!read_u8(bc,ip,idx_id))RT_ERR("ARR_GET: truncated idx");
            CHECK_ID(dst,"ARR_GET dst");
            if((int)base>=sym_count)  RT_ERR("ARR_GET: base out of range");
            CHECK_ID(idx_id,"ARR_GET idx"); NEED_INT(idx_id,"ARR_GET idx");
            int32_t idx=VAR(idx_id).ival;
            ARR_CHECK(base,idx,"ARR_GET");
            uint8_t elem_slot=(uint8_t)(base+1+idx);
            VAR(dst)=vars[elem_slot]; break;
        }

        /* OP_ARR_SET  base_id  idx_var  src_var
           Stores VAR(src) into vars[base_id + 1 + idx].               */
        case OP_ARR_SET: {
            uint8_t base, idx_id, src;
            if(!read_u8(bc,ip,base))  RT_ERR("ARR_SET: truncated base");
            if(!read_u8(bc,ip,idx_id))RT_ERR("ARR_SET: truncated idx");
            if(!read_u8(bc,ip,src))   RT_ERR("ARR_SET: truncated src");
            if((int)base>=sym_count)  RT_ERR("ARR_SET: base out of range");
            CHECK_ID(idx_id,"ARR_SET idx"); NEED_INT(idx_id,"ARR_SET idx");
            CHECK_ID(src,"ARR_SET src");
            int32_t idx=VAR(idx_id).ival;
            ARR_CHECK(base,idx,"ARR_SET");
            uint8_t elem_slot=(uint8_t)(base+1+idx);
            vars[elem_slot]=VAR(src); break;
        }

        /* OP_ARR_SET_IMM  base_id  idx_var  type  value...
           type: 0x01=int(i32), 0x02=str(len+bytes), 0x03=bool(u8)    */
        case OP_ARR_SET_IMM: {
            uint8_t base, idx_id, vtype;
            if(!read_u8(bc,ip,base))  RT_ERR("ARR_SET_IMM: truncated base");
            if(!read_u8(bc,ip,idx_id))RT_ERR("ARR_SET_IMM: truncated idx");
            if(!read_u8(bc,ip,vtype)) RT_ERR("ARR_SET_IMM: truncated vtype");
            if((int)base>=sym_count)  RT_ERR("ARR_SET_IMM: base out of range");
            CHECK_ID(idx_id,"ARR_SET_IMM idx"); NEED_INT(idx_id,"ARR_SET_IMM idx");
            int32_t idx=VAR(idx_id).ival;
            ARR_CHECK(base,idx,"ARR_SET_IMM");
            uint8_t elem_slot=(uint8_t)(base+1+idx);
            if (vtype==0x01) {
                int32_t v; if(!read_i32(bc,ip,v))RT_ERR("ARR_SET_IMM: truncated int");
                vars[elem_slot].type=VAR_INT; vars[elem_slot].ival=v;
            } else if (vtype==0x02) {
                char buf[NSA_MAX_STR_LEN+1];
                if(!read_str(bc,ip,buf,sizeof(buf)))RT_ERR("ARR_SET_IMM: truncated str");
                vars[elem_slot].type=VAR_STR;
                memset(vars[elem_slot].sval,0,sizeof(vars[elem_slot].sval));
                strncpy(vars[elem_slot].sval,buf,NSA_MAX_STR_LEN);
            } else if (vtype==0x03) {
                uint8_t v; if(!read_u8(bc,ip,v))RT_ERR("ARR_SET_IMM: truncated bool");
                vars[elem_slot].type=VAR_BOOL; vars[elem_slot].ival=v?1:0;
            } else { RT_ERR1("ARR_SET_IMM: unknown value type 0x%02x",(unsigned)vtype); }
            break;
        }

        /* OP_ARR_LEN  dst_int  base_id
           Stores the declared size (vars[base_id].ival) into VAR(dst). */
        case OP_ARR_LEN: {
            uint8_t dst, base;
            if(!read_u8(bc,ip,dst)) RT_ERR("ARR_LEN: truncated dst");
            if(!read_u8(bc,ip,base))RT_ERR("ARR_LEN: truncated base");
            CHECK_ID(dst,"ARR_LEN dst");
            if((int)base>=sym_count)RT_ERR("ARR_LEN: base out of range");
            if(vars[base].type!=VAR_INT)RT_ERR("ARR_LEN: array not initialized");
            VAR(dst).type=VAR_INT; VAR(dst).ival=vars[base].ival; break;
        }


        /* ── Floating point (v2.5) ───────────────────────────────────────── */

        /* OP_SET_FLOAT  dst  f64(8 bytes LE) */
        case OP_SET_FLOAT: {
            uint8_t dst; double d;
            if(!read_u8(bc,ip,dst))  RT_ERR("SET_FLOAT: truncated dst");
            if(!read_f64(bc,ip,d))   RT_ERR("SET_FLOAT: truncated value");
            CHECK_ID(dst,"SET_FLOAT");
            VAR(dst).type=VAR_FLOAT; VAR(dst).dval=d; break;
        }
        /* OP_FADD/FSUB/FMUL/FDIV  dst  a  b */
        case OP_FADD: case OP_FSUB: case OP_FMUL: case OP_FDIV: {
            uint8_t dst,a,b;
            if(!read_u8(bc,ip,dst)) RT_ERR("FMATH: truncated dst");
            if(!read_u8(bc,ip,a))   RT_ERR("FMATH: truncated a");
            if(!read_u8(bc,ip,b))   RT_ERR("FMATH: truncated b");
            CHECK_ID(dst,"FMATH dst"); NEED_FLOAT(a,"FMATH a"); NEED_FLOAT(b,"FMATH b");
            double da=VAR(a).dval, db=VAR(b).dval, res=0;
            if      (op==OP_FADD) res=da+db;
            else if (op==OP_FSUB) res=da-db;
            else if (op==OP_FMUL) res=da*db;
            else {
                if(db==0.0) RT_ERR("FDIV: division by zero");
                res=da/db;
            }
            VAR(dst).type=VAR_FLOAT; VAR(dst).dval=res; break;
        }
        /* OP_FNEG  dst */
        case OP_FNEG: {
            uint8_t dst;
            if(!read_u8(bc,ip,dst)) RT_ERR("FNEG: truncated dst");
            NEED_FLOAT(dst,"FNEG"); VAR(dst).dval=-VAR(dst).dval; break;
        }
        /* OP_ITOF  dst  src_int */
        case OP_ITOF: {
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst)) RT_ERR("ITOF: truncated dst");
            if(!read_u8(bc,ip,src)) RT_ERR("ITOF: truncated src");
            CHECK_ID(dst,"ITOF dst"); NEED_INT(src,"ITOF src");
            VAR(dst).type=VAR_FLOAT; VAR(dst).dval=(double)VAR(src).ival; break;
        }
        /* OP_FTOI  dst  src_float */
        case OP_FTOI: {
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst)) RT_ERR("FTOI: truncated dst");
            if(!read_u8(bc,ip,src)) RT_ERR("FTOI: truncated src");
            CHECK_ID(dst,"FTOI dst"); NEED_FLOAT(src,"FTOI src");
            VAR(dst).type=VAR_INT; VAR(dst).ival=(int32_t)VAR(src).dval; break;
        }
        /* OP_FTOS  dst  src_float  → string with up to 6 decimals, trimmed */
        case OP_FTOS: {
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst)) RT_ERR("FTOS: truncated dst");
            if(!read_u8(bc,ip,src)) RT_ERR("FTOS: truncated src");
            CHECK_ID(dst,"FTOS dst"); NEED_FLOAT(src,"FTOS src");
            char buf[64];
            float_to_str(VAR(src).dval, buf, sizeof(buf));
            VAR(dst).type=VAR_STR;
            strncpy(VAR(dst).sval,buf,NSA_MAX_STR_LEN);
            VAR(dst).sval[NSA_MAX_STR_LEN]='\0';
            break;
        }
        /* OP_FCMP  dst_bool  a  op_byte  b */
        case OP_FCMP: {
            uint8_t dst,a,op_byte,b;
            if(!read_u8(bc,ip,dst))     RT_ERR("FCMP: truncated dst");
            if(!read_u8(bc,ip,a))       RT_ERR("FCMP: truncated a");
            if(!read_u8(bc,ip,op_byte)) RT_ERR("FCMP: truncated op");
            if(!read_u8(bc,ip,b))       RT_ERR("FCMP: truncated b");
            CHECK_ID(dst,"FCMP dst"); NEED_FLOAT(a,"FCMP a"); NEED_FLOAT(b,"FCMP b");
            double da=VAR(a).dval, db=VAR(b).dval; bool res=false;
            switch(op_byte){
                case 0: res=(da==db); break; case 1: res=(da!=db); break;
                case 2: res=(da< db); break; case 3: res=(da> db); break;
                case 4: res=(da<=db); break; case 5: res=(da>=db); break;
                default: RT_ERR("FCMP: invalid op_byte");
            }
            VAR(dst).type=VAR_BOOL; VAR(dst).ival=res?1:0; break;
        }
        /* OP_FPRINT / OP_FPRINT_NL  id  — print float var */
        case OP_FPRINT: case OP_FPRINT_NL: {
            uint8_t id;
            if(!read_u8(bc,ip,id)) RT_ERR("FPRINT: truncated id");
            CHECK_ID(id,"FPRINT"); NEED_FLOAT(id,"FPRINT");
            char buf[64]; float_to_str(VAR(id).dval, buf, sizeof(buf));
            if(op==OP_FPRINT_NL) printf("%s\n",buf);
            else                 printf("%s",buf);
            break;
        }

        /* ── String indexing (v2.5) ──────────────────────────────────────
         * OP_SGET  dst  src  idx  — get char at index → 1-char string      */
        case OP_SGET: {
            uint8_t dst,src,idx;
            if(!read_u8(bc,ip,dst)) RT_ERR("SGET: truncated dst");
            if(!read_u8(bc,ip,src)) RT_ERR("SGET: truncated src");
            if(!read_u8(bc,ip,idx)) RT_ERR("SGET: truncated idx");
            CHECK_ID(dst,"SGET dst"); NEED_STR(src,"SGET src");
            CHECK_ID(idx,"SGET idx"); NEED_INT(idx,"SGET idx");
            int32_t i=VAR(idx).ival;
            int slen=(int)strlen(VAR(src).sval);
            if(i<0||i>=slen) RT_ERR("SGET: index out of range");
            VAR(dst).type=VAR_STR;
            VAR(dst).sval[0]=VAR(src).sval[i];
            VAR(dst).sval[1]='\0';
            break;
        }
        /* OP_SSET  dst  idx  src  — replace char at index                  */
        case OP_SSET: {
            uint8_t dst,idx,src;
            if(!read_u8(bc,ip,dst)) RT_ERR("SSET: truncated dst");
            if(!read_u8(bc,ip,idx)) RT_ERR("SSET: truncated idx");
            if(!read_u8(bc,ip,src)) RT_ERR("SSET: truncated src");
            NEED_STR(dst,"SSET dst"); NEED_INT(idx,"SSET idx"); NEED_STR(src,"SSET src");
            int32_t i=VAR(idx).ival;
            int slen=(int)strlen(VAR(dst).sval);
            if(i<0||i>=slen) RT_ERR("SSET: index out of range");
            if(VAR(src).sval[0]=='\0') RT_ERR("SSET: char string is empty");
            VAR(dst).sval[i]=VAR(src).sval[0];
            break;
        }
        /* OP_SSUB  dst  src  start  len  — extract substring               */
        case OP_SSUB: {
            uint8_t dst,src,start_id,len_id;
            if(!read_u8(bc,ip,dst))     RT_ERR("SSUB: truncated dst");
            if(!read_u8(bc,ip,src))     RT_ERR("SSUB: truncated src");
            if(!read_u8(bc,ip,start_id))RT_ERR("SSUB: truncated start");
            if(!read_u8(bc,ip,len_id))  RT_ERR("SSUB: truncated len");
            CHECK_ID(dst,"SSUB dst"); NEED_STR(src,"SSUB src");
            NEED_INT(start_id,"SSUB start"); NEED_INT(len_id,"SSUB len");
            int32_t start=VAR(start_id).ival;
            int32_t len  =VAR(len_id).ival;
            int     slen =(int)strlen(VAR(src).sval);
            if(start<0||start>=slen) RT_ERR("SSUB: start out of range");
            if(len<0) RT_ERR("SSUB: negative length");
            if(start+len>slen) len=slen-start;
            if(len>(int)NSA_MAX_STR_LEN) len=(int)NSA_MAX_STR_LEN;
            VAR(dst).type=VAR_STR;
            memcpy(VAR(dst).sval, VAR(src).sval+start, (size_t)len);
            VAR(dst).sval[len]='\0';
            break;
        }

        /* ── File I/O (v2.5) ────────────────────────────────────────────── */

        /* OP_FOPEN  fd_var  path_var  mode_var                             */
        case OP_FOPEN: {
            uint8_t fd_id, path_id, mode_id;
            if(!read_u8(bc,ip,fd_id))  RT_ERR("FOPEN: truncated fd");
            if(!read_u8(bc,ip,path_id))RT_ERR("FOPEN: truncated path");
            if(!read_u8(bc,ip,mode_id))RT_ERR("FOPEN: truncated mode");
            CHECK_ID(fd_id,"FOPEN fd");
            NEED_STR(path_id,"FOPEN path");
            NEED_STR(mode_id,"FOPEN mode");
            const char* path=VAR(path_id).sval;
            const char* mode=VAR(mode_id).sval;
            int flags=0, perm=0644;
            if      (strcmp(mode,"r")==0) flags=O_RDONLY;
            else if (strcmp(mode,"w")==0) flags=O_WRONLY|O_CREAT|O_TRUNC;
            else if (strcmp(mode,"a")==0) flags=O_WRONLY|O_CREAT|O_APPEND;
            else RT_ERR("FOPEN: invalid mode (use r/w/a)");
            int raw_fd=open(path,flags,(mode_t)perm);
            /* Store result as integer fd slot value; negative = error */
            VAR(fd_id).type=VAR_FILE;
            VAR(fd_id).ival=raw_fd;
            /* also track in open_fds table for cleanup */
            int slot_idx = (int)(call_depth>0
                ? (uint8_t*)(&VAR(fd_id)) - (uint8_t*)call_stack[call_depth-1].locals
                : fd_id);
            (void)slot_idx;
            /* raw_fd is stored directly in ival — enough for close/read/write */
            break;
        }
        /* OP_FCLOSE  fd_var                                                */
        case OP_FCLOSE: {
            uint8_t fd_id;
            if(!read_u8(bc,ip,fd_id)) RT_ERR("FCLOSE: truncated fd");
            CHECK_ID(fd_id,"FCLOSE fd");
            if(VAR(fd_id).type!=VAR_FILE) RT_ERR("FCLOSE: not a file handle");
            int raw_fd=VAR(fd_id).ival;
            if(raw_fd>=0) close(raw_fd);
            VAR(fd_id).ival=-1;
            break;
        }
        /* OP_FREAD  dst_str  fd_var  — read entire file into string        */
        case OP_FREAD: {
            uint8_t dst,fd_id;
            if(!read_u8(bc,ip,dst))   RT_ERR("FREAD: truncated dst");
            if(!read_u8(bc,ip,fd_id)) RT_ERR("FREAD: truncated fd");
            CHECK_ID(dst,"FREAD dst");
            if(VAR(fd_id).type!=VAR_FILE) RT_ERR("FREAD: not a file handle");
            int raw_fd=VAR(fd_id).ival;
            if(raw_fd<0) RT_ERR("FREAD: file not open");
            char buf[NSA_MAX_STR_LEN+1];
            memset(buf,0,sizeof(buf));
            ssize_t total=0;
            while(total<(ssize_t)NSA_MAX_STR_LEN) {
                ssize_t r=read(raw_fd, buf+total, NSA_MAX_STR_LEN-total);
                if(r<=0) break;
                total+=r;
            }
            buf[total]='\0';
            VAR(dst).type=VAR_STR;
            memcpy(VAR(dst).sval, buf, (size_t)total+1);
            break;
        }
        /* OP_FWRITE  fd_var  src_str                                       */
        case OP_FWRITE: {
            uint8_t fd_id, src;
            if(!read_u8(bc,ip,fd_id)) RT_ERR("FWRITE: truncated fd");
            if(!read_u8(bc,ip,src))   RT_ERR("FWRITE: truncated src");
            if(VAR(fd_id).type!=VAR_FILE) RT_ERR("FWRITE: not a file handle");
            NEED_STR(src,"FWRITE src");
            int raw_fd=VAR(fd_id).ival;
            if(raw_fd<0) RT_ERR("FWRITE: file not open");
            const char* s=VAR(src).sval;
            size_t slen=strlen(s);
            size_t written=0;
            while(written<slen){
                ssize_t w=write(raw_fd,s+written,slen-written);
                if(w<=0) break;
                written+=(size_t)w;
            }
            break;
        }
        /* OP_FEXISTS  dst_bool  path_var                                   */
        case OP_FEXISTS: {
            uint8_t dst, path_id;
            if(!read_u8(bc,ip,dst))    RT_ERR("FEXISTS: truncated dst");
            if(!read_u8(bc,ip,path_id))RT_ERR("FEXISTS: truncated path");
            CHECK_ID(dst,"FEXISTS dst");
            NEED_STR(path_id,"FEXISTS path");
            struct stat st; int r=stat(VAR(path_id).sval,&st);
            VAR(dst).type=VAR_BOOL;
            VAR(dst).ival=(r==0)?1:0;
            break;
        }



        /* ════════════════════════════════════════════════════════════════
         * PROCESS CONTROL  (v2.5.1)
         * ════════════════════════════════════════════════════════════════ */

        /* ── OP_FORK  dst ───────────────────────────────────────────────
         *
         * Calls SYS_FORK (= 2).
         * Parent: dst = child PID  (> 0)
         * Child:  dst = 0
         * Error:  dst = negative errno
         *
         * Typical pattern:
         *   fork pid
         *   let is_child = false
         *   cmp is_child pid == 0
         *   if is_child then
         *     // child code
         *     exit 0
         *   end
         *   // parent code
         *   waitpid ret pid
         * ────────────────────────────────────────────────────────────── */
        case OP_FORK: {
            uint8_t dst;
            if (!read_u8(bc,ip,dst)) RT_ERR("FORK: truncated dst");
            CHECK_ID(dst,"FORK dst");
            /* Save fork() return value to a stack-local variable BEFORE
             * writing to the heap (VAR array).  After fork(), child and
             * parent share CoW pages.  If the child writes VAR(dst)=0
             * before the CoW fault fires for the parent, the parent would
             * read 0 too.  A local (stack) variable is per-process and
             * is not subject to this race. */
            volatile int fork_ret = nsa_syscall(2, 0, 0, 0); /* SYS_FORK = 2 */
            VAR(dst).type = VAR_INT;
            VAR(dst).ival = fork_ret;
            break;
        }

        /* ── OP_EXEC  dst  path_id  argc  arg0_id ... argN_id ──────────
         *
         * Calls SYS_EXECVE (= 6).
         * Builds a char* argv[] array on the C stack and passes it to
         * the kernel.  envp is passed as NULL (kernel inherits parent env).
         *
         * On success: the calling process image is replaced — never returns.
         * On failure: dst = -errno (negative), VM continues.
         *
         * Note: after a successful exec in the child, the VM loop in the
         * child process is gone — the new program takes over completely.
         * ────────────────────────────────────────────────────────────── */
        case OP_EXEC: {
            uint8_t dst, path_id, argc;
            if (!read_u8(bc,ip,dst))     RT_ERR("EXEC: truncated dst");
            if (!read_u8(bc,ip,path_id)) RT_ERR("EXEC: truncated path");
            if (!read_u8(bc,ip,argc))    RT_ERR("EXEC: truncated argc");
            CHECK_ID(dst,     "EXEC dst");
            CHECK_ID(path_id, "EXEC path");
            NEED_STR(path_id, "EXEC path");
            if (argc > 16) RT_ERR("EXEC: too many arguments (max 16)");

            /* Read arg var ids */
            uint8_t arg_ids[16];
            for (int ai = 0; ai < (int)argc; ai++) {
                if (!read_u8(bc,ip,arg_ids[ai])) RT_ERR("EXEC: truncated arg id");
                CHECK_ID(arg_ids[ai],"EXEC arg");
                NEED_STR(arg_ids[ai],"EXEC arg");
            }

            /* Build argv + copy strings onto heap so they remain valid
             * after the kernel replaces our process image.
             *
             * Layout in one malloc block:
             *   [char* argv[argc+1]]          ← pointer array
             *   [char  arg0\0 arg1\0 ... ]    ← string data
             *
             * Total size = (argc+1)*sizeof(char*) + sum(strlen(arg)+1)
             */
            size_t ptr_block  = (size_t)(argc + 1) * sizeof(char*);
            size_t str_total  = 0;
            for (int ai = 0; ai < (int)argc; ai++)
                str_total += strlen(VAR(arg_ids[ai]).sval) + 1;

            char* exec_buf = (char*)malloc(ptr_block + str_total);
            if (!exec_buf) RT_ERR("EXEC: out of memory for argv");

            char** argv_arr  = (char**)exec_buf;
            char*  str_area  = exec_buf + ptr_block;

            for (int ai = 0; ai < (int)argc; ai++) {
                const char* s = VAR(arg_ids[ai]).sval;
                size_t slen   = strlen(s) + 1;
                memcpy(str_area, s, slen);
                argv_arr[ai] = str_area;
                str_area    += slen;
            }
            argv_arr[argc] = nullptr;

            /* Path also copied to heap */
            const char* src_path = VAR(path_id).sval;
            size_t path_len = strlen(src_path) + 1;
            char* exec_path = (char*)malloc(path_len);
            if (!exec_path) { free(exec_buf); RT_ERR("EXEC: out of memory for path"); }
            memcpy(exec_path, src_path, path_len);

            /* SYS_EXECVE = 6: execve(path, argv, envp=NULL)
             * On success: never returns — kernel replaces process image.
             * exec_buf and exec_path are intentionally NOT freed on success
             * because the process image is replaced entirely.
             * On failure: free and store errno.                           */
            int ret = nsa_syscall(6, (int)(uintptr_t)exec_path,
                                     (int)(uintptr_t)argv_arr,
                                     0);
            /* Only reaches here on failure */
            free(exec_buf);
            free(exec_path);
            VAR(dst).type = VAR_INT;
            VAR(dst).ival = ret; /* negative errno */
            break;
        }

        /* ── OP_WAITPID  dst  pid_id  [opts] ───────────────────────────
         *
         * Calls SYS_WAITPID (= 20): waitpid(pid, &status, opts).
         * dst = returned child PID on success, or -errno on error.
         * The exit status is discarded; use OP_SYSCALL with a sysbuf
         * int if you need the actual exit code.
         *
         * opts: 0 = block until child done (default)
         *       1 = WNOHANG (return immediately if no child exited)
         * ────────────────────────────────────────────────────────────── */
        case OP_WAITPID: {
            uint8_t dst, pid_id;
            if (!read_u8(bc,ip,dst))    RT_ERR("WAITPID: truncated dst");
            if (!read_u8(bc,ip,pid_id)) RT_ERR("WAITPID: truncated pid");
            CHECK_ID(dst,    "WAITPID dst");
            CHECK_ID(pid_id, "WAITPID pid");

            int opts; SC_ARG(opts,"WAITPID opts");

            int status = 0; /* we discard but need a valid address */
            int pid_val = (int)VAR(pid_id).ival;

            /* SYS_WAITPID = 20: waitpid(pid, &status, opts) */
            int ret = nsa_syscall(20, pid_val,
                                      (int)(uintptr_t)&status,
                                      opts);
            VAR(dst).type = VAR_INT;
            VAR(dst).ival = ret;
            break;
        }

        /* ── OP_EXIT  [code] ────────────────────────────────────────────
         *
         * Calls SYS_EXIT (= 1). Never returns.
         * Use in child after exec failure, or to exit the NSA program
         * with a specific exit code.
         * ────────────────────────────────────────────────────────────── */
        case OP_EXIT: {
            int code; SC_ARG(code,"EXIT code");
            /* SYS_EXIT = 1 */
            nsa_syscall(1, code, 0, 0);
            /* should never reach here, but silence compiler */
            return 0;
        }

        /* ════════════════════════════════════════════════════════════════
         * SYSCALL INTERFACE  (v2.5)
         * ════════════════════════════════════════════════════════════════
         *
         * Helpers to decode the variable-length argument encoding used by
         * OP_SYSCALL / OP_SYSBUF_ALLOC / OP_SYSBUF_WRITE / OP_SYSBUF_READ.
         *
         * flags byte: 0x00 = next byte is var id  (read ival from slot)
         *             0x01 = next 4 bytes are i32 immediate
         *             0x02 = literal zero (no extra bytes)
         *
         * read_sc_arg() decodes one argument, returns false on error.
         * ────────────────────────────────────────────────────────────── */

        /* ── OP_SYSCALL  dst  [num]  [a]  [b]  [c] ────────────────────
         *
         * Executes: result = syscall(num, a, b, c)
         * On nusaOS i386: int $0x80, eax=num ebx=a ecx=b edx=c
         * Stores signed return value in dst (int).
         *
         * If NSA_SYSCALL_SUPPORTED == 0 (non-i386 host), stores -1.
         * ────────────────────────────────────────────────────────────── */
        case OP_SYSCALL: {
            uint8_t dst;
            if (!read_u8(bc,ip,dst)) RT_ERR("SYSCALL: truncated dst");
            CHECK_ID(dst,"SYSCALL dst");

            int num, a, b, c;
            SC_ARG(num,"SYSCALL num");
            SC_ARG(a,  "SYSCALL a");
            SC_ARG(b,  "SYSCALL b");
            SC_ARG(c,  "SYSCALL c");

            int ret = nsa_syscall(num, a, b, c);

            VAR(dst).type = VAR_INT;
            VAR(dst).ival = ret;
            break;
        }

        /* ── OP_SYSBUF_ALLOC  dst  [size] ──────────────────────────────
         *
         * Allocates a zero-filled byte buffer of <size> bytes on the heap.
         * Stores the buffer's address (as integer) in dst.
         * Buffer is tracked in the pool and freed when the program ends.
         *
         * Usage in .nsa:
         *   sysbuf buf 256       // allocate 256-byte buffer
         *   addrof ptr buf_str   // (or use addrof for string vars)
         * ────────────────────────────────────────────────────────────── */
        case OP_SYSBUF_ALLOC: {
            uint8_t dst;
            if (!read_u8(bc,ip,dst)) RT_ERR("SYSBUF_ALLOC: truncated dst");
            CHECK_ID(dst,"SYSBUF_ALLOC dst");

            int sz; SC_ARG(sz,"SYSBUF_ALLOC size");
            if (sz <= 0 || sz > 65536) RT_ERR("SYSBUF_ALLOC: size out of range (1-65536)");
            if (sysbuf_count >= NSA_MAX_SYSBUFS) RT_ERR("SYSBUF_ALLOC: too many buffers (max 32)");

            void* buf = calloc(1, (size_t)sz);
            if (!buf) RT_ERR("SYSBUF_ALLOC: out of memory");

            sysbuf_ptr[sysbuf_count] = buf;
            sysbuf_len[sysbuf_count] = (size_t)sz;
            sysbuf_count++;

            VAR(dst).type = VAR_INT;
            VAR(dst).ival = (int32_t)(uintptr_t)buf;
            break;
        }

        /* ── OP_SYSBUF_WRITE  buf_var  [offset]  [src_var] ─────────────
         *
         * Copies the string content of src_var into the raw buffer at
         * the given byte offset.  Writes strlen(src)+1 bytes (NUL-term).
         *
         * Usage in .nsa:
         *   bufwrite buf 0 my_str   // write my_str at offset 0
         * ────────────────────────────────────────────────────────────── */
        case OP_SYSBUF_WRITE: {
            uint8_t buf_id;
            if (!read_u8(bc,ip,buf_id)) RT_ERR("SYSBUF_WRITE: truncated buf id");
            CHECK_ID(buf_id,"SYSBUF_WRITE buf");

            int offset; SC_ARG(offset,"SYSBUF_WRITE offset");

            uint8_t src_id;
            /* src flags */
            uint8_t src_fl;
            if (!read_u8(bc,ip,src_fl)) RT_ERR("SYSBUF_WRITE: truncated src flags");
            if (src_fl != 0x00) RT_ERR("SYSBUF_WRITE: src must be a string var id");
            if (!read_u8(bc,ip,src_id)) RT_ERR("SYSBUF_WRITE: truncated src id");
            CHECK_ID(src_id,"SYSBUF_WRITE src");
            NEED_STR(src_id,"SYSBUF_WRITE src");

            /* Resolve the buffer address stored in buf_id (int) */
            if (VAR(buf_id).type != VAR_INT) RT_ERR("SYSBUF_WRITE: buf must be int (address)");
            uint8_t* raw = (uint8_t*)(uintptr_t)(uint32_t)VAR(buf_id).ival;
            if (!raw) RT_ERR("SYSBUF_WRITE: null buffer address");

            /* Find pool entry to check bounds */
            size_t pool_size = 0;
            for (int pi=0;pi<sysbuf_count;pi++) {
                if (sysbuf_ptr[pi] == (void*)raw) { pool_size = sysbuf_len[pi]; break; }
            }
            const char* src_s = VAR(src_id).sval;
            size_t copy_len = strlen(src_s)+1;
            if (pool_size && (size_t)offset + copy_len > pool_size)
                RT_ERR("SYSBUF_WRITE: write would overflow buffer");

            memcpy(raw + offset, src_s, copy_len);
            break;
        }

        /* ── OP_SYSBUF_READ  dst  buf_var  [offset]  [len] ─────────────
         *
         * Reads <len> bytes from the raw buffer (at offset) into the
         * string variable dst.  NUL-terminates the result.
         * Capped at NSA_MAX_STR_LEN (254) characters.
         *
         * Usage in .nsa:
         *   bufread result buf 0 64   // read 64 bytes from buf into result
         * ────────────────────────────────────────────────────────────── */
        case OP_SYSBUF_READ: {
            uint8_t dst, buf_id;
            if (!read_u8(bc,ip,dst))    RT_ERR("SYSBUF_READ: truncated dst");
            if (!read_u8(bc,ip,buf_id)) RT_ERR("SYSBUF_READ: truncated buf id");
            CHECK_ID(dst,    "SYSBUF_READ dst");
            CHECK_ID(buf_id, "SYSBUF_READ buf");

            int offset; SC_ARG(offset,"SYSBUF_READ offset");
            int len;    SC_ARG(len,   "SYSBUF_READ len");

            if (VAR(buf_id).type != VAR_INT) RT_ERR("SYSBUF_READ: buf must be int (address)");
            const uint8_t* raw = (const uint8_t*)(uintptr_t)(uint32_t)VAR(buf_id).ival;
            if (!raw) RT_ERR("SYSBUF_READ: null buffer address");

            if (len < 0) len = 0;
            if (len > NSA_MAX_STR_LEN) len = NSA_MAX_STR_LEN;

            VAR(dst).type = VAR_STR;
            memcpy(VAR(dst).sval, raw + offset, (size_t)len);
            VAR(dst).sval[len] = '\0';
            break;
        }

        /* ── OP_ADDR_OF  dst  src ───────────────────────────────────────
         *
         * Stores the address of src's string buffer (sval) in dst (int).
         * This is the fast path for passing string pointers to syscalls
         * without allocating a separate sysbuf.
         *
         * The string must stay alive (not reassigned) while the pointer
         * is in use — NSA has no GC so this is the caller's responsibility.
         *
         * Usage in .nsa:
         *   let path = "/home/test.txt"
         *   addrof  ptr  path
         *   syscall fd  7  ptr  65  0   // SYS_OPEN = 7, O_RDONLY=0 O_WRONLY=1
         * ────────────────────────────────────────────────────────────── */
        case OP_ADDR_OF: {
            uint8_t dst, src;
            if (!read_u8(bc,ip,dst)) RT_ERR("ADDR_OF: truncated dst");
            if (!read_u8(bc,ip,src)) RT_ERR("ADDR_OF: truncated src");
            CHECK_ID(dst,"ADDR_OF dst");
            CHECK_ID(src,"ADDR_OF src");
            NEED_STR(src,"ADDR_OF src");

            VAR(dst).type = VAR_INT;
            VAR(dst).ival = (int32_t)(uintptr_t)VAR(src).sval;
            break;
        }

        default:
            RT_ERR2("unknown opcode 0x%02X at offset %zu",(unsigned)op,ip-1);
        }
    }

#undef SC_ARG
#undef RT_ERR
#undef RT_ERR1
#undef RT_ERR2
#undef VAR
#undef CHECK_ID
#undef NEED_INT
#undef NEED_STR
#undef ARR_CHECK

    return 0;
}

} // namespace NsaVM