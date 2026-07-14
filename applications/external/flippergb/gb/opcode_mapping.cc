#include "cpu.h"
/* clang-format off */

/**
 * This section contains functions which map to actual opcodes which are executed
 * by the Gameboy's processor.
 */

void CPU::opcode_00() { opcode_nop(); }
void CPU::opcode_01() { opcode_ld(bc); }
void CPU::opcode_02() { opcode_ld(Address(bc), a); }
void CPU::opcode_03() { opcode_inc(bc); }
void CPU::opcode_04() { opcode_inc(b); }
void CPU::opcode_05() { opcode_dec(b); }
void CPU::opcode_06() { opcode_ld(b); }
void CPU::opcode_07() { opcode_rlca(); }
void CPU::opcode_08() { u16 nn = get_word_from_pc(); opcode_ld(Address(nn), sp); }
void CPU::opcode_09() { opcode_add_hl(bc); }
void CPU::opcode_0A() { opcode_ld(a, Address(bc)); }
void CPU::opcode_0B() { opcode_dec(bc); }
void CPU::opcode_0C() { opcode_inc(c); }
void CPU::opcode_0D() { opcode_dec(c); }
void CPU::opcode_0E() { opcode_ld(c); }
void CPU::opcode_0F() { opcode_rrca(); }
void CPU::opcode_10() { opcode_stop(); }
void CPU::opcode_11() { opcode_ld(de); }
void CPU::opcode_12() { opcode_ld(Address(de), a); }
void CPU::opcode_13() { opcode_inc(de); }
void CPU::opcode_14() { opcode_inc(d); }
void CPU::opcode_15() { opcode_dec(d); }
void CPU::opcode_16() { opcode_ld(d); }
void CPU::opcode_17() { opcode_rla(); }
void CPU::opcode_18() { opcode_jr(); }
void CPU::opcode_19() { opcode_add_hl(de); }
void CPU::opcode_1A() { opcode_ld(a, Address(de)); }
void CPU::opcode_1B() { opcode_dec(de); }
void CPU::opcode_1C() { opcode_inc(e); }
void CPU::opcode_1D() { opcode_dec(e); }
void CPU::opcode_1E() { opcode_ld(e); }
void CPU::opcode_1F() { opcode_rra(); }
void CPU::opcode_20() { opcode_jr(Condition::NZ); }
void CPU::opcode_21() { opcode_ld(hl); }
void CPU::opcode_22() { opcode_ldi(Address(hl), a); }
void CPU::opcode_23() { opcode_inc(hl); }
void CPU::opcode_24() { opcode_inc(h); }
void CPU::opcode_25() { opcode_dec(h); }
void CPU::opcode_26() { opcode_ld(h); }
void CPU::opcode_27() { opcode_daa(); }
void CPU::opcode_28() { opcode_jr(Condition::Z); }
void CPU::opcode_29() { opcode_add_hl(hl); }
void CPU::opcode_2A() { opcode_ldi(a, Address(hl)); }
void CPU::opcode_2B() { opcode_dec(hl); }
void CPU::opcode_2C() { opcode_inc(l); }
void CPU::opcode_2D() { opcode_dec(l); }
void CPU::opcode_2E() { opcode_ld(l); }
void CPU::opcode_2F() { opcode_cpl(); }
void CPU::opcode_30() { opcode_jr(Condition::NC); }
void CPU::opcode_31() { opcode_ld(sp); }
void CPU::opcode_32() { opcode_ldd(Address(hl), a); }
void CPU::opcode_33() { opcode_inc(sp); }
void CPU::opcode_34() { opcode_inc(Address(hl)); }
void CPU::opcode_35() { opcode_dec(Address(hl)); }
void CPU::opcode_36() { opcode_ld(Address(hl)); }
void CPU::opcode_37() { opcode_scf(); }
void CPU::opcode_38() { opcode_jr(Condition::C); }
void CPU::opcode_39() { opcode_add_hl(sp); }
void CPU::opcode_3A() { opcode_ldd(a, Address(hl)); }
void CPU::opcode_3B() { opcode_dec(sp); }
void CPU::opcode_3C() { opcode_inc(a); }
void CPU::opcode_3D() { opcode_dec(a); }
void CPU::opcode_3E() { opcode_ld(a); }
void CPU::opcode_3F() { opcode_ccf(); }
void CPU::opcode_40() { opcode_ld(b, b); }
void CPU::opcode_41() { opcode_ld(b, c); }
void CPU::opcode_42() { opcode_ld(b, d); }
void CPU::opcode_43() { opcode_ld(b, e); }
void CPU::opcode_44() { opcode_ld(b, h); }
void CPU::opcode_45() { opcode_ld(b, l); }
void CPU::opcode_46() { opcode_ld(b, Address(hl)); }
void CPU::opcode_47() { opcode_ld(b, a); }
void CPU::opcode_48() { opcode_ld(c, b); }
void CPU::opcode_49() { opcode_ld(c, c); }
void CPU::opcode_4A() { opcode_ld(c, d); }
void CPU::opcode_4B() { opcode_ld(c, e); }
void CPU::opcode_4C() { opcode_ld(c, h); }
void CPU::opcode_4D() { opcode_ld(c, l); }
void CPU::opcode_4E() { opcode_ld(c, Address(hl)); }
void CPU::opcode_4F() { opcode_ld(c, a); }
void CPU::opcode_50() { opcode_ld(d, b); }
void CPU::opcode_51() { opcode_ld(d, c); }
void CPU::opcode_52() { opcode_ld(d, d); }
void CPU::opcode_53() { opcode_ld(d, e); }
void CPU::opcode_54() { opcode_ld(d, h); }
void CPU::opcode_55() { opcode_ld(d, l); }
void CPU::opcode_56() { opcode_ld(d, Address(hl)); }
void CPU::opcode_57() { opcode_ld(d, a); }
void CPU::opcode_58() { opcode_ld(e, b); }
void CPU::opcode_59() { opcode_ld(e, c); }
void CPU::opcode_5A() { opcode_ld(e, d); }
void CPU::opcode_5B() { opcode_ld(e, e); }
void CPU::opcode_5C() { opcode_ld(e, h); }
void CPU::opcode_5D() { opcode_ld(e, l); }
void CPU::opcode_5E() { opcode_ld(e, Address(hl)); }
void CPU::opcode_5F() { opcode_ld(e, a); }
void CPU::opcode_60() { opcode_ld(h, b); }
void CPU::opcode_61() { opcode_ld(h, c); }
void CPU::opcode_62() { opcode_ld(h, d); }
void CPU::opcode_63() { opcode_ld(h, e); }
void CPU::opcode_64() { opcode_ld(h, h); }
void CPU::opcode_65() { opcode_ld(h, l); }
void CPU::opcode_66() { opcode_ld(h, Address(hl)); }
void CPU::opcode_67() { opcode_ld(h, a); }
void CPU::opcode_68() { opcode_ld(l, b); }
void CPU::opcode_69() { opcode_ld(l, c); }
void CPU::opcode_6A() { opcode_ld(l, d); }
void CPU::opcode_6B() { opcode_ld(l, e); }
void CPU::opcode_6C() { opcode_ld(l, h); }
void CPU::opcode_6D() { opcode_ld(l, l); }
void CPU::opcode_6E() { opcode_ld(l, Address(hl)); }
void CPU::opcode_6F() { opcode_ld(l, a); }
void CPU::opcode_70() { opcode_ld(Address(hl), b); }
void CPU::opcode_71() { opcode_ld(Address(hl), c); }
void CPU::opcode_72() { opcode_ld(Address(hl), d); }
void CPU::opcode_73() { opcode_ld(Address(hl), e); }
void CPU::opcode_74() { opcode_ld(Address(hl), h); }
void CPU::opcode_75() { opcode_ld(Address(hl), l); }
void CPU::opcode_76() { opcode_halt(); }
void CPU::opcode_77() { opcode_ld(Address(hl), a); }
void CPU::opcode_78() { opcode_ld(a, b); }
void CPU::opcode_79() { opcode_ld(a, c); }
void CPU::opcode_7A() { opcode_ld(a, d); }
void CPU::opcode_7B() { opcode_ld(a, e); }
void CPU::opcode_7C() { opcode_ld(a, h); }
void CPU::opcode_7D() { opcode_ld(a, l); }
void CPU::opcode_7E() { opcode_ld(a, Address(hl)); }
void CPU::opcode_7F() { opcode_ld(a, a); }
void CPU::opcode_80() { opcode_add_a(b); }
void CPU::opcode_81() { opcode_add_a(c); }
void CPU::opcode_82() { opcode_add_a(d); }
void CPU::opcode_83() { opcode_add_a(e); }
void CPU::opcode_84() { opcode_add_a(h); }
void CPU::opcode_85() { opcode_add_a(l); }
void CPU::opcode_86() { opcode_add_a(Address(hl)); }
void CPU::opcode_87() { opcode_add_a(a); }
void CPU::opcode_88() { opcode_adc(b); }
void CPU::opcode_89() { opcode_adc(c); }
void CPU::opcode_8A() { opcode_adc(d); }
void CPU::opcode_8B() { opcode_adc(e); }
void CPU::opcode_8C() { opcode_adc(h); }
void CPU::opcode_8D() { opcode_adc(l); }
void CPU::opcode_8E() { opcode_adc(Address(hl)); }
void CPU::opcode_8F() { opcode_adc(a); }
void CPU::opcode_90() { opcode_sub(b); }
void CPU::opcode_91() { opcode_sub(c); }
void CPU::opcode_92() { opcode_sub(d); }
void CPU::opcode_93() { opcode_sub(e); }
void CPU::opcode_94() { opcode_sub(h); }
void CPU::opcode_95() { opcode_sub(l); }
void CPU::opcode_96() { opcode_sub(Address(hl)); }
void CPU::opcode_97() { opcode_sub(a); }
void CPU::opcode_98() { opcode_sbc(b); }
void CPU::opcode_99() { opcode_sbc(c); }
void CPU::opcode_9A() { opcode_sbc(d); }
void CPU::opcode_9B() { opcode_sbc(e); }
void CPU::opcode_9C() { opcode_sbc(h); }
void CPU::opcode_9D() { opcode_sbc(l); }
void CPU::opcode_9E() { opcode_sbc(Address(hl)); }
void CPU::opcode_9F() { opcode_sbc(a); }
void CPU::opcode_A0() { opcode_and(b); }
void CPU::opcode_A1() { opcode_and(c); }
void CPU::opcode_A2() { opcode_and(d); }
void CPU::opcode_A3() { opcode_and(e); }
void CPU::opcode_A4() { opcode_and(h); }
void CPU::opcode_A5() { opcode_and(l); }
void CPU::opcode_A6() { opcode_and(Address(hl)); }
void CPU::opcode_A7() { opcode_and(a); }
void CPU::opcode_A8() { opcode_xor(b); }
void CPU::opcode_A9() { opcode_xor(c); }
void CPU::opcode_AA() { opcode_xor(d); }
void CPU::opcode_AB() { opcode_xor(e); }
void CPU::opcode_AC() { opcode_xor(h); }
void CPU::opcode_AD() { opcode_xor(l); }
void CPU::opcode_AE() { opcode_xor(Address(hl)); }
void CPU::opcode_AF() { opcode_xor(a); }
void CPU::opcode_B0() { opcode_or(b); }
void CPU::opcode_B1() { opcode_or(c); }
void CPU::opcode_B2() { opcode_or(d); }
void CPU::opcode_B3() { opcode_or(e); }
void CPU::opcode_B4() { opcode_or(h); }
void CPU::opcode_B5() { opcode_or(l); }
void CPU::opcode_B6() { opcode_or(Address(hl)); }
void CPU::opcode_B7() { opcode_or(a); }
void CPU::opcode_B8() { opcode_cp(b); }
void CPU::opcode_B9() { opcode_cp(c); }
void CPU::opcode_BA() { opcode_cp(d); }
void CPU::opcode_BB() { opcode_cp(e); }
void CPU::opcode_BC() { opcode_cp(h); }
void CPU::opcode_BD() { opcode_cp(l); }
void CPU::opcode_BE() { opcode_cp(Address(hl)); }
void CPU::opcode_BF() { opcode_cp(a); }
void CPU::opcode_C0() { opcode_ret(Condition::NZ); }
void CPU::opcode_C1() { opcode_pop(bc); }
void CPU::opcode_C2() { opcode_jp(Condition::NZ); }
void CPU::opcode_C3() { opcode_jp(); }
void CPU::opcode_C4() { opcode_call(Condition::NZ); }
void CPU::opcode_C5() { opcode_push(bc); }
void CPU::opcode_C6() { opcode_add_a(); }
void CPU::opcode_C7() { opcode_rst(rst::rst1); }
void CPU::opcode_C8() { opcode_ret(Condition::Z); }
void CPU::opcode_C9() { opcode_ret(); }
void CPU::opcode_CA() { opcode_jp(Condition::Z); }
void CPU::opcode_CB() { /* External Ops */ }
void CPU::opcode_CC() { opcode_call(Condition::Z); }
void CPU::opcode_CD() { opcode_call(); }
void CPU::opcode_CE() { opcode_adc(); }
void CPU::opcode_CF() { opcode_rst(rst::rst2); }
void CPU::opcode_D0() { opcode_ret(Condition::NC); }
void CPU::opcode_D1() { opcode_pop(de); }
void CPU::opcode_D2() { opcode_jp(Condition::NC); }
void CPU::opcode_D3() { /* Undefined */ }
void CPU::opcode_D4() { opcode_call(Condition::NC); }
void CPU::opcode_D5() { opcode_push(de); }
void CPU::opcode_D6() { opcode_sub(); }
void CPU::opcode_D7() { opcode_rst(rst::rst3); }
void CPU::opcode_D8() { opcode_ret(Condition::C); }
void CPU::opcode_D9() { opcode_reti(); }
void CPU::opcode_DA() { opcode_jp(Condition::C); }
void CPU::opcode_DB() { /* Undefined */ }
void CPU::opcode_DC() { opcode_call(Condition::C); }
void CPU::opcode_DD() { /* Undefined */ }
void CPU::opcode_DE() { opcode_sbc(); }
void CPU::opcode_DF() { opcode_rst(rst::rst4); }
void CPU::opcode_E0() { opcode_ldh_into_data(); }
void CPU::opcode_E1() { opcode_pop(hl); }
void CPU::opcode_E2() { opcode_ldh_into_c(); }
void CPU::opcode_E3() { /* Undefined */ }
void CPU::opcode_E4() { /* Undefined */ }
void CPU::opcode_E5() { opcode_push(hl); }
void CPU::opcode_E6() { opcode_and(); }
void CPU::opcode_E7() { opcode_rst(rst::rst5); }
void CPU::opcode_E8() { opcode_add_sp(); }
void CPU::opcode_E9() { opcode_jp(Address(hl)); }
void CPU::opcode_EA() { opcode_ld_to_addr(a); }
void CPU::opcode_EB() { /* Undefined */ }
void CPU::opcode_EC() { /* Undefined */ }
void CPU::opcode_ED() { /* Undefined */ }
void CPU::opcode_EE() { opcode_xor(); }
void CPU::opcode_EF() { opcode_rst(rst::rst6); }
void CPU::opcode_F0() { opcode_ldh_into_a(); }
void CPU::opcode_F1() { opcode_pop(af); }
void CPU::opcode_F2() { opcode_ldh_c_into_a(); }
void CPU::opcode_F3() { opcode_di(); }
void CPU::opcode_F4() { /* Undefined */ }
void CPU::opcode_F5() { opcode_push(af); }
void CPU::opcode_F6() { opcode_or(); }
void CPU::opcode_F7() { opcode_rst(rst::rst7); }
void CPU::opcode_F8() { opcode_ldhl(); }
void CPU::opcode_F9() { opcode_ld(sp, hl); }
void CPU::opcode_FA() { opcode_ld_from_addr(a); }
void CPU::opcode_FB() { opcode_ei(); }
void CPU::opcode_FC() { /* Undefined */ }
void CPU::opcode_FD() { /* Undefined */ }
void CPU::opcode_FE() { opcode_cp(); }
void CPU::opcode_FF() { opcode_rst(rst::rst8); }

