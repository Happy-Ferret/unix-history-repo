/*-
 * Copyright (c) 2016 Andriy Voskoboinyk <avos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef R21A_PRIV_H
#define R21A_PRIV_H

/*
 * MAC initialization values.
 */
static const struct rtwn_mac_prog rtl8821au_mac[] = {
	{ 0x421, 0x0f }, { 0x428, 0x0a }, { 0x429, 0x10 }, { 0x430, 0x00 },
	{ 0x431, 0x00 }, { 0x432, 0x00 }, { 0x433, 0x01 }, { 0x434, 0x04 },
	{ 0x435, 0x05 }, { 0x436, 0x07 }, { 0x437, 0x08 }, { 0x43c, 0x04 },
	{ 0x43d, 0x05 }, { 0x43e, 0x07 }, { 0x43f, 0x08 }, { 0x440, 0x5d },
	{ 0x441, 0x01 }, { 0x442, 0x00 }, { 0x444, 0x10 }, { 0x445, 0x00 },
	{ 0x446, 0x00 }, { 0x447, 0x00 }, { 0x448, 0x00 }, { 0x449, 0xf0 },
	{ 0x44a, 0x0f }, { 0x44b, 0x3e }, { 0x44c, 0x10 }, { 0x44d, 0x00 },
	{ 0x44e, 0x00 }, { 0x44f, 0x00 }, { 0x450, 0x00 }, { 0x451, 0xf0 },
	{ 0x452, 0x0f }, { 0x453, 0x00 }, { 0x456, 0x5e }, { 0x460, 0x66 },
	{ 0x461, 0x66 }, { 0x4c8, 0x3f }, { 0x4c9, 0xff }, { 0x4cc, 0xff },
	{ 0x4cd, 0xff }, { 0x4ce, 0x01 }, { 0x500, 0x26 }, { 0x501, 0xa2 },
	{ 0x502, 0x2f }, { 0x503, 0x00 }, { 0x504, 0x28 }, { 0x505, 0xa3 },
	{ 0x506, 0x5e }, { 0x507, 0x00 }, { 0x508, 0x2b }, { 0x509, 0xa4 },
	{ 0x50a, 0x5e }, { 0x50b, 0x00 }, { 0x50c, 0x4f }, { 0x50d, 0xa4 },
	{ 0x50e, 0x00 }, { 0x50f, 0x00 }, { 0x512, 0x1c }, { 0x514, 0x0a },
	{ 0x516, 0x0a }, { 0x525, 0x4f }, { 0x550, 0x10 }, { 0x551, 0x10 },
	{ 0x559, 0x02 }, { 0x55c, 0x50 }, { 0x55d, 0xff }, { 0x605, 0x30 },
	{ 0x607, 0x07 }, { 0x608, 0x0e }, { 0x609, 0x2a }, { 0x620, 0xff },
	{ 0x621, 0xff }, { 0x622, 0xff }, { 0x623, 0xff }, { 0x624, 0xff },
	{ 0x625, 0xff }, { 0x626, 0xff }, { 0x627, 0xff }, { 0x638, 0x50 },
	{ 0x63c, 0x0a }, { 0x63d, 0x0a }, { 0x63e, 0x0e }, { 0x63f, 0x0e },
	{ 0x640, 0x40 }, { 0x642, 0x40 }, { 0x643, 0x00 }, { 0x652, 0xc8 },
	{ 0x66e, 0x05 }, { 0x700, 0x21 }, { 0x701, 0x43 }, { 0x702, 0x65 },
	{ 0x703, 0x87 }, { 0x708, 0x21 }, { 0x709, 0x43 }, { 0x70a, 0x65 },
	{ 0x70b, 0x87 }, { 0x718, 0x40 }
};


/*
 * Baseband initialization values.
 */
#define R21A_COND_EXT_PA_5G	0x01
#define R21A_COND_EXT_LNA_5G	0x02
#define R21A_COND_BOARD_DEF	0x04
#define R21A_COND_BT		0x08

