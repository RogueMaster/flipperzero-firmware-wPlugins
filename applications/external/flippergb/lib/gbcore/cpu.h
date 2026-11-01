#pragma once

#include "address.h"
#include "register.h"
#include "definitions.h"

class Gameboy;

enum class Condition {
    NZ,
    Z,
    NC,
    C,
};

namespace rst {
const u16 rst1 = 0x00;
const u16 rst2 = 0x08;
const u16 rst3 = 0x10;
const u16 rst4 = 0x18;
const u16 rst5 = 0x20;
const u16 rst6 = 0x28;
const u16 rst7 = 0x30;
const u16 rst8 = 0x38;
} // namespace rst

namespace interrupts {
const u16 vblank = 0x40;
const u16 lcdc_status = 0x48;
const u16 timer = 0x50;
const u16 serial = 0x58;
const u16 joypad = 0x60;
} // namespace interrupts


class CPU {
public:
    CPU(Gameboy& inGb);

    /* Initialise registers to the documented DMG post-boot state
     * (the Nintendo boot ROM is not shipped nor emulated) */
    void init_post_boot();

    auto tick() -> Cycles;

    auto execute_opcode(u8 opcode, u16 opcode_pc) -> Cycles;

    auto execute_normal_opcode(u8 opcode, u16 opcode_pc) -> Cycles;
    auto execute_cb_opcode(u8 opcode, u16 opcode_pc) -> Cycles;

    ByteRegister interrupt_flag;
    ByteRegister interrupt_enabled;

    /* save-state support */
    struct State {
        u16 af, bc, de, hl, sp, pc;
        u8 ime, ime_pend, halt, stop;
    };
    void export_state(State* s) const {
        s->af = af.value();
        s->bc = bc.value();
        s->de = de.value();
        s->hl = hl.value();
        s->sp = sp.value();
        s->pc = pc.value();
        s->ime = interrupts_enabled;
        s->ime_pend = ime_pending;
        s->halt = halted;
        s->stop = stopped;
    }
    void import_state(const State* s) {
        af.set(s->af);
        bc.set(s->bc);
        de.set(s->de);
        hl.set(s->hl);
        sp.set(s->sp);
        pc.set(s->pc);
        interrupts_enabled = s->ime != 0;
        ime_pending = s->ime_pend != 0;
        halted = s->halt != 0;
        stopped = s->stop != 0;
        halt_bug = false;
    }

private:
    auto handle_interrupts(u8 fired_interrupts) -> bool; /* true = dispatched */
    auto handle_interrupt(u8 interrupt_bit, u16 interrupt_vector, u8 fired_interrupts) -> bool;

    Gameboy& gb;

    bool interrupts_enabled = false;
    bool ime_pending = false; /* EI takes effect AFTER the next instruction */
    bool halted = false;
    bool stopped = false; /* STOP: woken only by joypad activity */
    bool halt_bug = false; /* HALT w/ IME=0 and pending IF&IE: PC repeats */

public:
    /* Joypad line activity: wakes STOP unconditionally (real DMG behaviour:
     * STOP exits on a joypad line change regardless of IE/IF/IME). */
    void notify_joypad() {
        if(stopped) {
            stopped = false;
            halted = false;
        }
    }

private:

    bool branch_taken = false;

    /* Basic registers */
    ByteRegister a, b, c, d, e, h, l;

    /* 'Group' registers for operations which use two registers as a word */
    RegisterPair af;
    RegisterPair bc;
    RegisterPair de;
    RegisterPair hl;

    /*
     * Flags set dependant on the result of the last operation
     *  0x80 - produced 0
     *  0x40 - was a subtraction
     *  0x20 - lower half of the byte overflowed 15
     *  0x10 - overflowed 255 or underflowed 0 for additions/subtractions
     */
    FlagRegister f;

    void set_flag_zero(bool set);
    void set_flag_subtract(bool set);
    void set_flag_half_carry(bool set);
    void set_flag_carry(bool set);

