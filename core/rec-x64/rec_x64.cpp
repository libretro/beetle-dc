#include "deps/xbyak/xbyak.h"
#include "deps/xbyak/xbyak_util.h"

#include "types.h"

#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X64
#define EXPLODE_SPANS

#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/modules/ccn.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_rom.h"
#include "x64_regalloc.h"

struct DynaRBI : RuntimeBlockInfo
{
   virtual u32 Relink() {
      return 0;
   }

   virtual void Relocate(void* dst) {
      verify(false);
   }
};

extern int cycle_counter;

extern "C" {

void ngen_FailedToFindBlock_internal(void)
{
	rdv_FailedToFindBlock(Sh4cntx.pc);
}

};

void(*ngen_FailedToFindBlock)() = &ngen_FailedToFindBlock_internal;

#ifdef __MACH__
#define _U "_"
#else
#define _U
#endif

#ifdef _WIN32
#define WIN32_ONLY(x) x
#else
#define WIN32_ONLY(x)
#endif

#define STRINGIFY(x) #x
#define _S(x) STRINGIFY(x)
#define CPU_RUNNING 135266148
#define PC 135266120

#ifdef _WIN32
        // Fully naked function in win32 for proper SEH prologue
        __asm__ (
                        ".text                                                  \n\t"
                        ".p2align 4,,15                                 \n\t"
                        ".globl ngen_mainloop                   \n\t"
                        ".def   ngen_mainloop;  .scl    2;      .type   32;     .endef  \n\t"
                        ".seh_proc      ngen_mainloop           \n\t"
                "ngen_mainloop:                                         \n\t"
#else
void ngen_mainloop(void* v_cntx)
{
        __asm__ (
#endif
                        "pushq %rbx                                             \n\t"
WIN32_ONLY(     ".seh_pushreg %rbx                              \n\t")
                        "pushq %rbp                                             \n\t"
#ifdef _WIN32
                        ".seh_pushreg %rbp                              \n\t"
                        "pushq %rdi                                             \n\t"
                        ".seh_pushreg %rdi                              \n\t"
                        "pushq %rsi                                             \n\t"
                        ".seh_pushreg %rsi                              \n\t"
#endif
                        "pushq %r12                                             \n\t"
WIN32_ONLY(     ".seh_pushreg %r12                              \n\t")
                        "pushq %r13                                             \n\t"
WIN32_ONLY(     ".seh_pushreg %r13                              \n\t")
                        "pushq %r14                                             \n\t"
WIN32_ONLY(     ".seh_pushreg %r14                              \n\t")
                        "pushq %r15                                             \n\t"
#ifdef _WIN32
                        ".seh_pushreg %r15                              \n\t"
                        "subq $40, %rsp                                 \n\t"   // 32-byte shadow space + 8 for stack 16-byte alignment
                        ".seh_stackalloc 40                             \n\t"
                        ".seh_endprologue                               \n\t"
#else
                        "subq $8, %rsp                                  \n\t"   // 8 for stack 16-byte alignment
#endif
                        "movl $" _S(SH4_TIMESLICE) "," _U "cycle_counter(%rip)  \n"

                "1:                                                                             \n\t"   // run_loop
                        "movq " _U "p_sh4rcb(%rip), %rax        \n\t"
                        "movl " _S(CPU_RUNNING) "(%rax), %edx   \n\t"
                        "testl %edx, %edx                                       \n\t"
                        "je 3f                                                          \n"             // end_run_loop

                "2:                                                                             \n\t"   // slice_loop
                        "movq " _U "p_sh4rcb(%rip), %rax        \n\t"
#ifdef _WIN32
                        "movl " _S(PC)"(%rax), %ecx     \n\t"
#else
                        "movl " _S(PC)"(%rax), %edi     \n\t"
#endif
                        "call " _U "bm_GetCode                          \n\t"
                        "call *%rax                                             \n\t"
                        "movl " _U "cycle_counter(%rip), %ecx \n\t"
                        "testl %ecx, %ecx                                       \n\t"
                        "jg 2b                                                          \n\t"   // slice_loop

                        "addl $" _S(SH4_TIMESLICE) ", %ecx              \n\t"
                        "movl %ecx, " _U "cycle_counter(%rip)   \n\t"
                        "call " _U "UpdateSystem_INTC           \n\t"
                        "jmp 1b                                                         \n"             // run_loop

                "3:                                                                             \n\t"   // end_run_loop

#ifdef _WIN32
                        "addq $40, %rsp                                         \n\t"
#else
                        "addq $8, %rsp                                          \n\t"
#endif
                        "popq %r15                                                      \n\t"
                        "popq %r14                                                      \n\t"
                        "popq %r13                                                      \n\t"
                        "popq %r12                                                      \n\t"
#ifdef _WIN32
                        "popq %rsi                                                      \n\t"
                        "popq %rdi                                                      \n\t"
#endif
                        "popq %rbp                                                      \n\t"
                        "popq %rbx                                                      \n\t"
#ifdef _WIN32
                        "ret                                                            \n\t"
                        ".seh_endproc                                           \n\t"
        );
#else
        );
}
#endif

