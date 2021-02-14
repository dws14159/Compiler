#include "stdafx.h"
#include "z80.h"
#include "intf.h"
extern Interface *theUI;

/*
| S     | S     |Sign flag (Bit 7)                            |
| Z     |  Z    |Zero flag (Bit 6)                            |
| HC    |   H   |Half Carry flag (Bit 4)                      |
| P/V   |    P  |Parity/Overflow flag (Bit 2, V=overflow)     |
| N     |     N |Add/Subtract flag (Bit 1)                    |
| CY    |      C|Carry flag (Bit 0)                           |
*/
uchar Zmask = 0x40;
uchar Smask = 0x80;
uchar Hmask = 0x08;
uchar Pmask = 0x04; // or perhaps PVmask
uchar Vmask = 0x04;
uchar Nmask = 0x02;
uchar Cmask = 0x01;

const int opWR=0, opRD=1;

z80Emu::z80Emu()
{
	cold_reset();
}

void z80Emu::memwrite16(ushort addr,ushort val)
{
	memwrite(addr  ,uchar(val));
	memwrite(addr+1,uchar(val>>8));
}

void z80Emu::memwrite(ushort addr,uchar val)
{
	if (addr >= 0xff00)
	{
		// memory mapped io
		theUI->io_write(addr,val);
	}
	else if (addr<MAXRAM)
		ram[addr]=val;
}

uchar z80Emu::memread(ushort addr,int bumpR)
{
	uchar ret=0;
	addr &= 0x0000ffff;
	if (addr >= 0xff00)
		ret=theUI->io_read(addr);
	else if (addr<MAXRAM)
		ret=ram[addr];
	if (bumpR)
		regR++;

	return ret;
}

ushort z80Emu::memread16(ushort ptr,int bumpR)
{
	ushort addr=memread(ptr+1,bumpR);
	addr *= 256;
	addr += memread(ptr,bumpR);
	return addr;
}

// Sets zero flag according to passed in value, NOT "to passed in value".
// If value is zero Z flag is set TRUE.
void z80Emu::setZ(uchar val)
{
	if (val)
		regF &= ~Zmask;
	else
		regF |= Zmask;
}

int z80Emu::getoffset(ushort ptr)
{
	int offset=memread(ptr,0);
	if (offset>127)
		offset-=256;
	return offset;
}

void z80Emu::run()
{
	// runmode=1;
	// exit condition? reimplement runmode and exit when interface resets it to 0
	// also need to reimplement yield so that the UI can function while we're hogging the CPU.
	// also do something clever with op_time so we can see how fast we're running
	int op_time=0;
	while (1 /*runmode*/)  
		execute_op(&op_time);
}

ushort z80Emu::step_idx(int idx) // IX(1)/IY(2) stuff
{
	ushort len=1,addr;
	uchar offset,val;
	uchar *il=(idx==1) ? &idxregs[0] : &idxregs[2];
	uchar *ih=(idx==1) ? &idxregs[1] : &idxregs[3];
	addr=ushort(*ih)*256+*il;
	switch (memread(pc+1,1))
	{
	case 0x21: // ld ix,NN
		*il = memread(pc+2,1);
		*ih = memread(pc+3,1);
		len=4;
		break;
	case 0x36: // ld (ix+N),N		
		offset=memread(pc+2,1);
		val=memread(pc+3,1);
		memwrite(addr+offset, val);
		len=4;
		break;
	case 0x39: // add ix,sp
		{
			ushort newix=*ih*256+*il+sp;
			*ih=uchar(newix/256);
			*il=uchar(newix);
			len=2;
		}
		break;
	case 0x66: // ld h,(ix+N)
		offset=memread(pc+2,1);
		regH=memread(addr+offset,0);
		len=3;
		break;
	case 0x6e: // ld l,(ix+N)
		offset=memread(pc+2,1);
		regL=memread(addr+offset,0);
		len=3;
		break;
	case 0xe1: // pop ix
		*il=memread(sp  ,0);
		*ih=memread(sp+1,0);
		sp+=2;
		len=2;
		break;
	case 0xe5: // push ix
		sp-=2;
		memwrite(sp  ,*il);
		memwrite(sp+1,*ih);
		len=2;
		break;
	default:
		break;
	}
	return len;
}

ushort z80Emu::step_cb()
{
	// I _think_ all CB are length 2. Function returns length of instruction including the CB.
	ushort len=2;
	switch (memread(pc+1,1))
	{
	case 0x27: // sla a
		regA<<=1;
		break;
	case 0x2f: // sra a
		regA>>=1;
		break;
	default:
		break;
	}
	return len;
}

int z80Emu::parity(uchar val)
{
	int parity=1;
	while (val)
	{
		if (val & 1)
			parity=1-parity;
		val/=2;
	}
	return parity;
}

// op_alu: perform maths and set flags
uchar z80Emu::op_alu(int op,uchar val)
{
	uchar new_flag=0;
	uchar setflags=0; // indicates which flags to set; all others are zeroed
	uchar test=0;
	switch (op)
	{
	case ALU_AND:
		regA &= val;
		test=regA;
		setflags=Smask|Zmask|Hmask|Pmask;
		// AND sets N and C zero
		break;
	case ALU_OR:
		regA |= val;
		test=regA;
		setflags=Smask|Zmask|Hmask|Pmask;
		// OR sets N and C zero
		break;
	case ALU_XOR:
		regA ^= val;
		test=regA;
		setflags=Smask|Zmask|Hmask|Pmask;
		// OR sets N and C zero
		break;
	case ALU_XCF: // set or complement carry flag
		// get new value for carry flag. val: true=set, false=complement
		new_flag = val ? Cmask : ((regF & Cmask) ^ Cmask);

		// merge other flags
		new_flag |= (regF & ~Cmask);

		// scf/ccf reset H and N
		new_flag &= ~(Hmask|Nmask);

		// setflags is still zero so new_flag will just be assigned to regF after the switch
		break;
	case ALU_NEG: // neg
		regA^=255;
		regA++;
		test=regA;
		new_flag=Nmask;
		if (regA==0x80) // a was -128 so neg overflows: a was 1000 0000, 2's comp 0111 1111, inc 1000 0000
			new_flag|=Vmask;
		setflags=~(Nmask|Vmask);
		break;
	case ALU_ADC: // add with carry
		if ((255-regA)<(val+(regF&Cmask))) new_flag=Vmask|Cmask;
		regA += val;
		test=regA;
		setflags=~(Nmask|Cmask);
		break;
	case ALU_ADD: // add without carry
		if ((255-regA)<val) new_flag=Vmask|Cmask;
		regA += val;
		test=regA;
		setflags=~(Nmask|Cmask);
		break;
	case ALU_SUB: // subtract without carry
		if (val>regA) new_flag=Vmask|Cmask|Nmask;
		regA-=val;
		test=regA;
		setflags=~(Vmask|Nmask);
		break;
	case ALU_SBC: // subtract with carry
		if ((val+(regF|Cmask)>regA) || (val==0xff)) new_flag=Vmask|Cmask|Nmask;
		regA-=val;
		test=regA;
		setflags=~(Vmask|Nmask);
		break;
	case ALU_CP: // compare
		if (val>regA) new_flag=Vmask|Cmask|Nmask;
		test=regA-val;
		setflags=~(Vmask|Nmask);
		break;
	case ALU_INC: // inc (8 bit)
		test=val+1;
		new_flag=regF&Cmask;
		setflags=Smask|Zmask|Hmask;
		break;
	case ALU_DEC: // dec (8 bit)
		test=val-1;
		new_flag=regF&Cmask;
		setflags=Smask|Zmask|Hmask;
		break;
	case ALU_SLA: // sla: C<-76543210<-0 how does this differ from rlc?
	case ALU_RLC: // rlc: val*=2, overflow goes in carry. rlca uses same code but restores SZP flags
		if (val>127) new_flag=Cmask;
		test=val*2;
		setflags=(Smask|Zmask|Pmask);
		break;
	case ALU_SRA: // 0->76543210->C appears to be the same as RRC
	case ALU_RRC: // rlc: val/=2, underflow goes in carry.
		if (val&1) new_flag=Cmask;
		test=val/2;
		setflags=(Smask|Zmask|Pmask);
		break;
	case ALU_SLL: // C<-76543210<-C appears to be the same as RL
	case ALU_RL:  // rl: C<-76543210<-C
		test=val*2 + (regF&Cmask); // test=6543210C
		if (val&128) new_flag=Cmask; // C=bit 7 of val
		setflags=(Smask|Zmask|Pmask);
		break;
	case ALU_SRL: // C->76543210->C appears to be the same as RR
	case ALU_RR:  // rl: C->76543210->C
		test=val/2 + ((regF&Cmask)?128:0); // test=C7654321
		if (val&Cmask) new_flag=Cmask; // C=bit 0 of val
		setflags=(Smask|Zmask|Pmask);
		break;
	case ALU_BIT: // bit test. val has already been ANDed with the bit. 
		// If we need to set parity we'll need another parameter on op_alu() for the bit number or an extra 
		// parity test (or perhaps the caller can call parity() and set P accordingly)
		test=val;
		new_flag=Hmask|(regF&(Cmask|Smask|Pmask)); // sets H; clears N; ignores C; S and P unknown so leave alone
		setflags=(Zmask);
		break;
	case ALU_CPL: // cpl
		regA^=255;
		new_flag=regF | Hmask | Nmask;
		break;
	case ALU_IR: // ld a,i/r
		test=regA=val;
		new_flag=regF & Cmask;
		setflags=(Smask|Zmask|Pmask);
		break;
	}
	if (setflags & Smask) if (test & Smask) new_flag |= Smask;
	if (setflags & Zmask) if (!test)        new_flag |= Zmask;
	if (setflags & Hmask) if (test & Hmask) new_flag |= Hmask;
	if (setflags & Pmask) if (parity(test)) new_flag |= Pmask;
	regF = new_flag;
	return test;
}