    /* Note: Not const because this also sets the 'branch_taken' member
     * variable if a branch is taken. This allows the correct cycle
     * count to be used */
    auto is_condition(Condition condition) -> bool;

    /* Program counter */
    WordRegister pc;

    /* Stack pointer */
    WordRegister sp;

    auto get_byte_from_pc() -> u8;
    auto get_signed_byte_from_pc() -> s8;
    auto get_word_from_pc() -> u16;

    template <typename T> void stack_push(const T& reg);
    template <typename T> void stack_pop(T& reg);

    /* Opcode Helper Functions */

    /* ADC */
    void _opcode_adc(u8 value);

    void opcode_adc();
    void opcode_adc(const ByteRegister& reg);
    void opcode_adc(const Address&& addr);

    /* ADD */
    void _opcode_add(u8 reg, u8 value);

    void opcode_add_a();
    void opcode_add_a(const ByteRegister& reg);
    void opcode_add_a(const Address& addr);

    void _opcode_add_hl(u16 value);
    void opcode_add_hl(const RegisterPair& reg_pair);
    void opcode_add_hl(const WordRegister& word_reg);

    void opcode_add_sp();

    void opcode_add_signed();

    /* AND */
    void _opcode_and(u8 value);

    void opcode_and();
    void opcode_and(ByteRegister& reg);
    void opcode_and(Address&& addr);

    /* BIT */
    void _opcode_bit(u8 bit, u8 value);

    void opcode_bit(u8 bit, ByteRegister& reg);
    void opcode_bit(u8 bit, Address&& addr);

    /* CALL */
    void opcode_call();
    void opcode_call(Condition condition);

    /* CCF */
    void opcode_ccf();

    /* CP */
    void _opcode_cp(u8 value);

    void opcode_cp();
    void opcode_cp(const ByteRegister& reg);
    void opcode_cp(const Address& addr);

    /* CPL */
    void opcode_cpl();

    /* DAA */
    void opcode_daa();

    /* DEC */
    void opcode_dec(ByteRegister& reg);
    void opcode_dec(RegisterPair& reg);
    void opcode_dec(WordRegister& reg);
    void opcode_dec(Address&& addr);

    /* DI */
    void opcode_di();

    /* EI */
    void opcode_ei();

    /* INC */
    void opcode_inc(ByteRegister& reg);
    void opcode_inc(RegisterPair& reg);
    void opcode_inc(WordRegister& reg);
    void opcode_inc(Address&& addr);

    /* JP */
    void opcode_jp();
    void opcode_jp(Condition condition);
    void opcode_jp(const Address& addr);

    /* JR */
    void opcode_jr();
    void opcode_jr(Condition condition);

    /* HALT */
    void opcode_halt();

    /* LD */
    void opcode_ld(ByteRegister& reg);
    void opcode_ld(ByteRegister& reg, const ByteRegister& byte_reg);
    void opcode_ld(ByteRegister& reg, const Address& address);

    void opcode_ld(RegisterPair& reg);

    void opcode_ld(WordRegister& reg);
    void opcode_ld(WordRegister& reg, const RegisterPair& reg_pair);

    void opcode_ld(const Address& address);
    void opcode_ld(const Address& address, const ByteRegister& byte_reg);
    void opcode_ld(const Address& address, const WordRegister& word_reg);

    // (nn), A
    void opcode_ld_to_addr(const ByteRegister& reg);
    void opcode_ld_from_addr(ByteRegister& reg);

    /* LDD */
    auto _opcode_ldd(u8 value) -> u8;

    void opcode_ldd(ByteRegister& reg, const Address& address);
    void opcode_ldd(const Address& address, const ByteRegister& reg);

    /* LDH */
    // A, (n)
    void opcode_ldh_into_a();
    // (n), A
    void opcode_ldh_into_data();
    // (reg), A
    void opcode_ldh_into_c();
    // A, (reg)
    void opcode_ldh_c_into_a();

    /* LDHL */
    void opcode_ldhl();

    /* LDI */
    void opcode_ldi(ByteRegister& reg, const Address& address);
    void opcode_ldi(const Address& address, const ByteRegister& reg);