#undef _U
#undef _S

RuntimeBlockInfo* ngen_AllocateBlock(void)
{
   return new DynaRBI();
}

void ngen_blockcheckfail(u32 pc) {
	printf("X64 JIT: SMC invalidation at %08X\n", pc);
	rdv_BlockCheckFail(pc);
}

class BlockCompilerx64 : public Xbyak::CodeGenerator{
public:
	BlockCompilerx64() : Xbyak::CodeGenerator(64 * 1024, emit_GetCCPtr()), regalloc(this)
	{
#ifdef _WIN32
      call_regs.push_back(ecx);
      call_regs.push_back(edx);
      call_regs.push_back(r8d);
      call_regs.push_back(r9d);

      call_regs64.push_back(rcx);
      call_regs64.push_back(rdx);
      call_regs64.push_back(r8);
      call_regs64.push_back(r9);
#else
      call_regs.push_back(edi);
      call_regs.push_back(esi);
      call_regs.push_back(edx);
      call_regs.push_back(ecx);

      call_regs64.push_back(rdi);
      call_regs64.push_back(rsi);
      call_regs64.push_back(rdx);
      call_regs64.push_back(rcx);
#endif

		call_regsxmm.push_back(xmm0);
		call_regsxmm.push_back(xmm1);
		call_regsxmm.push_back(xmm2);
		call_regsxmm.push_back(xmm3);
	}