void z80Emu::execute_op(int *time)
{
	uchar byte=memread(pc,1);
	uchar val1,val2;
	ushort addr;
	// anything we don't know will look like a nop
	*time=4;
	switch (byte)
	{
	case 0x00: // nop
		*time=4;
		pc++;
		break;
	case 0x02: // ld (bc),a
		addr=op_r16(1,rBC,0,false);
		memwrite(addr,regA);
		*time=7;
		pc++;
		break;
	case 0x07: // rlca
		// Perform rlc a, but flags SZP are not affected and time=4
		val1=regF & (Smask|Zmask|Pmask);
		regA=op_alu(ALU_RLC,regA);
		regF = regF & ~(Smask|Zmask|Pmask) | val1;
		*time=4;
		break;
	case 0x08: // ex af,af'
		val1=altregs[rA];
		val2=altregs[rF];
		altregs[rA]=regA;
		altregs[rF]=regF;
		regA=val1;
		regF=val2;
		*time=4;
		pc++;
		break;
	case 0x0a: // ld a,(bc)
		addr=op_r16(1,rBC,0,false);
		regA=memread(addr,0);
		*time=7;
		pc++;
		break;
	case 0x10: // djnz d
		op_jump(6,0x10,time);
		break;
	case 0x0f: // rrca
		// Perform rrc a, but flags SZP are not affected and time=4
		val1=regF & (Smask|Zmask|Pmask);
		regA=op_alu(ALU_RRC,regA);
		regF = regF & ~(Smask|Zmask|Pmask) | val1;
		pc++;
		*time=4;
		break;
	case 0x12: // ld (de),a
		addr=op_r16(1,rDE,0,false);
		memwrite(addr,regA);
		*time=7;
		pc++;
		break;
	case 0x17: // rla
		// Perform rl a, but flags SZP are not affected and time=4
		val1=regF & (Smask|Zmask|Pmask);
		regA=op_alu(ALU_RL,regA);
		regF = regF & ~(Smask|Zmask|Pmask) | val1;
		*time=4;
		break;
	case 0x18: // jr d
		op_jump(4,0x18,time);
		break;
	case 0x1a: // ld a,(de)
		addr=op_r16(1,rDE,0,false);
		regA=memread(addr,0);
		*time=7;
		pc++;
		break;
	case 0x1f: // rra
		// Perform rr a, but flags SZP are not affected and time=4
		val1=regF & (Smask|Zmask|Pmask);
		regA=op_alu(ALU_RR,regA);
		regF = regF & ~(Smask|Zmask|Pmask) | val1;
		*time=4;
		break;
	case 0x22: // ld (NN),hl
		addr=memread16(pc+1,1);
		memwrite16(addr,op_r16(1,rHL,0,false));
		*time=16;
		pc+=3;
		break;
	case 0x2a: // ld hl,(NN)
		addr=memread16(pc+1,1);
		op_r16(0,rHL,memread16(addr,0),false);
		*time=20;
		pc+=3;
		break;
	case 0x2f: // cpl
		op_alu(ALU_CPL,0);
		*time=4;
		break;
	case 0x32: // ld (NN),a
		addr=memread16(pc+1,1);
		memwrite(addr,regA);
		*time=13;
		pc+=3;
		break;
	case 0x37: // scf
		*time=4;
		op_alu(ALU_XCF,0);
		pc++;
		break;
	case 0x3a: // ld a,(NN)
		addr=memread16(pc+1,1);
		regA=memread(addr,0);
		*time=13;
		pc+=3;
		break;
	case 0x3f: // ccf
		*time=4;
		op_alu(ALU_XCF,1);
		pc++;
		break;
	case 0x76: // halt;
		*time=4;
		break;
	case 0xc3: // jp NN
		op_jump(3,0xc3,time);
		break;
	case 0xc6: // add a,N
		op_alu(ALU_ADD,memread(pc+1,1));
		*time=7;
		pc+=2;
		break;
	case 0xc9: // ret
		op_jump(5,0xc9,time);
		*time=10;
		break;
	case 0xcb: // CB stuff;
		pc+=execute_cb(time);
		break;
	case 0xcd: // call NN
		op_jump(2,0xcd,time);
		break;
	case 0xce: // adc a,N
		op_alu(ALU_ADC,memread(pc+1,1));
		*time=7;
		pc+=2;
		break;
	case 0xd3: // out (n),a
		val1=memread(pc+1,1);
		op_out(1,val1,regA,0);
		pc+=2;
		break;
	case 0xd6: // sub n
		val1=memread(pc+1,1);
		op_alu(ALU_SUB,val1);
		*time=7;
		pc+=2;
		break;
	case 0xd9: // exx
		val1=regB; regB=altregs[rB]; altregs[rB]=val1;
		val1=regC; regB=altregs[rC]; altregs[rC]=val1;
		val1=regD; regB=altregs[rD]; altregs[rD]=val1;
		val1=regE; regB=altregs[rE]; altregs[rE]=val1;
		val1=regH; regB=altregs[rH]; altregs[rH]=val1;
		val1=regL; regB=altregs[rL]; altregs[rL]=val1;
		*time=4;
		pc++;
		break;
	case 0xdd: // use ix instead of hl
		{
			int other=0;
			index=1;
			pc++;
			execute_op(&other);
			index=0;
			*time=4+other;
		}
		break;
	case 0xde: // sbc a,n
		val1=memread(pc+1,1);
		op_alu(ALU_SBC,val1);
		*time=7;
		pc+=2;
		break;
	case 0xe3: // ex (sp),hl/ix/iy
		addr=op_r16(1,rSP,0,false);
		memwrite16(addr,op_r16(1,rHL,0,false));
		*time=19;
		pc++;
		break;
	case 0xe6: // and n
		val1=memread(pc+1,1);
		op_alu(ALU_AND,val1);
		*time=7;
		pc+=2;
		break;
	case 0xe9: // jp (hl)
		op_jump(8,0xe9,time);
		break;
	case 0xeb: // ex de,hl
		val1=regH;
		val2=regL;
		regH=regD;
		regL=regE;
		regD=val1;
		regE=val2;
		*time=4;
		pc++;
		break;
	case 0xed: // ED stuff;
		pc+=execute_ed(time);
		break;
	case 0xee: // xor n
		val1=memread(pc+1,1);
		op_alu(ALU_XOR,val1);
		*time=7;
		pc+=2;
		break;
	case 0xf6: // or n
		val1=memread(pc+1,1);
		op_alu(ALU_OR,val1);
		*time=7;
		pc+=2;
		break;
	case 0xf9: // ld sp,hl/ix/iy
		op_r16(0,rSP,op_r16(1,rHL,0,0),false);
		*time=6;
		pc++;
		break;
	case 0xfd: // use iy instead of hl
		{
			int other=0;
			index=2;
			pc++;
			execute_op(&other);
			index=0;
			*time=4+other;
		}
		break;
	case 0xfe: // cp n
		val1=memread(pc+1,1);
		op_alu(ALU_CP,val1);
		*time=7;
		pc+=2;
		break;
	default:
		{
			int rDD=(byte/16)&3;

			if (0){} // for a vaguely regular structure below
/*
Instructions not yet implemented

  daa; out(n),a; in a,(n); di; ei; im X; ind; indr; ini; inir;
  ldd; lddr; ldi; ldir; outd; otdr; outi; otir; 
  rld; rrd; 
*/
			// 000??000 nop/ex af,af'/djnz d/jr d handled above                               // 000??000
			else if ((byte & 0xe7)==0x20) op_jump(4,byte,time);                               // 001cc000 jr cc,d
			else if ((byte & 0xcf)==0x01) pc+=op_ld( 3,byte,time,0);                          // 00dd0001 ld dd,NN
			// 00??0010 ld (bc/de),a/ld (nn),hl/ld (nn),a handled above                       // 00??0010
			else if ((byte & 0xcf)==0x03) { pc++;*time=6;op_r16(0,rDD,op_r16(1,rDD,0,false)+1,false);}// 00dd0011 inc dd			
			else if ((byte & 0xc7)==0x04) pc+=op_ld(14,byte,time,0);                          // 00rrr100 inc r 0x04+r*8
			else if ((byte & 0xc7)==0x05) pc+=op_ld(15,byte,time,0);                          // 00rrr101 dec r 0x05+r*8
			else if ((byte & 0xc7)==0x06) pc+=op_ld( 2,byte,time,0);                          // 00rrr110 ld r,N
			else if ((byte & 0xcf)==0x09) { pc++; *time=11; op_r16(4,rDD,0,false); }              // 00dd1001 add hl,dd
			// 00dd1010 ld a,(bc)/ld a,(de)/ld hl,(nn)/ld a,(nn) handled above                // 00??1010
			else if ((byte & 0xcf)==0x03) { pc++;*time=6;op_r16(0,rDD,op_r16(1,rDD,0,false)-1,false);}// 00dd1011 dec dd
			// 00???111 rlca/rrca/rla/rra/daa/cpl/scf/ccf handled above                       // 00???111
			else if ((byte & 0xc0)==0x40) pc+=op_ld( 1,byte,time,0);                          // 01rrrsss ld r,s
			// 01110110 halt handled above                                                    // 01110110
			else if ((byte & 0xf8)==0x80) pc+=op_ld( 6,byte,time,ALU_ADD);                    // 10000rrr add a,r 0x80+r
			else if ((byte & 0xf8)==0x88) pc+=op_ld( 6,byte,time,ALU_ADC);                    // 10001rrr adc a,r 0x88+r
			else if ((byte & 0xf8)==0x90) pc+=op_ld( 6,byte,time,ALU_SUB);                    // 10010rrr sub a,r 0x90+r
			else if ((byte & 0xf8)==0x98) pc+=op_ld( 6,byte,time,ALU_SBC);                    // 10011rrr sbc a,r 0x98+r
			else if ((byte & 0xf8)==0xa0) pc+=op_ld( 6,byte,time,ALU_AND);                    // 10100rrr and r 0xa0+r
			else if ((byte & 0xf8)==0xa8) pc+=op_ld( 6,byte,time,ALU_XOR);                    // 10101rrr xor r 0xa8+r
			else if ((byte & 0xf8)==0xb0) pc+=op_ld( 6,byte,time,ALU_OR );                    // 10110rrr or r 0xb0+r
			else if ((byte & 0xf8)==0xb8) pc+=op_ld( 6,byte,time,ALU_CP );                    // 10111rrr cp r 0xb8+r
			else if ((byte & 0xc7)==0xc0) op_jump(5,byte,time);                               // 11???000 ret cc
			else if ((byte & 0xcf)==0xc1) { pc++; *time=10; op_r16(3,rDD,0,false); }              // 11dd0001 pop  dd
			// 11??1001 ret/exx/jp (hl)/ld sp,hl handled above                                // 11??1001
			else if ((byte & 0xc7)==0xc2) op_jump(3,byte,time);	                              // 11ccc010 jp cc,nn
			// 110??011 jp nn/cb prefix/out(n),a/in a,(n) handled above                       // 110??011
			// 111??011 ex (sp),hl/ex de,hl/di/ei handled above                               // 111??011
			else if ((byte & 0xc7)==0xc4) op_jump(2,byte,time);                               // 11???100 call cc,nn
			else if ((byte & 0xcf)==0xc5) { pc++; *time=10; op_r16(2,rDD,0,false); }              // 11dd0101 push dd
			// 11dd1101 call nn/ix prefix/ed prefix/iy prefix handled above                   // 11dd1101
			// 11???110 add a,n/adc a,n/sub n/sbc a,n/and n/xor n/or n/cp n handled above     // 11???110
			else if ((byte & 0xc7)==0xc7) op_jump(1,byte,time);                               // 11aaa111 rst aaa
			}
		break;
	}
}

// Handles jumps of various kinds - 1=rst; 2=call(cc); 3=jp(cc); 4=jr(cc); 5=ret(cc); 6=djnz; 7=reti/retn
// No return value - PC points to the right place at the end.
void z80Emu::op_jump(int typ,uchar byte,int *time)
{
	int bits345=(byte/8)&7;
	uchar offset;
	ushort dest;
	if (typ==4) bits345&=3; // only nz/z/nc/c for jr
	int cond=0;
	if (byte==0xcd || byte==0xc3 || byte==0x18 || byte==0xc9) // unconditional call/jump/jr/ret
		cond=1;
	else switch (bits345)
	{
		case 0: cond=!(regF&Zmask); break; // nz
		case 1: cond=  regF&Zmask ; break; //  z
		case 2: cond=!(regF&Cmask); break; // nc
		case 3: cond=  regF&Cmask ; break; //  c
		case 4: cond=!(regF&Pmask); break; // po
		case 5: cond=  regF&Pmask ; break; // pe
		case 6: cond=!(regF&Smask); break; //  p
		case 7: cond=  regF&Smask ; break; //  m
	}
	switch (typ)
	{
	case 1: // rst
		op_r16(2,-1,pc+1,false); // push return address
		pc = byte & 0x38;
		*time=11;
		break;

	case 2: // call cc,NN
		dest=memread16(pc+1,1);
		pc+=3;
		if (cond)
		{
			op_r16(2,-1,pc,false); // push return address
			pc=dest;
			*time=17;
		}
		else *time=10;
		break;

	case 3: // jp cc,NN
		dest=memread16(pc+1,1);
		pc+=3;
		if (cond)
		{
			pc=dest;
			*time=10;
		}
		else *time=10; // ?
		break;

	case 4: // jr cc,N
		offset=memread(pc+1,1);
		pc+=2;
		if (cond)
		{
			pc+=offset;
			if (offset>127)
				pc-=256;
			*time=12;
		}
		else *time=7;
		break;

	case 5: // ret cc
		pc++;
		if (cond)
		{
			pc=op_r16(3,-1,0,false); // pop return address
			*time=11;
		}
		else *time=5;
		break;

	case 6: // djnz
		offset=memread(pc+1,1);
		pc+=2;
		regB--;
		if (regB)
		{
			pc+=offset;
			if (offset>127)
				pc-=256;
			*time=12;
		}
		else *time=8;
		break;
	
	case 7: // reti, retn
		pc=op_r16(3,-1,0,false); // pop return address
		*time=14;
		break;

	case 8: // jp (hl/ix/iy)
		pc=memread16(op_r16(1,rHL,0,false),1);
		*time=4;
		break;
	}
}

uchar z80Emu::execute_cb(int *time)
{
	uchar len=2;
	uchar byte;
	if (index==0)
		byte=memread(pc+1,1);
	else
		byte=memread(pc+2,1);
	if(0){}
	else if ((byte & 0xf8)==0x00) op_ld(16,byte,time,ALU_RLC); // rlc r: 00000rrr
	else if ((byte & 0xf8)==0x08) op_ld(16,byte,time,ALU_RRC); // rrc r: 00001rrr
	else if ((byte & 0xf8)==0x10) op_ld(16,byte,time,ALU_RL);  // rl r:  00010rrr
	else if ((byte & 0xf8)==0x18) op_ld(16,byte,time,ALU_RR);  // rr r:  00011rrr
	else if ((byte & 0xf8)==0x20) op_ld(16,byte,time,ALU_SLA); // sla r: 00100rrr
	else if ((byte & 0xf8)==0x28) op_ld(16,byte,time,ALU_SRA); // sra r: 00101rrr
	else if ((byte & 0xf8)==0x30) op_ld(16,byte,time,ALU_SLL); // sll r: 00110rrr
	else if ((byte & 0xf8)==0x38) op_ld(16,byte,time,ALU_SRL); // srl r: 00111rrr
	else if ((byte & 0xc0)==0x40) op_ld(18,byte,time,0);       // bit b,r: 01bbbrrr
	else if ((byte & 0xc0)==0x80) op_ld(17,byte,time,0);       // res b,r: 10bbbrrr
	else if ((byte & 0xc0)==0xc0) op_ld(17,byte,time,0);       // set b,r: 11bbbrrr
	return len;
}