    /* NOP */
    void opcode_nop();

    /* OR */
    void _opcode_or(u8 value);

    void opcode_or();
    void opcode_or(const ByteRegister& reg);
    void opcode_or(const Address& addr);

    /* POP */
    void opcode_pop(RegisterPair& reg);

    /* PUSH */
    void opcode_push(const RegisterPair& reg);

    /* RES */
    void opcode_res(u8 bit, ByteRegister& reg);
    void opcode_res(u8 bit, Address&& addr);

    /* RET */
    void opcode_ret();
    void opcode_ret(Condition condition);

    /* RETI */
    void opcode_reti();

    /* RL */
    auto _opcode_rl(u8 value) -> u8;

    void opcode_rla();
    void opcode_rl(ByteRegister& reg);
    void opcode_rl(Address&& addr);

    /* RLC */
    auto _opcode_rlc(u8 value) -> u8;

    void opcode_rlca();
    void opcode_rlc(ByteRegister& reg);
    void opcode_rlc(Address&& addr);

    /* RR */
    auto _opcode_rr(u8 value) -> u8;

    void opcode_rra();
    void opcode_rr(ByteRegister& reg);
    void opcode_rr(Address&& addr);

    /* RRC */
    auto _opcode_rrc(u8 value) -> u8;

    void opcode_rrca();
    void opcode_rrc(ByteRegister& reg);
    void opcode_rrc(Address&& addr);

    /* RST */
    void opcode_rst(u8 offset);

    /* SBC */
    void _opcode_sbc(u8 value);

    void opcode_sbc();
    void opcode_sbc(ByteRegister& reg);
    void opcode_sbc(Address&& addr);

    /* SCF */
    void opcode_scf();

    /* SET */
    void opcode_set(u8 bit, ByteRegister& reg);
    void opcode_set(u8 bit, Address&& addr);

    /* SLA */
    auto _opcode_sla(u8 value) -> u8;

    void opcode_sla(ByteRegister& reg);
    void opcode_sla(Address&& addr);

    /* SRA */
    auto _opcode_sra(u8 value) -> u8;

    void opcode_sra(ByteRegister& reg);
    void opcode_sra(Address&& addr);

    /* SRL */
    auto _opcode_srl(u8 value) -> u8;

    void opcode_srl(ByteRegister& reg);
    void opcode_srl(Address&& addr);

    /* STOP */
    void opcode_stop();

    /* SUB */
    void _opcode_sub(u8 value);

    void opcode_sub();
    void opcode_sub(ByteRegister& reg);
    void opcode_sub(Address&& addr);

    /* SWAP */
    auto _opcode_swap(u8 value) -> u8;

    void opcode_swap(ByteRegister& reg);
    void opcode_swap(Address&& addr);

    /* XOR */
    void _opcode_xor(u8 value);

    void opcode_xor();
    void opcode_xor(const ByteRegister& reg);
    void opcode_xor(const Address& addr);