	void compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
   {
      if (force_checks) {
			CheckBlock(block);
		}
		regalloc.DoAlloc(block);

		sub(dword[rip + &cycle_counter], block->guest_cycles);

#ifdef _WIN32
		sub(rsp, 0x28);		// 32-byte shadow space + 8 byte alignment
#else
		sub(rsp, 0x8);		// align stack
#endif

		for (size_t i = 0; i < block->oplist.size(); i++)
      {
         shil_opcode& op  = block->oplist[i];

         regalloc.OpBegin(&op, i);

         switch (op.op)
         {
            case shop_ifb:
               if (op.rs1._imm)
               {
                  mov(rax, (size_t)&next_pc);
                  mov(dword[rax], op.rs2._imm);
               }

               mov(call_regs[0], op.rs3._imm);

			   GenCall(OpDesc[op.rs3._imm]->oph);
               break;

            case shop_jcond:
            case shop_jdyn:
               {
				  if (op.rs2.is_imm())
				  {
				     mov(ecx, regalloc.MapRegister(op.rs1));
                     add(ecx, op.rs2._imm);
					 mov(regalloc.MapRegister(op.rd), ecx);
                  }
				  else
					 mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
               }
               break;

            case shop_mov32:
               {
                  verify(op.rd.is_reg());

                  verify(op.rs1.is_reg() || op.rs1.is_imm());

				  if (regalloc.IsAllocf(op.rd))
					 shil_param_to_host_reg(op.rs1, regalloc.MapXRegister(op.rd));
				  else
					 shil_param_to_host_reg(op.rs1, regalloc.MapRegister(op.rd));
               }
               break;

            case shop_mov64:
               {
				  verify(op.rd.is_r64());
				  verify(op.rs1.is_r64());

#ifdef EXPLODE_SPANS
				  movss(regalloc.MapXRegister(op.rd, 0), regalloc.MapXRegister(op.rs1, 0));
				  movss(regalloc.MapXRegister(op.rd, 1), regalloc.MapXRegister(op.rs1, 1));
#else
				  mov(rax, (uintptr_t)op.rs1.reg_ptr());
				  mov(rax, qword[rax]);
				  mov(rcx, (uintptr_t)op.rd.reg_ptr());
				  mov(qword[rcx], rax);
#endif
               }
               break;

            case shop_readm:
               {
  				  u32 size = op.flags & 0x7f;

  				  if (op.rs1.is_imm())
  				  {
  					 bool isram = false;
  					 void* ptr = _vmem_read_const(op.rs1._imm, isram, size);

  					 if (isram)
  					 {
						// Immediate pointer to RAM: super-duper fast access
  						mov(rax, reinterpret_cast<uintptr_t>(ptr));
  						switch (size)
  						{
  						case 2:
  							movsx(regalloc.MapRegister(op.rd), word[rax]);
  							break;

  						case 4:
							if (regalloc.IsAllocg(op.rd))
								mov(regalloc.MapRegister(op.rd), dword[rax]);
							else
								movd(regalloc.MapXRegister(op.rd), dword[rax]);
  							break;

  						default:
  							die("Invalid size");
  							break;
  						}
  					 }
  					 else
  					 {
  						// Not RAM: the returned pointer is a memory handler
  						mov(call_regs[0], op.rs1._imm);

  						switch(size)
  						{
  						case 2:
  							GenCall((void (*)())ptr);
							movsx(ecx, ax);
  							break;

  						case 4:
  							GenCall((void (*)())ptr);
  							mov(ecx, eax);
  							break;

  						default:
  							die("Invalid size");
  							break;
  						}
  						host_reg_to_shil_param(op.rd, ecx);
  					}
  				}
  				else
  				{
  				   // Not an immediate address
				   shil_param_to_host_reg(op.rs1, call_regs[0]);
				   if (!op.rs3.is_null())
				   {
					   if (op.rs3.is_imm())
						   add(call_regs[0], op.rs3._imm);
					   else
							add(call_regs[0], regalloc.MapRegister(op.rs3));
				   }

				   if (size == 1) {
					  GenCall(ReadMem8);
				      movsx(ecx, al);
				   }
				   else if (size == 2) {
					  GenCall(ReadMem16);
					  movsx(ecx, ax);
				   }
				   else if (size == 4) {
					  GenCall(ReadMem32);
					  mov(ecx, eax);
				   }
				   else if (size == 8) {
					  GenCall(ReadMem64);
					  mov(rcx, rax);
				   }
				   else {
					  die("1..8 bytes");
				   }

				   if (size != 8)
					   host_reg_to_shil_param(op.rd, ecx);
				   else {
#ifdef EXPLODE_SPANS
					   verify(op.rd.count() == 2 && regalloc.IsAllocf(op.rd, 0) && regalloc.IsAllocf(op.rd, 1));
					   movd(regalloc.MapXRegister(op.rd, 0), ecx);
					   shr(rcx, 32);
					   movd(regalloc.MapXRegister(op.rd, 1), ecx);
#else
					   mov(rax, (uintptr_t)op.rd.reg_ptr());
					   mov(qword[rax], rcx);
#endif
				    }
  				 }
               }
               break;

            case shop_writem:
               {
                  u32 size = op.flags & 0x7f;
				shil_param_to_host_reg(op.rs1, call_regs[0]);
				if (!op.rs3.is_null())
				{
					if (op.rs3.is_imm())
						add(call_regs[0], op.rs3._imm);
					else
						add(call_regs[0], regalloc.MapRegister(op.rs3));
				}

                  if (size != 8)
					shil_param_to_host_reg(op.rs2, call_regs[1]);
				else {
#ifdef EXPLODE_SPANS
					verify(op.rs2.count() == 2 && regalloc.IsAllocf(op.rs2, 0) && regalloc.IsAllocf(op.rs2, 1));
					movd(call_regs[1], regalloc.MapXRegister(op.rs2, 1));
					shl(call_regs64[1], 32);
					movd(eax, regalloc.MapXRegister(op.rs2, 0));
					or_(call_regs64[1], rax);
#else
					mov(rax, (uintptr_t)op.rs2.reg_ptr());
					mov(call_regs64[1], qword[rax]);
#endif
				}

                  if (size == 1)
					GenCall(WriteMem8);
                  else if (size == 2)
					GenCall(WriteMem16);
                  else if (size == 4)
					GenCall(WriteMem32);
                  else if (size == 8)
					GenCall(WriteMem64);
                  else {
                     die("1..8 bytes");
                  }
               }
               break;

#ifndef CANONICAL_TEST
            case shop_sync_sr:
               GenCall(UpdateSR);
               break;
            case shop_sync_fpscr:
               GenCall(UpdateFPSCR);
               break;

            case shop_swaplb:
               if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
            	  mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
               ror(Xbyak::Reg16(regalloc.MapRegister(op.rd).getIdx()), 8);
               break;

            case shop_neg:
               if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
            	  mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
               neg(regalloc.MapRegister(op.rd));
               break;
            case shop_not:
               if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
            	  mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
 				not_(regalloc.MapRegister(op.rd));
              break;

            case shop_and:
               GenBinaryOp(op, &BlockCompilerx64::and_);
               break;
            case shop_or:
               GenBinaryOp(op, &BlockCompilerx64::or_);
               break;
            case shop_xor:
               GenBinaryOp(op, &BlockCompilerx64::xor_);
               break;
            case shop_add:
               GenBinaryOp(op, &BlockCompilerx64::add);
               break;
            case shop_sub:
               GenBinaryOp(op, &BlockCompilerx64::sub);
               break;

#define SHIFT_OP(natop) \
		if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))	\
		   mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));	\
		if (op.rs2.is_imm())	\
		   natop(regalloc.MapRegister(op.rd), op.rs2._imm);	\
		else if (op.rs2.is_reg())	\
		   natop(regalloc.MapRegister(op.rd), Xbyak::Reg8(regalloc.MapRegister(op.rs2).getIdx()));

            case shop_shl:
               SHIFT_OP(shl)
               break;
            case shop_shr:
               SHIFT_OP(shr)
               break;
            case shop_sar:
               SHIFT_OP(sar)
               break;
            case shop_ror:
               SHIFT_OP(ror)
               break;

            case shop_adc:
               if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
            	  mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
               cmp(regalloc.MapRegister(op.rs3), 1);	// C = ~rs3
               cmc();		// C = rs3
               adc(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs2)); // (C,rd)=rs1+rs2+rs3(C)
               setc(al);
               movzx(regalloc.MapRegister(op.rd2), al);	// rd2 = C
               break;
            /* FIXME buggy
			case shop_sbc:
				if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
					mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				cmp(regalloc.MapRegister(op.rs3), 1);	// C = ~rs3
				cmc();		// C = rs3
				mov(ecx, 1);
				mov(regalloc.MapRegister(op.rd2), 0);
				mov(eax, regalloc.MapRegister(op.rs2));
				neg(eax);
				adc(regalloc.MapRegister(op.rd), eax); // (C,rd)=rs1-rs2+rs3(C)
				cmovc(regalloc.MapRegister(op.rd2), ecx);	// rd2 = C
				break;
                */
            case shop_rocr:
            case shop_rocl:
               if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
            	  mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
               cmp(regalloc.MapRegister(op.rs2), 1);	// C = ~rs2
               cmc();		// C = rs2
               if (op.op == shop_rocr)
            	  rcr(regalloc.MapRegister(op.rd), 1);
               else
            	  rcl(regalloc.MapRegister(op.rd), 1);
               setc(al);
               movzx(regalloc.MapRegister(op.rd2), al);	// rd2 = C
               break;

            case shop_shld:
            case shop_shad:
            {
               if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
            	  mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
               Xbyak::Label negative_shift;
               Xbyak::Label non_zero;
               Xbyak::Label exit;

               mov(ecx, regalloc.MapRegister(op.rs2));
               cmp(ecx, 0);
               js(negative_shift);
               shl(regalloc.MapRegister(op.rd), cl);
               jmp(exit);

               L(negative_shift);
               test(ecx, 0x1f);
               jnz(non_zero);
               if (op.op == shop_shld)
            	  xor_(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rd));
               else
            	  sar(regalloc.MapRegister(op.rd), 31);
               jmp(exit);

               L(non_zero);
               neg(ecx);
               if (op.op == shop_shld)
            	  shr(regalloc.MapRegister(op.rd), cl);
               else
            	  sar(regalloc.MapRegister(op.rd), cl);
               L(exit);
            }
            break;

            case shop_test:
            case shop_seteq:
            case shop_setge:
            case shop_setgt:
            case shop_setae:
            case shop_setab:
            {
               if (op.op == shop_test)
               {
            	  if (op.rs2.is_imm())
            		 test(regalloc.MapRegister(op.rs1), op.rs2._imm);
            	  else
            		 test(regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
               }
               else
               {
            	  if (op.rs2.is_imm())
            		 cmp(regalloc.MapRegister(op.rs1), op.rs2._imm);
            	  else
            		 cmp(regalloc.MapRegister(op.rs1), regalloc.MapRegister(op.rs2));
               }
               switch (op.op)
               {
               case shop_test:
               case shop_seteq:
            	  sete(al);
            	  break;
               case shop_setge:
            	  setge(al);
            	  break;
               case shop_setgt:
            	  setg(al);
            	  break;
               case shop_setae:
            	  setae(al);
            	  break;
               case shop_setab:
            	  seta(al);
            	  break;
               default:
            	  die("invalid case");
            	  break;
               }
               movzx(regalloc.MapRegister(op.rd), al);
            }
            break;

            /*
			case shop_setpeq:
				// TODO
				break;
             */

            case shop_mul_u16:
               movzx(eax, Xbyak::Reg16(regalloc.MapRegister(op.rs1).getIdx()));
               movzx(ecx, Xbyak::Reg16(regalloc.MapRegister(op.rs2).getIdx()));
               mul(ecx);
               mov(regalloc.MapRegister(op.rd), eax);
               break;
            case shop_mul_s16:
               movsx(eax, Xbyak::Reg16(regalloc.MapRegister(op.rs1).getIdx()));
               movsx(ecx, Xbyak::Reg16(regalloc.MapRegister(op.rs2).getIdx()));
               mul(ecx);
               mov(regalloc.MapRegister(op.rd), eax);
               break;
            case shop_mul_i32:
               mov(eax, regalloc.MapRegister(op.rs1));
               mul(regalloc.MapRegister(op.rs2));
               mov(regalloc.MapRegister(op.rd), eax);
               break;
            case shop_mul_u64:
               mov(eax, regalloc.MapRegister(op.rs1));
               mov(ecx, regalloc.MapRegister(op.rs2));
               mul(rcx);
               mov(regalloc.MapRegister(op.rd), eax);
               shr(rax, 32);
               mov(regalloc.MapRegister(op.rd2), eax);
               break;
            case shop_mul_s64:
               movsxd(rax, regalloc.MapRegister(op.rs1));
               movsxd(rcx, regalloc.MapRegister(op.rs2));
               mul(rcx);
               mov(regalloc.MapRegister(op.rd), eax);
               shr(rax, 32);
               mov(regalloc.MapRegister(op.rd2), eax);
               break;

            /*
			case shop_pref:
				// TODO
				break;
                */

            case shop_ext_s8:
               mov(eax, regalloc.MapRegister(op.rs1));
               movsx(regalloc.MapRegister(op.rd), al);
               break;
            case shop_ext_s16:
               movsx(regalloc.MapRegister(op.rd), Xbyak::Reg16(regalloc.MapRegister(op.rs1).getIdx()));
               break;

               //
               // FPU
               //

            case shop_fadd:
               GenBinaryFOp(op, &BlockCompilerx64::addss);
               break;
            case shop_fsub:
               GenBinaryFOp(op, &BlockCompilerx64::subss);
               break;
            case shop_fmul:
               GenBinaryFOp(op, &BlockCompilerx64::mulss);
               break;
            case shop_fdiv:
               GenBinaryFOp(op, &BlockCompilerx64::divss);
               break;

            case shop_fabs:
               if (regalloc.mapf(op.rd) != regalloc.mapf(op.rs1))
            	  movss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs1));
               mov(rcx, (size_t)&float_abs_mask);
               movss(xmm0, dword[rcx]);
               pand(regalloc.MapXRegister(op.rd), xmm0);
               break;
            case shop_fneg:
               if (regalloc.mapf(op.rd) != regalloc.mapf(op.rs1))
            	  movss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs1));
               mov(rcx, (size_t)&float_sign_mask);
               movss(xmm0, dword[rcx]);
               pxor(regalloc.MapXRegister(op.rd), xmm0);
               break;

            case shop_fsqrt:
               sqrtss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs1));
               break;

            case shop_fmac:
               if (regalloc.mapf(op.rd) != regalloc.mapf(op.rs1))
               	movss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs1));
               if (cpu.has(Xbyak::util::Cpu::tFMA))
               	vfmadd231ss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs2), regalloc.MapXRegister(op.rs3));
					else
					{
						movss(xmm0, regalloc.MapXRegister(op.rs2));
						mulss(xmm0, regalloc.MapXRegister(op.rs3));
						addss(regalloc.MapXRegister(op.rd), xmm0);
					}
               break;

            case shop_fsrra:
   				// RSQRTSS has an |error| <= 1.5*2^-12 where the SH4 FSRRA needs |error| <= 2^-21
   				sqrtss(xmm0, regalloc.MapXRegister(op.rs1));
   				mov(eax, 0x3f800000);	// 1.0
   				movd(regalloc.MapXRegister(op.rd), eax);
   				divss(regalloc.MapXRegister(op.rd), xmm0);
               break;

            case shop_fsetgt:
            case shop_fseteq:
               ucomiss(regalloc.MapXRegister(op.rs1), regalloc.MapXRegister(op.rs2));
               if (op.op == shop_fsetgt)
               {
               	seta(al);
               }
               else
               {
               	//special case
               	//We want to take in account the 'unordered' case on the fpu
               	lahf();
               	test(ah, 0x44);
               	setnp(al);
               }
               movzx(regalloc.MapRegister(op.rd), al);
               break;

            case shop_fsca:
               movzx(rax, Xbyak::Reg16(regalloc.MapRegister(op.rs1).getIdx()));
               mov(rcx, (uintptr_t)&sin_table);
