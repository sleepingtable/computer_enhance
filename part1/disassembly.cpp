#include <cassert>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>


using u8  = uint8_t;
using u16 = uint16_t;
using s8  = int8_t;
using s16 = int16_t;


auto read_binary(const char* file_path, size_t &size) {
    auto file = fopen(file_path, "rb");
    if (file == nullptr)
        throw std::runtime_error{std::format(
            "No file {} found",
            std::string{file_path}
        )};

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);

    auto data  = std::make_unique<u8[]>(size);
    fread(data.get(), 1, size, file);
    fclose(file);
    return data;
}


static struct {
    char value[3];
} REG_ENCODING[2][8] = {
    {{"al"}, {"cl"}, {"dl"}, {"bl"}, {"ah"}, {"ch"}, {"dh"}, {"bh"}},  // w = 0
    {{"ax"}, {"cx"}, {"dx"}, {"bx"}, {"sp"}, {"bp"}, {"si"}, {"di"}}   // w = 1
};


static struct {
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


static struct {
    char value[4];
} OPS_ENCODING[8] {"add", "or", "adc", "sbb", "and", "sub", "", "cmp"};


struct {
    char value[5];
} JUMP_ENCODING[16] {
    {"jo"}, {"jno"}, {"jb"}, {"jnb"}, {"je"}, {"jne"}, {"jbe"}, {"jnbe"},
    {"js"}, {"jns"}, {"jp"}, {"jnp"}, {"jl"}, {"jnl"}, {"jle"}, {"jnle"}
};


struct {
    char value[7];
} LOOP_ENCODING[4] { {"loopnz"}, {"loopz"}, {"loop"}, {"jcxz"} };


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
    /* 10001100 */  unimplemented,
    /* 10001101 */  unimplemented,
    /* 10001110 */  unimplemented,
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


void disassembly(const char *file_path) {
    size_t size;
    auto binary = read_binary(file_path, size);
    size_t offset;

    std::cout << "; " << file_path << std::endl;
    Instr instr;
    for(const u8 *b = binary.get(); b < binary.get() + size;) {
        disassembly_table[*b](b, instr);
        std::cout << to_string(instr) << std::endl;
    }
}


int main(int argc, char** argv) {
    if (argc < 2)
        throw std::runtime_error{"No binary input file provided"};
    disassembly(argv[1]);
}