static const uint16_t rtl8821au_bb_regs[] = {
	0x800, 0x804, 0x808, 0x80c, 0x810, 0x814, 0x818, 0x820, 0x824,
	0x828, 0x82c, 0x830, 0x834, 0x838, 0x83c, 0x840, 0x844, 0x848,
	0x84c, 0x850, 0x854, 0x858, 0x85c, 0x860, 0x864, 0x868, 0x86c,
	0x870, 0x874, 0x878, 0x87c, 0x8a0, 0x8a4, 0x8a8, 0x8ac, 0x8b4,
	0x8b8, 0x8bc, 0x8c0, 0x8c4, 0x8c8, 0x8cc, 0x8d4, 0x8d8, 0x8f8,
	0x8fc, 0x900, 0x90c, 0x910, 0x914, 0x918, 0x91c, 0x920, 0x924,
	0x928, 0x92c, 0x930, 0x934, 0x960, 0x964, 0x968, 0x96c, 0x970,
	0x974, 0x978, 0x97c, 0x980, 0x984, 0x988, 0x990, 0x994, 0x998,
	0x99c, 0x9a0, 0x9a4, 0x9a8, 0x9ac, 0x9b0, 0x9b4, 0x9b8, 0x9bc,
	0x9d0, 0x9d4, 0x9d8, 0x9dc, 0x9e0, 0x9e4, 0x9e8, 0xa00, 0xa04,
	0xa08, 0xa0c, 0xa10, 0xa14, 0xa18, 0xa1c, 0xa20, 0xa24, 0xa28,
	0xa2c, 0xa70, 0xa74, 0xa78, 0xa7c, 0xa80, 0xa84, 0xb00, 0xb04,
	0xb08, 0xb0c, 0xb10, 0xb14, 0xb18, 0xb1c, 0xb20, 0xb24, 0xb28,
	0xb2c, 0xb30, 0xb34, 0xb38, 0xb3c, 0xb40, 0xb44, 0xb48, 0xb4c,
	0xb50, 0xb54, 0xb58, 0xb5c, 0xc00, 0xc04, 0xc08, 0xc0c, 0xc10,
	0xc14, 0xc1c, 0xc20, 0xc24, 0xc28, 0xc2c, 0xc30, 0xc34, 0xc38,
	0xc3c, 0xc40, 0xc44, 0xc48, 0xc4c, 0xc50, 0xc54, 0xc58, 0xc5c,
	0xc60, 0xc64, 0xc68, 0xc6c, 0xc70, 0xc74, 0xc78, 0xc7c, 0xc80,
	0xc84, 0xc94, 0xc98, 0xc9c, 0xca0, 0xca4, 0xca8, 0xcb0, 0xcb4,
	0xcb8
};

static const uint32_t rtl8821au_bb_vals[] = {
	0x0020d090, 0x080112e0, 0x0e028211, 0x92131111, 0x20101261,
	0x020c3d10, 0x03a00385, 0x00000000, 0x00030fe0, 0x00000000,
	0x002081dd, 0x2aaaeec8, 0x0037a706, 0x06489b44, 0x0000095b,
	0xc0000001, 0x40003cde, 0x62103f8b, 0x6cfdffb8, 0x28874706,
	0x0001520c, 0x8060e000, 0x74210168, 0x6929c321, 0x79727432,
	0x8ca7a314, 0x888c2878, 0x08888888, 0x31612c2e, 0x00000152,
	0x000fd000, 0x00000013, 0x7f7f7f7f, 0xa2000338, 0x0ff0fa0a,
	0x000fc080, 0x6c10d7ff, 0x0ca52090, 0x1bf00020, 0x00000000,
	0x00013169, 0x08248492, 0x940008a0, 0x290b5612, 0x400002c0,
	0x00000000, 0x00000700, 0x00000000, 0x0000fc00, 0x00000404,
	0x1c1028c0, 0x64b11a1c, 0xe0767233, 0x055aa500, 0x00000004,
	0xfffe0000, 0xfffffffe, 0x001fffff, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x801fffff, 0x000003ff, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x27100000,
	0xffff0100, 0xffffff5c, 0xffffffff, 0x000000ff, 0x00480080,
	0x00000000, 0x00000000, 0x81081008, 0x01081008, 0x01081008,
	0x01081008, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00005d00, 0x00000003, 0x00000001, 0x00d047c8, 0x01ff800c,
	0x8c8a8300, 0x2e68000f, 0x9500bb78, 0x11144028, 0x00881117,
	0x89140f00, 0x1a1b0000, 0x090e1317, 0x00000204, 0x00900000,
	0x101fff00, 0x00000008, 0x00000900, 0x225b0606, 0x21805490,
	0x001f0000, 0x03100040, 0x0000b000, 0xae0201eb, 0x01003207,
	0x00009807, 0x01000000, 0x00000002, 0x00000002, 0x0000001f,
	0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c, 0x13121110,
	0x17161514, 0x0000003a, 0x00000000, 0x00000000, 0x13000032,
	0x48080000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000007, 0x00042020, 0x80410231, 0x00000000, 0x00000100,
	0x01000000, 0x40000003, 0x2c2c2c2c, 0x30303030, 0x30303030,
	0x2c2c2c2c, 0x2c2c2c2c, 0x2c2c2c2c, 0x2c2c2c2c, 0x2a2a2a2a,
	0x2a2a2a2a, 0x2a2a2a2a, 0x2a2a2a2a, 0x2a2a2a2a, 0x00000020,
	0x001c1208, 0x30000c1c, 0x00000058, 0x34344443, 0x07003333,
	0x19791979, 0x19791979, 0x19791979, 0x19791979, 0x19791979,
	0x19791979, 0x19791979, 0x19791979, 0x0100005c, 0x00000000,
	0x00000000, 0x00000029, 0x08040201, 0x80402010, 0x77775747,
	0x10000077, 0x00508240
};