#ifdef EXPLODE_SPANS
               movss(regalloc.MapXRegister(op.rd, 0), dword[rcx + rax * 8]);
               movss(regalloc.MapXRegister(op.rd, 1), dword[rcx + (rax * 8) + 4]);
#else
               mov(rcx, qword[rcx + rax * 8]);
               mov(rdx, (uintptr_t)op.rd.reg_ptr());
               mov(qword[rdx], rcx);
#endif
               break;

            case shop_fipr:
				{
					mov(rax, (size_t)op.rs1.reg_ptr());
					movaps(regalloc.MapXRegister(op.rd), dword[rax]);
					mov(rax, (size_t)op.rs2.reg_ptr());
					mulps(regalloc.MapXRegister(op.rd), dword[rax]);
					const Xbyak::Xmm &rd = regalloc.MapXRegister(op.rd);
					// Only first-generation 64-bit CPUs lack SSE3 support
					if (cpu.has(Xbyak::util::Cpu::tSSE3))
					{
						haddps(rd, rd);
						haddps(rd, rd);
					}
					else
					{
						movhlps(xmm1, rd);
						addps(rd, xmm1);
						movaps(xmm1, rd);
						shufps(xmm1, xmm1,1);
						addss(rd, xmm1);
					}
				}
            break;

            case shop_ftrv:
            	mov(rax, (uintptr_t)op.rs1.reg_ptr());