    /* clang-format off */
    /* Opcodes */
    void opcode_00(); void opcode_01(); void opcode_02(); void opcode_03(); void opcode_04(); void opcode_05(); void opcode_06(); void opcode_07(); void opcode_08(); void opcode_09(); void opcode_0A(); void opcode_0B(); void opcode_0C(); void opcode_0D(); void opcode_0E(); void opcode_0F();
    void opcode_10(); void opcode_11(); void opcode_12(); void opcode_13(); void opcode_14(); void opcode_15(); void opcode_16(); void opcode_17(); void opcode_18(); void opcode_19(); void opcode_1A(); void opcode_1B(); void opcode_1C(); void opcode_1D(); void opcode_1E(); void opcode_1F();
    void opcode_20(); void opcode_21(); void opcode_22(); void opcode_23(); void opcode_24(); void opcode_25(); void opcode_26(); void opcode_27(); void opcode_28(); void opcode_29(); void opcode_2A(); void opcode_2B(); void opcode_2C(); void opcode_2D(); void opcode_2E(); void opcode_2F();
    void opcode_30(); void opcode_31(); void opcode_32(); void opcode_33(); void opcode_34(); void opcode_35(); void opcode_36(); void opcode_37(); void opcode_38(); void opcode_39(); void opcode_3A(); void opcode_3B(); void opcode_3C(); void opcode_3D(); void opcode_3E(); void opcode_3F();
    void opcode_40(); void opcode_41(); void opcode_42(); void opcode_43(); void opcode_44(); void opcode_45(); void opcode_46(); void opcode_47(); void opcode_48(); void opcode_49(); void opcode_4A(); void opcode_4B(); void opcode_4C(); void opcode_4D(); void opcode_4E(); void opcode_4F();
    void opcode_50(); void opcode_51(); void opcode_52(); void opcode_53(); void opcode_54(); void opcode_55(); void opcode_56(); void opcode_57(); void opcode_58(); void opcode_59(); void opcode_5A(); void opcode_5B(); void opcode_5C(); void opcode_5D(); void opcode_5E(); void opcode_5F();
    void opcode_60(); void opcode_61(); void opcode_62(); void opcode_63(); void opcode_64(); void opcode_65(); void opcode_66(); void opcode_67(); void opcode_68(); void opcode_69(); void opcode_6A(); void opcode_6B(); void opcode_6C(); void opcode_6D(); void opcode_6E(); void opcode_6F();
    void opcode_70(); void opcode_71(); void opcode_72(); void opcode_73(); void opcode_74(); void opcode_75(); void opcode_76(); void opcode_77(); void opcode_78(); void opcode_79(); void opcode_7A(); void opcode_7B(); void opcode_7C(); void opcode_7D(); void opcode_7E(); void opcode_7F();
    void opcode_80(); void opcode_81(); void opcode_82(); void opcode_83(); void opcode_84(); void opcode_85(); void opcode_86(); void opcode_87(); void opcode_88(); void opcode_89(); void opcode_8A(); void opcode_8B(); void opcode_8C(); void opcode_8D(); void opcode_8E(); void opcode_8F();
    void opcode_90(); void opcode_91(); void opcode_92(); void opcode_93(); void opcode_94(); void opcode_95(); void opcode_96(); void opcode_97(); void opcode_98(); void opcode_99(); void opcode_9A(); void opcode_9B(); void opcode_9C(); void opcode_9D(); void opcode_9E(); void opcode_9F();
    void opcode_A0(); void opcode_A1(); void opcode_A2(); void opcode_A3(); void opcode_A4(); void opcode_A5(); void opcode_A6(); void opcode_A7(); void opcode_A8(); void opcode_A9(); void opcode_AA(); void opcode_AB(); void opcode_AC(); void opcode_AD(); void opcode_AE(); void opcode_AF();
    void opcode_B0(); void opcode_B1(); void opcode_B2(); void opcode_B3(); void opcode_B4(); void opcode_B5(); void opcode_B6(); void opcode_B7(); void opcode_B8(); void opcode_B9(); void opcode_BA(); void opcode_BB(); void opcode_BC(); void opcode_BD(); void opcode_BE(); void opcode_BF();
    void opcode_C0(); void opcode_C1(); void opcode_C2(); void opcode_C3(); void opcode_C4(); void opcode_C5(); void opcode_C6(); void opcode_C7(); void opcode_C8(); void opcode_C9(); void opcode_CA(); void opcode_CB(); void opcode_CC(); void opcode_CD(); void opcode_CE(); void opcode_CF();
    void opcode_D0(); void opcode_D1(); void opcode_D2(); void opcode_D3(); void opcode_D4(); void opcode_D5(); void opcode_D6(); void opcode_D7(); void opcode_D8(); void opcode_D9(); void opcode_DA(); void opcode_DB(); void opcode_DC(); void opcode_DD(); void opcode_DE(); void opcode_DF();
    void opcode_E0(); void opcode_E1(); void opcode_E2(); void opcode_E3(); void opcode_E4(); void opcode_E5(); void opcode_E6(); void opcode_E7(); void opcode_E8(); void opcode_E9(); void opcode_EA(); void opcode_EB(); void opcode_EC(); void opcode_ED(); void opcode_EE(); void opcode_EF();
    void opcode_F0(); void opcode_F1(); void opcode_F2(); void opcode_F3(); void opcode_F4(); void opcode_F5(); void opcode_F6(); void opcode_F7(); void opcode_F8(); void opcode_F9(); void opcode_FA(); void opcode_FB(); void opcode_FC(); void opcode_FD(); void opcode_FE(); void opcode_FF();