static const struct rtwn_bb_prog rtl8821au_bb[] = {
	{
		nitems(rtl8821au_bb_regs),
		rtl8821au_bb_regs,
		rtl8821au_bb_vals,
		{ 0 },
		NULL
	}
};

static const uint32_t rtl8821au_agc_vals0[] = {
	0xbf000001, 0xbf020001, 0xbf040001, 0xbf060001, 0xbe080001,
	0xbd0a0001, 0xbc0c0001, 0xba0e0001, 0xb9100001, 0xb8120001,
	0xb7140001, 0xb6160001, 0xb5180001, 0xb41a0001, 0xb31c0001,
	0xb21e0001, 0xb1200001, 0xb0220001, 0xaf240001, 0xae260001,
	0xad280001, 0xac2a0001, 0xab2c0001, 0xaa2e0001, 0xa9300001,
	0xa8320001, 0xa7340001, 0xa6360001, 0xa5380001, 0xa43a0001,
	0x683c0001, 0x673e0001, 0x66400001, 0x65420001, 0x64440001,
	0x63460001, 0x62480001, 0x614a0001, 0x474c0001, 0x464e0001,
	0x45500001, 0x44520001, 0x43540001, 0x42560001, 0x41580001,
	0x285a0001, 0x275c0001, 0x265e0001, 0x25600001, 0x24620001,
	0x0a640001, 0x09660001, 0x08680001, 0x076a0001, 0x066c0001,
	0x056e0001, 0x04700001, 0x03720001, 0x02740001, 0x01760001,
	0x01780001, 0x017a0001, 0x017c0001, 0x017e0001
}, rtl8821au_agc_vals1_pa_lna_5g[] = {
	0xfb000101, 0xfa020101, 0xf9040101, 0xf8060101, 0xf7080101,
	0xf60a0101, 0xf50c0101, 0xf40e0101, 0xf3100101, 0xf2120101,
	0xf1140101, 0xf0160101, 0xef180101, 0xee1a0101, 0xed1c0101,
	0xec1e0101, 0xeb200101, 0xea220101, 0xe9240101, 0xe8260101,
	0xe7280101, 0xe62a0101, 0xe52c0101, 0xe42e0101, 0xe3300101,
	0xa5320101, 0xa4340101, 0xa3360101, 0x87380101, 0x863a0101,
	0x853c0101, 0x843e0101, 0x69400101, 0x68420101, 0x67440101,
	0x66460101, 0x49480101, 0x484a0101, 0x474c0101, 0x2a4e0101,
	0x29500101, 0x28520101, 0x27540101, 0x26560101, 0x25580101,
	0x245a0101, 0x235c0101, 0x055e0101, 0x04600101, 0x03620101,
	0x02640101, 0x01660101, 0x01680101, 0x016a0101, 0x016c0101,
	0x016e0101, 0x01700101, 0x01720101
}, rtl8821au_agc_vals1[] = {
	0xff000101, 0xff020101, 0xfe040101, 0xfd060101, 0xfc080101,
	0xfd0a0101, 0xfc0c0101, 0xfb0e0101, 0xfa100101, 0xf9120101,
	0xf8140101, 0xf7160101, 0xf6180101, 0xf51a0101, 0xf41c0101,
	0xf31e0101, 0xf2200101, 0xf1220101, 0xf0240101, 0xef260101,
	0xee280101, 0xed2a0101, 0xec2c0101, 0xeb2e0101, 0xea300101,
	0xe9320101, 0xe8340101, 0xe7360101, 0xe6380101, 0xe53a0101,
	0xe43c0101, 0xe33e0101, 0xa5400101, 0xa4420101, 0xa3440101,
	0x87460101, 0x86480101, 0x854a0101, 0x844c0101, 0x694e0101,
	0x68500101, 0x67520101, 0x66540101, 0x49560101, 0x48580101,
	0x475a0101, 0x2a5c0101, 0x295e0101, 0x28600101, 0x27620101,
	0x26640101, 0x25660101, 0x24680101, 0x236a0101, 0x056c0101,
	0x046e0101, 0x03700101, 0x02720101
}, rtl8821au_agc_vals2[] = {
	0x01740101, 0x01760101, 0x01780101, 0x017a0101, 0x017c0101,
	0x017e0101
};