#if 0	// vfmadd231ps and vmulps cause rounding problems
            	if (cpu.has(Xbyak::util::Cpu::tFMA))
            	{
            		movaps(xmm0, xword[rax]);					// fn[0-4]
            		mov(rax, (uintptr_t)op.rs2.reg_ptr());		// fm[0-15]

            		pshufd(xmm1, xmm0, 0x00);					// fn[0]
            		vmulps(xmm2, xmm1, xword[rax]);				// fm[0-3]
            		pshufd(xmm1, xmm0, 0x55);					// fn[1]
            		vfmadd231ps(xmm2, xmm1, xword[rax + 16]);	// fm[4-7]
            		pshufd(xmm1, xmm0, 0xaa);					// fn[2]
            		vfmadd231ps(xmm2, xmm1, xword[rax + 32]);	// fm[8-11]
            		pshufd(xmm1, xmm0, 0xff);					// fn[3]
            		vfmadd231ps(xmm2, xmm1, xword[rax + 48]);	// fm[12-15]
            		mov(rax, (uintptr_t)op.rd.reg_ptr());
            		movaps(xword[rax], xmm2);
            	}
            	else
#endif
            	{
            		movaps(xmm3, xword[rax]);                   //xmm0=vector
            		pshufd(xmm0, xmm3, 0);                      //xmm0={v0}
            		pshufd(xmm1, xmm3, 0x55);                   //xmm1={v1}
            		pshufd(xmm2, xmm3, 0xaa);                   //xmm2={v2}
            		pshufd(xmm3, xmm3, 0xff);                   //xmm3={v3}

            		//do the matrix mult !
            		mov(rax, (uintptr_t)op.rs2.reg_ptr());
            		mulps(xmm0, xword[rax + 0]);   //v0*=vm0
            		mulps(xmm1, xword[rax + 16]);  //v1*=vm1
            		mulps(xmm2, xword[rax + 32]);  //v2*=vm2
            		mulps(xmm3, xword[rax + 48]);  //v3*=vm3

            		addps(xmm0, xmm1);	 //sum it all up
            		addps(xmm2, xmm3);
            		addps(xmm0, xmm2);

            		mov(rax, (uintptr_t)op.rd.reg_ptr());
            		movaps(xword[rax], xmm0);
            	}
               break;

            case shop_frswap:
               mov(rax, (uintptr_t)op.rs1.reg_ptr());
               mov(rcx, (uintptr_t)op.rd.reg_ptr());
					if (cpu.has(Xbyak::util::Cpu::tAVX))
					{
						vmovaps(ymm0, yword[rax]);
						vmovaps(ymm1, yword[rcx]);
						vmovaps(yword[rax], ymm1);
						vmovaps(yword[rcx], ymm0);

						vmovaps(ymm0, yword[rax + 32]);
						vmovaps(ymm1, yword[rcx + 32]);
						vmovaps(yword[rax + 32], ymm1);
						vmovaps(yword[rcx + 32], ymm0);
					}
					else
					{
						for (int i = 0; i < 4; i++)
						{
							movaps(xmm0, xword[rax + (i * 16)]);
							movaps(xmm1, xword[rcx + (i * 16)]);
							movaps(xword[rax + (i * 16)], xmm1);
							movaps(xword[rcx + (i * 16)], xmm0);
						}
					}
               break;

            case shop_cvt_f2i_t:
               mov(rcx, (uintptr_t)&cvtf2i_pos_saturation);
               movss(xmm0, dword[rcx]);
               minss(xmm0, regalloc.MapXRegister(op.rs1));
               cvttss2si(regalloc.MapRegister(op.rd), xmm0);
               break;
            case shop_cvt_i2f_n:
            case shop_cvt_i2f_z:
               cvtsi2ss(regalloc.MapXRegister(op.rd), regalloc.MapRegister(op.rs1));
               break;