    /* CB Opcodes */
    void opcode_CB_00(); void opcode_CB_01(); void opcode_CB_02(); void opcode_CB_03(); void opcode_CB_04(); void opcode_CB_05(); void opcode_CB_06(); void opcode_CB_07(); void opcode_CB_08(); void opcode_CB_09(); void opcode_CB_0A(); void opcode_CB_0B(); void opcode_CB_0C(); void opcode_CB_0D(); void opcode_CB_0E(); void opcode_CB_0F();
    void opcode_CB_10(); void opcode_CB_11(); void opcode_CB_12(); void opcode_CB_13(); void opcode_CB_14(); void opcode_CB_15(); void opcode_CB_16(); void opcode_CB_17(); void opcode_CB_18(); void opcode_CB_19(); void opcode_CB_1A(); void opcode_CB_1B(); void opcode_CB_1C(); void opcode_CB_1D(); void opcode_CB_1E(); void opcode_CB_1F();
    void opcode_CB_20(); void opcode_CB_21(); void opcode_CB_22(); void opcode_CB_23(); void opcode_CB_24(); void opcode_CB_25(); void opcode_CB_26(); void opcode_CB_27(); void opcode_CB_28(); void opcode_CB_29(); void opcode_CB_2A(); void opcode_CB_2B(); void opcode_CB_2C(); void opcode_CB_2D(); void opcode_CB_2E(); void opcode_CB_2F();
    void opcode_CB_30(); void opcode_CB_31(); void opcode_CB_32(); void opcode_CB_33(); void opcode_CB_34(); void opcode_CB_35(); void opcode_CB_36(); void opcode_CB_37(); void opcode_CB_38(); void opcode_CB_39(); void opcode_CB_3A(); void opcode_CB_3B(); void opcode_CB_3C(); void opcode_CB_3D(); void opcode_CB_3E(); void opcode_CB_3F();
    void opcode_CB_40(); void opcode_CB_41(); void opcode_CB_42(); void opcode_CB_43(); void opcode_CB_44(); void opcode_CB_45(); void opcode_CB_46(); void opcode_CB_47(); void opcode_CB_48(); void opcode_CB_49(); void opcode_CB_4A(); void opcode_CB_4B(); void opcode_CB_4C(); void opcode_CB_4D(); void opcode_CB_4E(); void opcode_CB_4F();
    void opcode_CB_50(); void opcode_CB_51(); void opcode_CB_52(); void opcode_CB_53(); void opcode_CB_54(); void opcode_CB_55(); void opcode_CB_56(); void opcode_CB_57(); void opcode_CB_58(); void opcode_CB_59(); void opcode_CB_5A(); void opcode_CB_5B(); void opcode_CB_5C(); void opcode_CB_5D(); void opcode_CB_5E(); void opcode_CB_5F();
    void opcode_CB_60(); void opcode_CB_61(); void opcode_CB_62(); void opcode_CB_63(); void opcode_CB_64(); void opcode_CB_65(); void opcode_CB_66(); void opcode_CB_67(); void opcode_CB_68(); void opcode_CB_69(); void opcode_CB_6A(); void opcode_CB_6B(); void opcode_CB_6C(); void opcode_CB_6D(); void opcode_CB_6E(); void opcode_CB_6F();
    void opcode_CB_70(); void opcode_CB_71(); void opcode_CB_72(); void opcode_CB_73(); void opcode_CB_74(); void opcode_CB_75(); void opcode_CB_76(); void opcode_CB_77(); void opcode_CB_78(); void opcode_CB_79(); void opcode_CB_7A(); void opcode_CB_7B(); void opcode_CB_7C(); void opcode_CB_7D(); void opcode_CB_7E(); void opcode_CB_7F();
    void opcode_CB_80(); void opcode_CB_81(); void opcode_CB_82(); void opcode_CB_83(); void opcode_CB_84(); void opcode_CB_85(); void opcode_CB_86(); void opcode_CB_87(); void opcode_CB_88(); void opcode_CB_89(); void opcode_CB_8A(); void opcode_CB_8B(); void opcode_CB_8C(); void opcode_CB_8D(); void opcode_CB_8E(); void opcode_CB_8F();
    void opcode_CB_90(); void opcode_CB_91(); void opcode_CB_92(); void opcode_CB_93(); void opcode_CB_94(); void opcode_CB_95(); void opcode_CB_96(); void opcode_CB_97(); void opcode_CB_98(); void opcode_CB_99(); void opcode_CB_9A(); void opcode_CB_9B(); void opcode_CB_9C(); void opcode_CB_9D(); void opcode_CB_9E(); void opcode_CB_9F();
    void opcode_CB_A0(); void opcode_CB_A1(); void opcode_CB_A2(); void opcode_CB_A3(); void opcode_CB_A4(); void opcode_CB_A5(); void opcode_CB_A6(); void opcode_CB_A7(); void opcode_CB_A8(); void opcode_CB_A9(); void opcode_CB_AA(); void opcode_CB_AB(); void opcode_CB_AC(); void opcode_CB_AD(); void opcode_CB_AE(); void opcode_CB_AF();
    void opcode_CB_B0(); void opcode_CB_B1(); void opcode_CB_B2(); void opcode_CB_B3(); void opcode_CB_B4(); void opcode_CB_B5(); void opcode_CB_B6(); void opcode_CB_B7(); void opcode_CB_B8(); void opcode_CB_B9(); void opcode_CB_BA(); void opcode_CB_BB(); void opcode_CB_BC(); void opcode_CB_BD(); void opcode_CB_BE(); void opcode_CB_BF();
    void opcode_CB_C0(); void opcode_CB_C1(); void opcode_CB_C2(); void opcode_CB_C3(); void opcode_CB_C4(); void opcode_CB_C5(); void opcode_CB_C6(); void opcode_CB_C7(); void opcode_CB_C8(); void opcode_CB_C9(); void opcode_CB_CA(); void opcode_CB_CB(); void opcode_CB_CC(); void opcode_CB_CD(); void opcode_CB_CE(); void opcode_CB_CF();
    void opcode_CB_D0(); void opcode_CB_D1(); void opcode_CB_D2(); void opcode_CB_D3(); void opcode_CB_D4(); void opcode_CB_D5(); void opcode_CB_D6(); void opcode_CB_D7(); void opcode_CB_D8(); void opcode_CB_D9(); void opcode_CB_DA(); void opcode_CB_DB(); void opcode_CB_DC(); void opcode_CB_DD(); void opcode_CB_DE(); void opcode_CB_DF();
    void opcode_CB_E0(); void opcode_CB_E1(); void opcode_CB_E2(); void opcode_CB_E3(); void opcode_CB_E4(); void opcode_CB_E5(); void opcode_CB_E6(); void opcode_CB_E7(); void opcode_CB_E8(); void opcode_CB_E9(); void opcode_CB_EA(); void opcode_CB_EB(); void opcode_CB_EC(); void opcode_CB_ED(); void opcode_CB_EE(); void opcode_CB_EF();
    void opcode_CB_F0(); void opcode_CB_F1(); void opcode_CB_F2(); void opcode_CB_F3(); void opcode_CB_F4(); void opcode_CB_F5(); void opcode_CB_F6(); void opcode_CB_F7(); void opcode_CB_F8(); void opcode_CB_F9(); void opcode_CB_FA(); void opcode_CB_FB(); void opcode_CB_FC(); void opcode_CB_FD(); void opcode_CB_FE(); void opcode_CB_FF();
    /* clang-format on */

    friend class Debugger;
};