static const struct rtwn_agc_prog rtl8821au_agc[] = {
	{
		nitems(rtl8821au_agc_vals0),
		rtl8821au_agc_vals0,
		{ 0 },
		NULL
	},
	/*
	 * For devices with external 5GHz PA / LNA.
	 */
	{
		nitems(rtl8821au_agc_vals1_pa_lna_5g),
		rtl8821au_agc_vals1_pa_lna_5g,
		{ R21A_COND_EXT_PA_5G | R21A_COND_EXT_LNA_5G, 0 },
		/*
		 * Others.
		 */
		&(const struct rtwn_agc_prog){
			nitems(rtl8821au_agc_vals1),
			rtl8821au_agc_vals1,
			{ 0 },
			NULL
		}
	},
	{
		nitems(rtl8821au_agc_vals2),
		rtl8821au_agc_vals2,
		{ 0 },
		NULL
	}
};


/*
 * RF initialization values.
 */
static const uint8_t rtl8821au_rf_regs0[] = {
	0x18, 0x56, 0x66, 0x00, 0x1e, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0xef, 0x3e, 0x3f,
	0x3e, 0x3f, 0x3e, 0x3f, 0x3e, 0x3f, 0xef, 0x18, 0x89, 0x8b, 0xef,
	0x3a, 0x3b, 0x3c, 0x3a, 0x3b, 0x3c, 0x3a, 0x3b, 0x3c, 0x3a, 0x3b,
	0x3c, 0x3a, 0x3b, 0x3c, 0x3a, 0x3b, 0x3c, 0x3a, 0x3b, 0x3c, 0x3a,
	0x3b, 0x3c, 0x3a, 0x3b, 0x3c, 0x3a, 0x3b, 0x3c, 0x3a, 0x3b, 0x3c,
	0x3a, 0x3b, 0x3c, 0x3a, 0x3b, 0x3c, 0x3a, 0x3b, 0x3c, 0x3a, 0x3b,
	0x3c, 0x3a, 0x3b, 0x3c, 0x3a, 0x3b, 0x3c, 0x3a, 0x3b, 0x3c, 0x3a,
	0x3b, 0x3c, 0x3a, 0x3b, 0x3c, 0x3a, 0x3b, 0x3c, 0xef, 0xef
}, rtl8821au_rf_regs1[] = {
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
	0x34, 0x34
}, rtl8821au_rf_regs2[] = {
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0xef, 0x18,
	0xef
}, rtl8821au_rf_regs3[] = {
	0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0xef, 0x18,
	0xef, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0xef, 0xef, 0x3c, 0x3c
}, rtl8821au_rf_regs4[] = {
	0x3c, 0xef, 0x18, 0xef, 0x08, 0xef, 0xdf, 0x1f, 0x58, 0x59, 0x61,
	0x62, 0x63, 0x64, 0x65
}, rtl8821au_rf_regs5[] = {
	0x18, 0xef, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b,
	0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0xef, 0xef, 0x34, 0x34,
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0xef, 0xed,
	0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
	0xed, 0xed, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0xed,
	0xef, 0xdf, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0xef, 0x51,
	0x52, 0x53, 0x54, 0x56, 0x51, 0x52, 0x53, 0x70, 0x71, 0x72, 0x74,
	0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0xed, 0x45, 0x45, 0x45,
	0x46, 0x46, 0x46, 0x46, 0xdf, 0xb3, 0xb4, 0xb7, 0x1c, 0xc4, 0x18,
	0xfe, 0xfe, 0x18,
};

