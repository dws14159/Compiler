# Compiler

A long time ago I read somewhere "every programmer should have written a compiler".
As I had some time available this seemed like a good idea.
The file dates suggest this could go back as far as 2003 so there won't be any "Modern C++" here. Rewriting to Modern wouldn't be a bad use of my current free time.
There is already a certain amount of rewriting going on anyway.

The first question was: which platform? I didn't want to create anything on the scale of gcc and thought maybe the compiler being 
able to compile its own source code would be a reasonable target.

I didn't want to get into the complexities of x86 assembly. I considered 680x0 but don't know it well enough.
So I picked z80 which I know from when I used to program my ZX81 in asembly.

But the ZX81 wouldn't be big enough. So I decided on a z80 emulator as the platform. This would just execute z80 instructions, not using T-states, just updating
register variables and using a block of PC memory as the emulated RAM. The point of this project would be to support the compiler, not to be a full
emulator product in its own right. It runs as a Win32 application, using memory mapped IO for emulated peripherals.

To test this I would of course need a Z80 assembler. And disassembler.

So we have the following projects:

**z80emu**: The emulator, a Win32 GUI application.

**z80asm**: Command line assembler.

**z80dis**: Command line disassembler.

**z80cc**: Command line C compiler for the above emulator.

**z80link**: Linker

The compiler generates text blocks like this:

```
:_main
@@length 59
{
  e5 dd e5 21 fc ff 39 f9 dd 21 00 00 dd 39 dd 36 
  00 00 dd 36 01 ff dd 36 02 01 dd 36 03 ff dd 66 
  01 dd 6e 00 3e 03 77 dd 66 03 dd 6e 02 3e 0e 77 
  3e 00 21 04 00 39 f9 dd e1 e1 c9 
}
```

This is the C runtime file crt0.o that calls main:

```
:__init
@@refs _main,5
@@length 8
{
  21 ff fe f9 cd 00 00 76
}
```

The linker will put the address of _main into the 5th byte, i.e. the 00 00 after cd. This is how the lack of relocatable addressing will be navigated.