// Should set flags but I can't find any clear details on how they are affected.
void z80Emu::batch_compare(schar direction,int repeat)
{
	ushort addr;
	uchar src;
	do {
		addr=op_r16(1,rHL,0,true);
		src=memread(addr,0);
		op_alu(ALU_CP,src);
		op_r16(opWR,rHL,op_r16(opRD,rHL,0,false)+direction,false);
		op_r16(opWR,rBC,op_r16(opRD,rBC,0,false)-1,false);
	} while (regA != src && op_r16(opRD,rBC,0,false) && repeat);
}

/*
+-+-------------+----+---+------+------------+---------------------+----------------------+
|I|Mnemonic     |Clck|Siz|SZHPNC|  OP-Code   |    Description      |        Notes         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|LDD          | 16 | 2 |--0*0-|ED A8       |Load and Decrement   |(DE)=(HL),HL=HL-1,#   |
|3|LDDR         |21/1| 2 |--000-|ED B8       |Load, Dec., Repeat   |LDD till BC=0         |
|3|LDI          | 16 | 2 |--0*0-|ED A0       |Load and Increment   |(DE)=(HL),HL=HL+1,#   |
|3|LDIR         |21/1| 2 |--000-|ED B0       |Load, Inc., Repeat   |LDI till BC=0         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
*/

// Should set flags but I can't find any clear details on how they are affected.
void z80Emu::batch_load(schar direction,int repeat)
{
	ushort src,dst;
	uchar byt;
	do {
		src=op_r16(opRD,rHL,0,false);
		dst=op_r16(opRD,rDE,0,false);
		byt=memread(src,0);
		memwrite(dst,byt);
		op_r16(opWR,rHL,op_r16(opRD,rHL,0,false)+direction,false);
		op_r16(opWR,rDE,op_r16(opRD,rDE,0,false)+direction,false);
		op_r16(opWR,rBC,op_r16(opRD,rBC,0,false)-1,false);
	} while (repeat && op_r16(opRD,rBC,0,false));
}

// src ignored if incdec - dereferences hl instead
void z80Emu::op_out(int typ,uchar dest,uchar src, schar incdec)
{
	ushort addr;
	uchar newF;
	switch (typ)
	{
	case 0: // sends src to dest, ignores the rest. Internal function which just happens to match case 1 right now.
	case 1: // out (dest),src
		theUI->handle_out(dest,src);
		break;

	case 2: // outi/outd: (c)=(hl); hl+=incdec; b--; flags set according to b. src/dest ignored
		addr=op_r16(opRD,rHL,0,false);
		src=memread(addr,0);
		op_out(0,regC,src,0);
		op_r16(opWR,rHL,op_r16(opRD,rHL,0,false)+incdec,false);
		regB--;
		newF=regF&(Smask|Hmask|Pmask|Cmask);
		newF|=Nmask;
		if (!regB) newF|=Zmask;
		regF=newF;
		break;

	case 3: // otir/otdr
		do {
			op_out(2,0,0,incdec);
		} while (!(regF&Zmask));
	}
}

uchar z80Emu::execute_ed(int *time)
{
	uchar len=2;
	uchar byte=memread(pc+1,1);
	int rDD=(byte/16)&3;
	switch (byte)
	{
	case 0xa3: op_out(2,0,0, 1); break; // outi
	case 0xab: op_out(2,0,0,-1); break; // outd
	case 0xb3: op_out(3,0,0, 1); break; // otir
	case 0xbb: op_out(3,0,0,-1); break; // otdr

	case 0x41: op_out(1,regC,regB,0); break;
	case 0x49: op_out(1,regC,regC,0); break;
	case 0x51: op_out(1,regC,regD,0); break;
	case 0x59: op_out(1,regC,regE,0); break;
	case 0x61: op_out(1,regC,regH,0); break;
	case 0x69: op_out(1,regC,regL,0); break;
	case 0x71: op_out(1,regC,   0,0); break;
	case 0x79: op_out(1,regC,regA,0); break;

	case 0x44: // neg
		op_alu(ALU_NEG,0);
		*time=8;
		break;
	case 0x47: // ld i,a
		regI=regA;
		*time=9;
		break;
	case 0x4f: // ld r,a
		regR=regA;
		*time=9;
		break;
	case 0x57: // ld a,i
		op_alu(ALU_IR,regI);
		*time=9;
		break;
	case 0x5f: // ld a,r
		op_alu(ALU_IR,regR);
		*time=9;
		break;
	case 0x67: // rrd
		{
			ushort addr=op_r16(1,rHL,0,true);
			uchar mem=memread(addr,0);

			// a:(hl) -> nybbles 1,2,3,4
			// nybbles 1,4,2,3 -> a:(hl)

			uchar nyb1=(regA >> 4) & 0x0f;
			uchar nyb2=regA & 0xf0;
			uchar nyb3=(mem >> 4) & 0x0f;
			uchar nyb4=mem & 0x0f;

			regA = (nyb1 << 4) | nyb4;
			mem =  (nyb2 << 4) | nyb3;

			memwrite(addr,mem);

			uchar new_flag=regF & Cmask;
			if (regA & 0x80)  new_flag |= Smask;
			if (!regA)        new_flag |= Zmask;
			if (parity(regA)) new_flag |= Pmask;
			regF = new_flag;
		}
		break;
	case 0x6f: // rld 
		{
			ushort addr=op_r16(1,rHL,0,true);
			uchar mem=memread(addr,0);

			// a:(hl) -> nybbles 1,2,3,4
			// nybbles 1,3,4,2 -> a:(hl)

			uchar nyb1=(regA >> 4) & 0x0f;
			uchar nyb2=regA & 0xf0;
			uchar nyb3=(mem >> 4) & 0x0f;
			uchar nyb4=mem & 0x0f;

			regA = (nyb1 << 4) | nyb3;
			mem =  (nyb4 << 4) | nyb2;

			memwrite(addr,mem);

			uchar new_flag=regF & Cmask;
			if (regA & 0x80)  new_flag |= Smask;
			if (!regA)        new_flag |= Zmask;
			if (parity(regA)) new_flag |= Pmask;
			regF = new_flag;
		}
		break;
	case 0xa0: // ldi
		batch_load(1,0);
		break;
	case 0xa1: // cpi
		batch_compare(1,0);
		break;
	case 0xa8: // ldd
		batch_load(-1,0);
		break;
	case 0xa9: // cpd
		batch_compare(-1,0);
		break;
	case 0xb0: // ldir
		batch_load(1,1);
		break;
	case 0xb1: // cpir
		batch_compare(1,1);
		break;
	case 0xb8: // lddr
		batch_load(-1,1);
		break;
	case 0xb9: // cpdr
		batch_compare(-1,1);
		break;
	default:
		if(0){}
		else if ((byte & 0xcf)==0x43) len=op_ld(5,byte,time,0);                        // 01dd0011 ld (NN),dd
		else if ((byte & 0xcf)==0x4a) { len=2; *time=15; op_r16(4,rDD,regF & Cmask,false);}// 01dd1010 adc hl,dd
		else if ((byte & 0xcf)==0x4b) len=op_ld(4,byte,time,0);                        // 01dd1011 ld dd,(NN) 
		else if ((byte & 0xcf)==0x42) { len=2; *time=15; op_r16(5,rDD,regF & Cmask,false);}// 01dd0010 sbc hl,dd
		break;
	}
	return len;
}

// get register or dereference hl for opcode classes such as ld a,b/ld a,(hl)
// was done generically, but this proved to be a problem when 0x38 caused (hl) to be
// read and a read-to-reset button was reset when it shouldn't have been.
void z80Emu::get_src(uchar bits012,uchar *src,int *dt)
{
	int reg_xlate[]={rB,rC,rD,rE,rH,rL,0,rA};
	if (bits012==6)
	{
		// problem: for byte=0x3e ld a,N, this reads (hl), which resets a read-to-reset button

		ushort addr=op_r16(1,rHL,0,true); // 1==use pc+1 if index!=0
		*src=memread(addr,0);
		*dt=1; // isn't always 3. for LD, yes, but for BIT it's 4.  The 
		// value is therefore instruction dependent.
	}
	else *src=regs[reg_xlate[bits012]];
}

// so named because it was originally just performing ld.  However, it seems
// to have grown...
uchar z80Emu::op_ld(int typ,uchar byte,int *time,int alu)
{
	// Registers aren't stored in 8 sequential locations so we need a conversion table
	// The index represents registers b,c,d,e,h,l,?,a (we need to handle (hl) separately)
	uchar len = 1, val = 0;
	int write = 0, t_hl = 0, t_reg = 0;
	int reg_xlate[]={rB,rC,rD,rE,rH,rL,0,rA};
	uchar bits012=byte&7;
	uchar bits345=(byte/8)&7;
	uchar bits45=bits345/2;
	uchar src;
	int dt=0;
	switch (typ)
	{
	case 1: // ld r,s
		{
			write=2;
			get_src(bits012,&src,&dt);
			val=src;
			t_hl=7;
			t_reg=4;
			if (bits012==6)
				t_reg=7;
			if (index)
				len++; // extra byte for ld (ix+N),N
		}
		break;
	case 2: // ld r,N
		{
			len=2;
			write=2;
			t_hl=10;
			t_reg=4;
			if (index!=0 && bits012==6)
			{
				len=3; // extra byte for ld (ix+N),N
				val=memread(pc+2,1);
			}
			else
				val=memread(pc+1,1);
		}
		break;
	case 3: // ld dd,NN (bc,de,hl,sp)
		{
			op_r16(0,bits45,memread16(pc+1,1),false);
			*time=10;
			len=3;
		}
		break;
	case 4: // ld dd,(NN) (bc,de,hl,sp)
		{
			ushort addr=memread16(pc+2,1);
			op_r16(0,bits45,memread16(addr,0),false);
			*time=20;
			len=4; // these are ED instructions. 2a XX XX ld hl,(NN) is handled separately
		}
		break;
	case 5: // ld (NN),dd (bc de hl sp)
		{
			ushort addr=memread16(pc+2,1);
			memwrite16(addr,op_r16(1,bits45,0,false));
			*time=20;
			len=4; // these are ED instructions.
		}
		break;
	case 6: // add/adc/sub/sbc/and/xor/or/cp a,r
		{
			get_src(bits012,&src,&dt);
			*time=4+dt*3;
			if (!index)
				*time+=8;
			op_alu(alu,src);
			break;
		}
		break;
/*	case 7: // add a,r
	case 8: // and r
	case 9: // or r
	case 10: // xor r
	case 11: // sub r
	case 12: // sbc r
	case 13: // cp r */

	case 14: // inc r
		{
			if (bits345==6)
			{
				// inc (hl)
				ushort addr=op_r16(1,rHL,0,true);
				memwrite(addr, op_alu(11,memread(addr,0)));
				*time=11;
			}
			else
			{
				// inc r
				*time=4;
				regs[reg_xlate[bits345]] = op_alu(11,regs[reg_xlate[bits345]]);
			}
		}
		break;
	case 15: // dec r
		{
			if (bits345==6)
			{
				// dec (hl)
				ushort addr=op_r16(1,rHL,0,true);
				memwrite(addr, op_alu(12,memread(addr,0)));
				*time=11;
			}
			else
			{
				// dec r
				*time=4;
				regs[reg_xlate[bits345]] = op_alu(12,regs[reg_xlate[bits345]]);
			}
		}
		break;
		// Thought: if we pass in the op_alu op number this function can be somewhat simplified; all the below
		// for instance may be reduced to one case.
	case 16: // rlc/rrc/rl/rr r
		{
			write=1;
			t_hl=15;
			t_reg=8;
			get_src(bits012,&src,&dt);
			val=op_alu(alu,src);
		}
		break;
	case 17: // bit set or reset
		{
			get_src(bits012,&src,&dt);
			src = (byte&64) ? (src | (1<<bits345)) : (src & ~(1<<bits345));
			write=1;
			t_hl=15;
			t_reg=8;
			val=src;
		}
		break;
	case 18: // bit test
		{
			get_src(bits012,&src,&dt);
			op_alu(ALU_BIT, src & (1<<bits345));
			*time=8+dt*4;
		}
		break;
	}

	if (write==1)
	{
		if (bits012==6)
		{
			ushort addr=op_r16(1,rHL,0,true);
			memwrite(addr, val);
			*time=t_hl;
		}
		else
		{
			regs[reg_xlate[bits012]]=val;
			*time=t_reg;
		}
	}
	else if (write==2)
	{
		if (bits345==6)
		{
			ushort addr=op_r16(1,rHL,0,true);
			memwrite(addr, val);
			*time=t_hl;
		}
		else
		{
			regs[reg_xlate[bits345]]=val;
			*time=t_reg;
		}
	}
	return len;
}