static const uint32_t rtl8821au_rf_vals0[] = {
	0x1712a, 0x51cf2, 0x40000, 0x10000, 0x80000, 0x00830, 0x21800,
	0x28000, 0x48000, 0x94838, 0x44980, 0x48000, 0x0d480, 0x42240,
	0xf0380, 0x90000, 0x22852, 0x65540, 0x88001, 0x20000, 0x00380,
	0x90018, 0x20380, 0xa0018, 0x40308, 0xa0018, 0x60018, 0xa0018,
	0x00000, 0x1712a, 0x00080, 0x80180, 0x01000, 0x00244, 0x38027,
	0x82000, 0x00244, 0x30113, 0x82000, 0x0014c, 0x28027, 0x82000,
	0x000cc, 0x27027, 0x42000, 0x0014c, 0x1f913, 0x42000, 0x0010c,
	0x17f10, 0x12000, 0x000d0, 0x08027, 0xca000, 0x00244, 0x78027,
	0x82000, 0x00244, 0x70113, 0x82000, 0x0014c, 0x68027, 0x82000,
	0x000cc, 0x67027, 0x42000, 0x0014c, 0x5f913, 0x42000, 0x0010c,
	0x57f10, 0x12000, 0x000d0, 0x48027, 0xca000, 0x00244, 0xb8027,
	0x82000, 0x00244, 0xb0113, 0x82000, 0x0014c, 0xa8027, 0x82000,
	0x000cc, 0xa7027, 0x42000, 0x0014c, 0x9f913, 0x42000, 0x0010c,
	0x97f10, 0x12000, 0x000d0, 0x88027, 0xca000, 0x00000, 0x01100
}, rtl8821au_rf_vals1_def_or_bt[] = {
	0x4adf5, 0x49df2, 0x48def, 0x47dec, 0x46de9, 0x45ccb, 0x4488d,
	0x4348d, 0x4248a, 0x4108d, 0x4008a, 0x2adf4, 0x29df1
}, rtl8821au_rf_vals1_ext_5g[] = {
	0x4a0f3, 0x490b1, 0x480ae, 0x470ab, 0x4608b, 0x45069, 0x44048,
	0x43045, 0x42026, 0x41023, 0x40002, 0x2a0f3, 0x290f0
}, rtl8821au_rf_vals1[] = {
	0x4adf7, 0x49df3, 0x48def, 0x47dec, 0x46de9, 0x45ccb, 0x4488d,
	0x4348d, 0x4248a, 0x4108d, 0x4008a, 0x2adf7, 0x29df2
}, rtl8821au_rf_vals2_ext_5g[] = {
	0x280af, 0x270ac, 0x2608b, 0x25069, 0x24048, 0x23045, 0x22026,
	0x21023, 0x20002, 0x0a0d7, 0x090d3, 0x080b1, 0x070ae, 0x0608d,
	0x0506b, 0x0404a, 0x03047, 0x02044, 0x01025, 0x00004, 0x00000,
	0x1712a, 0x00040
}, rtl8821au_rf_vals2[] = {
	0x28dee, 0x27deb, 0x26ccd, 0x25cca, 0x2488c, 0x2384c, 0x22849,
	0x21449, 0x2004d, 0x0adf7, 0x09df4, 0x08df1, 0x07dee, 0x06dcd,
	0x05ccd, 0x04cca, 0x0388c, 0x02888, 0x01488, 0x00486, 0x00000,
	0x1712a, 0x00040
}, rtl8821au_rf_vals3_def_or_bt[] = {
	0x00128, 0x08128, 0x10128, 0x201c8, 0x281c8, 0x301c8, 0x401c8,
	0x481c8, 0x501c8, 0x00000, 0x1712a, 0x00010, 0x063b5, 0x0e3b5,
	0x163b5, 0x1e3b5, 0x263b5, 0x2e3b5, 0x363b5, 0x3e3b5, 0x463b5,
	0x4e3b5, 0x563b5, 0x5e3b5, 0x00000, 0x00008, 0x001b6, 0x00492
}, rtl8821au_rf_vals3[] = {
	0x00145, 0x08145, 0x10145, 0x20196, 0x28196, 0x30196, 0x401c7,
	0x481c7, 0x501c7, 0x00000, 0x1712a, 0x00010, 0x056b3, 0x0d6b3,
	0x156b3, 0x1d6b3, 0x26634, 0x2e634, 0x36634, 0x3e634, 0x467b4,
	0x4e7b4, 0x567b4, 0x5e7b4, 0x00000, 0x00008, 0x0022a, 0x00594
}, rtl8821au_rf_vals4_def_or_bt[] = {
	0x00800, 0x00000, 0x1712a, 0x00002, 0x02000, 0x00000, 0x000c0,
	0x00064, 0x81184, 0x6016c, 0xefd83, 0x93fcc, 0x110eb, 0x1c27c,
	0x93016
}, rtl8821au_rf_vals4_ext_5g[] = {
	0x00820, 0x00000, 0x1712a, 0x00002, 0x02000, 0x00000, 0x000c0,
	0x00064, 0x81184, 0x6016c, 0xead53, 0x93bc4, 0x110e9, 0x1c67c,
	0x93015
}, rtl8821au_rf_vals4[] = {
	0x00900, 0x00000, 0x1712a, 0x00002, 0x02000, 0x00000, 0x000c0,
	0x00064, 0x81184, 0x6016c, 0xead53, 0x93bc4, 0x714e9, 0x1c67c,
	0x91016
}, rtl8821au_rf_vals5[] = {
	0x00006, 0x02000, 0x3824b, 0x3024b, 0x2844b, 0x20f4b, 0x18f4b,
	0x104b2, 0x08049, 0x00148, 0x7824b, 0x7024b, 0x6824b, 0x60f4b,
	0x58f4b, 0x504b2, 0x48049, 0x40148, 0x00000, 0x00100, 0x0adf3,
	0x09df0, 0x08d70, 0x07d6d, 0x06cee, 0x05ccc, 0x044ec, 0x034ac,
	0x0246d, 0x0106f, 0x0006c, 0x00000, 0x00010, 0x0adf2, 0x09def,
	0x08dec, 0x07de9, 0x06cec, 0x05ce9, 0x044ec, 0x034e9, 0x0246c,
	0x01469, 0x0006c, 0x00000, 0x00001, 0x38da7, 0x300c2, 0x288e2,
	0x200b8, 0x188a5, 0x10fbc, 0x08f71, 0x00240, 0x00000, 0x020a2,
	0x00080, 0x00120, 0x08120, 0x10120, 0x00085, 0x08085, 0x10085,
	0x18085, 0x00000, 0x00c31, 0x00622, 0xfc70b, 0x0017e, 0x51df3,
	0x00c01, 0x006d6, 0xfc649, 0x49661, 0x7843e, 0x00382, 0x51400,
	0x00160, 0x08160, 0x10160, 0x00124, 0x08124, 0x10124, 0x18124,
	0x0000c, 0x00140, 0x08140, 0x10140, 0x00124, 0x08124, 0x10124,
	0x18124, 0x00088, 0xf0e18, 0x1214c, 0x3000c, 0x539d2, 0xafe00,
	0x1f12a, 0x0c350, 0x0c350, 0x1712a,
};

