// typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

/*
| S     | S     |Sign flag (Bit 7)                            |
| Z     |  Z    |Zero flag (Bit 6)                            |
| HC    |   H   |Half Carry flag (Bit 4)                      |
| P/V   |    P  |Parity/Overflow flag (Bit 2, V=overflow)     |
| N     |     N |Add/Subtract flag (Bit 1)                    |
| CY    |      C|Carry flag (Bit 0)                           |
*/
#define Zmask 0x40
#define Smask 0x80
#define Hmask 0x08
#define Pmask 0x04 // or perhaps PVmask
#define Vmask 0x04
#define Nmask 0x02
#define Cmask 0x01

#define rA 0
#define rF 1
#define rB 2
#define rC 3
#define rD 4
#define rE 5
#define rH 6
#define rL 7
#define rI 8
#define rR 9

#define regA regs[rA]
#define regF regs[rF]
#define regB regs[rB]
#define regC regs[rC]
#define regD regs[rD]
#define regE regs[rE]
#define regH regs[rH]
#define regL regs[rL]
#define regI regs[rI]
#define regR regs[rR]

#define rBC 0
#define rDE 1
#define rHL 2
#define rSP 3
#define rPC 4
#define rIX 5
#define rIY 6
#define rAF 7

#define ALU_AND 1
#define ALU_OR  2
#define ALU_XOR 3
#define ALU_XCF 4
#define ALU_NEG 5
#define ALU_ADC 6
#define ALU_ADD 7
#define ALU_SUB 8
#define ALU_SBC 9
#define ALU_CP  10
#define ALU_INC 11
#define ALU_DEC 12
#define ALU_RLC 13
#define ALU_RRC 14
#define ALU_RL  15
#define ALU_RR  16
#define ALU_SLA 17
#define ALU_SRA 18
#define ALU_SLL 19
#define ALU_SRL 20
#define ALU_BIT 21
#define ALU_CPL 22
#define ALU_IR  23

const int MAXRAM=0xff00;

class z80Emu
{
private:
	ushort pc,sp;
	uchar  regs[10];   // a f b c d e h l i r
	uchar  altregs[8]; // a'f'b'c'd'e'h'l'
	uchar  idxregs[4]; // IXIY - so regH and regL can be substituted for dd/fd
	uchar  ram[MAXRAM]; // of course, a z80 chip doesn't actually contain any RAM...
	int    index;

	ushort memread16(ushort ptr,int bumpR);
	ushort step_cb();
	ushort step_idx(int idx);
	ushort op_r16(int typ,int reg,ushort val,int idxoffset);
	uchar  op_alu(int op,uchar val);
	void   memwrite16(ushort addr,ushort val);
	void   setZ(uchar val);
	void   disass3(char *instr, int i_len,int typ,char *mask,uchar *idx,uchar op);
	void   disass_idx(char *t, int t_len,int idx);
	void   disass_idx_cb(char *t, int t_len,int idx);
	void   disass_cb(char *t, int t_len);
	void   disass_ed(char *t,int t_len);
	void   op_jump(int typ,uchar byte,int *time);
	void   batch_compare(int direction,int repeat);
	void   batch_load(int direction,int repeat);
	void   get_src(uchar bits012,uchar *src,int *dt);
	void   op_out(int typ,uchar dest,uchar src,int incdec);
	int    getZ(){return regF & Zmask;}
	int    getoffset(ushort ptr);
	int    parity(uchar val);
	int    op_ld(int typ,uchar byte,int *time,int alu);
	int    execute_ed(int *time);
	int    execute_cb(int *time);

public:
	z80Emu();

//	ushort step(); deprecated by execute_op
	ushort getpc(){return pc;}
	uchar  memread(ushort addr,int bumpR);
	void   execute_op(int *time);
	void   memwrite(ushort addr,uchar val);
	void   run();
	void   report(char *t, int t_len);
	void   memdump(char *t,int t_len,int nLines);
	void   disass(char *t, int t_len,ushort *pcptr);
	void   warm_reset();
	void   cold_reset();
	int    loadFile(char *fnam);
	int    loadFileAt(char *fnam,int addr);
};