#endif

            default:
               shil_chf[op.op](&op);
               break;
         }
		 regalloc.OpEnd(&op);
      }

	  mov(rax, (size_t)&next_pc);

	  switch (block->BlockType) {

		case BET_StaticJump:
		case BET_StaticCall:
			//next_pc = block->BranchBlock;
			mov(dword[rax], block->BranchBlock);
			break;

		case BET_Cond_0:
		case BET_Cond_1:
			{
				//next_pc = next_pc_value;
				//if (*jdyn == 0)
				//next_pc = branch_pc_value;

				mov(dword[rax], block->NextBlock);

				if (block->has_jcond)
					mov(rdx, (size_t)&Sh4cntx.jdyn);
				else
					mov(rdx, (size_t)&sr.T);

				cmp(dword[rdx], block->BlockType & 1);
				Xbyak::Label branch_not_taken;

				jne(branch_not_taken, T_SHORT);
				mov(dword[rax], block->BranchBlock);
				L(branch_not_taken);
			}
			break;

		case BET_DynamicJump:
		case BET_DynamicCall:
		case BET_DynamicRet:
			//next_pc = *jdyn;
			mov(rdx, (size_t)&Sh4cntx.jdyn);
			mov(edx, dword[rdx]);
			mov(dword[rax], edx);
			break;

		case BET_DynamicIntr:
		case BET_StaticIntr:
			if (block->BlockType == BET_DynamicIntr) {
				//next_pc = *jdyn;
				mov(rdx, (size_t)&Sh4cntx.jdyn);
				mov(edx, dword[rdx]);
				mov(dword[rax], edx);
			}
			else {
				//next_pc = next_pc_value;
				mov(dword[rax], block->NextBlock);
			}

			GenCall(UpdateINTC);
			break;

		default:
			die("Invalid block end type");
		}

#ifdef _WIN32
		add(rsp, 0x28);