// if we are using an index reg,
// idxoffset=0: return the value of the index reg
// idxoffset=1: return the value of the index reg + the offset in pc+1
//   (1 because dd/fd is executed as a 1 byte instruction)
ushort z80Emu::op_r16(int typ,int reg,ushort val,bool addoffset)
{
	uchar hi = (uchar)(val / 256);
	uchar lo = (uchar)val;
	ushort ret=0;
	char offset=0;
	if (index==1 && reg==rHL)
	{
		if (addoffset)
		{
			offset=(char)memread(pc+1,1);
		}
		return op_r16(typ,rIX,val,false)+offset;
	}
	else if (index==2 && reg==rHL)
	{
		if (addoffset)
		{
			offset=(char)memread(pc+1,1);
		}
		return op_r16(typ,rIY,val,false)+offset;
	}

	switch (typ)
	{
	case opWR: // write to 16 bit reg
		{
			switch (reg)
			{
			case rBC: regB=hi; regC=lo; break;
			case rDE: regD=hi; regE=lo; break;
			case rHL: regH=hi; regL=lo; break;
			case rSP: sp=val; break;
			case rPC: pc=val; break;
			case rIX: idxregs[0]=hi; idxregs[1]=lo; break;
			case rIY: idxregs[2]=hi; idxregs[3]=lo; break;
			case rAF: regA=hi; regF=lo; break;
			}
		}
		break;
	case opRD: // read from 16 bit reg
		{
			switch (reg)
			{
			case rBC: ret=ushort(regB)*256+regC; break;
			case rDE: ret=ushort(regD)*256+regE; break;
			case rHL: ret=ushort(regH)*256+regL; break;
			case rSP: ret=sp; break;
			case rPC: ret=pc; break;
			case rIX: ret=ushort(idxregs[0])*256+idxregs[1]; break;
			case rIY: ret=ushort(idxregs[2])*256+idxregs[3]; break;
			case rAF: ret=ushort(regA)*256+regF; break;
			}
		}
		break;
	case 2: // push register or value onto stack
		{
			ushort tmp;
			sp-=2;
			switch (reg)
			{
			case -1:  tmp=val; break;
			case rSP: tmp=op_r16(1,rAF,0,false); break;
			default:  tmp=op_r16(1,reg,0,false); break;
			}
			memwrite16(sp,tmp);
		}
		break;
	case 3: // pop register or value off stack
		{
			ushort tmp=memread16(sp,0);
			sp+=2;
			switch (reg)
			{
			case -1:  ret=tmp; break;
			case rSP: op_r16(0,rAF,tmp,false); break;
			default:  op_r16(0,reg,tmp,false); break;
			}
		}
		break;
	case 4: // add register to hl with carry in val
		{
			ushort tmpHL=op_r16(1,rHL,0,false);
			ushort tmpDD=op_r16(1,reg,0,false);

			regF &= ~(Nmask|Cmask); // zero N and C

			// we have a carry (overflow) if HL+DD+C>65535
			if (val)
			{
				if (65535-tmpHL<=tmpDD) regF|=Cmask;
			}
			else
			{
				if (65535-tmpHL<tmpDD)  regF|=Cmask;
			}
			ushort newHL=tmpHL+tmpDD+val;
			op_r16(0,rHL,newHL,false);

			/*if (65535-op_r16(1,rHL,0,false)-val > op_r16(1,reg,0,false) || (op_r16(1,reg,0,false)==65535 && val))
			{
				regF |= Cmask; // set C
			}
			// write to rHL the sum of rHL and reg
			op_r16(0,rHL,op_r16(1,rHL,0,false)+op_r16(1,reg,0,false),false); */
		}
		break;
	case 5: // subtract register from hl with carry in val
		{
			ushort rg=op_r16(1,reg,0,false);
			ushort hl=op_r16(1,rHL,0,false);

			regF &= ~(Vmask|Cmask); // zero N and C
			regF |= Nmask;          // set N

			if (rg+val>hl || (rg==65535 && val))
			{
				regF |= Cmask; // set C
			}
			// write to rHL the sum of rHL and reg
			op_r16(0,rHL,hl-rg-val,false);
		}
		break;
	}
	return ret;
}

int z80Emu::loadFile(char *fnam)
{
	int ret=1;
	FILE *fp;
	fopen_s(&fp,fnam, "rb");
	if (fp)
	{
		warm_reset();
		uchar c;
		int i = 0;
		while (ret)
		{
			c= (uchar)fgetc(fp);
			if (feof(fp))
				break;

			if (i<MAXRAM)
			{
				ram[i++]=c;
			}
			else
			{
				ret=0;
			}
		}
		fclose(fp);
	}
	else
	{
		ret=0;
	}
	return ret;
}

int z80Emu::loadFileAt(char *fnam,int addr)
{
	int ret=1;
	FILE *fp;
	fopen_s(&fp, fnam, "rb");
	if (fp)
	{
		warm_reset();
		uchar c;
		int i=addr;
		while (ret)
		{
			c= (uchar)fgetc(fp);
			if (feof(fp))
				break;

			if (i<MAXRAM)
			{
				ram[i++]=c;
			}
			else
			{
				ret=0;
			}
		}
		fclose(fp);
	}
	else
	{
		ret=0;
	}
	return ret;
}

void z80Emu::cold_reset()
{
	for (int i=0; i<MAXRAM; ram[i++]=255);
	warm_reset();
}

void z80Emu::warm_reset()
{
	int i=0;
	for (i=0; i<4; idxregs[i++]=0);
	for (i=0; i<8; regs[i++]=0);
	pc=0;
	sp=MAXRAM; // perhaps should init to zero and let software(ROM?) do it
	index=0;
}

void z80Emu::memdump(char *t,int t_len,int nLines)
{
	// addr<MAXRAM
	// 0000: 00 00 00 00 00 00 00 00 \n =32 bytes per line
	int addr=0;
	*t*=0;
	for (int i=0; i<nLines; i++)
	{
		sprintf_s(t + strlen(t), t_len - strlen(t), "%04x: ", addr);
		for (int j=0; j<8; j++)
		{
			uchar byte = addr < MAXRAM ? ram[addr] : 0;
			addr++;
			sprintf_s(t + strlen(t), t_len - strlen(t), "%02x ", byte);
		}
		strcat_s(t, t_len , "\n");
	}
}

void z80Emu::report(char *t, int t_len)
{
//uchar regs[8]; // a f b c d e h l

	sprintf_s(t, t_len,
		"a=%02x f=%02x b=%02x c=%02x\nd=%02x e=%02x h=%02x l=%02x\n",
		regs[0],regs[1],regs[2],regs[3],regs[4],regs[5],regs[6],regs[7]);

	sprintf_s(t + strlen(t), t_len - strlen(t),
		"S=%c Z=%c H=%c P=%c N=%c C=%c\n",
		(regs[1]&128)?'1':'0',
		(regs[1]& 64)?'1':'0',
		(regs[1]& 16)?'1':'0',
		(regs[1]&  4)?'1':'0',
		(regs[1]&  2)?'1':'0',
		(regs[1]&  1)?'1':'0');

	sprintf_s(t + strlen(t), t_len - strlen(t),
		"PC=%04x SP=%04x\nIX=%04x IY=%04x\n",pc,sp,
		idxregs[0]*256+idxregs[1], idxregs[2]*256+idxregs[3]);

/*
| S     | S     |Sign flag (Bit 7)                            |
| Z     |  Z    |Zero flag (Bit 6)                            |
| HC    |   H   |Half Carry flag (Bit 4)                      |
| P/V   |    P  |Parity/Overflow flag (Bit 2, V=overflow)     |
| N     |     N |Add/Subtract flag (Bit 1)                    |
| CY    |      C|Carry flag (Bit 0)                           |
+*/
}

void z80Emu::disass3(char* instr, int i_len, int typ, char* mask, uchar* idx, uchar op)
{
	ushort addr;
	uchar val1;
	ushort bits012 = op & 7;
	ushort bits345 = (op / 8) & 7;
	ushort bits45 = bits345 / 2;
	ushort bits34 = bits345 & 3;
	ushort bits456 = (op / 16) & 7;
	static char* regnames[8] = { "b","c","d","e","h","l","(hl)","a" };
	static char* ioregs[8] = { "b","c","d","e","h","l","??","a" };
	static char* dregs[4] = { "bc","de","hl","sp" };
	static char* stkregs[4] = { "bc","de","hl","af" };
	static char* flags[8] = { "nz","z","nc","c","po","pe","p","m" };

	switch (typ)
	{
	case 0: // mask contains decoded instruction
		strcpy_s(instr, i_len, mask);
		break;
	case 1: // register in op bits 3-5; index reg in idx; 8 bit offset in pc+2
		sprintf_s(instr, i_len, mask, regnames[bits345], idx, memread(pc + 2, 0));
		break;
	case 2: // index reg in idx; 8 bit offset in pc+2; register in op bits 0-2
		sprintf_s(instr, i_len, mask, idx, memread(pc + 2, 0), regnames[bits012]);
		break;
	case 3: // index reg in idx
		sprintf_s(instr, i_len, mask, idx);
		break;
	case 4: // index reg in idx, 16 bit address in pc+2 pc+3
		addr = memread16(pc + 2, 0);
		sprintf_s(instr, i_len, mask, idx, addr);
		break;
	case 5: // 16 bit address in pc+2,pc+3; double reg in op bits 4-5
		addr = memread16(pc + 2, 0);
		sprintf_s(instr, i_len, mask, addr, dregs[bits45]);
		break;
	case 6: // 16 bit address in pc+2 pc+3, index reg in idx
		addr = memread16(pc + 2, 0);
		sprintf_s(instr, i_len, mask, addr, idx);
		break;
	case 7: // register in op bits 4-6 except (hl)
		sprintf_s(instr, i_len, mask, ioregs[bits456]);
		break;
	case 8: // index register in idx; offset in pc+2
		sprintf_s(instr, i_len, mask, idx, memread(pc + 2, 0));
		break;
	case 9: // index register in idx; double reg in op bits 4-5 but replace hl with idx
		sprintf_s(instr, i_len, mask, idx, (bits45 == 2) ? (char*)idx : dregs[bits45]);
		break;
	case 10: // bit number in op bits 3-5; index in idx; offset in pc+2
		sprintf_s(instr, i_len, mask, '0' + bits345, idx, memread(pc + 2, 0));
		break;
	case 11: // 16 bit address in pc+1,pc+2
		addr = memread16(pc + 1, 0);
		sprintf_s(instr, i_len, mask, addr);
		break;
	case 12: // 8 bit value from pc+1
		sprintf_s(instr, i_len, mask, memread(pc + 1, 0));
		break;
	case 13: // index register in idx; offset in pc+2; 8 bit value in pc+3
		sprintf_s(instr, i_len, mask, idx, memread(pc + 2, 0), memread(pc + 3, 0));
		break;
	case 14: // register in op bits 0-2
		sprintf_s(instr, i_len, mask, regnames[bits012]);
		break;
	case 15: // jump type in op bits 4-5, 8 bit offset in pc+1 displayed as 16 bit pc+2+signed offset
		val1 = memread(pc + 1, 0);
		addr = pc + 2 + val1;
		if (val1 > 127) addr -= 256;
		sprintf_s(instr, i_len, mask, flags[bits34], addr);
		break;
	case 16: // 8 bit offset in pc+1 displayed as 16 bit pc+2+signed offset
		val1 = memread(pc + 1, 0);
		addr = pc + 2 + val1;
		if (val1 > 127) addr -= 256;
		sprintf_s(instr, i_len, mask, addr);
		break;
	case 17: // double reg in op bits 4-5, 16 bit val in pc+1,pc+2
		addr = memread16(pc + 1, 0);
		sprintf_s(instr, i_len, mask, dregs[bits45], addr);
		break;
	case 18: // double reg in op bits 4-5
		sprintf_s(instr, i_len, mask, dregs[bits45]);
		break;
	case 20: // stkregs in op bits 4-5
		sprintf_s(instr, i_len, mask, stkregs[bits45]);
		break;
	case 21: // register in op bits 3-5, 8 bit balue in pc+1
		val1 = memread(pc + 1, 0);
		sprintf_s(instr, i_len, mask, regnames[bits345], val1);
		break;
	case 22: // jump flags in op bits 3-5, 16 bit val in pc+1,pc+2
		addr = memread16(pc + 1, 0);
		sprintf_s(instr, i_len, mask, flags[bits345], addr);
		break;
	case 23: // address in op
		sprintf_s(instr, i_len, mask, op);
		break;
	case 24: // register in op bits 3-5
		sprintf_s(instr, i_len, mask, regnames[bits345]);
		break;
	case 25: // registers in op bits 3-5 and 0-2
		sprintf_s(instr, i_len, mask, regnames[bits345], regnames[bits012]);
		break;
	case 26: // bit number in op bits 3-5; register in op bits 0-2
		// else if ((byte & 0xc0)==0x40) disass2(t,16,&byte,"bit %c,%s"); // bit b,r: 01bbbrrr
		sprintf_s(instr, i_len, mask, '0' + bits345, regnames[bits012]);
		break;
	}
}

