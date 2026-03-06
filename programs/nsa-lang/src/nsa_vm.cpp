/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

#include "nsa_vm.h"
#include "nsa_opcodes.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <vector>

namespace NsaVM {

enum VarType : uint8_t { VAR_UNSET=0, VAR_INT, VAR_STR, VAR_BOOL };

struct VarSlot {
    VarType type;
    int32_t ival;
    char    sval[NSA_MAX_STR_LEN+1];
};

/* ── Call stack frame ──────────────────────────────────────────────── */
struct CallFrame {
    size_t  ret_ip;
    VarSlot locals[NSA_MAX_LOCALS];
};

/* ── Decode helpers ──────────────────────────────────────────────── */
static bool read_u8(const std::vector<uint8_t>& bc,size_t& ip,uint8_t& out){
    if(ip>=bc.size())return false; out=bc[ip++];return true;
}
static bool read_u16(const std::vector<uint8_t>& bc,size_t& ip,uint16_t& out){
    if(ip+2>bc.size())return false;
    out=(uint16_t)(bc[ip]|((uint16_t)bc[ip+1]<<8));ip+=2;return true;
}
static bool read_i32(const std::vector<uint8_t>& bc,size_t& ip,int32_t& out){
    if(ip+4>bc.size())return false;
    out=(int32_t)((uint32_t)bc[ip]|((uint32_t)bc[ip+1]<<8)|
                  ((uint32_t)bc[ip+2]<<16)|((uint32_t)bc[ip+3]<<24));
    ip+=4;return true;
}
static bool read_str(const std::vector<uint8_t>& bc,size_t& ip,char* buf,size_t bl){
    uint8_t len; if(!read_u8(bc,ip,len))return false;
    if(ip+len>bc.size()||len>bl-1)return false;
    memcpy(buf,&bc[ip],len);buf[len]='\0';ip+=len;return true;
}
static bool is_truthy(const VarSlot& v){
    switch(v.type){
        case VAR_INT:  return v.ival!=0;
        case VAR_BOOL: return v.ival!=0;
        case VAR_STR:  return v.sval[0]!='\0';
        default:       return false;
    }
}

#ifndef NSA_NO_LOOP_LIMIT
static const uint32_t MAX_BACK_JUMPS=10000000UL;
#endif

#define RT_ERR(msg) \
    do{fprintf(stderr,"%s: runtime error at offset %zu: %s\n",prog,ip-1,msg);return 1;}while(0)
#define RT_ERR1(fmt,a) \
    do{fprintf(stderr,"%s: runtime error at offset %zu: " fmt "\n",prog,ip-1,a);return 1;}while(0)
#define RT_ERR2(fmt,a,b) \
    do{fprintf(stderr,"%s: runtime error at offset %zu: " fmt "\n",prog,ip-1,a,b);return 1;}while(0)

/* VAR(id): resolve variable slot.
   When inside a function (call_depth>0), IDs refer to local frame.
   When at top level, IDs refer to globals.                          */
#define VAR(id) (call_depth>0 ? call_stack[call_depth-1].locals[(id)] : vars[(id)])

#define CHECK_ID(id,who) do{ \
    if(call_depth>0){ if((id)>=NSA_MAX_LOCALS) RT_ERR1(who ": local id %u out of range",(unsigned)(id)); } \
    else{ if((int)(id)>=sym_count) RT_ERR1(who ": var id %u out of range",(unsigned)(id)); } \
}while(0)
#define NEED_INT(id,who) if(VAR(id).type!=VAR_INT) RT_ERR1(who ": var %u is not an integer",(unsigned)(id))
#define NEED_STR(id,who) if(VAR(id).type!=VAR_STR) RT_ERR1(who ": var %u is not a string",(unsigned)(id))

