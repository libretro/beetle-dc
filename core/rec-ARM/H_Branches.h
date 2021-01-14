/*
 *	H_Branches.h
 *
 *
 */
#pragma once




namespace ARM
{



	inline static ptrdiff_t Literal(size_t FnAddr)
	{
		u8* pc_addr = (u8*)EMIT_GET_PTR();
		return (ptrdiff_t)((ptrdiff_t)FnAddr - ((ptrdiff_t)pc_addr+8));
	}

	EAPI CALL(size_t FnAddr, ConditionCode CC=AL)
	{
        bool isThumb = FnAddr & 1;
        FnAddr &= ~1;
		ptrdiff_t lit = Literal(FnAddr);

		if(0==lit) {
#ifndef VITA
			printf("Error, Compiler caught NULL literal, CALL(%08X)\n", FnAddr);
			verify(false);
#endif
			return;
		}
		if( (lit<-33554432) || (lit>33554428) )     // ..28 for BL ..30 for BLX
		{
#ifndef VITA
			printf("Warning, CALL(%08X) is out of range for literal(%08X)\n", FnAddr, lit);
			// verify(false);
#endif
			MOV32(IP, FnAddr, CC);
			BLX(IP, CC);
			return;
		}

        if (isThumb) {
            verify (CC==CC_EQ);
            BLX(lit, isThumb);
        } else {
            BL(lit,CC);
        }
	}



	EAPI JUMP(size_t FnAddr, ConditionCode CC=AL)
	{
        bool isThumb = FnAddr & 1;
        FnAddr &= ~1;
        
        verify(!isThumb);
		ptrdiff_t lit = Literal(FnAddr);

		/*if(0==lit) {
			printf("Error, Compiler caught NULL literal, JUMP(%08X)\n", FnAddr);
			verify(false);
			return;
		}*/
		if( (lit<-33554432) || (lit>33554428) )     // ..28 for BL ..30 for BLX
		{
#ifndef VITA
			printf("Warning, %X is out of range for imm jump! \n", FnAddr);
			//verify(false);
#endif
			MOV32(IP, FnAddr, CC);
			BX(IP, CC);
			return;
		}
        B(lit,CC);     // Note, wont work for THUMB*,  have to use bx which is reg only !
	}



}




