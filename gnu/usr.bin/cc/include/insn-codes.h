/* Generated automatically by the program `gencodes'
from the machine description file `md'.  */

#ifndef MAX_INSN_CODE

enum insn_code {
  CODE_FOR_tstsi_1 = 0,
  CODE_FOR_tstsi = 1,
  CODE_FOR_tsthi_1 = 2,
  CODE_FOR_tsthi = 3,
  CODE_FOR_tstqi_1 = 4,
  CODE_FOR_tstqi = 5,
  CODE_FOR_tstsf_cc = 6,
  CODE_FOR_tstsf = 7,
  CODE_FOR_tstdf_cc = 8,
  CODE_FOR_tstdf = 9,
  CODE_FOR_tstxf_cc = 10,
  CODE_FOR_tstxf = 11,
  CODE_FOR_cmpsi_1 = 12,
  CODE_FOR_cmpsi = 13,
  CODE_FOR_cmphi_1 = 14,
  CODE_FOR_cmphi = 15,
  CODE_FOR_cmpqi_1 = 16,
  CODE_FOR_cmpqi = 17,
  CODE_FOR_cmpsf_cc_1 = 30,
  CODE_FOR_cmpxf = 34,
  CODE_FOR_cmpdf = 35,
  CODE_FOR_cmpsf = 36,
  CODE_FOR_cmpxf_cc = 37,
  CODE_FOR_cmpxf_ccfpeq = 38,
  CODE_FOR_cmpdf_cc = 39,
  CODE_FOR_cmpdf_ccfpeq = 40,
  CODE_FOR_cmpsf_cc = 41,
  CODE_FOR_cmpsf_ccfpeq = 42,
  CODE_FOR_movsi = 48,
  CODE_FOR_movhi = 51,
  CODE_FOR_movstricthi = 52,
  CODE_FOR_movqi = 54,
  CODE_FOR_movstrictqi = 55,
  CODE_FOR_movsf = 57,
  CODE_FOR_swapdf = 59,
  CODE_FOR_movdf = 60,
  CODE_FOR_swapxf = 62,
  CODE_FOR_movxf = 63,
  CODE_FOR_movdi = 65,
  CODE_FOR_zero_extendhisi2 = 66,
  CODE_FOR_zero_extendqihi2 = 67,
  CODE_FOR_zero_extendqisi2 = 68,
  CODE_FOR_zero_extendsidi2 = 69,
  CODE_FOR_extendsidi2 = 70,
  CODE_FOR_extendhisi2 = 71,
  CODE_FOR_extendqihi2 = 72,
  CODE_FOR_extendqisi2 = 73,
  CODE_FOR_extendsfdf2 = 74,
  CODE_FOR_extenddfxf2 = 75,
  CODE_FOR_extendsfxf2 = 76,
  CODE_FOR_truncdfsf2 = 77,
  CODE_FOR_truncxfsf2 = 79,
  CODE_FOR_truncxfdf2 = 80,
  CODE_FOR_fixuns_truncxfsi2 = 81,
  CODE_FOR_fixuns_truncdfsi2 = 82,
  CODE_FOR_fixuns_truncsfsi2 = 83,
  CODE_FOR_fix_truncxfdi2 = 84,
  CODE_FOR_fix_truncdfdi2 = 85,
  CODE_FOR_fix_truncsfdi2 = 86,
  CODE_FOR_fix_truncxfsi2 = 90,
  CODE_FOR_fix_truncdfsi2 = 91,
  CODE_FOR_fix_truncsfsi2 = 92,
  CODE_FOR_floatsisf2 = 96,
  CODE_FOR_floatdisf2 = 97,
  CODE_FOR_floatsidf2 = 98,
  CODE_FOR_floatdidf2 = 99,
  CODE_FOR_floatsixf2 = 100,
  CODE_FOR_floatdixf2 = 101,
  CODE_FOR_adddi3 = 108,
  CODE_FOR_addsi3 = 109,
  CODE_FOR_addhi3 = 110,
  CODE_FOR_addqi3 = 111,
  CODE_FOR_addxf3 = 113,
  CODE_FOR_adddf3 = 114,
  CODE_FOR_addsf3 = 115,
  CODE_FOR_subdi3 = 116,
  CODE_FOR_subsi3 = 117,
  CODE_FOR_subhi3 = 118,
  CODE_FOR_subqi3 = 119,
  CODE_FOR_subxf3 = 120,
  CODE_FOR_subdf3 = 121,
  CODE_FOR_subsf3 = 122,
  CODE_FOR_mulhi3 = 124,
  CODE_FOR_mulsi3 = 126,
  CODE_FOR_umulqihi3 = 127,
  CODE_FOR_mulqihi3 = 128,
  CODE_FOR_umulsidi3 = 129,
  CODE_FOR_mulsidi3 = 130,
  CODE_FOR_mulxf3 = 131,
  CODE_FOR_muldf3 = 132,
  CODE_FOR_mulsf3 = 133,
  CODE_FOR_divqi3 = 134,
  CODE_FOR_udivqi3 = 135,
  CODE_FOR_divxf3 = 136,
  CODE_FOR_divdf3 = 137,
  CODE_FOR_divsf3 = 138,
  CODE_FOR_divmodsi4 = 139,
  CODE_FOR_divmodhi4 = 140,
  CODE_FOR_udivmodsi4 = 141,
  CODE_FOR_udivmodhi4 = 142,
  CODE_FOR_andsi3 = 143,
  CODE_FOR_andhi3 = 144,
  CODE_FOR_andqi3 = 145,
  CODE_FOR_iorsi3 = 146,
  CODE_FOR_iorhi3 = 147,
  CODE_FOR_iorqi3 = 148,
  CODE_FOR_xorsi3 = 149,
  CODE_FOR_xorhi3 = 150,
  CODE_FOR_xorqi3 = 151,
  CODE_FOR_negdi2 = 152,
  CODE_FOR_negsi2 = 153,
  CODE_FOR_neghi2 = 154,
  CODE_FOR_negqi2 = 155,
  CODE_FOR_negsf2 = 156,
  CODE_FOR_negdf2 = 157,
  CODE_FOR_negxf2 = 159,
  CODE_FOR_abssf2 = 161,
  CODE_FOR_absdf2 = 162,
  CODE_FOR_absxf2 = 164,
  CODE_FOR_sqrtsf2 = 166,
  CODE_FOR_sqrtdf2 = 167,
  CODE_FOR_sqrtxf2 = 169,
  CODE_FOR_sindf2 = 172,
  CODE_FOR_sinsf2 = 173,
  CODE_FOR_cosdf2 = 175,
  CODE_FOR_cossf2 = 176,
  CODE_FOR_one_cmplsi2 = 178,
  CODE_FOR_one_cmplhi2 = 179,
  CODE_FOR_one_cmplqi2 = 180,
  CODE_FOR_ashldi3 = 181,
  CODE_FOR_ashldi3_const_int = 182,
  CODE_FOR_ashldi3_non_const_int = 183,
  CODE_FOR_ashlsi3 = 184,
  CODE_FOR_ashlhi3 = 185,
  CODE_FOR_ashlqi3 = 186,
  CODE_FOR_ashrdi3 = 187,
  CODE_FOR_ashrdi3_const_int = 188,
  CODE_FOR_ashrdi3_non_const_int = 189,
  CODE_FOR_ashrsi3 = 190,
  CODE_FOR_ashrhi3 = 191,
  CODE_FOR_ashrqi3 = 192,
  CODE_FOR_lshrdi3 = 193,
  CODE_FOR_lshrdi3_const_int = 194,
  CODE_FOR_lshrdi3_non_const_int = 195,
  CODE_FOR_lshrsi3 = 196,
  CODE_FOR_lshrhi3 = 197,
  CODE_FOR_lshrqi3 = 198,
  CODE_FOR_rotlsi3 = 199,
  CODE_FOR_rotlhi3 = 200,
  CODE_FOR_rotlqi3 = 201,
  CODE_FOR_rotrsi3 = 202,
  CODE_FOR_rotrhi3 = 203,
  CODE_FOR_rotrqi3 = 204,
  CODE_FOR_seq = 211,
  CODE_FOR_sne = 213,
  CODE_FOR_sgt = 215,
  CODE_FOR_sgtu = 217,
  CODE_FOR_slt = 219,
  CODE_FOR_sltu = 221,
  CODE_FOR_sge = 223,
  CODE_FOR_sgeu = 225,
  CODE_FOR_sle = 227,
  CODE_FOR_sleu = 229,
  CODE_FOR_beq = 231,
  CODE_FOR_bne = 233,
  CODE_FOR_bgt = 235,
  CODE_FOR_bgtu = 237,
  CODE_FOR_blt = 239,
  CODE_FOR_bltu = 241,
  CODE_FOR_bge = 243,
  CODE_FOR_bgeu = 245,
  CODE_FOR_ble = 247,
  CODE_FOR_bleu = 249,
  CODE_FOR_jump = 261,
  CODE_FOR_indirect_jump = 262,
  CODE_FOR_casesi = 263,
  CODE_FOR_tablejump = 265,
  CODE_FOR_call_pop = 266,
  CODE_FOR_call = 269,
  CODE_FOR_call_value_pop = 272,
  CODE_FOR_call_value = 275,
  CODE_FOR_untyped_call = 278,
  CODE_FOR_untyped_return = 281,
  CODE_FOR_update_return = 282,
  CODE_FOR_return = 283,
  CODE_FOR_nop = 284,
  CODE_FOR_movstrsi = 285,
  CODE_FOR_cmpstrsi = 287,
  CODE_FOR_ffssi2 = 290,
  CODE_FOR_ffshi2 = 292,
  CODE_FOR_strlensi = 307,
  CODE_FOR_nothing };

#define MAX_INSN_CODE ((int) CODE_FOR_nothing)
#endif /* MAX_INSN_CODE */
