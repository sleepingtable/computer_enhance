#include <cassert>
#include <cstdint>
#include <cstring>
#include <format>
#include <iostream>
#include <stdexcept>


using u8  = uint8_t;
using u16 = uint16_t;
using s8  = int8_t;
using s16 = int16_t;


static u8 MEMORY[1 << 16];


void read_instructions(const char* file_path, size_t &size) {
    auto file = fopen(file_path, "rb");
    if (file == nullptr)
        throw std::runtime_error{std::format(
            "No file {} found",
            std::string{file_path}
        )};

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);

    fread(MEMORY, 1, size, file);
    fclose(file);
}


static constexpr struct {
    char value[3];
} REG_ENCODING[2][12] = {
    {{"al"}, {"cl"}, {"dl"}, {"bl"}, {"ah"}, {"ch"}, {"dh"}, {"bh"}},  // w = 0
    {{"ax"}, {"cx"}, {"dx"}, {"bx"}, {"sp"}, {"bp"}, {"si"}, {"di"},   // w = 1
     {"es"}, {"cs"}, {"ss"}, {"ds"}}  // reg mem, also wide
};


static constexpr struct {
    char value[8];
} MEM_ENCODING[8] = {
    {"bx + si"},
    {"bx + di"},
    {"bp + si"},
    {"bp + di"},
    {"si"},
    {"di"},
    {"bp"},
    {"bx"},
};


static constexpr struct {
    char value[4];
} OPS_ENCODING[8] {"add", "or", "adc", "sbb", "and", "sub", "", "cmp"};


static constexpr struct {
    char value[5];
} JUMP_ENCODING[16] {
    {"jo"}, {"jno"}, {"jb"}, {"jnb"}, {"je"}, {"jne"}, {"jbe"}, {"jnbe"},
    {"js"}, {"jns"}, {"jp"}, {"jnp"}, {"jl"}, {"jnl"}, {"jle"}, {"jnle"}
};


static constexpr struct {
    char value[7];
} LOOP_ENCODING[4] { {"loopnz"}, {"loopz"}, {"loop"}, {"jcxz"} };