/**
 * This section contains two-byte opcodes, which are triggered when prefixed with
 * the CB instruction above.
 */

void CPU::opcode_CB_00() { opcode_rlc(b); }
void CPU::opcode_CB_01() { opcode_rlc(c); }
void CPU::opcode_CB_02() { opcode_rlc(d); }
void CPU::opcode_CB_03() { opcode_rlc(e); }
void CPU::opcode_CB_04() { opcode_rlc(h); }
void CPU::opcode_CB_05() { opcode_rlc(l); }
void CPU::opcode_CB_06() { opcode_rlc(Address(hl)); }
void CPU::opcode_CB_07() { opcode_rlc(a); }
void CPU::opcode_CB_08() { opcode_rrc(b); }
void CPU::opcode_CB_09() { opcode_rrc(c); }
void CPU::opcode_CB_0A() { opcode_rrc(d); }
void CPU::opcode_CB_0B() { opcode_rrc(e); }
void CPU::opcode_CB_0C() { opcode_rrc(h); }
void CPU::opcode_CB_0D() { opcode_rrc(l); }
void CPU::opcode_CB_0E() { opcode_rrc(Address(hl)); }
void CPU::opcode_CB_0F() { opcode_rrc(a); }
void CPU::opcode_CB_10() { opcode_rl(b); }
void CPU::opcode_CB_11() { opcode_rl(c); }
void CPU::opcode_CB_12() { opcode_rl(d); }
void CPU::opcode_CB_13() { opcode_rl(e); }
void CPU::opcode_CB_14() { opcode_rl(h); }
void CPU::opcode_CB_15() { opcode_rl(l); }
void CPU::opcode_CB_16() { opcode_rl(Address(hl)); }
void CPU::opcode_CB_17() { opcode_rl(a); }
void CPU::opcode_CB_18() { opcode_rr(b); }
void CPU::opcode_CB_19() { opcode_rr(c); }
void CPU::opcode_CB_1A() { opcode_rr(d); }
void CPU::opcode_CB_1B() { opcode_rr(e); }
void CPU::opcode_CB_1C() { opcode_rr(h); }
void CPU::opcode_CB_1D() { opcode_rr(l); }
void CPU::opcode_CB_1E() { opcode_rr(Address(hl)); }
void CPU::opcode_CB_1F() { opcode_rr(a); }
void CPU::opcode_CB_20() { opcode_sla(b); }
void CPU::opcode_CB_21() { opcode_sla(c); }
void CPU::opcode_CB_22() { opcode_sla(d); }
void CPU::opcode_CB_23() { opcode_sla(e); }
void CPU::opcode_CB_24() { opcode_sla(h); }
void CPU::opcode_CB_25() { opcode_sla(l); }
void CPU::opcode_CB_26() { opcode_sla(Address(hl)); }
void CPU::opcode_CB_27() { opcode_sla(a); }
void CPU::opcode_CB_28() { opcode_sra(b); }
void CPU::opcode_CB_29() { opcode_sra(c); }
void CPU::opcode_CB_2A() { opcode_sra(d); }
void CPU::opcode_CB_2B() { opcode_sra(e); }
void CPU::opcode_CB_2C() { opcode_sra(h); }
void CPU::opcode_CB_2D() { opcode_sra(l); }
void CPU::opcode_CB_2E() { opcode_sra(Address(hl)); }
void CPU::opcode_CB_2F() { opcode_sra(a); }
void CPU::opcode_CB_30() { opcode_swap(b); }
void CPU::opcode_CB_31() { opcode_swap(c); }
void CPU::opcode_CB_32() { opcode_swap(d); }
void CPU::opcode_CB_33() { opcode_swap(e); }
void CPU::opcode_CB_34() { opcode_swap(h); }
void CPU::opcode_CB_35() { opcode_swap(l); }
void CPU::opcode_CB_36() { opcode_swap(Address(hl)); }
void CPU::opcode_CB_37() { opcode_swap(a); }
void CPU::opcode_CB_38() { opcode_srl(b); }
void CPU::opcode_CB_39() { opcode_srl(c); }
void CPU::opcode_CB_3A() { opcode_srl(d); }
void CPU::opcode_CB_3B() { opcode_srl(e); }
void CPU::opcode_CB_3C() { opcode_srl(h); }
void CPU::opcode_CB_3D() { opcode_srl(l); }
void CPU::opcode_CB_3E() { opcode_srl(Address(hl)); }
void CPU::opcode_CB_3F() { opcode_srl(a); }
void CPU::opcode_CB_40() { opcode_bit(0, b); }
void CPU::opcode_CB_41() { opcode_bit(0, c); }
void CPU::opcode_CB_42() { opcode_bit(0, d); }
void CPU::opcode_CB_43() { opcode_bit(0, e); }
void CPU::opcode_CB_44() { opcode_bit(0, h); }
void CPU::opcode_CB_45() { opcode_bit(0, l); }
void CPU::opcode_CB_46() { opcode_bit(0, Address(hl)); }
void CPU::opcode_CB_47() { opcode_bit(0, a); }
void CPU::opcode_CB_48() { opcode_bit(1, b); }
void CPU::opcode_CB_49() { opcode_bit(1, c); }
void CPU::opcode_CB_4A() { opcode_bit(1, d); }
void CPU::opcode_CB_4B() { opcode_bit(1, e); }
void CPU::opcode_CB_4C() { opcode_bit(1, h); }
void CPU::opcode_CB_4D() { opcode_bit(1, l); }
void CPU::opcode_CB_4E() { opcode_bit(1, Address(hl)); }
void CPU::opcode_CB_4F() { opcode_bit(1, a); }
void CPU::opcode_CB_50() { opcode_bit(2, b); }
void CPU::opcode_CB_51() { opcode_bit(2, c); }
void CPU::opcode_CB_52() { opcode_bit(2, d); }
void CPU::opcode_CB_53() { opcode_bit(2, e); }
void CPU::opcode_CB_54() { opcode_bit(2, h); }
void CPU::opcode_CB_55() { opcode_bit(2, l); }
void CPU::opcode_CB_56() { opcode_bit(2, Address(hl)); }
void CPU::opcode_CB_57() { opcode_bit(2, a); }
void CPU::opcode_CB_58() { opcode_bit(3, b); }
void CPU::opcode_CB_59() { opcode_bit(3, c); }
void CPU::opcode_CB_5A() { opcode_bit(3, d); }
void CPU::opcode_CB_5B() { opcode_bit(3, e); }
void CPU::opcode_CB_5C() { opcode_bit(3, h); }
void CPU::opcode_CB_5D() { opcode_bit(3, l); }
void CPU::opcode_CB_5E() { opcode_bit(3, Address(hl)); }
void CPU::opcode_CB_5F() { opcode_bit(3, a); }
void CPU::opcode_CB_60() { opcode_bit(4, b); }
void CPU::opcode_CB_61() { opcode_bit(4, c); }
void CPU::opcode_CB_62() { opcode_bit(4, d); }
void CPU::opcode_CB_63() { opcode_bit(4, e); }
void CPU::opcode_CB_64() { opcode_bit(4, h); }
void CPU::opcode_CB_65() { opcode_bit(4, l); }
void CPU::opcode_CB_66() { opcode_bit(4, Address(hl)); }
void CPU::opcode_CB_67() { opcode_bit(4, a); }
void CPU::opcode_CB_68() { opcode_bit(5, b); }
void CPU::opcode_CB_69() { opcode_bit(5, c); }
void CPU::opcode_CB_6A() { opcode_bit(5, d); }
void CPU::opcode_CB_6B() { opcode_bit(5, e); }
void CPU::opcode_CB_6C() { opcode_bit(5, h); }
void CPU::opcode_CB_6D() { opcode_bit(5, l); }
void CPU::opcode_CB_6E() { opcode_bit(5, Address(hl)); }
void CPU::opcode_CB_6F() { opcode_bit(5, a); }
void CPU::opcode_CB_70() { opcode_bit(6, b); }
void CPU::opcode_CB_71() { opcode_bit(6, c); }
void CPU::opcode_CB_72() { opcode_bit(6, d); }
void CPU::opcode_CB_73() { opcode_bit(6, e); }
void CPU::opcode_CB_74() { opcode_bit(6, h); }
void CPU::opcode_CB_75() { opcode_bit(6, l); }
void CPU::opcode_CB_76() { opcode_bit(6, Address(hl)); }
void CPU::opcode_CB_77() { opcode_bit(6, a); }
void CPU::opcode_CB_78() { opcode_bit(7, b); }
void CPU::opcode_CB_79() { opcode_bit(7, c); }
void CPU::opcode_CB_7A() { opcode_bit(7, d); }
void CPU::opcode_CB_7B() { opcode_bit(7, e); }
void CPU::opcode_CB_7C() { opcode_bit(7, h); }
void CPU::opcode_CB_7D() { opcode_bit(7, l); }
void CPU::opcode_CB_7E() { opcode_bit(7, Address(hl)); }
void CPU::opcode_CB_7F() { opcode_bit(7, a); }
void CPU::opcode_CB_80() { opcode_res(0, b); }
void CPU::opcode_CB_81() { opcode_res(0, c); }
void CPU::opcode_CB_82() { opcode_res(0, d); }
void CPU::opcode_CB_83() { opcode_res(0, e); }
void CPU::opcode_CB_84() { opcode_res(0, h); }
void CPU::opcode_CB_85() { opcode_res(0, l); }
void CPU::opcode_CB_86() { opcode_res(0, Address(hl)); }
void CPU::opcode_CB_87() { opcode_res(0, a); }
void CPU::opcode_CB_88() { opcode_res(1, b); }
void CPU::opcode_CB_89() { opcode_res(1, c); }
void CPU::opcode_CB_8A() { opcode_res(1, d); }
void CPU::opcode_CB_8B() { opcode_res(1, e); }
void CPU::opcode_CB_8C() { opcode_res(1, h); }
void CPU::opcode_CB_8D() { opcode_res(1, l); }
void CPU::opcode_CB_8E() { opcode_res(1, Address(hl)); }
void CPU::opcode_CB_8F() { opcode_res(1, a); }
void CPU::opcode_CB_90() { opcode_res(2, b); }
void CPU::opcode_CB_91() { opcode_res(2, c); }
void CPU::opcode_CB_92() { opcode_res(2, d); }
void CPU::opcode_CB_93() { opcode_res(2, e); }
void CPU::opcode_CB_94() { opcode_res(2, h); }
void CPU::opcode_CB_95() { opcode_res(2, l); }
void CPU::opcode_CB_96() { opcode_res(2, Address(hl)); }
void CPU::opcode_CB_97() { opcode_res(2, a); }
void CPU::opcode_CB_98() { opcode_res(3, b); }
void CPU::opcode_CB_99() { opcode_res(3, c); }
void CPU::opcode_CB_9A() { opcode_res(3, d); }
void CPU::opcode_CB_9B() { opcode_res(3, e); }
void CPU::opcode_CB_9C() { opcode_res(3, h); }
void CPU::opcode_CB_9D() { opcode_res(3, l); }
void CPU::opcode_CB_9E() { opcode_res(3, Address(hl)); }
void CPU::opcode_CB_9F() { opcode_res(3, a); }
void CPU::opcode_CB_A0() { opcode_res(4, b); }
void CPU::opcode_CB_A1() { opcode_res(4, c); }
void CPU::opcode_CB_A2() { opcode_res(4, d); }
void CPU::opcode_CB_A3() { opcode_res(4, e); }
void CPU::opcode_CB_A4() { opcode_res(4, h); }
void CPU::opcode_CB_A5() { opcode_res(4, l); }
void CPU::opcode_CB_A6() { opcode_res(4, Address(hl)); }
void CPU::opcode_CB_A7() { opcode_res(4, a); }
void CPU::opcode_CB_A8() { opcode_res(5, b); }
void CPU::opcode_CB_A9() { opcode_res(5, c); }
void CPU::opcode_CB_AA() { opcode_res(5, d); }
void CPU::opcode_CB_AB() { opcode_res(5, e); }
void CPU::opcode_CB_AC() { opcode_res(5, h); }
void CPU::opcode_CB_AD() { opcode_res(5, l); }
void CPU::opcode_CB_AE() { opcode_res(5, Address(hl)); }
void CPU::opcode_CB_AF() { opcode_res(5, a); }
void CPU::opcode_CB_B0() { opcode_res(6, b); }
void CPU::opcode_CB_B1() { opcode_res(6, c); }
void CPU::opcode_CB_B2() { opcode_res(6, d); }
void CPU::opcode_CB_B3() { opcode_res(6, e); }
void CPU::opcode_CB_B4() { opcode_res(6, h); }
void CPU::opcode_CB_B5() { opcode_res(6, l); }
void CPU::opcode_CB_B6() { opcode_res(6, Address(hl)); }
void CPU::opcode_CB_B7() { opcode_res(6, a); }
void CPU::opcode_CB_B8() { opcode_res(7, b); }
void CPU::opcode_CB_B9() { opcode_res(7, c); }
void CPU::opcode_CB_BA() { opcode_res(7, d); }
void CPU::opcode_CB_BB() { opcode_res(7, e); }
void CPU::opcode_CB_BC() { opcode_res(7, h); }
void CPU::opcode_CB_BD() { opcode_res(7, l); }
void CPU::opcode_CB_BE() { opcode_res(7, Address(hl)); }
void CPU::opcode_CB_BF() { opcode_res(7, a); }
void CPU::opcode_CB_C0() { opcode_set(0, b); }
void CPU::opcode_CB_C1() { opcode_set(0, c); }
void CPU::opcode_CB_C2() { opcode_set(0, d); }
void CPU::opcode_CB_C3() { opcode_set(0, e); }
void CPU::opcode_CB_C4() { opcode_set(0, h); }
void CPU::opcode_CB_C5() { opcode_set(0, l); }
void CPU::opcode_CB_C6() { opcode_set(0, Address(hl)); }
void CPU::opcode_CB_C7() { opcode_set(0, a); }
void CPU::opcode_CB_C8() { opcode_set(1, b); }
void CPU::opcode_CB_C9() { opcode_set(1, c); }
void CPU::opcode_CB_CA() { opcode_set(1, d); }
void CPU::opcode_CB_CB() { opcode_set(1, e); }
void CPU::opcode_CB_CC() { opcode_set(1, h); }
void CPU::opcode_CB_CD() { opcode_set(1, l); }
void CPU::opcode_CB_CE() { opcode_set(1, Address(hl)); }
void CPU::opcode_CB_CF() { opcode_set(1, a); }
void CPU::opcode_CB_D0() { opcode_set(2, b); }
void CPU::opcode_CB_D1() { opcode_set(2, c); }
void CPU::opcode_CB_D2() { opcode_set(2, d); }
void CPU::opcode_CB_D3() { opcode_set(2, e); }
void CPU::opcode_CB_D4() { opcode_set(2, h); }
void CPU::opcode_CB_D5() { opcode_set(2, l); }
void CPU::opcode_CB_D6() { opcode_set(2, Address(hl)); }
void CPU::opcode_CB_D7() { opcode_set(2, a); }
void CPU::opcode_CB_D8() { opcode_set(3, b); }
void CPU::opcode_CB_D9() { opcode_set(3, c); }
void CPU::opcode_CB_DA() { opcode_set(3, d); }
void CPU::opcode_CB_DB() { opcode_set(3, e); }
void CPU::opcode_CB_DC() { opcode_set(3, h); }
void CPU::opcode_CB_DD() { opcode_set(3, l); }
void CPU::opcode_CB_DE() { opcode_set(3, Address(hl)); }
void CPU::opcode_CB_DF() { opcode_set(3, a); }
void CPU::opcode_CB_E0() { opcode_set(4, b); }
void CPU::opcode_CB_E1() { opcode_set(4, c); }
void CPU::opcode_CB_E2() { opcode_set(4, d); }
void CPU::opcode_CB_E3() { opcode_set(4, e); }
void CPU::opcode_CB_E4() { opcode_set(4, h); }
void CPU::opcode_CB_E5() { opcode_set(4, l); }
void CPU::opcode_CB_E6() { opcode_set(4, Address(hl)); }
void CPU::opcode_CB_E7() { opcode_set(4, a); }
void CPU::opcode_CB_E8() { opcode_set(5, b); }
void CPU::opcode_CB_E9() { opcode_set(5, c); }
void CPU::opcode_CB_EA() { opcode_set(5, d); }
void CPU::opcode_CB_EB() { opcode_set(5, e); }
void CPU::opcode_CB_EC() { opcode_set(5, h); }
void CPU::opcode_CB_ED() { opcode_set(5, l); }
void CPU::opcode_CB_EE() { opcode_set(5, Address(hl)); }
void CPU::opcode_CB_EF() { opcode_set(5, a); }
void CPU::opcode_CB_F0() { opcode_set(6, b); }
void CPU::opcode_CB_F1() { opcode_set(6, c); }
void CPU::opcode_CB_F2() { opcode_set(6, d); }
void CPU::opcode_CB_F3() { opcode_set(6, e); }
void CPU::opcode_CB_F4() { opcode_set(6, h); }
void CPU::opcode_CB_F5() { opcode_set(6, l); }
void CPU::opcode_CB_F6() { opcode_set(6, Address(hl)); }
void CPU::opcode_CB_F7() { opcode_set(6, a); }
void CPU::opcode_CB_F8() { opcode_set(7, b); }
void CPU::opcode_CB_F9() { opcode_set(7, c); }
void CPU::opcode_CB_FA() { opcode_set(7, d); }
void CPU::opcode_CB_FB() { opcode_set(7, e); }
void CPU::opcode_CB_FC() { opcode_set(7, h); }
void CPU::opcode_CB_FD() { opcode_set(7, l); }
void CPU::opcode_CB_FE() { opcode_set(7, Address(hl)); }
void CPU::opcode_CB_FF() { opcode_set(7, a); }