int run(const std::vector<uint8_t>& bc,int sym_count,const char* prog){
    if(sym_count<0||sym_count>(int)NSA_MAX_VARS){
        fprintf(stderr,"%s: runtime error: invalid sym_count %d\n",prog,sym_count);return 1;
    }

    /* IMPORTANT: VarSlot[200] = ~51 KB, CallFrame[64] = ~1056 KB.
     * Declaring these on the stack causes immediate stack overflow on NusaOS
     * (and any OS with a small per-process stack).
     * Using 'static' puts them in the BSS/data segment instead.
     * This is safe because nsa is single-threaded and run() is never
     * called recursively.                                               */
    static VarSlot  vars[NSA_MAX_VARS];
    static CallFrame call_stack[NSA_MAX_CALL_DEPTH];

    /* Zero-init both arrays at the start of each run() call */
    memset(vars,       0, sizeof(vars));
    memset(call_stack, 0, sizeof(call_stack));

    for(int i=0;i<sym_count;i++){
        vars[i].type=VAR_UNSET;
    }

    int call_depth=0;

    size_t ip=0;
#ifndef NSA_NO_LOOP_LIMIT
    uint32_t back_jump_count=0;
#endif

    while(ip<bc.size()){
        uint8_t op=bc[ip++];
        switch((NsaOpcode)op){

        case OP_NOP: break;
        case OP_HALT: return 0;

        /* I/O */
        case OP_PRINT_STR:{
            char buf[NSA_MAX_STR_LEN+1];
            if(!read_str(bc,ip,buf,sizeof(buf)))RT_ERR("PRINT_STR: truncated");
            printf("%s\n",buf);break;
        }
        case OP_PRINT_STR_NL:{
            char buf[NSA_MAX_STR_LEN+1];
            if(!read_str(bc,ip,buf,sizeof(buf)))RT_ERR("PRINT_STR_NL: truncated");
            printf("%s",buf);break;
        }
        case OP_PRINT_VAR:{
            uint8_t id; if(!read_u8(bc,ip,id))RT_ERR("PRINT_VAR: truncated");
            CHECK_ID(id,"PRINT_VAR");
            switch(VAR(id).type){
                case VAR_INT:  printf("%d\n",VAR(id).ival);break;
                case VAR_STR:  printf("%s\n",VAR(id).sval);break;
                case VAR_BOOL: printf("%s\n",VAR(id).ival?"true":"false");break;
                default:       printf("(unset)\n");break;
            }
            break;
        }
        case OP_PRINT_VAR_NL:{
            uint8_t id; if(!read_u8(bc,ip,id))RT_ERR("PRINT_VAR_NL: truncated");
            CHECK_ID(id,"PRINT_VAR_NL");
            switch(VAR(id).type){
                case VAR_INT:  printf("%d",VAR(id).ival);break;
                case VAR_STR:  printf("%s",VAR(id).sval);break;
                case VAR_BOOL: printf("%s",VAR(id).ival?"true":"false");break;
                default:       printf("(unset)");break;
            }
            break;
        }
        case OP_INPUT_INT:{
            uint8_t id; if(!read_u8(bc,ip,id))RT_ERR("INPUT_INT: truncated");
            CHECK_ID(id,"INPUT_INT"); fflush(stdout);
            long v=0; char line[64];
            if(fgets(line,sizeof(line),stdin)){char* e;v=strtol(line,&e,10);if(e==line)v=0;}
            VAR(id).type=VAR_INT;VAR(id).ival=(int32_t)v;break;
        }
        case OP_INPUT_STR:{
            uint8_t id; if(!read_u8(bc,ip,id))RT_ERR("INPUT_STR: truncated");
            CHECK_ID(id,"INPUT_STR"); fflush(stdout);
            VAR(id).type=VAR_STR; memset(VAR(id).sval,0,sizeof(VAR(id).sval));
            if(fgets(VAR(id).sval,(int)sizeof(VAR(id).sval),stdin)){
                size_t l=strlen(VAR(id).sval);
                if(l>0&&VAR(id).sval[l-1]=='\n')VAR(id).sval[l-1]='\0';
            }
            break;
        }

        /* Variables */
        case OP_SET_STR:{
            uint8_t id; if(!read_u8(bc,ip,id))RT_ERR("SET_STR: truncated id");
            CHECK_ID(id,"SET_STR");
            char buf[NSA_MAX_STR_LEN+1];
            if(!read_str(bc,ip,buf,sizeof(buf)))RT_ERR("SET_STR: truncated data");
            VAR(id).type=VAR_STR;memset(VAR(id).sval,0,sizeof(VAR(id).sval));
            strncpy(VAR(id).sval,buf,NSA_MAX_STR_LEN);break;
        }
        case OP_SET_INT:{
            uint8_t id; int32_t val;
            if(!read_u8(bc,ip,id))RT_ERR("SET_INT: truncated id");
            CHECK_ID(id,"SET_INT");
            if(!read_i32(bc,ip,val))RT_ERR("SET_INT: truncated value");
            VAR(id).type=VAR_INT;VAR(id).ival=val;break;
        }
        case OP_SET_BOOL:{
            uint8_t id,val;
            if(!read_u8(bc,ip,id))RT_ERR("SET_BOOL: truncated id");
            CHECK_ID(id,"SET_BOOL");
            if(!read_u8(bc,ip,val))RT_ERR("SET_BOOL: truncated value");
            VAR(id).type=VAR_BOOL;VAR(id).ival=val?1:0;break;
        }
        case OP_COPY:{
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst))RT_ERR("COPY: truncated dst");
            if(!read_u8(bc,ip,src))RT_ERR("COPY: truncated src");
            CHECK_ID(dst,"COPY dst");CHECK_ID(src,"COPY src");
            VAR(dst)=VAR(src);break;
        }

        /* Arithmetic IMM */
        case OP_ADD_IMM:case OP_SUB_IMM:case OP_MUL_IMM:
        case OP_DIV_IMM:case OP_MOD_IMM:{
            uint8_t id; int32_t imm;
            if(!read_u8(bc,ip,id))RT_ERR("ARITH_IMM: truncated id");
            CHECK_ID(id,"ARITH_IMM");NEED_INT(id,"ARITH_IMM");
            if(!read_i32(bc,ip,imm))RT_ERR("ARITH_IMM: truncated imm");
            if((op==OP_DIV_IMM||op==OP_MOD_IMM)&&imm==0)RT_ERR("division/modulo by zero");
            switch((NsaOpcode)op){
                case OP_ADD_IMM:VAR(id).ival+=imm;break;case OP_SUB_IMM:VAR(id).ival-=imm;break;
                case OP_MUL_IMM:VAR(id).ival*=imm;break;case OP_DIV_IMM:VAR(id).ival/=imm;break;
                case OP_MOD_IMM:VAR(id).ival%=imm;break;default:break;
            }
            break;
        }

        /* Arithmetic VAR */
        case OP_ADD_VAR:case OP_SUB_VAR:case OP_MUL_VAR:
        case OP_DIV_VAR:case OP_MOD_VAR:{
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst))RT_ERR("ARITH_VAR: truncated dst");
            if(!read_u8(bc,ip,src))RT_ERR("ARITH_VAR: truncated src");
            CHECK_ID(dst,"ARITH_VAR dst");NEED_INT(dst,"ARITH_VAR dst");
            CHECK_ID(src,"ARITH_VAR src");NEED_INT(src,"ARITH_VAR src");
            if((op==OP_DIV_VAR||op==OP_MOD_VAR)&&VAR(src).ival==0)RT_ERR("division/modulo by zero");
            switch((NsaOpcode)op){
                case OP_ADD_VAR:VAR(dst).ival+=VAR(src).ival;break;
                case OP_SUB_VAR:VAR(dst).ival-=VAR(src).ival;break;
                case OP_MUL_VAR:VAR(dst).ival*=VAR(src).ival;break;
                case OP_DIV_VAR:VAR(dst).ival/=VAR(src).ival;break;
                case OP_MOD_VAR:VAR(dst).ival%=VAR(src).ival;break;
                default:break;
            }
            break;
        }

        /* Unary */
        case OP_INC:{uint8_t id;if(!read_u8(bc,ip,id))RT_ERR("INC: truncated");
            CHECK_ID(id,"INC");NEED_INT(id,"INC");VAR(id).ival++;break;}
        case OP_DEC:{uint8_t id;if(!read_u8(bc,ip,id))RT_ERR("DEC: truncated");
            CHECK_ID(id,"DEC");NEED_INT(id,"DEC");VAR(id).ival--;break;}
        case OP_NOT:{uint8_t id;if(!read_u8(bc,ip,id))RT_ERR("NOT: truncated");
            CHECK_ID(id,"NOT");
            if(VAR(id).type==VAR_INT||VAR(id).type==VAR_BOOL)VAR(id).ival=VAR(id).ival?0:1;
            else RT_ERR("NOT: not int or bool");break;}
        case OP_NEG:{uint8_t id;if(!read_u8(bc,ip,id))RT_ERR("NEG: truncated");
            CHECK_ID(id,"NEG");NEED_INT(id,"NEG");VAR(id).ival=-VAR(id).ival;break;}

        /* String ops */
        case OP_CONCAT:{
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst))RT_ERR("CONCAT: truncated dst");
            if(!read_u8(bc,ip,src))RT_ERR("CONCAT: truncated src");
            CHECK_ID(dst,"CONCAT dst");NEED_STR(dst,"CONCAT dst");
            CHECK_ID(src,"CONCAT src");NEED_STR(src,"CONCAT src");
            size_t dl=strlen(VAR(dst).sval),sl=strlen(VAR(src).sval);
            if(dl+sl>NSA_MAX_STR_LEN)sl=NSA_MAX_STR_LEN-dl;
            strncat(VAR(dst).sval,VAR(src).sval,sl);break;
        }
        case OP_CONCAT_LIT:{
            uint8_t dst; if(!read_u8(bc,ip,dst))RT_ERR("CONCAT_LIT: truncated dst");
            CHECK_ID(dst,"CONCAT_LIT");NEED_STR(dst,"CONCAT_LIT");
            char buf[NSA_MAX_STR_LEN+1];
            if(!read_str(bc,ip,buf,sizeof(buf)))RT_ERR("CONCAT_LIT: truncated literal");
            size_t dl=strlen(VAR(dst).sval),sl=strlen(buf);
            if(dl+sl>NSA_MAX_STR_LEN)sl=NSA_MAX_STR_LEN-dl;
            strncat(VAR(dst).sval,buf,sl);break;
        }
        case OP_LEN:{
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst))RT_ERR("LEN: truncated dst");
            if(!read_u8(bc,ip,src))RT_ERR("LEN: truncated src");
            CHECK_ID(dst,"LEN dst");CHECK_ID(src,"LEN src");NEED_STR(src,"LEN src");
            VAR(dst).type=VAR_INT;VAR(dst).ival=(int32_t)strlen(VAR(src).sval);break;
        }
        case OP_STR_TO_INT:{
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst))RT_ERR("STR_TO_INT: truncated dst");
            if(!read_u8(bc,ip,src))RT_ERR("STR_TO_INT: truncated src");
            CHECK_ID(dst,"STR_TO_INT dst");CHECK_ID(src,"STR_TO_INT src");NEED_STR(src,"STR_TO_INT src");
            char* e;long v=strtol(VAR(src).sval,&e,10);
            VAR(dst).type=VAR_INT;VAR(dst).ival=(int32_t)v;break;
        }
        case OP_INT_TO_STR:{
            uint8_t dst,src;
            if(!read_u8(bc,ip,dst))RT_ERR("INT_TO_STR: truncated dst");
            if(!read_u8(bc,ip,src))RT_ERR("INT_TO_STR: truncated src");
            CHECK_ID(dst,"INT_TO_STR dst");CHECK_ID(src,"INT_TO_STR src");NEED_INT(src,"INT_TO_STR src");
            VAR(dst).type=VAR_STR;
            snprintf(VAR(dst).sval,sizeof(VAR(dst).sval),"%d",VAR(src).ival);break;
        }

        /* Comparison */
        case OP_CMP_EQ:case OP_CMP_NE:case OP_CMP_LT:
        case OP_CMP_GT:case OP_CMP_LE:case OP_CMP_GE:{
            uint8_t dst,va,vb;
            if(!read_u8(bc,ip,dst))RT_ERR("CMP: truncated dst");
            if(!read_u8(bc,ip,va)) RT_ERR("CMP: truncated a");
            if(!read_u8(bc,ip,vb)) RT_ERR("CMP: truncated b");
            CHECK_ID(dst,"CMP dst");CHECK_ID(va,"CMP a");CHECK_ID(vb,"CMP b");
            NEED_INT(va,"CMP a");NEED_INT(vb,"CMP b");
            int32_t a=VAR(va).ival,b=VAR(vb).ival;bool res;
            switch((NsaOpcode)op){
                case OP_CMP_EQ:res=(a==b);break;case OP_CMP_NE:res=(a!=b);break;
                case OP_CMP_LT:res=(a<b); break;case OP_CMP_GT:res=(a>b); break;
                case OP_CMP_LE:res=(a<=b);break;case OP_CMP_GE:res=(a>=b);break;
                default:res=false;break;
            }
            VAR(dst).type=VAR_BOOL;VAR(dst).ival=res?1:0;break;
        }

        /* Logical */
        case OP_AND:case OP_OR:{
            uint8_t dst,va,vb;
            if(!read_u8(bc,ip,dst))RT_ERR("LOGICAL: truncated dst");
            if(!read_u8(bc,ip,va)) RT_ERR("LOGICAL: truncated a");
            if(!read_u8(bc,ip,vb)) RT_ERR("LOGICAL: truncated b");
            CHECK_ID(dst,"LOGICAL dst");CHECK_ID(va,"LOGICAL a");CHECK_ID(vb,"LOGICAL b");
            bool a=is_truthy(VAR(va)),b=is_truthy(VAR(vb));
            VAR(dst).type=VAR_BOOL;VAR(dst).ival=(op==OP_AND)?(a&&b):(a||b);break;
        }

        /* Forward jumps */
        case OP_JMP_FWD:{
            uint16_t off; if(!read_u16(bc,ip,off))RT_ERR("JMP_FWD: truncated");
            if(ip+off>bc.size())RT_ERR("JMP_FWD: past end");ip+=off;break;
        }
        case OP_JMP_IF_TRUE:case OP_JMP_IF_FALSE:{
            uint8_t id;uint16_t off;
            if(!read_u8(bc,ip,id))  RT_ERR("JMP_IF: truncated id");
            CHECK_ID(id,"JMP_IF");
            if(!read_u16(bc,ip,off))RT_ERR("JMP_IF: truncated offset");
            bool take=(op==OP_JMP_IF_TRUE)?is_truthy(VAR(id)):!is_truthy(VAR(id));
            if(take){if(ip+off>bc.size())RT_ERR("JMP_IF: past end");ip+=off;}break;
        }
        case OP_JMP_IF_EQ:case OP_JMP_IF_NE:
        case OP_JMP_IF_LT:case OP_JMP_IF_GT:
        case OP_JMP_IF_LE:case OP_JMP_IF_GE:{
            uint8_t id;int32_t cmp;uint16_t off;
            if(!read_u8(bc,ip,id))   RT_ERR("JMP_CMP: truncated id");
            CHECK_ID(id,"JMP_CMP");
            if(!read_i32(bc,ip,cmp)) RT_ERR("JMP_CMP: truncated cmp");
            if(!read_u16(bc,ip,off)) RT_ERR("JMP_CMP: truncated offset");
            int32_t val=(VAR(id).type==VAR_INT)?VAR(id).ival:0;bool take;
            switch((NsaOpcode)op){
                case OP_JMP_IF_EQ:take=(val==cmp);break;case OP_JMP_IF_NE:take=(val!=cmp);break;
                case OP_JMP_IF_LT:take=(val<cmp); break;case OP_JMP_IF_GT:take=(val>cmp); break;
                case OP_JMP_IF_LE:take=(val<=cmp);break;case OP_JMP_IF_GE:take=(val>=cmp);break;
                default:take=false;break;
            }
            if(take){if(ip+off>bc.size())RT_ERR("JMP_CMP: past end");ip+=off;}break;
        }

        /* Backward jumps */
        case OP_JMP_BACK:{
            uint16_t off; if(!read_u16(bc,ip,off))RT_ERR("JMP_BACK: truncated");
#ifndef NSA_NO_LOOP_LIMIT
            if(++back_jump_count>MAX_BACK_JUMPS)RT_ERR("infinite loop detected (limit 10M)");
#endif
            if(off>ip)RT_ERR("JMP_BACK: underflow");ip-=off;break;
        }
        case OP_JMP_BACK_TRUE:case OP_JMP_BACK_FALSE:{
            uint8_t id;uint16_t off;
            if(!read_u8(bc,ip,id))  RT_ERR("JMP_BACK_T/F: truncated id");
            CHECK_ID(id,"JMP_BACK_T/F");
            if(!read_u16(bc,ip,off))RT_ERR("JMP_BACK_T/F: truncated offset");
            bool take=(op==OP_JMP_BACK_TRUE)?is_truthy(VAR(id)):!is_truthy(VAR(id));
            if(take){
#ifndef NSA_NO_LOOP_LIMIT
                if(++back_jump_count>MAX_BACK_JUMPS)RT_ERR("infinite loop detected");
#endif
                if(off>ip)RT_ERR("JMP_BACK_T/F: underflow");ip-=off;
            }
            break;
        }
        case OP_JMP_BACK_NZ:case OP_JMP_BACK_Z:{
            uint8_t id;uint16_t off;
            if(!read_u8(bc,ip,id))  RT_ERR("JMP_BACK_NZ/Z: truncated id");
            CHECK_ID(id,"JMP_BACK_NZ/Z");
            if(!read_u16(bc,ip,off))RT_ERR("JMP_BACK_NZ/Z: truncated offset");
            int32_t val=(VAR(id).type==VAR_INT||VAR(id).type==VAR_BOOL)?VAR(id).ival:0;
            bool take=(op==OP_JMP_BACK_NZ)?(val!=0):(val==0);
            if(take){
#ifndef NSA_NO_LOOP_LIMIT
                if(++back_jump_count>MAX_BACK_JUMPS)RT_ERR("infinite loop detected");
#endif
                if(off>ip)RT_ERR("JMP_BACK_NZ/Z: underflow");ip-=off;
            }
            break;
        }

        /* ── Functions ───────────────────────────────────────────────────
         *
         * LOAD_ARG local_id global_id
         *   Runs BEFORE CALL (call_depth not yet incremented).
         *   Stages global vars into call_stack[call_depth].locals[local_id].
         *
         * CALL abs_addr
         *   Saves ret_ip into call_stack[call_depth], increments call_depth,
         *   then jumps to abs_addr.
         *
         * RET
         *   Decrements call_depth, restores ip.
         *
         * STORE_RET global_id local_id
         *   Runs AFTER CALL returns.
         *   call_depth was already decremented by RET, so the frame that
         *   just returned is at call_stack[call_depth] (still readable).
         * ---------------------------------------------------------------- */
        case OP_LOAD_ARG:{
            uint8_t local_id,global_id;
            if(!read_u8(bc,ip,local_id)) RT_ERR("LOAD_ARG: truncated local_id");
            if(!read_u8(bc,ip,global_id))RT_ERR("LOAD_ARG: truncated global_id");
            if(local_id>=NSA_MAX_LOCALS)        RT_ERR("LOAD_ARG: local_id out of range");
            if((int)global_id>=sym_count)       RT_ERR("LOAD_ARG: global_id out of range");
            if(call_depth>=NSA_MAX_CALL_DEPTH)  RT_ERR("LOAD_ARG: call stack overflow");
            /* Stage into the NEXT frame (call_stack[call_depth], not yet active) */
            call_stack[call_depth].locals[local_id]=vars[global_id];
            break;
        }
        case OP_CALL:{
            uint16_t addr; if(!read_u16(bc,ip,addr))RT_ERR("CALL: truncated address");
            if(call_depth>=NSA_MAX_CALL_DEPTH)RT_ERR("call stack overflow (max depth 64)");
            call_stack[call_depth].ret_ip=ip;
            call_depth++;
            ip=addr;
            break;
        }
        case OP_RET:{
            if(call_depth<=0)RT_ERR("RET outside of function");
            call_depth--;
            ip=call_stack[call_depth].ret_ip;
            break;
        }
        case OP_STORE_RET:{
            uint8_t global_id,local_id;
            if(!read_u8(bc,ip,global_id))RT_ERR("STORE_RET: truncated global_id");
            if(!read_u8(bc,ip,local_id)) RT_ERR("STORE_RET: truncated local_id");
            if((int)global_id>=sym_count)      RT_ERR("STORE_RET: global_id out of range");
            if(local_id>=NSA_MAX_LOCALS)       RT_ERR("STORE_RET: local_id out of range");
            /* Frame that just returned is still at call_stack[call_depth] */
            vars[global_id]=call_stack[call_depth].locals[local_id];
            break;
        }

        default:
            RT_ERR2("unknown opcode 0x%02X at offset %zu",(unsigned)op,ip-1);
        }
    }

#undef RT_ERR
#undef RT_ERR1
#undef RT_ERR2
#undef VAR
#undef CHECK_ID
#undef NEED_INT
#undef NEED_STR

    return 0;
}

} // namespace NsaVM