static u16 REGS[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u16 CLOCKS {0};


static u8 *const HALF_REGS[8] = {
    reinterpret_cast<u8*>(&REGS[0]),
    reinterpret_cast<u8*>(&REGS[1]),
    reinterpret_cast<u8*>(&REGS[2]),
    reinterpret_cast<u8*>(&REGS[3]),
    reinterpret_cast<u8*>(&REGS[0]) + 1,
    reinterpret_cast<u8*>(&REGS[1]) + 1,
    reinterpret_cast<u8*>(&REGS[2]) + 1,
    reinterpret_cast<u8*>(&REGS[3]) + 1,
};


enum Flags { Sign, Zero, Parity };


// for now only parity, zero and sign
static constexpr char FLAG_NAMES[16][3] {
    {"S"}, {"Z"}, {"P"}
};


// XXXXXXXXXXXXXPZS
static u16 FLAGS {0};


static u16 IP {0};


struct Reg {
    u8 val;
    u8 w;
};
std::string to_string(const Reg &reg) {
    return REG_ENCODING[reg.w][reg.val].value;
}


struct Mem {
    bool has_reg;
    u8 reg;
    bool has_disp;
    u16 disp;
};
std::string to_string(const Mem &mem) {
    s16 disp = (s16)mem.disp;
    if (mem.has_reg && mem.has_disp)
        return std::format("[{} {} {}]", MEM_ENCODING[mem.reg].value, disp >= 0 ? "+" : "-", std::abs(disp));
    if (mem.has_reg)
        return std::format("[{}]", MEM_ENCODING[mem.reg].value);
    if (mem.has_disp)
        return std::format("[{}]", mem.disp);
    throw std::runtime_error("Neither reg nor mem");
}


struct Imm {
    u16 val;
    u8 w;
};
std::string to_string(const Imm &imm) {
    return std::format("{}", (s16)imm.val);
}


union Op {
    Reg reg;
    Mem mem;
    Imm imm;
};


enum OpType {
    Reg,
    Mem,
    Imm
};


struct Instr {
    std::string instr;
    OpType op0_t;
    OpType op1_t;
    Op op0;
    Op op1;
    bool reversed;  // if true, INSTR op1, op0
};
std::string to_string(const Instr &instr) {
    std::string op0;
    std::string op1;
    switch (instr.op0_t) {
        case Reg:
            op0 = to_string(instr.op0.reg); break;
        case Mem:
            op0 = to_string(instr.op0.mem); break;
        case Imm:
            op0 = to_string(instr.op0.imm); break;
    }
    switch (instr.op1_t) {
        case Reg:
            op1 = to_string(instr.op1.reg); break;
        case Mem:
            op1 = to_string(instr.op1.mem); break;
        case Imm:
            op1 = to_string(instr.op1.imm); break;
    }
    std::string suffix_instr;
    std::string prefix_imm;
    if (instr.op1_t == Imm && instr.op0_t == Mem) {
        // size is ambiguous, need to specify
        assert(!instr.reversed);
        if (instr.instr == "mov")
            prefix_imm = instr.op1.imm.w == 1 ? "word " : " byte ";
        else
            suffix_instr = instr.op1.imm.w == 1 ? " word" : " byte";
    }
    if (instr.op0_t == Imm) {  // special instruction, ignore op1, only op0 is used
                               // $ means relative jump
                               // need to add two (otherwise points to next instr) and always explicitely sign
        s16 reljump = ((s16)instr.op0.imm.val) + 2;
        return std::format("{} ${}{}", instr.instr, reljump >=0 ? "+" : "-", std::abs(reljump));
    }
    if (instr.reversed)
        return std::format("{}{} {}, {}", instr.instr, suffix_instr, op1, op0);
    return std::format("{}{} {}, {}{}", instr.instr, suffix_instr, op0, prefix_imm, op1);
}


std::string print_reg_val(const struct Reg &reg) {
    char hex[7];
    if (reg.w == 1)
        sprintf(hex, "%x", REGS[reg.val]);
    else
        sprintf(hex, "%x", REGS[reg.val % 4]);
    return std::string(hex);
}


void print_one_reg(const char* name, const u16 &value) {
    char hex[2];
    std::cout << "\t\t" << name << ": 0x";
    for (s8 h = 3; h >= 0; --h) {
        sprintf(hex, "%x", (value >> (h * 4)) & 0xF);
        std::cout << hex;
    }
    std::cout << " (" << value << ")" << std::endl;
}


void print_all_regs() {
    std::cout << "Final registers:" << std::endl;
    print_one_reg("ip", IP);
    for (u8 i = 0; i < 12; ++i)
        print_one_reg(REG_ENCODING[1][i].value, REGS[i]);
}


template<typename U>
void update_flags(const U &val, const u8 &w) {
    u8 nbbits = (w + 1) * 8;
    FLAGS = 0;
    FLAGS |= ((u16)val == 0) << Flags::Zero;
    FLAGS |= ((u16)((val >> (nbbits - 1)) & 1)) << Flags::Sign;
    FLAGS |= ((u16)(1 ^ __builtin_parity(val & 0xFF))) << Flags::Parity;
}


u16 get_addr(const struct Mem &mem) {
    u16 addr {0};
    if (mem.has_reg) {
        switch(mem.reg) {
            case 0:  // bx + si
                addr += REGS[3] + REGS[6];  break;
            case 1:  // bx + di
                addr += REGS[3] + REGS[7];  break;
            case 2:  // bp + si
                addr += REGS[5] + REGS[6];  break;
            case 3:  // bp + di
                addr += REGS[5] + REGS[7];  break;
            case 4:  // si
                addr += REGS[6];            break;
            case 5:  // di
                addr += REGS[7];            break;
            case 6:  // bp
                addr += REGS[5];            break;
            case 7:  // bx
                addr += REGS[3];            break;
        }
    }
    if (mem.has_disp) {
        addr += mem.disp;
    }
    return addr;
}


u16 load(const struct Mem &mem) {
    u16 addr {get_addr(mem)};
    return ((u16)MEMORY[addr]) + (((u16)MEMORY[addr + 1]) << 8);
}


void store(const struct Mem &mem, const u16 &val) {
    u16 addr {get_addr(mem)};
    MEMORY[addr] = (u8)(val & 0xFF);
    MEMORY[addr + 1] = (u8)((val >> 8) &0xFF);
}


void apply_instr(const std::string &instr, const OpType &dest_t, const OpType &src_t, const Op &dest, const Op &src) {
    u16 src_val;
    if(src_t == Imm)
        src_val = src.imm.val;
    else if (src_t == Mem)
        src_val = load(src.mem);
    else
        src_val = src.reg.w == 1 ? REGS[src.reg.val] : (u16)*HALF_REGS[src.reg.val];

    u16 dest_val;
    if (dest_t == Mem)
        dest_val = load(dest.mem);
    else if (dest_t == Reg)
        dest_val = dest.reg.w == 1 ? REGS[dest.reg.val] : (u16)*HALF_REGS[dest.reg.val];
    else
        throw std::runtime_error{"Should not get to apply_instr in the instruction destination is an immediate"};

    if (instr == "mov") {
        dest_val = src_val;
        goto update_values;
    } else if (instr == "add") {
        dest_val += src_val;
        goto update_flags_dest;
    } else if (instr == "sub") {
        dest_val -= src_val;
        goto update_flags_dest;
    } else if (instr == "cmp") {
        if (dest_t == Reg && dest.reg.w == 0)
            update_flags((u8)dest_val - (u8)src_val, 0);
        else
            update_flags(dest_val - src_val, 1);
    } else {
        throw std::runtime_error(std::format("Instruction application not implemented: {}", instr));
    }
    return;

    update_flags_dest:
    update_flags(dest_val, dest_t == Reg ? dest.reg.w : 1);

    update_values:
    if (dest_t == Mem)
        store(dest.mem, dest_val);
    else {
        if (dest.reg.w == 1)
            REGS[dest.reg.val] = dest_val;
        else
            *HALF_REGS[dest.reg.val] = (u8)dest_val;
    }
}


void cond_jump(const std::string &instr, const struct Imm &imm, const std::string jump, const std::string neg_jump, Flags flag) {
    if (instr == jump) {
        if((FLAGS >> flag & 1) == 1)
            IP += (s16)imm.val;
    } else if (instr == neg_jump) {
        if((FLAGS >> flag & 1) == 0)
            IP += (s16)imm.val;
    }
}


void apply_jump(const std::string &instr, const Op &dest) {
    cond_jump(instr, dest.imm, "je", "jne", Flags::Zero);
    cond_jump(instr, dest.imm, "jp", "jnp", Flags::Parity);
    cond_jump(instr, dest.imm, "js", "jns", Flags::Sign);
    // others not defined, programs containing them may loop
}


std::string flags_to_string(const u16 &flags) {
    std::string ret;
    for(int flag = Flags::Sign; flag <= Flags::Parity; ++flag)
        if (((flags >> flag) & 1) == 1)
            ret += FLAG_NAMES[flag];
    return ret;
}


std::string flag_change(const u16 &prev, const u16 &next) {
    if (prev == next)
        return "";

    return std::format(" flags:{}->{}", flags_to_string(prev), flags_to_string(next));
}


std::string ip_change(const u16 &prev_IP) {
    char prev[7];
    char next[7];
    sprintf(prev, "%x", prev_IP);
    sprintf(next, "%x", IP);
    return std::format("ip:0x{}->0x{}", prev, next);
}


std::string sim_instr(const Instr &instr, const u16 &prev_IP) {
    const OpType& dest_t = instr.reversed ? instr.op1_t : instr.op0_t;
    const OpType& src_t  = instr.reversed ? instr.op0_t : instr.op1_t;
    const Op& dest = instr.reversed ? instr.op1 : instr.op0;
    const Op& src  = instr.reversed ? instr.op0 : instr.op1;
    std::string dis = to_string(instr);
    std::string reg = REG_ENCODING[dest.reg.w][dest.reg.val].value;
    u16 flags = FLAGS;
    std::string reg_change;
    if (dest_t == Reg) {
        std::string init = print_reg_val(dest.reg);
        apply_instr(instr.instr, dest_t, src_t, dest, src);
        std::string final = print_reg_val(dest.reg);
        reg_change = init != final ? std::format(" {}:0x{}->0x{}", reg, init, final) : "";
    } else if (dest_t == Imm) {
        apply_jump(instr.instr, dest);
    } else if (dest_t == Mem) {
        apply_instr(instr.instr, dest_t, src_t, dest, src);
    }
    return std::format("{} ; {}{}{}", dis, ip_change(prev_IP), reg_change, flag_change(flags, FLAGS));
}


u16 ea(const struct Mem &mem) {
    if (mem.has_disp && !mem.has_reg)
        return 6;
    if (mem.has_reg && (!mem.has_disp || mem.disp == 0)) {
        if (mem.reg > 3)
            return 5;
        if (mem.reg == 0 || mem.reg == 3)
            return 7;
        return 8;
    }
    if (mem.reg > 3)
        return 9;
    if (mem.reg == 0 || mem.reg == 3)
        return 11;
    return 12;
}


u16 estimate_clocks_mov(const OpType &dest_t, const OpType &src_t, const Op &dest, const Op &src) {
    // ignoring segment registers here
    if (dest_t == Mem) {
        if (src_t == Reg) {
            if (src.reg.val == 0 || (src.reg.val == 4 && src.reg.w == 0))
                return 10;                  // mem <- acc
            return 9 + ea(dest.mem);        // mem <- reg
        }
        return 10 + ea(dest.mem);           // mem <- imm
    }
    if (src_t == Mem && (dest.reg.val == 0 || (src.reg.val == 4 && src.reg.w == 0)))
        return 10;                          // acc <- mem
    if (src_t == Reg)
        return 2;                           // reg <- reg
    if (src_t == Imm)
        return 4;                           // reg <- imm
    return 8 + ea(src.mem);                 // reg <- mem
}


u16 estimate_clocks_add(const OpType &dest_t, const OpType &src_t, const Op &dest, const Op &src) {
    if (dest_t == Mem) {
        if (src_t == Imm)
            return 17 + ea(dest.mem);       // mem += imm
        return 16 + ea(dest.mem);           // mem += reg
    }
    if (src_t == Mem)
        return 9 + ea(src.mem);             // reg += mem
    if (src_t == Reg)
        return 3;                           // reg += reg
    return 4;                               // reg += imm
}

std::string estimate_clocks(const Instr &instr) {
    const OpType& dest_t = instr.reversed ? instr.op1_t : instr.op0_t;
    const OpType& src_t  = instr.reversed ? instr.op0_t : instr.op1_t;
    const Op& dest = instr.reversed ? instr.op1 : instr.op0;
    const Op& src  = instr.reversed ? instr.op0 : instr.op1;
    std::string dis = to_string(instr);
    u16 clocks;
    if (instr.instr == "mov")
        clocks = estimate_clocks_mov(dest_t, src_t, dest, src);
    else if (instr.instr == "add")
        clocks = estimate_clocks_add(dest_t, src_t, dest, src);
    else
        throw std::runtime_error{"Clocks not implemented for this instruction"};
    CLOCKS += clocks;
    return std::format("{} ; Clocks: +{} = {}", dis, clocks, CLOCKS);
}


void unimplemented(const u8 *& /* unused */, Instr & /* unused */) {
    throw std::runtime_error("unimplemented");
}


// load flag
void lf(const u8 *const &b, u8 &outflag, const u8 offset, const u8 nbbits) {
    outflag = *b >> offset & ((1 << nbbits) - 1);
}


void instr_rm_op(OpType &op_t, Op &op, const u8 &w, const u8 &mod, const u8 &rm) {
    op_t = mod == 0b11 ? Reg : Mem;
    if (mod == 0b11) {
        op.reg = {rm, w};
    } else {
        op.mem.has_reg = (mod != 0b00 || rm != 0b110);
        if (op.mem.has_reg)
            op.mem.reg = rm;
    }
}
void instr_rm_op0(Instr &i, const u8 &w, const u8 &mod, const u8 &rm) { instr_rm_op(i.op0_t, i.op0, w, mod, rm); }
void instr_rm_op1(Instr &i, const u8 &w, const u8 &mod, const u8 &rm) { instr_rm_op(i.op1_t, i.op1, w, mod, rm); }


void instr_reg_op(OpType &op_t, Op &op, const u8 &w, const u8 &reg) {
    op_t = Reg;
    op.reg = {reg, w};
}
void instr_reg_op0(Instr &i, const u8 &w, const u8 &reg) { instr_reg_op(i.op0_t, i.op0, w, reg); }
void instr_reg_op1(Instr &i, const u8 &w, const u8 &reg) { instr_reg_op(i.op1_t, i.op1, w, reg); }


u16 read_data(const u8 *&b, const u8 size) {
    assert(size > 0);

    if (size == 1)
        return (s16)*(s8*)b++;

    u16 ret = (u16)*b++;
    if (size == 2)
        ret |= ((u16)*b++) << 8;
    return ret;
}


void disp_op(const u8 *&b, Op &op, const u8 &mod, const u8 &rm) {
    u8 size = mod == 0 && rm == 0b110 ? 2 : mod % 0b11;
    op.mem.has_disp = (size > 0);
    if (size > 0)
        op.mem.disp = read_data(b, size);
}
void disp_op0(const u8 *&b, Instr &i, const u8 &mod, const u8 &rm) { disp_op(b, i.op0, mod, rm); }
void disp_op1(const u8 *&b, Instr &i, const u8 &mod, const u8 &rm) { disp_op(b, i.op1, mod, rm); }


void instr_imm_op(const u8 *&b, OpType &op_t, Op &op, const u8 &w, const u8 s = 0) {
    op_t = Imm;
    op.imm = {read_data(b, s == 1 ? 1 : w + 1), w};
}
void instr_imm_op0(const u8 *&b, Instr &i, const u8 &w, const u8 s = 0) { instr_imm_op(b, i.op0_t, i.op0, w, s); }
void instr_imm_op1(const u8 *&b, Instr &i, const u8 &w, const u8 s = 0) { instr_imm_op(b, i.op1_t, i.op1, w, s); }


void move_regmem_to_from_reg(const u8 *&b, Instr &i) {
    u8 d, w, mod, reg, rm;
    lf(b, d, 1, 1); lf(b, w, 0, 1); ++b;
    lf(b, mod, 6, 2); lf(b, reg, 3, 3); lf(b, rm, 0, 3); ++b;
    instr_rm_op0(i, w, mod, rm);
    disp_op0(b, i, mod, rm);
    instr_reg_op1(i, w, reg);
    i.reversed = d == 1;
    i.instr = "mov";
}


void imm_to_regmem(const u8 *&b, Instr &i, const u8 s = 0) {
    u8 w, mod, rm;
    lf(b, w, 0, 1); ++b;
    lf(b, mod, 6, 2); lf(b, rm, 0, 3); ++b;
    instr_rm_op0(i, w, mod, rm);
    disp_op0(b, i, mod, rm);
    instr_imm_op1(b, i, w, s);
    i.reversed = false;
}


void move_imm_to_regmem(const u8 *&b, Instr &i) {
    imm_to_regmem(b, i);
    i.instr = "mov";
}


void imm_to_reg(const u8 *&b, Instr &i) {
    u8 w, reg;
    lf(b, w, 3, 1); lf(b, reg, 0, 3); ++b;
    instr_reg_op0(i, w, reg);
    instr_imm_op1(b, i, w);
    i.reversed = false;
    i.instr = "mov";
}


void mem_to_acc(const u8 *&b, Instr &i) {
    u8 w; lf(b, w, 0, 1); ++b;
    instr_reg_op0(i, w, 0);           // 0 is acc
    instr_rm_op1(i, w, 0b00, 0b110);  // disp only
    disp_op1(b, i, 0b00, 0b110);
    i.reversed = false;
    i.instr = "mov";
}


void acc_to_mem(const u8 *&b, Instr &i) {
    mem_to_acc(b, i);
    i.reversed = true;
}


void regmem_to_seg(const u8 *&b, Instr &i) {
    u8 mod, sr, rm; ++b;
    lf(b, mod, 6, 2); lf(b, sr, 3, 2); lf(b, rm, 0, 3); ++b;
    instr_reg_op0(i, 1, 8 + sr);  // SR is wide, and kept here after other wide regs
    instr_rm_op1(i, 1, mod, rm);
    disp_op1(b, i, mod, rm);
    i.reversed = false;
    i.instr = "mov";
}


void seg_to_regmem(const u8 *&b, Instr &i) {
    regmem_to_seg(b, i);
    i.reversed = true;
}


void op_regmem_and_reg_to_either(const u8 *&b, Instr &i) {
    u8 op; lf(b, op, 3, 3);
    assert(op != 0b110);  // no op 110
    move_regmem_to_from_reg(b, i);
    i.instr = OPS_ENCODING[op].value;
}


void op_imm_from_regmem(const u8 *&b, Instr &i) {
    u8 op, s; lf(b + 1, op, 3, 3); lf(b, s, 1, 1);
    assert(op != 0b110);  // no op 110
    imm_to_regmem(b, i, s);
    i.instr = OPS_ENCODING[op].value;
}


void op_imm_from_acc(const u8 *&b, Instr &i) {
    u8 op, w; lf(b, op, 3, 3); lf(b, w, 0, 1); ++b;
    instr_reg_op0(i, w, 0);           // 0 is acc
    instr_imm_op1(b, i, w);
    i.instr = OPS_ENCODING[op].value;
    i.reversed = false;
}


void jumps(const u8 *&b, Instr &i) {
    u8 jump; lf(b, jump, 0, 4); ++b;
    instr_imm_op0(b, i, 0, 1);  // 8 bit and signed
    i.instr = JUMP_ENCODING[jump].value;
    i.reversed = false;
}


void loops(const u8 *&b, Instr &i) {
    u8 loop; lf(b, loop, 0, 2); ++b;
    instr_imm_op0(b, i, 0, 1);  // 8 bit and signed
    i.instr = LOOP_ENCODING[loop].value;
    i.reversed = false;
}


static void(*disassembly_table[256])(const u8 *&, Instr &instr) {
    /* 00000000 */  op_regmem_and_reg_to_either,
    /* 00000001 */  op_regmem_and_reg_to_either,
    /* 00000010 */  op_regmem_and_reg_to_either,
    /* 00000011 */  op_regmem_and_reg_to_either,
    /* 00000100 */  op_imm_from_acc,
    /* 00000101 */  op_imm_from_acc,
    /* 00000110 */  unimplemented,
    /* 00000111 */  unimplemented,
    /* 00001000 */  op_regmem_and_reg_to_either,
    /* 00001001 */  op_regmem_and_reg_to_either,
    /* 00001010 */  op_regmem_and_reg_to_either,
    /* 00001011 */  op_regmem_and_reg_to_either,
    /* 00001100 */  op_imm_from_acc,
    /* 00001101 */  op_imm_from_acc,
    /* 00001110 */  unimplemented,
    /* 00001111 */  unimplemented,
    /* 00010000 */  op_regmem_and_reg_to_either,
    /* 00010001 */  op_regmem_and_reg_to_either,
    /* 00010010 */  op_regmem_and_reg_to_either,
    /* 00010011 */  op_regmem_and_reg_to_either,
    /* 00010100 */  op_imm_from_acc,
    /* 00010101 */  op_imm_from_acc,
    /* 00010110 */  unimplemented,
    /* 00010111 */  unimplemented,
    /* 00011000 */  op_regmem_and_reg_to_either,
    /* 00011001 */  op_regmem_and_reg_to_either,
    /* 00011010 */  op_regmem_and_reg_to_either,
    /* 00011011 */  op_regmem_and_reg_to_either,
    /* 00011100 */  op_imm_from_acc,
    /* 00011101 */  op_imm_from_acc,
    /* 00011110 */  unimplemented,
    /* 00011111 */  unimplemented,
    /* 00100000 */  op_regmem_and_reg_to_either,
    /* 00100001 */  op_regmem_and_reg_to_either,
    /* 00100010 */  op_regmem_and_reg_to_either,
    /* 00100011 */  op_regmem_and_reg_to_either,
    /* 00100100 */  op_imm_from_acc,
    /* 00100101 */  op_imm_from_acc,
    /* 00100110 */  unimplemented,
    /* 00100111 */  unimplemented,
    /* 00101000 */  op_regmem_and_reg_to_either,
    /* 00101001 */  op_regmem_and_reg_to_either,
    /* 00101010 */  op_regmem_and_reg_to_either,
    /* 00101011 */  op_regmem_and_reg_to_either,
    /* 00101100 */  op_imm_from_acc,
    /* 00101101 */  op_imm_from_acc,
    /* 00101110 */  unimplemented,
    /* 00101111 */  unimplemented,
    /* 00110000 */  op_regmem_and_reg_to_either,
    /* 00110001 */  op_regmem_and_reg_to_either,
    /* 00110010 */  op_regmem_and_reg_to_either,
    /* 00110011 */  op_regmem_and_reg_to_either,
    /* 00110100 */  op_imm_from_acc,
    /* 00110101 */  op_imm_from_acc,
    /* 00110110 */  unimplemented,
    /* 00110111 */  unimplemented,
    /* 00111000 */  op_regmem_and_reg_to_either,
    /* 00111001 */  op_regmem_and_reg_to_either,
    /* 00111010 */  op_regmem_and_reg_to_either,
    /* 00111011 */  op_regmem_and_reg_to_either,
    /* 00111100 */  op_imm_from_acc,
    /* 00111101 */  op_imm_from_acc,
    /* 00111110 */  unimplemented,
    /* 00111111 */  unimplemented,
    /* 01000000 */  unimplemented,
    /* 01000001 */  unimplemented,
    /* 01000010 */  unimplemented,
    /* 01000011 */  unimplemented,
    /* 01000100 */  unimplemented,
    /* 01000101 */  unimplemented,
    /* 01000110 */  unimplemented,
    /* 01000111 */  unimplemented,
    /* 01001000 */  unimplemented,
    /* 01001001 */  unimplemented,
    /* 01001010 */  unimplemented,
    /* 01001011 */  unimplemented,
    /* 01001100 */  unimplemented,
    /* 01001101 */  unimplemented,
    /* 01001110 */  unimplemented,
    /* 01001111 */  unimplemented,
    /* 01010000 */  unimplemented,
    /* 01010001 */  unimplemented,
    /* 01010010 */  unimplemented,
    /* 01010011 */  unimplemented,
    /* 01010100 */  unimplemented,
    /* 01010101 */  unimplemented,
    /* 01010110 */  unimplemented,
    /* 01010111 */  unimplemented,
    /* 01011000 */  unimplemented,
    /* 01011001 */  unimplemented,
    /* 01011010 */  unimplemented,
    /* 01011011 */  unimplemented,
    /* 01011100 */  unimplemented,
    /* 01011101 */  unimplemented,
    /* 01011110 */  unimplemented,
    /* 01011111 */  unimplemented,
    /* 01100000 */  unimplemented,
    /* 01100001 */  unimplemented,
    /* 01100010 */  unimplemented,
    /* 01100011 */  unimplemented,
    /* 01100100 */  unimplemented,
    /* 01100101 */  unimplemented,
    /* 01100110 */  unimplemented,
    /* 01100111 */  unimplemented,
    /* 01101000 */  unimplemented,
    /* 01101001 */  unimplemented,
    /* 01101010 */  unimplemented,
    /* 01101011 */  unimplemented,
    /* 01101100 */  unimplemented,
    /* 01101101 */  unimplemented,
    /* 01101110 */  unimplemented,
    /* 01101111 */  unimplemented,
    /* 01110000 */  jumps,
    /* 01110001 */  jumps,
    /* 01110010 */  jumps,
    /* 01110011 */  jumps,
    /* 01110100 */  jumps,
    /* 01110101 */  jumps,
    /* 01110110 */  jumps,
    /* 01110111 */  jumps,
    /* 01111000 */  jumps,
    /* 01111001 */  jumps,
    /* 01111010 */  jumps,
    /* 01111011 */  jumps,
    /* 01111100 */  jumps,
    /* 01111101 */  jumps,
    /* 01111110 */  jumps,
    /* 01111111 */  jumps,
    /* 10000000 */  op_imm_from_regmem,
    /* 10000001 */  op_imm_from_regmem,
    /* 10000010 */  op_imm_from_regmem,
    /* 10000011 */  op_imm_from_regmem,
    /* 10000100 */  unimplemented,
    /* 10000101 */  unimplemented,
    /* 10000110 */  unimplemented,
    /* 10000111 */  unimplemented,
    /* 10001000 */  move_regmem_to_from_reg,
    /* 10001001 */  move_regmem_to_from_reg,
    /* 10001010 */  move_regmem_to_from_reg,
    /* 10001011 */  move_regmem_to_from_reg,
    /* 10001100 */  seg_to_regmem,
    /* 10001101 */  unimplemented,
    /* 10001110 */  regmem_to_seg,
    /* 10001111 */  unimplemented,
    /* 10010000 */  unimplemented,
    /* 10010001 */  unimplemented,
    /* 10010010 */  unimplemented,
    /* 10010011 */  unimplemented,
    /* 10010100 */  unimplemented,
    /* 10010101 */  unimplemented,
    /* 10010110 */  unimplemented,
    /* 10010111 */  unimplemented,
    /* 10011000 */  unimplemented,
    /* 10011001 */  unimplemented,
    /* 10011010 */  unimplemented,
    /* 10011011 */  unimplemented,
    /* 10011100 */  unimplemented,
    /* 10011101 */  unimplemented,
    /* 10011110 */  unimplemented,
    /* 10011111 */  unimplemented,
    /* 10100000 */  mem_to_acc,
    /* 10100001 */  mem_to_acc,
    /* 10100010 */  acc_to_mem,
    /* 10100011 */  acc_to_mem,
    /* 10100100 */  unimplemented,
    /* 10100101 */  unimplemented,
    /* 10100110 */  unimplemented,
    /* 10100111 */  unimplemented,
    /* 10101000 */  unimplemented,
    /* 10101001 */  unimplemented,
    /* 10101010 */  unimplemented,
    /* 10101011 */  unimplemented,
    /* 10101100 */  unimplemented,
    /* 10101101 */  unimplemented,
    /* 10101110 */  unimplemented,
    /* 10101111 */  unimplemented,
    /* 10110000 */  imm_to_reg,
    /* 10110001 */  imm_to_reg,
    /* 10110010 */  imm_to_reg,
    /* 10110011 */  imm_to_reg,
    /* 10110100 */  imm_to_reg,
    /* 10110101 */  imm_to_reg,
    /* 10110110 */  imm_to_reg,
    /* 10110111 */  imm_to_reg,
    /* 10111000 */  imm_to_reg,
    /* 10111001 */  imm_to_reg,
    /* 10111010 */  imm_to_reg,
    /* 10111011 */  imm_to_reg,
    /* 10111100 */  imm_to_reg,
    /* 10111101 */  imm_to_reg,
    /* 10111110 */  imm_to_reg,
    /* 10111111 */  imm_to_reg,
    /* 11000000 */  unimplemented,
    /* 11000001 */  unimplemented,
    /* 11000010 */  unimplemented,
    /* 11000011 */  unimplemented,
    /* 11000100 */  unimplemented,
    /* 11000101 */  unimplemented,
    /* 11000110 */  move_imm_to_regmem,
    /* 11000111 */  move_imm_to_regmem,
    /* 11001000 */  unimplemented,
    /* 11001001 */  unimplemented,
    /* 11001010 */  unimplemented,
    /* 11001011 */  unimplemented,
    /* 11001100 */  unimplemented,
    /* 11001101 */  unimplemented,
    /* 11001110 */  unimplemented,
    /* 11001111 */  unimplemented,
    /* 11010000 */  unimplemented,
    /* 11010001 */  unimplemented,
    /* 11010010 */  unimplemented,
    /* 11010011 */  unimplemented,
    /* 11010100 */  unimplemented,
    /* 11010101 */  unimplemented,
    /* 11010110 */  unimplemented,
    /* 11010111 */  unimplemented,
    /* 11011000 */  unimplemented,
    /* 11011001 */  unimplemented,
    /* 11011010 */  unimplemented,
    /* 11011011 */  unimplemented,
    /* 11011100 */  unimplemented,
    /* 11011101 */  unimplemented,
    /* 11011110 */  unimplemented,
    /* 11011111 */  unimplemented,
    /* 11100000 */  loops,
    /* 11100001 */  loops,
    /* 11100010 */  loops,
    /* 11100011 */  loops,
    /* 11100100 */  unimplemented,
    /* 11100101 */  unimplemented,
    /* 11100110 */  unimplemented,
    /* 11100111 */  unimplemented,
    /* 11101000 */  unimplemented,
    /* 11101001 */  unimplemented,
    /* 11101010 */  unimplemented,
    /* 11101011 */  unimplemented,
    /* 11101100 */  unimplemented,
    /* 11101101 */  unimplemented,
    /* 11101110 */  unimplemented,
    /* 11101111 */  unimplemented,
    /* 11110000 */  unimplemented,
    /* 11110001 */  unimplemented,
    /* 11110010 */  unimplemented,
    /* 11110011 */  unimplemented,
    /* 11110100 */  unimplemented,
    /* 11110101 */  unimplemented,
    /* 11110110 */  unimplemented,
    /* 11110111 */  unimplemented,
    /* 11111000 */  unimplemented,
    /* 11111001 */  unimplemented,
    /* 11111010 */  unimplemented,
    /* 11111011 */  unimplemented,
    /* 11111100 */  unimplemented,
    /* 11111101 */  unimplemented,
    /* 11111110 */  unimplemented,
    /* 11111111 */  unimplemented,
};


void disassembly(const char *file_path, const bool &simulation, const bool &clocks) {
    size_t size;
    read_instructions(file_path, size);
    size_t offset;

    std::cout << "; " << file_path << std::endl;
    Instr instr;
    const u8 *b;
    u16 prev_IP;
    while(static_cast<size_t>(IP) < size) {
        b = &MEMORY[IP];
        disassembly_table[*b](b, instr);
        prev_IP = IP;
        IP += (b - &MEMORY[IP]);  // add the amount of bytes read for disassembly
        if (simulation)
            std::cout << sim_instr(instr, prev_IP) << std::endl;
        else if (clocks)
            std::cout << estimate_clocks(instr) << std::endl;
        else
            std::cout << to_string(instr) << std::endl;
    }
    if(simulation) {
        std::cout << std::endl;
        print_all_regs();
        std::cout << "\tflags: " << flags_to_string(FLAGS) << std::endl;
    }
}


int main(int argc, char** argv) {
    memset(MEMORY, 0, 1 << 16);
    if (argc < 2)
        throw std::runtime_error{"No binary input file provided"};
    bool simulation {false};
    bool dump {false};
    bool clocks {false};
    for (u8 i = 2; i < argc; ++i)
        if (std::string{argv[i]} == "-s")
            simulation = true;
        else if (std::string{argv[i]} == "-d")
            dump = true;
        else if (std::string{argv[i]} == "-c")
            clocks = true;
        else
            throw std::runtime_error{"Invalid option"};
    if (simulation && clocks)
        throw std::runtime_error{"Cannot both simulate and estimate clocks (was lazy)"};
    disassembly(argv[1], simulation, clocks);
    if (dump) {
        auto file = fopen("dump.data", "wb");
        assert(fwrite(MEMORY, 1, 1 << 16, file) == 1 << 16);
        fclose(file);
    }
}