void z80Emu::disass_ed(char *t, int t_len)
{
	uchar byte=memread(pc+1,0);

	switch (byte)
	{
	case 0x44: disass3(t,t_len,0,"neg"    ,0,0); break;
	case 0x45: disass3(t,t_len,0,"reti"   ,0,0); break;
	case 0x4d: disass3(t,t_len,0,"retn"   ,0,0); break;
	case 0x46: disass3(t,t_len,0,"im 0"   ,0,0); break;
	case 0x56: disass3(t,t_len,0,"im 1"   ,0,0); break;
	case 0x5e: disass3(t,t_len,0,"im 2"   ,0,0); break;
	case 0xa0: disass3(t,t_len,0,"ldi"    ,0,0); break;
	case 0xa8: disass3(t,t_len,0,"ldd"    ,0,0); break;
	case 0xb0: disass3(t,t_len,0,"ldir"   ,0,0); break;
	case 0xb8: disass3(t,t_len,0,"lddr"   ,0,0); break;
	case 0xa3: disass3(t,t_len,0,"outi"   ,0,0); break;
	case 0xab: disass3(t,t_len,0,"outd"   ,0,0); break;
	case 0xb3: disass3(t,t_len,0,"otir"   ,0,0); break;
	case 0xbb: disass3(t,t_len,0,"otdr"   ,0,0); break;
	case 0xa1: disass3(t,t_len,0,"cpi"    ,0,0); break;
	case 0xa9: disass3(t,t_len,0,"cpd"    ,0,0); break;
	case 0xb1: disass3(t,t_len,0,"cpir"   ,0,0); break;
	case 0xb9: disass3(t,t_len,0,"cpdr"   ,0,0); break;
	case 0xa2: disass3(t,t_len,0,"ini"    ,0,0); break;
	case 0xaa: disass3(t,t_len,0,"ind"    ,0,0); break;
	case 0xb2: disass3(t,t_len,0,"inir"   ,0,0); break;
	case 0xba: disass3(t,t_len,0,"indr"   ,0,0); break;
	case 0x47: disass3(t,t_len,0,"ld i,a" ,0,0); break;
	case 0x4f: disass3(t,t_len,0,"ld r,a" ,0,0); break;
	case 0x57: disass3(t,t_len,0,"ld a,i" ,0,0); break;
	case 0x5f: disass3(t,t_len,0,"ld a,r" ,0,0); break;

	default:
		if(0){}

		else if ((byte & 0xcf)==0x42) disass3(t,t_len,18,"sbc hl,%s",0,byte);        // 01rr0010 sbc hl,rr 
		else if ((byte & 0xcf)==0x4a) disass3(t,t_len,18,"adc hl,%s",0,byte);        // 01rr1010 adc hl,rr 

		else if ((byte & 0xcf)==0x43) disass3(t,t_len, 5, "ld (%04xh),%s", 0, byte); // 01rr0011 ld (NN),rr
		else if ((byte & 0xc7)==0x41) disass3(t,t_len, 7, "out (c),%s",    0, byte); // 01rrr001 out (c),r except (hl)
		else if ((byte & 0xc7)==0x40) disass3(t,t_len, 7, "in %s,(c)",     0, byte); // 01rrr001 int r,(c) except (hl)

		// ?
		else disass3(t, t_len,0,"unknown",0,0); break;
	}
}

void z80Emu::disass_cb(char *t, int t_len)
{
	// All CB instructions are templated, so no individual switch statements possible.
	uchar byte=memread(pc+1,0);

	if(0){}
	else if ((byte & 0xf8)==0x00) disass3(t,t_len,14,"rlc %s" ,0,byte); // rlc r: 00000rrr
	else if ((byte & 0xf8)==0x08) disass3(t,t_len,14,"rrc %s" ,0,byte); // rrc r: 00001rrr
	else if ((byte & 0xf8)==0x10) disass3(t,t_len,14,"rl %s"  ,0,byte); // rl r: 00010rrr
	else if ((byte & 0xf8)==0x18) disass3(t,t_len,14,"rl %s"  ,0,byte); // rr r: 00011rrr
	else if ((byte & 0xf8)==0x20) disass3(t,t_len,14,"sla %s" ,0,byte); // sla r: 00100rrr
	else if ((byte & 0xf8)==0x28) disass3(t,t_len,14,"sra %s" ,0,byte); // sra r: 00101rrr
	else if ((byte & 0xf8)==0x30) disass3(t,t_len,14,"sll %s" ,0,byte); // sll r: 00110rrr
	else if ((byte & 0xf8)==0x38) disass3(t,t_len,14,"srl %s" ,0,byte); // srl r: 00111rrr
	else if ((byte & 0xc0)==0x40) disass3(t,t_len,26,"bit %c,%s" ,0,byte); // bit b,r: 01bbbrrr
	else if ((byte & 0xc0)==0x80) disass3(t,t_len,26,"res %c,%s" ,0,byte); // res b,r: 10bbbrrr
	else if ((byte & 0xc0)==0xc0) disass3(t,t_len,26,"set %c,%s" ,0,byte); // set b,r: 11bbbrrr
	else disass3(t, t_len,0,"unknown",0,0);
}

void z80Emu::disass_idx_cb(char *t, int t_len)
{
	uchar ixy[3];
	ixy[0]='i';
	ixy[1]='?';
	ixy[2]=0;
	uchar byte=memread(pc+3,0);
	switch (byte)
	{
	case 0x06: disass3(t,t_len,8,"rlc (%s+%d)",ixy,0); break;// dd cb nn 06 rlc (ixy+n)
	case 0x0e: disass3(t,t_len,8,"rrc (%s+%d)",ixy,0); break;// dd cb nn 0e rrc (ixy+n)
	case 0x16: disass3(t,t_len,8,"rl (%s+%d)" ,ixy,0); break;// dd cb nn 16 rl (ixy+n)
	case 0x1e: disass3(t,t_len,8,"rr (%s+%d)" ,ixy,0); break;// dd cb nn 1e rr (ixy+n)
	case 0x26: disass3(t,t_len,8,"sla (%s+%d)",ixy,0); break;// dd cb nn 26 sla (ixy+n)
	case 0x2e: disass3(t,t_len,8,"sra (%s+%d)",ixy,0); break;// dd cb nn 2e sra (ixy+n)
	case 0x30: disass3(t,t_len,8,"sll (%s+%d)",ixy,0); break;// dd cb nn 30 sll (ixy+n)
	case 0x3e: disass3(t,t_len,8,"srl (%s+%d)",ixy,0); break;// dd cb nn 3e srl (ixy+n)
	default:
		if (0){}
		else if ((byte & 0xc7)==0x46) disass3(t,t_len,10,"bit %c,(%s+%d)",ixy,byte); // dd cb nn 46+b*8 01bbb110 bit b,(ixy+n)
		else if ((byte & 0xc7)==0x86) disass3(t,t_len,10,"res %c,(%s+%d)",ixy,byte); // dd cb nn 86+b*8 10bbb110 res b,(ixy+n)
		else if ((byte & 0xc7)==0xc6) disass3(t,t_len,10,"set %c,(%s+%d)",ixy,byte); // dd cb nn c6+b*8 11bbb110 set b,(ixy+n)
		else disass3(t, t_len,0,"unknown",0,0);
	}
}

void z80Emu::disass_idx(char *t, int t_len, uchar idx)
{
	uchar ixy[3];
	ixy[0]='i';
	ixy[1]='?';
	ixy[2]=0;
	if (idx==1 || idx==2)
		ixy[1]=idx-1+'x';
	uchar byte=memread(pc+1,0);
	switch (byte)
	{
	case 0x36: disass3(t, t_len,13,"ld (%s+%d),%02xh",ixy,0); break;

	case 0x21: disass3(t,t_len,4,"ld %s,%04xh"   ,ixy,0); break;
	case 0x22: disass3(t,t_len,6,"ld (%04xh),%s"  ,ixy,0); break;
	case 0x23: disass3(t,t_len,3,"inc %s"         ,ixy,0); break;
	case 0x2a: disass3(t,t_len,4,"ld %s,(%04xh)"  ,ixy,0); break;
	case 0x2b: disass3(t,t_len,3,"dec %s"         ,ixy,0); break;
	case 0x34: disass3(t,t_len,8,"inc (%s+%d)"    ,ixy,0); break;
	case 0x35: disass3(t,t_len,8,"dec (%s+%d)"    ,ixy,0); break;
	case 0x86: disass3(t,t_len,8,"add a,(%s+%d)"  ,ixy,0); break;
	case 0x8e: disass3(t,t_len,8,"adc a,(%s+%d)"  ,ixy,0); break;
	case 0x96: disass3(t,t_len,8,"sub (%s+%d)"    ,ixy,0); break;
	case 0x9e: disass3(t,t_len,8,"sbc a,(%s+%d)"  ,ixy,0); break;
	case 0xa6: disass3(t,t_len,8,"and (%s+%d)"    ,ixy,0); break;
	case 0xb6: disass3(t,t_len,8,"or (%s+%d)"     ,ixy,0); break;
	case 0xae: disass3(t,t_len,8,"xor (%s+%d)"    ,ixy,0); break;
	case 0xbe: disass3(t,t_len,8,"cp (%s+%d)"     ,ixy,0); break;
	case 0xcb: disass_idx_cb(t, t_len                   ); break;
	case 0xe1: disass3(t,t_len,3,"pop %s"         ,ixy,0); break;
	case 0xe3: disass3(t,t_len,3,"ex (sp),%s"     ,ixy,0); break;
	case 0xe5: disass3(t,t_len,3,"push %s"        ,ixy,0); break;
	case 0xe9: disass3(t,t_len,3,"jp (%s)"        ,ixy,0); break;
	case 0xf9: disass3(t,t_len,3,"ld sp,%s"       ,ixy,0); break;
	default:
		{
			if(0){} // for a regular structure below

			else if ((byte & 0xf8)==0x70) disass3(t,t_len,2,"ld (%s+%d),%s",ixy,byte); // 70+r   01110rrr ld (ixy+n),r
			else if ((byte & 0xc7)==0x46) disass3(t,t_len,1,"ld %s,(%s+%d)",ixy,byte); // 46+r*8 01rrr110 ld r,(ixy+n)
			else if ((byte & 0xcf)==0x09) disass3(t,t_len,9,"add %s,%s",ixy,byte); // 09+rr*16 00rr1001 add ixy,{bc,de,ixy,sp}

			else disass3(t, t_len,0,"unknown",0,0);
		}
		break;
	}
}

