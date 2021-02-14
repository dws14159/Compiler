using ushort = unsigned short;
using uchar = unsigned char;
using schar = signed char;

enum { rA=0, rF, rB, rC, rD, rE, rH, rL, rI, rR };

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

enum { rBC=0, rDE, rHL, rSP, rPC, rIX, rIY, rAF };

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
	ushort op_r16(int typ,int reg,ushort val, bool addoffset);
	uchar  op_alu(int op,uchar val);
	void   memwrite16(ushort addr,ushort val);
	void   setZ(uchar val);
	void   disass3(char *instr, int i_len,int typ,char *mask,uchar *idx,uchar op);
	void   disass_idx(char *t, int t_len, uchar idx);
	void   disass_idx_cb(char *t, int t_len);
	void   disass_cb(char *t, int t_len);
	void   disass_ed(char *t,int t_len);
	void   op_jump(int typ,uchar byte,int *time);
	void   batch_compare(schar direction,int repeat);
	void   batch_load(schar direction,int repeat);
	void   get_src(uchar bits012,uchar *src,int *dt);
	void   op_out(int typ,uchar dest,uchar src, schar incdec);
	//int    getZ(){return regF & Zmask;}
	int    getoffset(ushort ptr);
	int    parity(uchar val);
	uchar  op_ld(int typ,uchar byte,int *time,int alu);
	uchar  execute_ed(int *time);
	uchar  execute_cb(int *time);

public:
	z80Emu();

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
