/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef ASM_CODE_ONLY /* C code ***********************************************/

/* registers.c is getting large and unwieldy, with every change requiring
 * line# changes in registers.res.  This is essentially an app_suite asm
 * test, with no errors expected, making it easier to expand.
 */

#include <stdio.h>
#include <stdlib.h>

void asm_test(char *undef, char *def);
void asm_test_avx(char *undef, char *def);

static void
asm_test_C(void)
{
    char undef[128];
    char def[128] = {0,};
    asm_test(undef, def);
    asm_test_avx(undef, def);
}

int
main(int argc, char *argv[])
{
    asm_test_C();

    printf("all done\n");
    return 0;
}

#else /* asm code *************************************************************/
#include "cpp2asm_defines.h"
START_FILE

#define FUNCNAME asm_test
/* void asm_test(char *undef, char *def); */
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XAX, ARG1
        mov      REG_XDX, ARG2
        push     REG_XBP
        mov      REG_XBP, REG_XSP
        END_PROLOG

        /* ensure top zeroed bits from shr are defined, and test bswap */
        mov      ecx, DWORD [REG_XAX] /* undef */
        shr      ecx, 16
        bswap    ecx
        movzx    ecx, cx
        cmp      ecx, HEX(1)

        movdqu   xmm0, [REG_XAX] /* undef */
        mov      ecx, DWORD [REG_XAX] /* undef */
        pxor     xmm1, xmm1
        punpcklwd xmm0, xmm1
        pextrw   ecx, xmm0, 7 /* top word came from xmm1 so defined */
        cmp      ecx, HEX(2)

        movdqu   xmm0, [REG_XAX] /* undef */
        pxor     xmm1, xmm1
        mov      ecx, DWORD [REG_XDX] /* def */
        pinsrd   xmm0, ecx, 0
        comiss   xmm0, xmm1 /* only looks at bottom 32 bits */

        movdqu   xmm0, [REG_XAX] /* undef */
        pxor     xmm1, xmm1
        mov      ecx, DWORD [REG_XDX] /* def */
        pinsrd   xmm0, ecx, 0
        pinsrd   xmm0, ecx, 1
        comisd   xmm0, xmm1 /* only looks at bottom 64 bits */

        movdqu   xmm0, [REG_XAX] /* undef */
        mov      ecx, DWORD [REG_XAX] /* undef */
        pxor     xmm1, xmm1
        movlhps  xmm0, xmm1
        pextrw   ecx, xmm0, 7 /* word came from xmm1 so defined */
        cmp      ecx, HEX(3)

        movdqu   xmm0, [REG_XAX] /* undef */
        mov      ecx, DWORD [REG_XAX] /* undef */
        pxor     xmm1, xmm1
        movhlps  xmm0, xmm1
        pextrw   ecx, xmm0, 1 /* word came from xmm1 so defined */
        cmp      ecx, HEX(3)

        movdqu   xmm0, [REG_XAX] /* undef */
        mov      ecx, DWORD [REG_XDX] /* def */
        pinsrd   xmm0, ecx, 0
        shufps   xmm0, xmm0, 0 /* bottom 4 bytes fill the whole thing */
        pextrw   ecx, xmm0, 7
        cmp      ecx, HEX(4)

        /* test unusual stack adjustments such as i#1500 */
        mov      REG_XCX, REG_XSP
        sub      REG_XCX, 16
        push     REG_XCX
        pop      REG_XSP
        mov      REG_XCX, PTRSZ [REG_XCX] /* unaddr if doesn't track "pop xsp" */
        add      REG_XSP, 16

        /* test pop into (esp) for i#1502 */
        push     REG_XCX
        push     REG_XCX
        pop      PTRSZ [REG_XSP]
        pop      REG_XCX

        /* test pop into (esp) for i#1502 */
        push     REG_XDX
        push     REG_XBX
        mov      edx, DWORD [REG_XAX] /* undef */
        or       ecx, HEX(28)
        mov      ebx, DWORD [REG_XAX + 4] /* undef */
        and      ebx, HEX(FFFFFF00)
        and      edx, HEX(C0014000)
        shl      ecx, HEX(B)
        or       ecx, edx
        cmp      ecx, 0
        cmp      ebx, 0
        pop      REG_XBX
        pop      REG_XDX

        /* test 4-byte div: really we want to ensure on fastpath (i#1573), but
         * how???
         */
        push     REG_XDX
        push     REG_XAX
        mov      ecx, DWORD [REG_XDX] /* def */
        mov      edx, DWORD [REG_XDX] /* def */
        div      DWORD [REG_XSP] /* 3 srcs and 2 dsts */
        cmp      eax,0 /* NOT uninit */
        cmp      edx,0 /* NOT uninit */
        pop      REG_XAX
        pop      REG_XDX

        /* Test i#1590 where whole-bb scratch regs conflict with sharing on
         * sub-dword instrs.  We need a bb with scratch ecx and eax.
         */
        jmp force_bb_i1590
    force_bb_i1590:
        cmp      BYTE [2 + REG_XDX], 0
        cmp      BYTE [3 + REG_XDX], 0
        test     dl, dl
        test     bl, bl
        jmp force_bb_i1590_end
    force_bb_i1590_end:

        /* Test i#1595: arrange for xl8 sharing with a 3rd scratch reg
         * to ensure we restore it properly.
         */
        jmp force_bb_i1595
    force_bb_i1595:
        /* Get ebx and ecx as scratch to ensure we share (xref i#1590) and
         * get edx as 3rd scratch so we crash if we mess it up.
         */
        mov      REG_XCX, REG_XAX
        mov      REG_XAX, REG_XAX
        cmp      BYTE [4 + REG_XDX], 0
        cmp      BYTE [5 + REG_XDX], 0
        jmp force_bb_i1595_end
    force_bb_i1595_end:

        /* XXX: add more tests here.  Avoid clobbering eax (holds undef mem) or
         * edx (holds def mem).  Do not place AVX instructions here: put them
         * into asm_test_avx().
         */

        add      REG_XSP, 0 /* make a legal SEH64 epilog */
        mov      REG_XSP, REG_XBP
        pop      REG_XBP
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME


#define FUNCNAME asm_test_avx
/* void asm_test_avx(char *undef, char *def); */
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XAX, ARG1
        mov      REG_XDX, ARG2
        push     REG_XBP
        mov      REG_XBP, REG_XSP
        END_PROLOG

        /* i#1577: only run AVX instructions on processors that support them */
        push     REG_XAX
        push     REG_XDX
        mov      eax, 1
        cpuid
#       define HAS_AVX 28
        mov      edx, 1
        shl      edx, 28
        test     edx, ecx
        pop      REG_XDX
        pop      REG_XAX
        je       no_avx

        movdqu   xmm0, [REG_XAX] /* undef */
        pxor     xmm1, xmm1
        mov      ecx, DWORD [REG_XDX] /* def */
        vpinsrd  xmm0, xmm0, ecx, 0 /* test vpinsrd (i#1559) */
        comiss   xmm0, xmm1 /* only looks at bottom 32 bits */

        /* XXX: add more tests here.  Avoid clobbering eax (holds undef mem) or
         * edx (holds def mem).
         */

     no_avx:
        add      REG_XSP, 0 /* make a legal SEH64 epilog */
        mov      REG_XSP, REG_XBP
        pop      REG_XBP
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME


END_FILE
#endif