void z80Emu::disass(char *t, int t_len,ushort *pcptr)
{
	uchar byte;
	*pcptr=pc;
	uchar mem0=memread(pc+0,0);
	uchar mem1=memread(pc+1,0);
	uchar mem2=memread(pc+2,0);
	uchar mem3=memread(pc+3,0);
	sprintf_s(t,t_len,"%02x %02x %02x %02x:",mem0,mem1,mem2,mem3);
	t_len -= strlen(t);
	t+=strlen(t);

	byte=memread(pc,0);
	switch (byte)
	{
	case 0xcb: disass_cb(t,t_len);     break;
	case 0xdd: disass_idx(t, t_len,1); break;
	case 0xed: disass_ed(t, t_len);    break;
	case 0xfd: disass_idx(t, t_len,2); break;

	case 0x00: disass3(t,t_len,0,"nop"        ,0,0); break;
	case 0x02: disass3(t,t_len,0,"ld (bc),a"  ,0,0); break;
	case 0x08: disass3(t,t_len,0,"ex af,af'"  ,0,0); break;
	case 0x07: disass3(t,t_len,0,"rlca"       ,0,0); break;
	case 0x0a: disass3(t,t_len,0,"ld a,(bc)"  ,0,0); break;
	case 0x0f: disass3(t,t_len,0,"rrca"       ,0,0); break;
	case 0x12: disass3(t,t_len,0,"ld (de),a"  ,0,0); break;
	case 0x17: disass3(t,t_len,0,"rla"        ,0,0); break;
	case 0x1a: disass3(t,t_len,0,"ld a,(de)"  ,0,0); break;
	case 0x1f: disass3(t,t_len,0,"rra"        ,0,0); break;
	case 0x23: disass3(t,t_len,0,"inc hl"     ,0,0); break;
	case 0x27: disass3(t,t_len,0,"daa"        ,0,0); break;
	case 0x2f: disass3(t,t_len,0,"cpl"        ,0,0); break;
	case 0x37: disass3(t,t_len,0,"scf"        ,0,0); break;
	case 0x3f: disass3(t,t_len,0,"ccf"        ,0,0); break;
	case 0x67: disass3(t,t_len,0,"rrd"        ,0,0); break;
	case 0x6f: disass3(t,t_len,0,"rld"        ,0,0); break;
	case 0x76: disass3(t,t_len,0,"halt"       ,0,0); break;
	case 0xc9: disass3(t,t_len,0,"ret"        ,0,0); break;
	case 0xd9: disass3(t,t_len,0,"exx"        ,0,0); break;
	case 0xe3: disass3(t,t_len,0,"ex (sp),hl" ,0,0); break;
	case 0xe9: disass3(t,t_len,0,"jp (hl)"    ,0,0); break;
	case 0xeb: disass3(t,t_len,0,"ex de,hl"   ,0,0); break;
	case 0xf3: disass3(t,t_len,0,"di"         ,0,0); break;
	case 0xf9: disass3(t,t_len,0,"ld sp,hl"   ,0,0); break;
	case 0xfb: disass3(t,t_len,0,"ei"         ,0,0); break;

	case 0xc6: disass3(t,t_len,12,"add a,%02xh"  ,0,0); break;
	case 0xce: disass3(t,t_len,12,"adc a,%02xh"  ,0,0); break;
	case 0xd3: disass3(t,t_len,12,"out(%d),a"    ,0,0); break;
	case 0xd6: disass3(t,t_len,12,"sub %02xh"    ,0,0); break;
	case 0xdb: disass3(t,t_len,12,"in a,(%02xh)" ,0,0); break;
	case 0xde: disass3(t,t_len,12,"sbc a,%02xh"  ,0,0); break;
	case 0x10: disass3(t,t_len,12,"djnz %02xh"   ,0,0); break;
	case 0xe6: disass3(t,t_len,12,"and %02xh"    ,0,0); break;
	case 0xee: disass3(t,t_len,12,"xor %02xh"    ,0,0); break;
	case 0xf6: disass3(t,t_len,12,"or %02xh"     ,0,0); break;
	case 0xfe: disass3(t,t_len,12,"cp %02xh"     ,0,0); break;

	case 0x21: disass3(t,t_len,11,"ld hl,%04xh" ,0,0); break;
	case 0x22: disass3(t,t_len,11,"ld (%04xh),hl",0,0); break;
	case 0x2a: disass3(t,t_len,11,"ld hl,(%04xh)",0,0); break;
	case 0x32: disass3(t,t_len,11,"ld (%04xh),a" ,0,0); break;
	case 0x3a: disass3(t,t_len,11,"ld a,(%04xh)" ,0,0); break;
	case 0xc3: disass3(t,t_len,11,"jp %04xh"     ,0,0); break;
	case 0xcd: disass3(t,t_len,11,"call %04xh"   ,0,0); break;

	case 0x18: disass3(t, t_len,16,"jr %04xh"     ,0,0); break;
	default:
		{
			if(0){} // for a regular structure below

			// f8=11111000
			else if ((byte & 0xf8)==0x80) disass3(t,t_len,14,"add a,%s" ,0,byte); // 80+r 10000rrr add a,r
			else if ((byte & 0xf8)==0x88) disass3(t,t_len,14,"adc a,%s" ,0,byte); // 88+r 10001rrr adc a,r
			else if ((byte & 0xf8)==0x90) disass3(t,t_len,14,"sub a,%s" ,0,byte); // 90+r 10010rrr sub r
			else if ((byte & 0xf8)==0x98) disass3(t,t_len,14,"sbc a,%s" ,0,byte); // 98+r 10011rrr sub r
			else if ((byte & 0xf8)==0xa0) disass3(t,t_len,14,"and %s"   ,0,byte); // a0+r 10100rrr and r
			else if ((byte & 0xf8)==0xa8) disass3(t,t_len,14,"xor %s"   ,0,byte); // a0+r 10101rrr xor r
			else if ((byte & 0xf8)==0xb0) disass3(t,t_len,14,"or %s"    ,0,byte); // b0+r 10110rrr or r
			else if ((byte & 0xf8)==0xb8) disass3(t,t_len,14,"cp %s"    ,0,byte); // b8+r 10111rrr cp r

			// e7=11100111
			else if ((byte & 0xe7)==0x20) disass3(t, t_len,15,"jr %s,%04xh",0,byte); // 20+flag*8 001ff000 jr flag,N

			// cf=11001111
			else if ((byte & 0xcf)==0x01) disass3(t,t_len,17,"ld %s,%04xh"   ,0,byte); // 08+rr*16 00rr1000 ld rr,NN
			else if ((byte & 0xcf)==0x03) disass3(t,t_len,18,"inc %s"        ,0,byte);
			else if ((byte & 0xcf)==0x09) disass3(t,t_len,18,"add hl,%s"     ,0,byte);
			else if ((byte & 0xcf)==0x40) disass3(t,t_len,17,"ld %s,(%04xh)" ,0,byte); // 4b+rr*16 01rr0000 ld rr,(NN)
			else if ((byte & 0xcf)==0x0b) disass3(t,t_len,20,"dec %s"        ,0,byte); // dec rr 00rr1011
			else if ((byte & 0xcf)==0xc1) disass3(t,t_len,20,"pop %s"        ,0,byte); // pop rr 11rr0001
			else if ((byte & 0xcf)==0xc5) disass3(t,t_len,20,"push %s"       ,0,byte); // push rr 11rr0101

			// c7=11000111
			else if ((byte & 0xc7)==0x06) disass3(t,t_len,21,"ld %s,%02xh"  ,0,byte); // 06+r*8    00rrr110 ld r,N
			else if ((byte & 0xc7)==0xc4) disass3(t,t_len,22,"call %s,%04xh" ,0,byte); // c4+flag*8 11fff100 call flag,NN
			else if ((byte & 0xc7)==0xc0) disass3(t,t_len,22,"ret %s"        ,0,byte); // c0+flag*8 11fff000 ret flag
			else if ((byte & 0xc7)==0xc2) disass3(t,t_len,22,"jp %s,%04xh"  ,0,byte); // c2+flag*8 11rrr010 jp flag,NN
			else if ((byte & 0xc7)==0xc7) disass3(t,t_len,23,"rst %02x"      ,0,byte&0x38); // c7+a 11aaa111 rst a

			// c5=11000101
			else if ((byte & 0xc5)==0x05) disass3(t,t_len,24,"dec %s",0,byte); // c5+reg*8 11rrr101 dec r
			else if ((byte & 0xc5)==0x04) disass3(t,t_len,24,"inc %s",0,byte); // c4+reg*8 11rrr100 dec r

			// c0=11000000. Halt has already been handled by the switch.
			else if ((byte & 0xc0)==0x40) disass3(t, t_len,25,"ld %s,%s",0,byte); // 40+r*8+r 01rrrsss ld r,s

			// ?
			else disass3(t, t_len,0,"unknown",0,0);
		}
		break;
	}
}