#else
		add(rsp, 0x8);
#endif
		ret();

		ready();

		block->code = (DynarecCodeEntryPtr)getCode();

		emit_Skip(getSize());
	}

	void ngen_CC_Start(const shil_opcode& op)
	{
		CC_pars.clear();
	}

	void ngen_CC_param(const shil_opcode& op, const shil_param& prm, CanonicalParamType tp) {
		switch (tp)
		{

		case CPT_u32:
		case CPT_ptr:
		case CPT_f32:
		{
			CC_PS t = { tp, &prm };
			CC_pars.push_back(t);
		}
		break;


		//store from EAX
		case CPT_u64rvL:
		case CPT_u32rv:
			mov(rcx, rax);
			host_reg_to_shil_param(prm, ecx);
			break;

		case CPT_u64rvH:
			// assuming CPT_u64rvL has just been called
			shr(rcx, 32);
			host_reg_to_shil_param(prm, ecx);
			break;

		// store from xmm0
		case CPT_f32rv:
			host_reg_to_shil_param(prm, xmm0);
#ifdef EXPLODE_SPANS
			// The x86 dynarec saves to mem as well
			//mov(rax, (uintptr_t)prm.reg_ptr());
			//movd(dword[rax], xmm0);
#endif
			break;
		}
	}

	void ngen_CC_Call(const shil_opcode& op, void* function)
	{
		int regused = 0;
		int xmmused = 0;

		for (int i = CC_pars.size(); i-- > 0;)
		{
         verify(xmmused < 4 && regused < 4);
			const shil_param& prm = *CC_pars[i].prm;
			switch (CC_pars[i].type) {
            //push the contents

            case CPT_u32:
               shil_param_to_host_reg(prm, call_regs[regused++]);
               break;

            case CPT_f32:
               shil_param_to_host_reg(prm, call_regsxmm[xmmused++]);
               break;

               //push the ptr itself
            case CPT_ptr:
               verify(prm.is_reg());

               mov(call_regs64[regused++], (size_t)prm.reg_ptr());

               break;
            default:
               // Other cases handled in ngen_CC_param
               break;
			}
		}
		GenCall((void (*)())function);
	}

	void RegPreload(u32 reg, Xbyak::Operand::Code nreg)
	{
	   mov(rax, (size_t)GetRegPtr(reg));
	   mov(Xbyak::Reg32(nreg), dword[rax]);
	}
	void RegWriteback(u32 reg, Xbyak::Operand::Code nreg)
	{
	   mov(rax, (size_t)GetRegPtr(reg));
	   mov(dword[rax], Xbyak::Reg32(nreg));
	}
	void RegPreload_FPU(u32 reg, s8 nreg)
	{
	   mov(rax, (size_t)GetRegPtr(reg));
	   movss(Xbyak::Xmm(nreg), dword[rax]);
	}
	void RegWriteback_FPU(u32 reg, s8 nreg)
	{
	   mov(rax, (size_t)GetRegPtr(reg));
	   movss(dword[rax], Xbyak::Xmm(nreg));
	}