static const struct rtwn_rf_prog rtl8821au_rf[] = {
	/* RF chain 0. */
	{
		nitems(rtl8821au_rf_regs0),
		rtl8821au_rf_regs0,
		rtl8821au_rf_vals0,
		{ 0 },
		NULL
	},
	/*
	 * No external PA/LNA; with or without BT.
	 */
	{
		nitems(rtl8821au_rf_regs1),
		rtl8821au_rf_regs1,
		rtl8821au_rf_vals1_def_or_bt,
		{ R21A_COND_BOARD_DEF, R21A_COND_BT, 0 },
		/*
		 * With external 5GHz PA and LNA.
		 */
		&(const struct rtwn_rf_prog){
			nitems(rtl8821au_rf_regs1),
			rtl8821au_rf_regs1,
			rtl8821au_rf_vals1_ext_5g,
			{ R21A_COND_EXT_PA_5G | R21A_COND_EXT_LNA_5G, 0 },
			/*
			 * Others.
			 */
			&(const struct rtwn_rf_prog){
				nitems(rtl8821au_rf_regs1),
				rtl8821au_rf_regs1,
				rtl8821au_rf_vals1,
				{ 0 },
				NULL
			}
		}
	},
	/*
	 * With external 5GHz PA and LNA.
	 */
	{
		nitems(rtl8821au_rf_regs2),
		rtl8821au_rf_regs2,
		rtl8821au_rf_vals2_ext_5g,
		{ R21A_COND_EXT_PA_5G | R21A_COND_EXT_LNA_5G, 0 },
		/*
		 * Others.
		 */
		&(const struct rtwn_rf_prog){
			nitems(rtl8821au_rf_regs2),
			rtl8821au_rf_regs2,
			rtl8821au_rf_vals2,
			{ 0 },
			NULL
		}
	},
	/*
	 * No external PA/LNA; with or without BT.
	 */
	{
		nitems(rtl8821au_rf_regs3),
		rtl8821au_rf_regs3,
		rtl8821au_rf_vals3_def_or_bt,
		{ R21A_COND_BOARD_DEF, R21A_COND_BT, 0 },
		/*
		 * Others.
		 */
		&(const struct rtwn_rf_prog){
			nitems(rtl8821au_rf_regs3),
			rtl8821au_rf_regs3,
			rtl8821au_rf_vals3,
			{ 0 },
			NULL
		}
	},
	/*
	 * No external PA/LNA; with or without BT.
	 */
	{
		nitems(rtl8821au_rf_regs4),
		rtl8821au_rf_regs4,
		rtl8821au_rf_vals4_def_or_bt,
		{ R21A_COND_BOARD_DEF, R21A_COND_BT, 0 },
		/*
		 * With external 5GHz PA and LNA.
		 */
		&(const struct rtwn_rf_prog){
			nitems(rtl8821au_rf_regs4),
			rtl8821au_rf_regs4,
			rtl8821au_rf_vals4_ext_5g,
			{ R21A_COND_EXT_PA_5G | R21A_COND_EXT_LNA_5G, 0 },
			/*
			 * Others.
			 */
			&(const struct rtwn_rf_prog){
				nitems(rtl8821au_rf_regs4),
				rtl8821au_rf_regs4,
				rtl8821au_rf_vals4,
				{ 0 },
				NULL
			}
		}
	},
	{
		nitems(rtl8821au_rf_regs5),
		rtl8821au_rf_regs5,
		rtl8821au_rf_vals5,
		{ 0 },
		NULL
	},
	{ 0, NULL, NULL, { 0 }, NULL }
};


/*
 * Registers to save before IQ calibration.
 */
static const uint16_t r21a_iq_bb_regs[] = {
	0x520, 0x550, 0x808, 0xa04, 0x90c, 0xc00, 0x838, 0x82c
};

static const uint16_t r21a_iq_afe_regs[] = {
	0xc5c, 0xc60, 0xc64, 0xc68
};

static const uint8_t r21a_iq_rf_regs[] = {
	0x65, 0x8f, 0x0
};

#endif	/* R21A_PRIV_H */