/*
I what (or values)

0 not implemented
1 fully implemented by emulator
2 implemented by disassembler


+-+-------------+----+---+------+------------+---------------------+----------------------+
|I|Mnemonic     |Clck|Siz|SZHPNC|  OP-Code   |    Description      |        Notes         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|ADC A,r      | 4  | 1 |***V0*|88+rb       |Add with Carry       |A=A+s+CY              |
|3|ADC A,N      | 7  | 2 |      |CE XX       |                     |                      |
|3|ADC A,(HL)   | 7  | 1 |      |8E          |                     |                      |
|3|ADC A,(IX+N) | 19 | 3 |      |DD 8E XX    |                     |                      |
|3|ADC A,(IY+N) | 19 | 3 |      |FD 8E XX    |                     |                      |
|3|ADC HL,BC    | 15 | 2 |**?V0*|ED 4A       |Add with Carry       |HL=HL+ss+CY           |
|3|ADC HL,DE    | 15 | 2 |      |ED 5A       |                     |                      |
|3|ADC HL,HL    | 15 | 2 |      |ED 6A       |                     |                      |
|3|ADC HL,SP    | 15 | 2 |      |ED 7A       |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|I|Mnemonic     |Clck|Siz|SZHPNC|  OP-Code   |    Description      |        Notes         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|ADD A,r      | 4  | 1 |***V0*|80+rb       |Add (8-bit)          |A=A+s                 |
|3|ADD A,N      | 7  | 2 |      |C6 XX       |                     |                      |
|3|ADD A,(HL)   | 7  | 1 |      |86          |                     |                      |
|3|ADD A,(IX+N) | 19 | 3 |      |DD 86 XX    |                     |                      |
|3|ADD A,(IY+N) | 19 | 3 |      |FD 86 XX    |                     |                      |
|3|ADD HL,BC    | 11 | 1 |--?-0*|09          |Add (16-bit)         |HL=HL+ss              |
|3|ADD HL,DE    | 11 | 1 |      |19          |                     |                      |
|3|ADD HL,HL    | 11 | 1 |      |29          |                     |                      |
|3|ADD HL,SP    | 11 | 1 |      |39          |                     |                      |
|3|ADD IX,BC    | 15 | 2 |--?-0*|DD 09       |Add (IX register)    |IX=IX+pp              |
|3|ADD IX,DE    | 15 | 2 |      |DD 19       |                     |                      |
|3|ADD IX,IX    | 15 | 2 |      |DD 29       |                     |                      |
|3|ADD IX,SP    | 15 | 2 |      |DD 39       |                     |                      |
|3|ADD IY,BC    | 15 | 2 |--?-0*|FD 09       |Add (IY register)    |IY=IY+rr              |
|3|ADD IY,DE    | 15 | 2 |      |FD 19       |                     |                      |
|3|ADD IY,IY    | 15 | 2 |      |FD 29       |                     |                      |
|3|ADD IY,SP    | 15 | 2 |      |FD 39       |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|AND r        | 4  | 1 |***P00|A0+rb       |Logical AND          |A=A&s                 |
|3|AND N        | 7  | 2 |      |E6 XX       |                     |                      |
|3|AND (HL)     | 7  | 1 |      |A6          |                     |                      |
|3|AND (IX+N)   | 19 | 3 |      |DD A6 XX    |                     |                      |
|3|AND (IY+N)   | 19 | 3 |      |FD A6 XX    |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|BIT b,r      | 8  | 2 |?*1?0-|CB 40+8*b+rb|Test Bit             |m&{2^b}               |
|3|BIT b,(HL)   | 12 | 2 |      |CB 46+8*b   |                     |                      |
|3|BIT b,(IX+N) | 20 | 4 |      |DD CB XX 46+8*b                   |                      |
|3|BIT b,(IY+N) | 20 | 4 |      |FD CB XX 46+8*b                   |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|CALL NN      | 17 | 3 |------|CD XX XX    |Unconditional Call   |-(SP)=PC,PC=nn        |
|3|CALL C,NN    |17/1| 3 |------|DC XX XX    |Conditional Call     |If Carry = 1          |
|3|CALL NC,NN   |17/1| 3 |      |D4 XX XX    |                     |If carry = 0          |
|3|CALL M,NN    |17/1| 3 |      |FC XX XX    |                     |If Sign = 1 (negative)|
|3|CALL P,NN    |17/1| 3 |      |F4 XX XX    |                     |If Sign = 0 (positive)|
|3|CALL Z,NN    |17/1| 3 |      |CC XX XX    |                     |If Zero = 1 (ans.=0)  |
|3|CALL NZ,NN   |17/1| 3 |      |C4 XX XX    |                     |If Zero = 0 (non-zero)|
|3|CALL PE,NN   |17/1| 3 |      |EC XX XX    |                     |If Parity = 1 (even)  |
|3|CALL PO,NN   |17/1| 3 |      |E4 XX XX    |                     |If Parity = 0 (odd)   |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|CCF          | 4  | 1 |--?-0*|3F          |Complement Carry Flag|CY=~CY                |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|CP r         | 4  | 1 |***V1*|B8+rb       |Compare              |Compare A-s           |
|3|CP N         | 7  | 2 |      |FE XX       |                     |                      |
|3|CP (HL)      | 7  | 1 |      |BE          |                     |                      |
|3|CP (IX+N)    | 19 | 3 |      |DD BE XX    |                     |                      |
|3|CP (IY+N)    | 19 | 3 |      |FD BE XX    |                     |                      |
|3|CPD          | 16 | 2 |****1-|ED A9       |Compare and Decrement|A-(HL),HL=HL-1,BC=BC-1|
|3|CPDR         |21/1| 2 |****1-|ED B9       |Compare, Dec., Repeat|CPD till A=(HL)or BC=0|
|3|CPI          | 16 | 2 |****1-|ED A1       |Compare and Increment|A-(HL),HL=HL+1,BC=BC-1|
|3|CPIR         |21/1| 2 |****1-|ED B1       |Compare, Inc., Repeat|CPI till A=(HL)or BC=0|
+-+-------------+----+---+------+------------+---------------------+----------------------+
|I|Mnemonic     |Clck|Siz|SZHPNC|  OP-Code   |    Description      |        Notes         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|CPL          | 4  | 1 |--1-1-|2F          |Complement           |A=~A                  |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|2|DAA          | 4  | 1 |***P-*|27          |Decimal Adjust Acc.  |A=BCD format  (dec.)  |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|DEC A        | 4  | 1 |***V1-|3D          |Decrement (8-bit)    |s=s-1                 |
|3|DEC B        | 4  | 1 |      |05          |                     |                      |
|3|DEC C        | 4  | 1 |      |0D          |                     |                      |
|3|DEC D        | 4  | 1 |      |15          |                     |                      |
|3|DEC E        | 4  | 1 |      |1D          |                     |                      |
|3|DEC H        | 4  | 1 |      |25          |                     |                      |
|3|DEC L        | 4  | 2 |      |2D          |                     |                      |
|3|DEC (HL)     | 11 | 1 |      |35          |                     |                      |
|3|DEC (IX+N)   | 23 | 3 |      |DD 35 XX    |                     |                      |
|3|DEC (IY+N)   | 23 | 3 |      |FD 35 XX    |                     |                      |
|3|DEC BC       | 6  | 1 |------|0B          |Decrement (16-bit)   |ss=ss-1               |
|3|DEC DE       | 6  | 1 |      |1B          |                     |                      |
|3|DEC HL       | 6  | 1 |      |2B          |                     |                      |
|3|DEC SP       | 6  | 1 |      |3B          |                     |                      |
|3|DEC IX       | 10 | 2 |------|DD 2B       |Decrement            |xx=xx-1               |
|3|DEC IY       | 10 | 2 |      |FD 2B       |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|I|Mnemonic     |Clck|Siz|SZHPNC|  OP-Code   |    Description      |        Notes         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|2|DI           | 4  | 1 |------|F3          |Disable Interrupts   |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|DJNZ $+2     |13/8| 1 |------|10          |Dec., Jump Non-Zero  |B=B-1 till B=0        |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|2|EI           | 4  | 1 |------|FB          |Enable Interrupts    |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|EX (SP),HL   | 19 | 1 |------|E3          |Exchange             |(SP)<->HL             |
|3|EX (SP),IX   | 23 | 2 |------|DD E3       |                     |(SP)<->xx             |
|3|EX (SP),IY   | 23 | 2 |      |FD E3       |                     |                      |
|3|EX AF,AF'    | 4  | 1 |------|08          |                     |AF<->AF'              |
|3|EX DE,HL     | 4  | 1 |------|EB          |                     |DE<->HL               |
|3|EXX          | 4  | 1 |------|D9          |Exchange             |qq<->qq'   (except AF)|
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|HALT         | 4  | 1 |------|76          |Halt                 |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|2|IM 0         | 8  | 2 |------|ED 46       |Interrupt Mode       |             (n=0,1,2)|
|2|IM 1         | 8  | 2 |      |ED 56       |                     |                      |
|2|IM 2         | 8  | 2 |      |ED 5E       |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|2|IN A,(N)     | 11 | 2 |------|DB XX       |Input                |A=(n)                 |
|2|IN (C)       | 12 | 2 |***P0-|ED 70       |Input*               |         (Unsupported)|
|2|IN A,(C)     | 12 | 2 |***P0-|ED 78       |Input                |r=(C)                 |
|2|IN B,(C)     | 12 | 2 |      |ED 40       |                     |                      |
|2|IN C,(C)     | 12 | 2 |      |ED 48       |                     |                      |
|2|IN D,(C)     | 12 | 2 |      |ED 50       |                     |                      |
|2|IN E,(C)     | 12 | 2 |      |ED 58       |                     |                      |
|2|IN H,(C)     | 12 | 2 |      |ED 60       |                     |                      |
|2|IN L,(C)     | 12 | 2 |      |ED 68       |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|INC A        | 4  | 1 |***V0-|3C          |Increment (8-bit)    |r=r+1                 |
|3|INC B        | 4  | 1 |      |04          |                     |                      |
|3|INC C        | 4  | 1 |      |0C          |                     |                      |
|3|INC D        | 4  | 1 |      |14          |                     |                      |
|3|INC E        | 4  | 1 |      |1C          |                     |                      |
|3|INC H        | 4  | 1 |      |24          |                     |                      |
|3|INC L        | 4  | 1 |      |2C          |                     |                      |
|3|INC BC       | 6  | 1 |------|03          |Increment (16-bit)   |ss=ss+1               |
|3|INC DE       | 6  | 1 |      |13          |                     |                      |
|3|INC HL       | 6  | 1 |      |23          |                     |                      |
|3|INC SP       | 6  | 1 |      |33          |                     |                      |
|3|INC IX       | 10 | 2 |------|DD 23       |Increment            |xx=xx+1               |
|3|INC IY       | 10 | 2 |      |FD 23       |                     |                      |
|3|INC (HL)     | 11 | 1 |***V0-|34          |Increment (indirect) |(HL)=(HL)+1           |
|3|INC (IX+N)   | 23 | 3 |***V0-|DD 34 XX    |Increment            |(xx+d)=(xx+d)+1       |
|3|INC (IY+N)   | 23 | 3 |      |FD 34 XX    |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|2|IND          | 16 | 2 |?*??1-|ED AA       |Input and Decrement  |(HL)=(C),HL=HL-1,B=B-1|
|2|INDR         |21/1| 2 |?1??1-|ED BA       |Input, Dec., Repeat  |IND till B=0          |
|2|INI          | 16 | 2 |?*??1-|ED A2       |Input and Increment  |(HL)=(C),HL=HL+1,B=B-1|
|2|INIR         |21/1| 2 |?1??1-|ED B2       |Input, Inc., Repeat  |INI till B=0          |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|JP $NN       | 10 | 3 |------|C3 XX XX    |Unconditional Jump   |PC=nn                 |
|3|JP (HL)      | 4  | 1 |------|E9          |Unconditional Jump   |PC=(HL)               |
|3|JP (IX)      | 8  | 2 |------|DD E9       |Unconditional Jump   |PC=(xx)               |
|3|JP (IY)      | 8  | 2 |      |FD E9       |                     |                      |
|3|JP C,$NN     |10/1| 3 |------|DA XX XX    |Conditional Jump     |If Carry = 1          |
|3|JP NC,$NN    |10/1| 3 |      |D2 XX XX    |                     |If Carry = 0          |
|3|JP M,$NN     |10/1| 3 |      |FA XX XX    |                     |If Sign = 1 (negative)|
|3|JP P,$NN     |10/1| 3 |      |F2 XX XX    |                     |If Sign = 0 (positive)|
|3|JP Z,$NN     |10/1| 3 |      |CA XX XX    |                     |If Zero = 1 (ans.= 0) |
|3|JP NZ,$NN    |10/1| 3 |      |C2 XX XX    |                     |If Zero = 0 (non-zero)|
|3|JP PE,$NN    |10/1| 3 |      |EA XX XX    |                     |If Parity = 1 (even)  |
|3|JP PO,$NN    |10/1| 3 |      |E2 XX XX    |                     |If Parity = 0 (odd)   |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|JR $N+2      | 12 | 2 |------|18 XX       |Relative Jump        |PC=PC+e               |
|3|JR C,$N+2    |12/7| 2 |------|38 XX       |Cond. Relative Jump  |If cc JR(cc=C,NC,NZ,Z)|
|3|JR NC,$N+2   |12/7| 2 |      |30 XX       |                     |                      |
|3|JR Z,$N+2    |12/7| 2 |      |28 XX       |                     |                      |
|3|JR NZ,$N+2   |12/7| 2 |      |20 XX       |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|I|Mnemonic     |Clck|Siz|SZHPNC|  OP-Code   |    Description      |        Notes         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|LD I,A       | 9  | 2 |------|ED 47       |Load*                |dst=src               |
|3|LD R,A       | 9  | 2 |      |ED 4F       |                     |                      |
|3|LD A,I       | 9  | 2 |**0*0-|ED 57       |Load*                |dst=src               |
|3|LD A,R       | 9  | 2 |      |ED 5F       |                     |                      |
|3|LD A,r       | 4  | 1 |------|78+rb       |Load (8-bit)         |dst=src               |
|3|LD A,N       | 7  | 2 |      |3E XX       |                     |                      |
|3|LD A,(BC)    | 7  | 1 |      |0A          |                     |                      |
|3|LD A,(DE)    | 7  | 1 |      |1A          |                     |                      |
|3|LD A,(HL)    | 7  | 1 |      |7E          |                     |                      |
|3|LD A,(IX+N)  | 19 | 3 |      |DD 7E XX    |                     |                      |
|3|LD A,(IY+N)  | 19 | 3 |      |FD 7E XX    |                     |                      |
|3|LD A,(NN)    | 13 | 3 |      |3A XX XX    |                     |                      |
|3|LD B,r       | 4  | 1 |      |40+rb       |                     |                      |
|3|LD B,N       | 7  | 2 |      |06 XX       |                     |                      |
|3|LD B,(HL)    | 7  | 1 |      |46          |                     |                      |
|3|LD B,(IX+N)  | 19 | 3 |      |DD 46 XX    |                     |                      |
|3|LD B,(IY+N)  | 19 | 3 |      |FD 46 XX    |                     |                      |
|3|LD C,r       | 4  | 1 |      |48+rb       |                     |                      |
|3|LD C,N       | 7  | 2 |      |0E XX       |                     |                      |
|3|LD C,(HL)    | 7  | 1 |      |4E          |                     |                      |
|3|LD C,(IX+N)  | 19 | 3 |      |DD 4E XX    |                     |                      |
|3|LD C,(IY+N)  | 19 | 3 |      |FD 4E XX    |                     |                      |
|3|LD D,r       | 4  | 1 |      |50+rb       |                     |                      |
|3|LD D,N       | 7  | 2 |      |16 XX       |                     |                      |
|3|LD D,(HL)    | 7  | 1 |      |56          |                     |                      |
|3|LD D,(IX+N)  | 19 | 3 |      |DD 56 XX    |                     |                      |
|3|LD D,(IY+N)  | 19 | 3 |      |FD 56 XX    |                     |                      |
|3|LD E,r       | 4  | 1 |      |58+rb       |                     |                      |
|3|LD E,N       | 7  | 2 |      |1E XX       |                     |                      |
|3|LD E,(HL)    | 7  | 1 |      |5E          |                     |                      |
|3|LD E,(IX+N)  | 19 | 3 |      |DD 5E XX    |                     |                      |
|3|LD E,(IY+N)  | 19 | 3 |      |FD 5E XX    |                     |                      |
|3|LD H,r       | 4  | 1 |      |60+rb       |                     |                      |
|3|LD H,N       | 7  | 2 |      |26 XX       |                     |                      |
|3|LD H,(HL)    | 7  | 1 |      |66          |                     |                      |
|3|LD H,(IX+N)  | 19 | 3 |      |DD 66 XX    |                     |                      |
|3|LD H,(IY+N)  | 19 | 3 |      |FD 66 XX    |                     |                      |
|3|LD L,r       | 4  | 1 |      |68+rb       |                     |                      |
|3|LD L,N       | 7  | 2 |      |2E XX       |                     |                      |
|3|LD L,(HL)    | 7  | 1 |      |6E          |                     |                      |
|3|LD L,(IX+N)  | 19 | 3 |      |DD 6E XX    |                     |                      |
|3|LD L,(IY+N)  | 19 | 3 |      |FD 6E XX    |                     |                      |
|3|LD BC,(NN)   | 20 | 4 |------|ED 4B XX XX |Load (16-bit)        |dst=src               |
|3|LD BC,NN     | 10 | 3 |      |01 XX XX    |                     |                      |
|3|LD DE,(NN)   | 20 | 4 |      |ED 5B XX XX |                     |                      |
|3|LD DE,NN     | 10 | 3 |      |11 XX XX    |                     |                      |
|3|LD HL,(NN)   | 20 | 3 |      |2A XX XX    |                     |                      |
|3|LD HL,NN     | 10 | 3 |      |21 XX XX    |                     |                      |
|3|LD SP,(NN)   | 20 | 4 |      |ED 7B XX XX |                     |                      |
|3|LD SP,HL     | 6  | 1 |      |F9          |                     |                      |
|3|LD SP,IX     | 10 | 2 |      |DD F9       |                     |                      |
|3|LD SP,IY     | 10 | 2 |      |FD F9       |                     |                      |
|3|LD SP,NN     | 10 | 3 |      |31 XX XX    |                     |                      |
|3|LD IX,(NN)   | 20 | 4 |      |DD 2A XX XX |                     |                      |
|3|LD IX,NN     | 14 | 4 |      |DD 21 XX XX |                     |                      |
|3|LD IY,(NN)   | 20 | 4 |      |FD 2A XX XX |                     |                      |
|3|LD IY,NN     | 14 | 4 |      |FD 21 XX XX |                     |                      |
|3|LD (HL),r    | 7  | 1 |------|70+rb       |Load (Indirect)      |dst=src               |
|3|LD (HL),N    | 10 | 2 |      |36 XX       |                     |                      |
|3|LD (BC),A    | 7  | 1 |      |02          |                     |                      |
|3|LD (DE),A    | 7  | 1 |      |12          |                     |                      |
|3|LD (NN),A    | 13 | 3 |      |32 XX XX    |                     |                      |
|3|LD (NN),BC   | 20 | 4 |      |ED 43 XX XX |                     |                      |
|3|LD (NN),DE   | 20 | 4 |      |ED 53 XX XX |                     |                      |
|3|LD (NN),HL   | 16 | 3 |      |22 XX XX    |                     |                      |
|3|LD (NN),IX   | 20 | 4 |      |DD 22 XX XX |                     |                      |
|3|LD (NN),IY   | 20 | 4 |      |FD 22 XX XX |                     |                      |
|3|LD (NN),SP   | 20 | 4 |      |ED 73 XX XX |                     |                      |
|3|LD (IX+N),r  | 19 | 3 |      |DD 70+rb XX |                     |                      |
|3|LD (IX+N),N  | 19 | 4 |      |DD 36 XX XX |                     |                      |
|3|LD (IY+N),r  | 19 | 3 |      |FD 70+rb XX |                     |                      |
|3|LD (IY+N),N  | 19 | 4 |      |FD 36 XX XX |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|LDD          | 16 | 2 |--0*0-|ED A8       |Load and Decrement   |(DE)=(HL),HL=HL-1,#   |
|3|LDDR         |21/1| 2 |--000-|ED B8       |Load, Dec., Repeat   |LDD till BC=0         |
|3|LDI          | 16 | 2 |--0*0-|ED A0       |Load and Increment   |(DE)=(HL),HL=HL+1,#   |
|3|LDIR         |21/1| 2 |--000-|ED B0       |Load, Inc., Repeat   |LDI till BC=0         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|I|Mnemonic     |Clck|Siz|SZHPNC|  OP-Code   |    Description      |        Notes         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|NEG          | 8  | 2 |***V1*|ED 44       |Negate               |A=-A                  |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|NOP          | 4  | 1 |------|00          |No Operation         |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|OR r         | 4  | 1 |***P00|B0+rb       |Logical inclusive OR |A=Avs                 |
|3|OR N         | 7  | 2 |      |F6 XX       |                     |                      |
|3|OR (HL)      | 7  | 1 |      |B6          |                     |                      |
|3|OR (IX+N)    | 19 | 3 |      |DD B6 XX    |                     |                      |
|3|OR (IY+N)    | 19 | 3 |      |FD B6 XX    |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|OUT (N),A    | 11 | 2 |------|D3 XX       |Output               |(n)=A                 |
|3|OUT (C),0    | 12 | 2 |------|ED 71       |Output*              |         (Unsupported)|
|3|OUT (C),A    | 12 | 2 |------|ED 79       |Output               |(C)=r                 |
|3|OUT (C),B    | 12 | 2 |      |ED 41       |                     |                      |
|3|OUT (C),C    | 12 | 2 |      |ED 49       |                     |                      |
|3|OUT (C),D    | 12 | 2 |      |ED 51       |                     |                      |
|3|OUT (C),E    | 12 | 2 |      |ED 59       |                     |                      |
|3|OUT (C),H    | 12 | 2 |      |ED 61       |                     |                      |
|3|OUT (C),L    | 12 | 2 |      |ED 69       |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|OUTD         | 16 | 2 |?*??1-|ED AB       |Output and Decrement |(C)=(HL),HL=HL-1,B=B-1|
|3|OTDR         |21/1| 2 |?1??1-|ED BB       |Output, Dec., Repeat |OUTD till B=0         |
|3|OUTI         | 16 | 2 |?*??1-|ED A3       |Output and Increment |(C)=(HL),HL=HL+1,B=B-1|
|3|OTIR         |21/1| 2 |?1??1-|ED B3       |Output, Inc., Repeat |OUTI till B=0         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|POP AF       | 10 | 1 |------|F1          |Pop                  |qq=(SP)+              |
|3|POP BC       | 10 | 1 |      |C1          |                     |                      |
|3|POP DE       | 10 | 1 |      |D1          |                     |                      |
|3|POP HL       | 10 | 1 |      |E1          |                     |                      |
|3|POP IX       | 14 | 2 |------|DD E1       |Pop                  |xx=(SP)+              |
|3|POP IY       | 14 | 2 |      |FD E1       |                     |                      |
|3|PUSH AF      | 11 | 1 |------|F5          |Push                 |-(SP)=qq              |
|3|PUSH BC      | 11 | 1 |      |C5          |                     |                      |
|3|PUSH DE      | 11 | 1 |      |D5          |                     |                      |
|3|PUSH HL      | 11 | 1 |      |E5          |                     |                      |
|3|PUSH IX      | 15 | 2 |------|DD E5       |Push                 |-(SP)=xx              |
|3|PUSH IY      | 15 | 2 |      |FD E5       |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|RES b,r      | 8  | 2 |------|CB 80+8*b+rb|Reset bit            |m=m&{~2^b}            |
|3|RES b,(HL)   | 15 | 2 |------|CB 86+8*b   |                     |                      |
|3|RES b,(IX+N) | 23 | 4 |------|DD CB XX 86+8*b                   |                      |
|3|RES b,(IY+N) | 23 | 4 |------|FD CB XX 86+8*b                   |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|RET          | 10 | 1 |------|C9          |Return               |PC=(SP)+              |
|3|RET C        |11/5| 1 |------|D8          |Conditional Return   |If Carry = 1          |
|3|RET NC       |11/5| 1 |      |D0          |                     |If Carry = 0          |
|3|RET M        |11/5| 1 |      |F8          |                     |If Sign = 1 (negative)|
|3|RET P        |11/5| 1 |      |F0          |                     |If Sign = 0 (positive)|
|3|RET Z        |11/5| 1 |      |C8          |                     |If Zero = 1 (ans.=0)  |
|3|RET NZ       |11/5| 1 |      |C0          |                     |If Zero = 0 (non-zero)|
|3|RET PE       |11/5| 1 |      |E8          |                     |If Parity = 1 (even)  |
|3|RET PO       |11/5| 1 |      |E0          |                     |If Parity = 0 (odd)   |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|RETI         | 14 | 2 |------|ED 4D       |Return from Interrupt|PC=(SP)+              |
|3|RETN         | 14 | 2 |------|ED 45       |Return from NMI      |PC=(SP)+              |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|I|Mnemonic     |Clck|Siz|SZHPNC|  OP-Code   |    Description      |        Notes         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|RLA          | 4  | 1 |--0-0*|17          |Rotate Left Acc.     |A={CY,A}<-            | C<-76543210<-C
|3|RL r         | 8  | 2 |**0P0*|CB 10+rb    |Rotate Left          |m={CY,m}<-            |
|3|RL (HL)      | 15 | 2 |      |CB 16       |                     |                      |
|3|RL (IX+N)    | 23 | 4 |      |DD CB XX 16 |                     |                      |
|3|RL (IY+N)    | 23 | 4 |      |FD CB XX 16 |                     |                      |
|3|RLCA         | 4  | 1 |--0-0*|07          |Rotate Left Cir. Acc.|A=A<-                 | A=A*2. overflow->bit0 and C
|3|RLC r        | 8  | 2 |**0P0*|CB 00+rb    |Rotate Left Circular |m=m<-                 |
|3|RLC (HL)     | 15 | 2 |      |CB 06       |                     |                      |
|3|RLC (IX+N)   | 23 | 4 |      |DD CB XX 06 |                     |                      |
|3|RLC (IY+N)   | 23 | 4 |      |FD CB XX 06 |                     |                      |
|2|RLD          | 18 | 2 |**0P0-|ED 6F       |Rotate Left 4 bits   |{A,(HL)}={A,(HL)}<- ##|
|3|RRA          | 4  | 1 |--0-0*|1F          |Rotate Right Acc.    |A=->{CY,A}            |
|3|RR r         | 8  | 2 |**0P0*|CB 18+rb    |Rotate Right         |m=->{CY,m}            |
|3|RR (HL)      | 15 | 2 |      |CB 1E       |                     |                      |
|3|RR (IX+N)    | 23 | 4 |      |DD CB XX 1E |                     |                      |
|3|RR (IY+N)    | 23 | 4 |      |FD CB XX 1E |                     |                      |
|3|RRCA         | 4  | 1 |--0-0*|0F          |Rotate Right Cir.Acc.|A=->A                 |
|3|RRC r        | 8  | 2 |**0P0*|CB 08+rb    |Rotate Right Circular|m=->m                 |
|3|RRC (HL)     | 15 | 2 |      |CB 0E       |                     |                      |
|3|RRC (IX+N)   | 23 | 4 |      |DD CB XX 0E |                     |                      |
|3|RRC (IY+N)   | 23 | 4 |      |FD CB XX 0E |                     |                      |
|2|RRD          | 18 | 2 |**0P0-|ED 67       |Rotate Right 4 bits  |{A,(HL)}=->{A,(HL)} ##|
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|RST 0        | 11 | 1 |------|C7          |Restart              | (p=0H,8H,10H,...,38H)| 1100 0111
|3|RST 08H      | 11 | 1 |      |CF          |                     |                      | 1100 1111
|3|RST 10H      | 11 | 1 |      |D7          |                     |                      | 11?? ?111
|3|RST 18H      | 11 | 1 |      |DF          |                     |                      | 
|3|RST 20H      | 11 | 1 |      |E7          |                     |                      | 
|3|RST 28H      | 11 | 1 |      |EF          |                     |                      | 
|3|RST 30H      | 11 | 1 |      |F7          |                     |                      | 
|3|RST 38H      | 11 | 1 |      |FF          |                     |                      | 
+-+-------------+----+---+------+------------+---------------------+----------------------+
|I|Mnemonic     |Clck|Siz|SZHPNC|  OP-Code   |    Description      |        Notes         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|SBC r        | 4  | 1 |***V1*|98+rb       |Subtract with Carry  |A=A-s-CY              |
|3|SBC A,N      | 7  | 2 |      |DE XX       |                     |                      |
|3|SBC (HL)     | 7  | 1 |      |9E          |                     |                      |
|3|SBC A,(IX+N) | 19 | 3 |      |DD 9E XX    |                     |                      |
|3|SBC A,(IY+N) | 19 | 3 |      |FD 9E XX    |                     |                      |
|3|SBC HL,BC    | 15 | 2 |**?V1*|ED 42       |Subtract with Carry  |HL=HL-ss-CY           |
|3|SBC HL,DE    | 15 | 2 |      |ED 52       |                     |                      |
|3|SBC HL,HL    | 15 | 2 |      |ED 62       |                     |                      |
|3|SBC HL,SP    | 15 | 2 |      |ED 72       |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|SCF          | 4  | 1 |--0-01|37          |Set Carry Flag       |CY=1                  |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|SET b,r      | 8  | 2 |------|CB C0+8*b+rb|Set bit              |m=mv{2^b}             |
|3|SET b,(HL)   | 15 | 2 |      |CB C6+8*b   |                     |                      |
|3|SET b,(IX+N) | 23 | 4 |      |DD CB XX C6+8*b                   |                      |
|3|SET b,(IY+N) | 23 | 4 |      |FD CB XX C6+8*b                   |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|SLA r        | 8  | 2 |**0P0*|CB 20+rb    |Shift Left Arithmetic|m=m*2                 | C<-76543210<-0
|3|SLA (HL)     | 15 | 2 |      |CB 26       |                     |                      |
|3|SLA (IX+N)   | 23 | 4 |      |DD CB XX 26 |                     |                      |
|3|SLA (IY+N)   | 23 | 4 |      |FD CB XX 26 |                     |                      |
|3|SRA r        | 8  | 2 |**0P0*|CB 28+rb    |Shift Right Arith.   |m=m/2                 |
|3|SRA (HL)     | 15 | 2 |      |CB 2E       |                     |                      |
|3|SRA (IX+N)   | 23 | 4 |      |DD CB XX 2E |                     |                      |
|3|SRA (IY+N)   | 23 | 4 |      |FD CB XX 2E |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|SLL r        | 8  | 2 |**0P0*|CB 30+rb    |Shift Left Logical*  |m={0,m,CY}<-          | C<-76543210<-C
|3|SLL (HL)     | 15 | 2 |      |CB 36       |                     |  (SLL instructions   |
|3|SLL (IX+N)   | 23 | 4 |      |DD CB XX 36 |                     |     are Unsupported) |
|3|SLL (IY+N)   | 23 | 4 |      |FD CB XX 36 |                     |                      |
|3|SRL r        | 8  | 2 |**0P0*|CB 38+rb    |Shift Right Logical  |m=->{0,m,CY}          |
|3|SRL (HL)     | 15 | 2 |      |CB 3E       |                     |                      |
|3|SRL (IX+N)   | 23 | 4 |      |DD CB XX 3E |                     |                      |
|3|SRL (IY+N)   | 23 | 4 |      |FD CB XX 3E |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|I|Mnemonic     |Clck|Siz|SZHPNC|  OP-Code   |    Description      |        Notes         |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|SUB r        | 4  | 1 |***V1*|90+rb       |Subtract             |A=A-s                 |
|3|SUB N        | 7  | 2 |      |D6 XX       |                     |                      |
|3|SUB (HL)     | 7  | 1 |      |96          |                     |                      |
|3|SUB (IX+N)   | 19 | 3 |      |DD 96 XX    |                     |                      |
|3|SUB (IY+N)   | 19 | 3 |      |FD 96 XX    |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
|3|XOR r        | 4  | 1 |***P00|A8+rb       |Logical Exclusive OR |A=Axs                 |
|3|XOR N        | 7  | 2 |      |EE XX       |                     |                      |
|3|XOR (HL)     | 7  | 1 |      |AE          |                     |                      |
|3|XOR (IX+N)   | 19 | 3 |      |DD AE XX    |                     |                      |
|3|XOR (IY+N)   | 19 | 3 |      |FD AE XX    |                     |                      |
+-+-------------+----+---+------+------------+---------------------+----------------------+
*/