private:
	typedef void (BlockCompilerx64::*X64BinaryOp)(const Xbyak::Operand&, const Xbyak::Operand&);
	typedef void (BlockCompilerx64::*X64BinaryFOp)(const Xbyak::Xmm&, const Xbyak::Operand&);

	void CheckBlock(RuntimeBlockInfo* block) {
	   mov(call_regs[0], block->addr);

	   s32 sz=block->sh4_code_size;
	   u32 sa=block->addr;

	   while (sz > 0)
	   {
		  void* ptr = (void*)GetMemPtr(sa, sz > 8 ? 8 : sz);
		  if (ptr)
		  {
			 mov(rax, reinterpret_cast<uintptr_t>(ptr));

			 if (sz >= 8) {
				mov(rdx, *(u64*)ptr);
				cmp(qword[rax], rdx);
				sz -= 8;
				sa += 8;
			 }
			 else if (sz >= 4) {
				mov(edx, *(u32*)ptr);
				cmp(dword[rax], edx);
				sz -= 4;
				sa += 4;
			 }
			 else {
				mov(edx, *(u16*)ptr);
				cmp(word[rax],dx);
				sz -= 2;
				sa += 2;
			 }
			 jne(reinterpret_cast<const void*>(&ngen_blockcheckfail));
		  }
	   }

	}

	void GenBinaryOp(const shil_opcode &op, X64BinaryOp natop)
	{
	   if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
		  mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
	   if (op.rs2.is_imm())
	   {
		  mov(ecx, op.rs2._imm);
		  (this->*natop)(regalloc.MapRegister(op.rd), ecx);
	   }
	   else
		  (this->*natop)(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs2));
	}

	void GenBinaryFOp(const shil_opcode &op, X64BinaryFOp natop)
	{
	   if (regalloc.mapf(op.rd) != regalloc.mapf(op.rs1))
		  movss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs1));
	   (this->*natop)(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs2));
	}

	template<class Ret, class... Params>
	void GenCall(Ret(*function)(Params...))
	{
#ifndef _WIN32
		// Need to save xmm registers as they are not preserved in linux/mach
		sub(rsp, 16);
		movd(ptr[rsp + 0], xmm8);
		movd(ptr[rsp + 4], xmm9);
		movd(ptr[rsp + 8], xmm10);
		movd(ptr[rsp + 12], xmm11);
#endif

	   call(function);

#ifndef _WIN32
	   movd(xmm8, ptr[rsp + 0]);
	   movd(xmm9, ptr[rsp + 4]);
	   movd(xmm10, ptr[rsp + 8]);
	   movd(xmm11, ptr[rsp + 12]);
	   add(rsp, 16);
#endif
	}

	// uses eax/rax
	void shil_param_to_host_reg(const shil_param& param, const Xbyak::Reg& reg)
	{
	   if (param.is_imm())
	   {
		  if (!reg.isXMM())
			 mov(reg, param._imm);
		  else
		  {
			 mov(eax, param._imm);
			 movd((const Xbyak::Xmm &)reg, eax);
		  }
	   }
	   else if (param.is_reg())
	   {
		  if (param.is_r32f())
		  {
			 if (!reg.isXMM())
				movd((const Xbyak::Reg32 &)reg, regalloc.MapXRegister(param));
			 else
				movss((const Xbyak::Xmm &)reg, regalloc.MapXRegister(param));
		  }
		  else
		  {
			 if (!reg.isXMM())
				mov((const Xbyak::Reg32 &)reg, regalloc.MapRegister(param));
			 else
				movd((const Xbyak::Xmm &)reg, regalloc.MapRegister(param));
		  }
	   }
	   else
	   {
		  verify(param.is_null());
	   }
	}

	// uses rax
	void host_reg_to_shil_param(const shil_param& param, const Xbyak::Reg& reg)
	{
	   if (regalloc.IsAllocg(param))
	   {
		  if (!reg.isXMM())
			 mov(regalloc.MapRegister(param), (const Xbyak::Reg32 &)reg);
		  else
			 movd(regalloc.MapRegister(param), (const Xbyak::Xmm &)reg);
	   }
	   else
	   {
		  if (!reg.isXMM())
			 movd(regalloc.MapXRegister(param), (const Xbyak::Reg32 &)reg);
		  else
			 movss(regalloc.MapXRegister(param), (const Xbyak::Xmm &)reg);
	   }
	}

	vector<Xbyak::Reg32> call_regs;
	vector<Xbyak::Reg64> call_regs64;
	vector<Xbyak::Xmm> call_regsxmm;

	struct CC_PS
	{
	   CanonicalParamType type;
	   const shil_param* prm;
	};
	vector<CC_PS> CC_pars;

	X64RegAlloc regalloc;
	Xbyak::util::Cpu cpu;
	static const u32 float_sign_mask;
	static const u32 float_abs_mask;
	static const f32 cvtf2i_pos_saturation;
};

const u32 BlockCompilerx64::float_sign_mask = 0x80000000;
const u32 BlockCompilerx64::float_abs_mask = 0x7fffffff;
const f32 BlockCompilerx64::cvtf2i_pos_saturation = 2147483520.0f;		// IEEE 754: 0x4effffff;

void X64RegAlloc::Preload(u32 reg, Xbyak::Operand::Code nreg)
{
   compiler->RegPreload(reg, nreg);
}
void X64RegAlloc::Writeback(u32 reg, Xbyak::Operand::Code nreg)
{
   compiler->RegWriteback(reg, nreg);
}
void X64RegAlloc::Preload_FPU(u32 reg, s8 nreg)
{
   compiler->RegPreload_FPU(reg, nreg);
}
void X64RegAlloc::Writeback_FPU(u32 reg, s8 nreg)
{
   compiler->RegWriteback_FPU(reg, nreg);
}

static BlockCompilerx64* compilerx64_data;

void ngen_Compile_x64(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
{
	verify(CPU_RUNNING == offsetof(Sh4RCB, cntx.CpuRunning));
	verify(PC == offsetof(Sh4RCB, cntx.pc));
	verify(emit_FreeSpace() >= 16 * 1024);

	compilerx64_data = new BlockCompilerx64();

	BlockCompilerx64 *compiler = compilerx64_data;
	
	compiler->compile(block, force_checks, reset, staging, optimise);

	delete compiler;
}

void ngen_CC_Call_x64(shil_opcode*op, void* function)
{
   BlockCompilerx64 *compiler = compilerx64_data;
   compiler->ngen_CC_Call(*op, function);
}

void ngen_CC_Param_x64(shil_opcode* op,shil_param* par,CanonicalParamType tp)
{
   BlockCompilerx64 *compiler = compilerx64_data;
   compiler->ngen_CC_param(*op, *par, tp);
}

void ngen_CC_Start_x64(shil_opcode* op)
{
   BlockCompilerx64 *compiler = compilerx64_data;
   compiler->ngen_CC_Start(*op);
}

void ngen_CC_Finish_x64(shil_opcode* op)
{
}
#endif
