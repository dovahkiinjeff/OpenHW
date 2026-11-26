/* Ported from our 2026-10-08 Ghidra recovery. Do not import raw retail
   pointers or process-layout casts here. Stable labels below identify recovered
   callbacks for save/load; original function addresses are never executed. */
#include "CataclysmScriptRuntime.h"
#include <cstdint>
static void cataM2_005db2a0();
static void cataM2_005db3c0();
static void cataM2_005db480();
static void cataM2_005db590();
static void cataM2_005db5d0();
static void cataM2_005db6f0();
static void cataM2_005db770();
static void cataM2_005db830();
static void cataM2_005dba70();
static void cataM2_005dbac0();
static void cataM2_005dcbd0();
static void cataM2_005dcf10();
static void cataM2_005dd040();
static void cataM2_005dd0a0();
static void cataM2_005dd1b0();
static void cataM2_005dd250();
static void cataM2_005dd280();
static void cataM2_005dd2d0();
static void cataM2_005dd370();
static void cataM2_005dd3d0();
static void cataM2_005dd4a0();
static void cataM2_005dd570();
static void cataM2_005dd5b0();
static void cataM2_005dd5f0();
static void cataM2_005dd670();
static void cataM2_005dd730();
static void cataM2_005dd790();
static void cataM2_005dd800();
static void cataM2_005dd870();
static void cataM2_005dda60();
static void cataM2_005ddad0();
static void cataM2_005ddb10();
static void cataM2_005ddc60();
static void cataM2_005ddca0();
static void cataM2_005ddce0();
static void cataM2_005ddd30();
static void cataM2_005dddb0();
static void cataM2_005dde90();
static void cataM2_005ddef0();
static void cataM2_005ddff0();
static void cataM2_005de050();
static void cataM2_005de090();
static void cataM2_005de1e0();
static void cataM2_005de220();
static void cataM2_005de270();
static void cataM2_005de2e0();
static void cataM2_005de380();
static void cataM2_005de440();
static void cataM2_005de4a0();
static void cataM2_005de4e0();
static void cataM2_005de530();
static void cataM2_005de5a0();
static void cataM2_005de5e0();
static void cataM2_005de650();
static void cataM2_005de810();
static void cataM2_005de870();
static void cataM2_005de920();
static void cataM2_005dea60();
static void cataM2_005deaa0();
static void cataM2_005deae0();
static void cataM2_005deb30();
static void cataM2_005deba0();
static void cataM2_005dec20();
static void cataM2_005decd0();
static void cataM2_005ded00();
static void cataM2_005ded90();
static void cataM2_005dee10();
static void cataM2_005dee40();
static void cataM2_005def00();
static void cataM2_005df0c0();
static void cataM2_005df130();
static void cataM2_005df1f0();
static void cataM2_005df320();
static void cataM2_005df3b0();
static void cataM2_005df3e0();
static void cataM2_005df420();
static void cataM2_005df4b0();
static void cataM2_005df560();
static void cataM2_005df5a0();
static void cataM2_005df690();
static void cataM2_005df7c0();
static void cataM2_005df840();
static void cataM2_005df900();
static void cataM2_005df950();
static void cataM2_005dfe10();
static void cataM2_005dfe40();
static void cataM2_005dfec0();
static void cataM2_005e00c0();
static void cataM2_005e0120();
static void cataM2_005e0150();
static void cataM2_005e01b0();
static void cataM2_005e0220();
static void cataM2_005e0290();
static void cataM2_005e02e0();
static void cataM2_005e0390();
static void cataM2_005e03d0();
static void cataM2_005e0440();
static void cataM2_005e0540();
static void cataM2_005e05a0();
static void cataM2_005e0bb0();
static void cataM2_005e3410();
static void cataM2_005f4730();
static void cataM2_00608010();
static void cataM2_00613940();
static void cataM2_00624aa0();
static void cataM2_0062ca80();
static void cataM2_00652ef0();
static void cataM2_0065f6f0();

static void cataM2_005db2a0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x4697443,cataShips());
  cataCall(0x00495bd0u,2,uVar1);
  cataCall(0x00498dd0u,4,0x546973c,0,0,0);
  cataCall(0x00491d30u,2,0x5137b,0);
  cataCall(0x00498dd0u,4,0x546dbde,0,0,0);
  cataCall(0x00498dd0u,4,0x534788,0,50000,0);
  cataCall(0x00498dd0u,4,0x78cc5c4,50000,50000,0);
  cataCall(0x00498dd0u,4,0x49684,50000,0,0);
  cataCall(0x00498dd0u,4,0xa8ccec4,50000,0xffff3cb0,0);
  cataCall(0x00498dd0u,4,0x584a88,0,0xffff3cb0,0);
  cataCall(0x00498dd0u,4,0xa8de2c4,0xffff3cb0,0xffff3cb0,0);
  cataCall(0x00498dd0u,4,0x5ba84,0xffff3cb0,0,0);
  cataCall(0x00498dd0u,4,0x78de9c4,0xffff3cb0,50000,0);
  cataCall(0x00490640u,3,0x946de9c,cataM2_005db480,cataM2_00652ef0);
  return;
}

static void cataM2_005db3c0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x5137b);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x004909f0u,0,18000);
    uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x646977c,uVar2);
    iVar1 = cataCall(0x00495a50u,4,uVar2);
    if (iVar1 != 0) {
      cataCall(0x00490640u,3,0x469857b,cataM2_005db5d0,cataM2_005db6f0);
      return;
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443cca1);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492ce0u,0);
    if (iVar1 == 0) {
      cataCall(0x00491d30u,2,0x443cca1,1);
      cataCall(0x00490640u,3,0x53a0c,cataM2_0065f6f0,cataM2_00652ef0);
    }
  }
  return;
}

static void cataM2_005db480(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00492910u,2,1,9);
  switch(uVar1) {
  case 1:
    uVar1 = 0x546dbde;
    break;
  case 2:
    uVar1 = 0x534788;
    break;
  case 3:
    uVar1 = 0x78cc5c4;
    break;
  case 4:
    uVar1 = 0x49684;
    break;
  case 5:
    uVar1 = 0xa8ccec4;
    break;
  case 6:
    uVar1 = 0x584a88;
    break;
  case 7:
    uVar1 = 0xa8de2c4;
    break;
  case 8:
    uVar1 = 0x5ba84;
    break;
  default:
    uVar1 = 0x78de9c4;
  }
  uVar1 = cataCall(0x00490b30u,1,uVar1,50000);
  uVar1 = cataCall(0x00490b30u,1,0x546973c,uVar1);
  cataCall(0x004991d0u,3,uVar1);
  cataCall(0x00492780u,0);
  uVar1 = cataCall(0x00490b30u,1,0x546973c);
  cataCall(0x004941a0u,1,uVar1);
  cataCall(0x00490640u,3,0x546973c,cataM2_00652ef0,cataM2_005db590);
  return;
}

static void cataM2_005db590(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00499870u,1,cataShips());
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x946de9c,cataM2_005db480,cataM2_00652ef0);
  }
  return;
}

static void cataM2_005db5d0(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  cataCall(0x00491d70u,2,0x5137b,1);
  cataCall(0x004920e0u,1,0x469857b);
  uVar1 = cataCall(0x00492910u,2,0x1e,0x3c);
  cataCall(0x00491f90u,2,0x469857b,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x004909f0u,0,18000);
  uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x5296679,uVar1);
  iVar2 = cataCall(0x00495a50u,4,uVar1);
  if (iVar2 != 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x5296679);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  uVar1 = cataCall(0x00490bb0u,1,0x12f9883);
  iVar2 = cataCall(0x00494510u,1,uVar1);
  if (iVar2 != 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x12f9883);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  cataCall(0x00492780u,0);
  uVar1 = cataCall(0x004909f0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x0049a100u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492190u,1,uVar1);
  return;
}

static void cataM2_005db6f0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00499870u,1,cataShips());
  if (iVar1 != 0) {
    cataCall(0x00491d70u,2,0x5137b,0);
    cataCall(0x00490640u,3,0x946de9c,cataM2_005db480,cataM2_00652ef0);
    return;
  }
  iVar1 = cataCall(0x004920a0u,1,0x469857b);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x469857b,cataM2_005db5d0,cataM2_005db6f0);
  }
  return;
}

static void cataM2_005db770(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491d30u,2,0xfdbd41e,0);
  cataCall(0x00491d30u,2,0x68087c5,0);
  cataCall(0x00491d30u,2,0xca04b13,0);
  cataCall(0x00491d30u,2,0x32243d3,0);
  cataCall(0x00491d30u,2,0xa4e5ad3,0);
  cataCall(0x00491d30u,2,0xddfdd03,0);
  cataCall(0x00491d30u,2,0x79d69c2,0);
  cataCall(0x00491d30u,2,0x120cee,0);
  cataCall(0x00491d30u,2,0x5137b,0);
  cataCall(0x00496720u,1,cataScriptText(0x008ab8b8));
  cataCall(0x00496720u,1,cataScriptText(0x008c83c0));
  cataCall(0x00491ec0u,1,0x89a6a54);
  cataCall(0x00490640u,3,0x1f32b25,cataM2_00613940,cataM2_005dbac0);
  return;
}

static void cataM2_005db830(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00497b00u,0);
  if ((iVar1 == 0) && (iVar1 = cataCall(0x00491e30u,1,0x858db5e4), iVar1 == 0)) {
    return;
  }
  cataCall(0x00491d70u,2,0x858db5e4,0);
  cataCall(0x00496530u,0);
  iVar1 = cataCall(0x00491e30u,1,0xfdbd41e);
  if (iVar1 != 0) {
    cataCall(0x00497630u,1,0x5a);
    cataCall(0x00491d70u,2,0xfdbd41e,0);
  }
  iVar1 = cataCall(0x00491e30u,1,0x68087c5);
  if (iVar1 != 0) {
    cataCall(0x00497aa0u,0);
    cataCall(0x00491d70u,2,0x68087c5,0);
  }
  iVar1 = cataCall(0x00491e30u,1,0xca04b13);
  if (iVar1 != 0) {
    cataCall(0x00497af0u,0);
    cataCall(0x00491d70u,2,0xca04b13,0);
  }
  iVar1 = cataCall(0x00491e30u,1,0x32243d3);
  if (iVar1 != 0) {
    cataCall(0x00496c30u,0,1);
    cataCall(0x00491d70u,2,0x32243d3,0);
  }
  iVar1 = cataCall(0x00491e30u,1,0xa4e5ad3);
  if (iVar1 != 0) {
    cataCall(0x00496ae0u,0);
    uVar2 = cataCall(0x00491e30u,1,0xf8bffc5);
    cataCall(0x004961b0u,1,uVar2);
    uVar2 = cataCall(0x00491e30u,1,0x3a1cffe);
    cataCall(0x00496210u,1,uVar2);
    uVar2 = cataCall(0x00491e30u,1,0x3d4ba05);
    cataCall(0x00496260u,1,uVar2);
    cataCall(0x00491d70u,2,0xa4e5ad3,0);
  }
  iVar1 = cataCall(0x00491e30u,1,0xddfdd03);
  if (iVar1 != 0) {
    cataCall(0x00496ae0u,0);
    cataCall(0x00496ae0u,0);
    uVar2 = cataCall(0x00491e30u,1,0xf8bffc5);
    cataCall(0x004961b0u,1,uVar2);
    uVar2 = cataCall(0x00491e30u,1,0x3a1cffe);
    cataCall(0x00496210u,1,uVar2);
    uVar2 = cataCall(0x00491e30u,1,0x3d4ba05);
    cataCall(0x00496260u,1,uVar2);
    cataCall(0x00491d70u,2,0xddfdd03,0);
  }
  iVar1 = cataCall(0x00491e30u,1,0x79d69c2);
  if (iVar1 != 0) {
    cataCall(0x00491d70u,2,0x79d69c2,0);
    cataCall(0x00491f50u,1,0x89a6a54);
  }
  iVar1 = cataCall(0x00491e30u,1,0x120cee);
  if (iVar1 != 0) {
    cataCall(0x00491d70u,2,0x120cee,0);
    cataCall(0x00498250u,1,0);
  }
  iVar1 = cataCall(0x00491e30u,1,0x443cca2);
  if ((iVar1 == 0) && (iVar1 = cataCall(0x00491e30u,1,0x8f624235), iVar1 != 0)) {
    cataCall(0x00491d30u,2,0x443cca2,1);
    cataCall(0x004980c0u,3,cataScriptText(0x0088b8b8),cataScriptText(0x0088b0cc),1);
  }
  cataCall(0x00490640u,3,0x5b5e4,cataM2_00652ef0,cataM2_005dba70);
  return;
}

static void cataM2_005dba70(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492060u,1,0x89a6a54);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x1f32b25,cataM2_00613940,cataM2_005dbac0);
  }
  return;
}

static void cataM2_005dbac0(void)
{
  CataValue uVar1;
  int iVar2;
  uint32_t uVar3;
  CataValue uVar4;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0xcff5860);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  if (iVar2 == 0) {
    cataCall(0x0051be70u,0);
    cataCall(0x00490640u,3,0x53a0c,cataM2_00652ef0,cataM2_00652ef0);
    return;
  }
  iVar2 = cataCall(0x00491e30u,1,0x5137b);
  if (iVar2 == 0) {
    iVar2 = -1;
    if (cataScriptMessageCount() != 0) {
      iVar2 = cataScriptMessageCount();
    }
joined_r0x005dbb39:
    if (iVar2 < 0) goto LAB_005dbc54;
    uVar3 = cataCall(0x00417ec0u,2,cataTeam(),iVar2);
    if (uVar3 < 0x67ae1d3) {
      if (uVar3 == 0x67ae1d2) {
        uVar1 = cataCall(0x00490bb0u,1,0x117ca33);
        uVar1 = cataCall(0x00490af0u,1,0x7ec6887,uVar1);
        cataCall(0x00492db0u,2,uVar1);
        uVar1 = cataCall(0x00490bb0u,1,0x117ca33);
        uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,uVar1);
        cataCall(0x00495560u,2,uVar1);
        uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,cataScriptText(0x0088b7e4));
        uVar1 = cataCall(0x00490bb0u,1,0x546975e,uVar1);
        iVar2 = cataCall(0x00495690u,3,uVar1);
        if (iVar2 == 0) {
          cataCall(0x00496eb0u,2,0x1000116b,0);
          cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
          return;
        }
        cataCall(0x00491d70u,2,0x89a6a54,3);
        cataCall(0x00491f50u,1,0x89a6a54);
        cataCall(0x00491d70u,2,0x5137b,1);
        cataCall(0x00497420u,1,0x5a);
        uVar1 = 0x9640805;
        goto LAB_005dc058;
      }
      if (uVar3 < 0x2e80a03) {
        if (uVar3 == 0x2e80a02) {
          cataCall(0x00491f00u,2,0x89a6a54,3);
          cataCall(0x00491f50u,1,0x89a6a54);
          cataCall(0x00491d70u,2,0x5137b,1);
          cataCall(0x00497580u,1,0x5a);
          uVar1 = 0xe80a041;
          goto LAB_005dc058;
        }
        if (uVar3 < 0x479e15) {
          if (uVar3 == 0x479e14) {
            iVar2 = cataCall(0x00496d40u,1,cataScriptText(0x0089069c));
            if ((iVar2 == 0) || (iVar2 = cataCall(0x00496d40u,1,cataScriptText(0x008906ac)), iVar2 == 0)) {
              cataCall(0x00496eb0u,2,0x1000216c,0);
            }
            cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
            return;
          }
          if (uVar3 != 0) {
            if (uVar3 != 0x4ce34) goto LAB_005dbc2b;
            cataCall(0x00491f00u,2,0x89a6a54,3);
            cataCall(0x00491f50u,1,0x89a6a54);
            cataCall(0x00497580u,1,0x5a);
            cataCall(0x00491d70u,2,0x5137b,1);
            uVar1 = 0x4ce352;
            goto LAB_005dc058;
          }
          goto LAB_005dbc3b;
        }
        if (uVar3 == 0x1a8a423) {
          cataCall(0x00496eb0u,2,0x1000216a,0);
          cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
          return;
        }
      }
      else {
        if (uVar3 < 0x4a38955) {
          if (uVar3 == 0x4a38954) {
            cataCall(0x00491d70u,2,0x5137b,1);
            cataCall(0x00496eb0u,2,0x10001174,0);
            cataCall(0x00490640u,3,0x4a5e094,cataM2_005f4730,cataM2_00624aa0);
            return;
          }
          if (uVar3 != 0x471a631) {
            if (uVar3 != 0x471a633) goto LAB_005dbc2b;
            cataCall(0x00491f00u,2,0x89a6a54,3);
            cataCall(0x00491f50u,1,0x89a6a54);
            cataCall(0x00491d70u,2,0x5137b,1);
            cataCall(0x00497580u,1,0x5a);
            uVar1 = cataCall(0x004907a0u,1,0xa8a880c,0x71a6331);
            cataCall(0x00494a00u,2,uVar1);
            uVar1 = 0x588674;
            uVar4 = 0x596d859;
            goto LAB_005dc05d;
          }
          cataCall(0x00491d70u,2,0x5137b,1);
          cataCall(0x00496eb0u,2,0x100010e1,0);
          uVar4 = 0x71a6312;
          goto LAB_005dc8c5;
        }
        if (uVar3 == 0x5138954) {
          cataCall(0x00491d70u,2,0x5137b,1);
          cataCall(0x00496eb0u,2,0x10001172,0);
          cataCall(0x00490640u,3,0x4a5e094,cataM2_005f4730,cataM2_00624aa0);
          return;
        }
      }
    }
    else if (uVar3 < 0x9502d36) {
      if (uVar3 == 0x9502d35) {
        cataCall(0x00496eb0u,2,0x8001351,0);
        uVar4 = 0xfa02d65;
        uVar1 = 0x98ddc43;
        goto LAB_005dc8ca;
      }
      if (0x927ec34 < uVar3) {
        if (uVar3 != 0x93100e4) goto LAB_005dbc2b;
        cataCall(0x00491f00u,2,0x89a6a54,3);
        cataCall(0x00491f50u,1,0x89a6a54);
        cataCall(0x00497630u,1,0x5a);
        cataCall(0x00491d70u,2,0x5137b,1);
        uVar1 = 0x275c542;
        goto LAB_005dc058;
      }
      if (uVar3 == 0x927ec34) {
        cataCall(0x00496eb0u,2,0x10001173,0);
        uVar1 = cataCall(0x004907a0u,1,0x76768e2,0x7cd7fde);
        cataCall(0x00494a00u,2,uVar1);
        uVar1 = cataCall(0x004907a0u,1,0x98ddc43,0x927ec34);
        cataCall(0x00494a00u,2,uVar1);
        cataCall(0x00495ea0u,1,cataScriptText(0x008c8ad8));
        cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
        return;
      }
      if (uVar3 == 0x8970534) {
        cataCall(0x00496eb0u,2,0x10002175,0);
        uVar4 = 0x8970534;
        uVar1 = 0x98ddc43;
        goto LAB_005dc8ca;
      }
      if (uVar3 == 0x8a89709) {
        cataCall(0x00491f00u,2,0x89a6a54,3);
        cataCall(0x00491f50u,1,0x89a6a54);
        cataCall(0x00491d70u,2,0x5137b,1);
        cataCall(0x00497580u,1,0x5a);
        cataCall(0x00491d30u,2,0x8d8b9e42,1);
        uVar1 = cataCall(0x004907a0u,1,0xa8a880c,0xa897022);
        cataCall(0x00494a00u,2,uVar1);
        uVar1 = cataCall(0x004907a0u,1,0xdbdde93,0x4c66683);
        cataCall(0x00494a00u,2,uVar1);
        uVar1 = 0x469857b;
        uVar4 = 0x761a0e5;
        goto LAB_005dc05d;
      }
    }
    else {
      if (uVar3 == 0xa6345b7) {
        cataCall(0x00491f00u,2,0x89a6a54,3);
        cataCall(0x00491f50u,1,0x89a6a54);
        cataCall(0x00491d70u,2,0x5137b,1);
        cataCall(0x00497420u,1,0x5a);
        uVar1 = 0x71a6332;
        goto LAB_005dc058;
      }
      if (uVar3 == 0xa7c9b35) {
        cataCall(0x00497000u,1,1);
        cataCall(0x00496eb0u,2,0x100020e7,0);
        cataCall(0x00491d70u,2,0x5137b,1);
        cataCall(0x004920e0u,1,0x89a6a54);
        uVar1 = cataCall(0x004907a0u,1,0x98ddc43,0xa7c9b35);
        cataCall(0x00494a00u,2,uVar1);
        uVar1 = cataCall(0x004907a0u,1,0xa8a880c,0x7c9b3c2);
        cataCall(0x00494a00u,2,uVar1);
        cataCall(0x00490640u,3,0x53a0c,cataM2_00652ef0,cataM2_00652ef0);
        return;
      }
      if (uVar3 == 0xd8bd352) {
        cataCall(0x00496eb0u,2,0x1000216d,0);
        cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
        return;
      }
    }
LAB_005dbc2b:
    cataCall(0x00417e10u,3,cataTeam(),cataTeam(),uVar3);
LAB_005dbc3b:
    if ((cataScriptMessageCount() == 0) || (cataScriptMessageCount() == 0))
    goto LAB_005dbc54;
    iVar2 = iVar2 + -1;
    goto joined_r0x005dbb39;
  }
  iVar2 = -1;
  if (cataScriptMessageCount() != 0) {
    iVar2 = cataScriptMessageCount();
  }
joined_r0x005dc18b:
  if (iVar2 < 0) {
LAB_005dbc54:
    iVar2 = cataCall(0x00491e30u,1,0x443cca3);
    if ((iVar2 == 0) && (iVar2 = cataCall(0x004920a0u,1,0x4617a78), iVar2 != 0)) {
      cataCall(0x00491d30u,2,0x443cca3,1);
      uVar1 = cataCall(0x004907a0u,1,0x596d859,0x4617a78);
      cataCall(0x00494a00u,2,uVar1);
    }
    return;
  }
  uVar3 = cataCall(0x00417ec0u,2,cataTeam(),iVar2);
  if (uVar3 < 0x71a6332) {
    if (uVar3 == 0x71a6331) {
      cataCall(0x00491f00u,2,0x89a6a54,3);
      cataCall(0x00491d70u,2,0x79d69c2,1);
      cataCall(0x00491d70u,2,0x32243d3,1);
      cataCall(0x00491d70u,2,0xfdbd41e,1);
      cataCall(0x00497ad0u,0);
      cataCall(0x00497a90u,0);
      cataCall(0x00491d70u,2,0xca04b13,1);
      cataCall(0x00491d70u,2,0x68087c5,1);
      cataCall(0x00491d70u,2,0x5137b,0);
      iVar2 = cataCall(0x00496c80u,1,0);
      if (iVar2 == 0) {
        cataCall(0x00496bc0u,1,1);
      }
      uVar1 = cataCall(0x004908c0u,1,0x596d859);
      cataCall(0x0049a2a0u,1,uVar1);
      uVar1 = cataCall(0x004907e0u,1,0x596d859);
      cataCall(0x004963a0u,2,cataScriptText(0x008c8aa4),uVar1);
      cataCall(0x00496eb0u,2,0x1000116e,0);
      cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
      return;
    }
    if (uVar3 < 0x275c543) {
      if (uVar3 == 0x275c542) {
        cataCall(0x00496eb0u,2,0x100010df,0);
        uVar1 = cataCall(0x004907a0u,1,0x98ddc43,0x4ae24);
        cataCall(0x00494a00u,2,uVar1);
        uVar4 = 0x479e04;
        goto LAB_005dc8c5;
      }
      if (0x4ca10 < uVar3) {
        if (uVar3 != 0x479e04) {
          if (uVar3 == 0x4ce352) {
            cataCall(0x00497ad0u,0);
            cataCall(0x00497a90u,0);
            cataCall(0x00491d70u,2,0xca04b13,1);
            cataCall(0x00491d70u,2,0x68087c5,1);
            iVar2 = cataCall(0x00496c80u,1,0);
            if (iVar2 == 0) {
              cataCall(0x00496bc0u,1,1);
            }
            cataCall(0x00491f00u,2,0x89a6a54,3);
            cataCall(0x00491d70u,2,0x79d69c2,1);
            cataCall(0x00491d70u,2,0x32243d3,1);
            cataCall(0x00491d70u,2,0xfdbd41e,1);
            cataCall(0x00491d70u,2,0x5137b,0);
            cataCall(0x00498dd0u,4,0x529631,0,0,0);
            cataCall(0x00498dd0u,4,0x529632,0,0,0);
            uVar1 = cataCall(0x004908c0u,1,0x893b502,20000);
            uVar1 = cataCall(0x00490b30u,1,0x529631,uVar1);
            cataCall(0x004991d0u,3,uVar1);
            uVar1 = cataCall(0x004908c0u,1,0x893b502,20000);
            uVar1 = cataCall(0x00490b30u,1,0x529632,uVar1);
            cataCall(0x004991d0u,3,uVar1);
            uVar1 = cataCall(0x004908c0u,1,0x893b502,cataScriptText(0x008c8acc));
            cataCall(0x00495e80u,2,uVar1);
            uVar1 = cataCall(0x00490b30u,1,0x529631,cataScriptText(0x008c8ac0));
            cataCall(0x00495e80u,2,uVar1);
            uVar1 = cataCall(0x00490b30u,1,0x529632,cataScriptText(0x008c8ab4));
            cataCall(0x00495e80u,2,uVar1);
            uVar1 = cataCall(0x004908c0u,1,0x893b502);
            cataCall(0x0049a2a0u,1,uVar1);
            cataCall(0x00496eb0u,2,0x10002169,0);
            cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
            return;
          }
          goto LAB_005dc299;
        }
        cataCall(0x00491d70u,2,0x5137b,0);
        cataCall(0x00496eb0u,2,0x10002168,0);
        cataCall(0x00491d30u,2,0x8f624235,1);
        uVar4 = 0x479e04;
        uVar1 = 0x98ddc43;
        goto LAB_005dc8ca;
      }
      if (uVar3 == 0x4ca10) {
        uVar1 = cataCall(0x00496190u,0);
        cataCall(0x00491d30u,2,0xf8bffc5,uVar1);
        uVar1 = cataCall(0x004961f0u,0);
        cataCall(0x00491d30u,2,0x3a1cffe,uVar1);
        uVar1 = cataCall(0x00496250u,0);
        cataCall(0x00491d30u,2,0x3d4ba05,uVar1);
        uVar1 = cataCall(0x00490bb0u,1,0xcff5860);
        cataCall(0x00496950u,1,uVar1);
        cataCall(0x004961b0u,1,0x159);
        cataCall(0x00496210u,1,3);
        cataCall(0x00496930u,1,0x9a8);
        cataCall(0x00496eb0u,2,0x100010e5,0);
        uVar4 = 0x47a89;
        goto LAB_005dc8c5;
      }
      if (uVar3 != 0) {
        if (uVar3 != 0x47a89) goto LAB_005dc299;
        cataCall(0x00491f00u,2,0x89a6a54,10);
        cataCall(0x00491d70u,2,0x79d69c2,1);
        cataCall(0x00491d70u,2,0xfdbd41e,1);
        cataCall(0x00491d70u,2,0xddfdd03,1);
        uVar1 = cataCall(0x004907e0u,1,0x893b502);
        cataCall(0x00496950u,1,uVar1);
        cataCall(0x004961b0u,1,0x1e);
        cataCall(0x00496210u,1,7);
        cataCall(0x00496930u,1,0x28a);
        cataCall(0x00496eb0u,2,0x100080e6,0);
        uVar1 = cataCall(0x004907a0u,1,0x893b502,0x47a89);
        cataCall(0x00494a00u,2,uVar1);
        uVar4 = 0xd829744;
        goto LAB_005dc8c5;
      }
      goto LAB_005dc2a9;
    }
    if (uVar3 < 0x64080f3) {
      if (uVar3 == 0x64080f2) {
        cataCall(0x00491d70u,2,0xa4e5ad3,1);
        cataCall(0x00491d70u,2,0x89a6a54,3);
        cataCall(0x00491d70u,2,0x79d69c2,1);
        cataCall(0x00491d70u,2,0x5137b,0);
        cataCall(0x00491d70u,2,0xfdbd41e,1);
        cataCall(0x00497710u,2,5000,cataScriptText(0x0074e524));
        cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
        return;
      }
      if (uVar3 != 0x469583e) {
        if (uVar3 == 0x58473f5) {
          cataCall(0x00491f00u,2,0x89a6a54,3);
          cataCall(0x00491f50u,1,0x89a6a54);
          cataCall(0x00497420u,1,0x5a);
          uVar1 = 0x469583e;
LAB_005dc058:
          uVar4 = 0xa8a880c;
LAB_005dc05d:
          uVar1 = cataCall(0x004907a0u,1,uVar4,uVar1);
          cataCall(0x00494a00u,2,uVar1);
          cataCall(0x00490640u,3,0x5b5e4,cataM2_00652ef0,cataM2_005dba70);
          return;
        }
        goto LAB_005dc299;
      }
      cataCall(0x00491f00u,2,0x89a6a54,3);
      cataCall(0x00491d70u,2,0x79d69c2,1);
      cataCall(0x00491d70u,2,0xfdbd41e,1);
      cataCall(0x00491d70u,2,0x32243d3,1);
      cataCall(0x00491d70u,2,0x5137b,0);
      cataCall(0x00497ad0u,0);
      cataCall(0x00497a90u,0);
      cataCall(0x00491d70u,2,0xca04b13,1);
      cataCall(0x00491d70u,2,0x68087c5,1);
      iVar2 = cataCall(0x00496c80u,1,0);
      if (iVar2 == 0) {
        cataCall(0x00496bc0u,1,150000);
      }
      uVar1 = cataCall(0x004908c0u,1,0x712bfb4);
      cataCall(0x0049a2a0u,1,uVar1);
      uVar1 = cataCall(0x004907e0u,1,0x712bfb4,cataScriptText(0x008c8ad8));
      cataCall(0x00495e50u,2,uVar1);
      uVar1 = cataCall(0x004907e0u,1,0x712bfb4);
      cataCall(0x004963a0u,2,cataScriptText(0x008c8aa4),uVar1);
      cataCall(0x00496eb0u,2,0x100020e3,0);
      uVar4 = 0x469583e;
      uVar1 = 0x98ddc43;
      goto LAB_005dc8ca;
    }
    if (uVar3 == 0x71a6312) {
      cataCall(0x00496eb0u,2,0x100080e2,0);
      uVar4 = 0x58473f5;
      goto LAB_005dc8c5;
    }
  }
  else if (uVar3 < 0x96bed58) {
    if (uVar3 == 0x96bed57) {
      cataCall(0x00496eb0u,2,0x10002176,0);
      cataCall(0x00496cf0u,1,cataScriptText(0x0089054c));
      cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
      return;
    }
    if (uVar3 < 0x7c9b3c3) {
      if (uVar3 == 0x7c9b3c2) {
        cataCall(0x00496eb0u,2,0x20012ca,0);
        uVar4 = 0x96bed57;
        goto LAB_005dc8c5;
      }
      if (uVar3 == 0x71a6332) {
        uVar1 = cataCall(0x004907e0u,1,0x596d859);
        cataCall(0x00496950u,1,uVar1);
        cataCall(0x00496eb0u,2,0x1000116f,0);
        uVar1 = cataCall(0x004907a0u,1,0xa8a880c,0x71a6333);
        cataCall(0x00494a00u,2,uVar1);
        cataCall(0x00491f90u,2,0x4617a78,3);
        cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
        return;
      }
      if (uVar3 == 0x71a6333) {
        cataCall(0x00491f00u,2,0x89a6a54,3);
        cataCall(0x00491d70u,2,0x79d69c2,1);
        cataCall(0x00491d70u,2,0xfdbd41e,1);
        cataCall(0x00491d70u,2,0x5137b,0);
        uVar1 = cataCall(0x004907a0u,1,0x596d859,0x4617a78);
        cataCall(0x00494a00u,2,uVar1);
        cataCall(0x00496eb0u,2,0x10002170,0);
        cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
        return;
      }
    }
    else if (uVar3 == 0x9640805) {
      cataCall(0x00454530u,0);
      uVar1 = cataCall(0x00490bb0u,1,0x117ca33,cataScriptText(0x0088b7e4));
      uVar1 = cataCall(0x00490bb0u,1,0x67ae1d2,uVar1);
      cataCall(0x00495690u,3,uVar1);
      uVar1 = cataCall(0x00496190u,0);
      cataCall(0x00491d30u,2,0xf8bffc5,uVar1);
      uVar1 = cataCall(0x004961f0u,0);
      cataCall(0x00491d30u,2,0x3a1cffe,uVar1);
      uVar1 = cataCall(0x00496250u,0);
      cataCall(0x00491d30u,2,0x3d4ba05,uVar1);
      uVar1 = cataCall(0x00490bb0u,1,0x67ae1d2);
      cataCall(0x00496950u,1,uVar1);
      cataCall(0x00496eb0u,2,0x1000116b,0);
      uVar4 = 0x64080f2;
      goto LAB_005dc8c5;
    }
  }
  else if (uVar3 < 0xe80a042) {
    if (uVar3 == 0xe80a041) {
      cataCall(0x00497ad0u,0);
      cataCall(0x00497a90u,0);
      cataCall(0x00491d70u,2,0xca04b13,1);
      cataCall(0x00491d70u,2,0x68087c5,1);
      iVar2 = cataCall(0x00496c80u,1,0);
      if (iVar2 == 0) {
        cataCall(0x00496bc0u,1,1);
      }
      uVar1 = cataCall(0x004908c0u,1,0x893b502);
      cataCall(0x0049a2a0u,1,uVar1);
      uVar1 = cataCall(0x004907e0u,1,0x893b502,cataScriptText(0x008c8a84));
      cataCall(0x00495e50u,2,uVar1);
      uVar1 = cataCall(0x004907e0u,1,0x893b502);
      cataCall(0x004963a0u,2,cataScriptText(0x008c83dc),uVar1);
      cataCall(0x00496eb0u,2,0x100020e0,0);
      uVar1 = cataCall(0x004907a0u,1,0x98ddc43,0x56a45e2);
      cataCall(0x00494a00u,2,uVar1);
      uVar1 = cataCall(0x004907a0u,1,0x893b502,0x56a45e2);
      cataCall(0x00494a00u,2,uVar1);
      uVar4 = 0xe80a042;
LAB_005dc8c5:
      uVar1 = 0xa8a880c;
LAB_005dc8ca:
      uVar1 = cataCall(0x004907a0u,1,uVar1,uVar4);
      cataCall(0x00494a00u,2,uVar1);
LAB_005dc8d8:
      cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
      return;
    }
    if (uVar3 == 0xa897022) {
      cataCall(0x00497ad0u,0);
      cataCall(0x00497a90u,0);
      cataCall(0x00491d70u,2,0xca04b13,1);
      cataCall(0x00491d70u,2,0x68087c5,1);
      iVar2 = cataCall(0x00496c80u,1,0);
      if (iVar2 == 0) {
        cataCall(0x00496bc0u,1,1);
      }
      uVar1 = cataCall(0x00490820u,1,0x4e668f3);
      cataCall(0x0049a2a0u,1,uVar1);
      uVar1 = cataCall(0x00490bb0u,1,0x4e668f3);
      cataCall(0x004963a0u,2,cataScriptText(0x008c8a94),uVar1);
      cataCall(0x00496eb0u,2,0x100020e4,0);
      uVar1 = cataCall(0x004907a0u,1,0xa8a880c,0x4ca10);
      cataCall(0x00494a00u,2,uVar1);
      uVar4 = 0x8a89709;
      uVar1 = 0x98ddc43;
      goto LAB_005dc8ca;
    }
    if (uVar3 == 0xd829744) {
      cataCall(0x00491d70u,2,0x5137b,0);
      cataCall(0x00496eb0u,2,0x10002171,0);
      cataCall(0x00496cf0u,1,cataScriptText(0x00890560));
      cataCall(0x00490640u,3,0xe8e63cc,cataM2_005f4730,cataM2_005db830);
      return;
    }
  }
  else if (uVar3 == 0xe80a042) {
    cataCall(0x00491d70u,2,0x5137b,0);
    cataCall(0x00491f00u,2,0x89a6a54,3);
    cataCall(0x00491d70u,2,0x79d69c2,1);
    cataCall(0x00491d70u,2,0xfdbd41e,1);
    cataCall(0x00497710u,2,4000,cataScriptText(0x0074e524));
    uVar1 = cataCall(0x004907e0u,1,0x893b502);
    cataCall(0x00496950u,1,uVar1);
    cataCall(0x004961b0u,1,0x13f);
    cataCall(0x00496210u,1,0xb);
    cataCall(0x00496930u,1,0x28a);
    uVar1 = cataCall(0x004907e0u,1,0x893b502,0);
    cataCall(0x004984c0u,2,uVar1);
    goto LAB_005dc8d8;
  }
LAB_005dc299:
  cataCall(0x00417e10u,3,cataTeam(),cataTeam(),uVar3);
LAB_005dc2a9:
  if ((cataScriptMessageCount() == 0) || (cataScriptMessageCount() == 0))
  goto LAB_005dbc54;
  iVar2 = iVar2 + -1;
  goto joined_r0x005dc18b;
}

static void cataM2_005dcbd0(void)
{
  int iVar1;
  uint32_t uVar2;
  CataValue uVar3;
  const char *pcVar4;
  const char *puVar5;
  const char *puVar6;
  
  cataScriptScope(0);
  iVar1 = -1;
  if (cataScriptMessageCount() != 0) {
    iVar1 = cataScriptMessageCount();
  }
  for (; -1 < iVar1; iVar1 = iVar1 + -1) {
    uVar2 = cataCall(0x00417ec0u,2,cataTeam(),iVar1);
    if (uVar2 < 0x8a8970a) {
      if (uVar2 == 0x8a89709) {
        puVar6 = cataScriptLocalizedText(0x008c8540);
        puVar5 = cataScriptLocalizedText(0x008c8534);
        pcVar4 = cataScriptText(0x008c8b18);
LAB_005dce81:
        cataCall(0x00494e70u,3,pcVar4,puVar5,puVar6);
      }
      else if (uVar2 < 0x479e15) {
        if (uVar2 == 0x479e14) {
          cataCall(0x00494f60u,1,cataScriptText(0x008c8b38));
        }
        else if (uVar2 != 0) {
          if (uVar2 == 0x4ae24) {
            puVar6 = cataScriptLocalizedText(0x008c84e0);
            puVar5 = cataScriptLocalizedText(0x008c84d4);
            pcVar4 = cataScriptText(0x008c8b28);
          }
          else {
            if (uVar2 != 0x479e04) goto LAB_005dce49;
            puVar6 = cataScriptLocalizedText(0x008c84f8);
            puVar5 = cataScriptLocalizedText(0x008c84ec);
            pcVar4 = cataScriptText(0x008c8b38);
          }
          goto LAB_005dce81;
        }
      }
      else if (uVar2 == 0x469583e) {
        cataCall(0x00494e70u,3,cataScriptText(0x008c8afc),cataScriptLocalizedText(0x008c851c),
                     cataScriptLocalizedText(0x008c8528));
        uVar3 = cataCall(0x004907e0u,1,0x712bfb4);
        cataCall(0x004963e0u,2,cataScriptText(0x008c83b8),uVar3);
      }
      else if (uVar2 == 0x56a45e2) {
        cataCall(0x00495ea0u,1,cataScriptText(0x008c8acc));
        cataCall(0x00495ea0u,1,cataScriptText(0x008c8ac0));
        cataCall(0x00495ea0u,1,cataScriptText(0x008c8ab4));
        cataCall(0x00494f60u,1,cataScriptText(0x008c8b28));
        cataCall(0x00494e70u,3,cataScriptText(0x008c8b08),cataScriptLocalizedText(0x008c8504)
                     ,cataScriptLocalizedText(0x008c8510));
        uVar3 = cataCall(0x004907e0u,1,0x893b502);
        cataCall(0x004963e0u,2,cataScriptText(0x008c83b8),uVar3);
      }
      else {
        if (uVar2 != 0x8970534) goto LAB_005dce49;
        cataCall(0x00491d30u,2,0x85e36f74,1);
        cataCall(0x00494f60u,1,cataScriptText(0x008c8b18));
      }
    }
    else if (uVar2 < 0xd2247a5) {
      if (uVar2 == 0xd2247a4) {
        cataCall(0x0051be70u,0);
      }
      else if (uVar2 == 0x927ec34) {
        cataCall(0x00491d30u,2,0x83c491e4,1);
        cataCall(0x004964e0u,1,cataScriptText(0x008c83b8));
        cataCall(0x00494f60u,1,cataScriptText(0x008c8afc));
      }
      else if (uVar2 == 0xa40f2f4) {
        cataCall(0x004964e0u,1,cataScriptText(0x008c83b8));
        cataCall(0x00494f60u,1,cataScriptText(0x008c8b08));
        cataCall(0x00495ea0u,1,cataScriptText(0x008c8a84));
      }
      else {
        if (uVar2 != 0xa7c9b35) goto LAB_005dce49;
        cataCall(0x004d6480u,0);
        cataCall(0x00494e70u,3,cataScriptText(0x008c7f90),cataScriptLocalizedText(0x008c854c),
                     cataScriptLocalizedText(0x008c8558));
        cataCall(0x00497290u,2,2,cataScriptLocalizedText(0x008c84b0));
      }
    }
    else {
      if (uVar2 == 0xfa02d65) {
        puVar6 = cataScriptLocalizedText(0x008c84c8);
        puVar5 = cataScriptLocalizedText(0x008c84bc);
        pcVar4 = cataScriptText(0x008c8ae8);
        goto LAB_005dce81;
      }
      if (uVar2 == 0xfa02d75) {
        cataCall(0x00494f60u,1,cataScriptText(0x008c8ae8));
      }
      else {
LAB_005dce49:
        cataCall(0x00417e10u,3,cataTeam(),cataTeam(),uVar2);
      }
    }
    if ((cataScriptMessageCount() == 0) || (cataScriptMessageCount() == 0))
    break;
  }
  iVar1 = cataCall(0x00491e30u,1,0x443cca4);
  if (((iVar1 == 0) && (iVar1 = cataCall(0x00491e30u,1,0x83c491e4), iVar1 != 0)) &&
     (iVar1 = cataCall(0x00491e30u,1,0x85e36f74), iVar1 != 0)) {
    cataCall(0x00491d30u,2,0x443cca4,1);
    uVar3 = cataCall(0x004907a0u,1,0xa8a880c,0xa7c9b35);
    cataCall(0x00494a00u,2,uVar3);
  }
  return;
}

static void cataM2_005dcf10(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x588674);
  if (iVar1 != 0) {
    cataCall(0x00498dd0u,4,0x68cb840,0,0,0);
    uVar2 = cataCall(0x004908c0u,1,0x712bfb4,50000);
    uVar2 = cataCall(0x00490b30u,1,0x68cb840,uVar2);
    cataCall(0x004991d0u,3,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x8a89709);
    cataCall(0x00494a00u,2,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x761a0e5,0x588674);
    cataCall(0x00494a00u,2,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xdbdde91,0x588674);
    cataCall(0x00494a00u,2,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xdbdde92,0x588674);
    cataCall(0x00494a00u,2,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xdbdde93,0x588674);
    cataCall(0x00494a00u,2,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x54c9664,0x588674);
    cataCall(0x00494a00u,2,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x76768e2,0x588674);
    cataCall(0x00494a00u,2,uVar2);
  }
  return;
}

static void cataM2_005dd040(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x0049aae0u,2,cataShips(),5);
  uVar1 = cataCall(0x00490bb0u,1,0x4e668f3,cataShips());
  cataCall(0x00495bd0u,2,uVar1);
  cataCall(0x00493490u,1,0x80);
  cataCall(0x00493800u,0);
  return;
}

static void cataM2_005dd0a0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x443cca5);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492ce0u,0);
    if (iVar1 == 0) {
      cataCall(0x00491d30u,2,0x443cca5,1);
      cataCall(0x00491d30u,2,0x86a92424,1);
      cataCall(0x00490640u,3,0x53a0c,cataM2_00652ef0,cataM2_005dd2d0);
      return;
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443cca6);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00491e30u,1,0x83c491e4);
    if (iVar1 == 0) {
      iVar1 = cataCall(0x00492120u,1,0x7cd7fde);
      if (iVar1 == 0) goto LAB_005dd16a;
    }
    cataCall(0x00491d30u,2,0x443cca6,1);
    cataCall(0x00491d30u,2,0x86a92424,1);
    cataCall(0x00493800u,0);
    cataCall(0x00490640u,3,0x78a9955,cataM2_005dd250,cataM2_005dd280);
    return;
  }
LAB_005dd16a:
  iVar1 = cataCall(0x00492120u,1,0x588674);
  if (iVar1 != 0) {
    uVar2 = cataCall(0x00490b30u,1,0x68cb840);
    cataCall(0x00493600u,1,uVar2);
    cataCall(0x00490640u,3,0xa7c9b35,cataM2_00652ef0,cataM2_005dd1b0);
  }
  return;
}

static void cataM2_005dd1b0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x1f6fd13);
  if (iVar1 != 0) {
    cataCall(0x00491d30u,2,0x86a92424,1);
    cataCall(0x00493800u,0);
    cataCall(0x00490640u,3,0x53a0c,cataM2_00652ef0,cataM2_005dd2d0);
    return;
  }
  iVar1 = cataCall(0x00492ce0u,0);
  if (iVar1 == 0) {
    cataCall(0x00491d30u,2,0x86a92424,1);
    uVar2 = cataCall(0x00490bb0u,1,0x4e668f3);
    iVar1 = cataCall(0x00492d20u,1,uVar2);
    if (iVar1 == 0) {
      cataCall(0x00490640u,3,0x53a0c,cataM2_00652ef0,cataM2_005dd2d0);
    }
  }
  return;
}

static void cataM2_005dd250(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491f90u,2,0x4943d,10);
  return;
}

static void cataM2_005dd280(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004920a0u,1,0x4943d);
  if (iVar1 != 0) {
    cataCall(0x00497850u,1,cataShips());
    cataCall(0x00490640u,3,0x53a0c,cataM2_00652ef0,cataM2_005dd2d0);
  }
  return;
}

static void cataM2_005dd2d0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x443cca7);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x00490bb0u,1,0x857bce3);
    uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,uVar2);
    iVar1 = cataCall(0x00498630u,2,uVar2);
    if (iVar1 == 0) {
      uVar2 = cataCall(0x00490bb0u,1,0x4e668f3);
      iVar1 = cataCall(0x00492d20u,1,uVar2);
      if (iVar1 == 0) {
        cataCall(0x00491d30u,2,0x443cca7,1);
        uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x8970534);
        cataCall(0x00494a00u,2,uVar2);
      }
    }
  }
  return;
}

static void cataM2_005dd370(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x0049aae0u,2,cataShips(),5);
  uVar1 = cataCall(0x00490bb0u,1,0x4e668f3,cataShips());
  cataCall(0x00495bd0u,2,uVar1);
  cataCall(0x00491d30u,2,0x5137b,1);
  return;
}

static void cataM2_005dd3d0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x588674);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x479e04,cataM2_0065f6f0,cataM2_005dd4a0);
    return;
  }
  iVar1 = cataCall(0x00491e30u,1,0x5137b);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x00490bb0u,1,0xe89ea93);
    uVar2 = cataCall(0x004907a0u,1,0x76768e2,uVar2);
    iVar1 = cataCall(0x00494450u,2,uVar2);
    if (iVar1 != 0) {
      iVar1 = cataCall(0x00492ce0u,0);
      if (iVar1 == 0) {
        cataCall(0x00490640u,3,0x479e04,cataM2_0065f6f0,cataM2_005dd4a0);
        return;
      }
    }
    iVar1 = cataCall(0x00491e30u,1,0x86a92424);
    if (iVar1 != 0) {
      cataCall(0x00490640u,3,0x61df215,cataM2_005de270,cataM2_00652ef0);
    }
  }
  return;
}

static void cataM2_005dd4a0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004920a0u,1,0x479e04);
  if (iVar1 != 0) {
    cataCall(0x00495eb0u,0);
    uVar2 = cataCall(0x004907e0u,1,0x76768e2,1);
    uVar2 = cataCall(0x00492910u,2,4,8,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x9379e54,cataScriptText(0x0088b3c4),uVar2);
    cataCall(0x00499c70u,5,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x9379e54,5);
    cataCall(0x0049aae0u,2,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x9379e54);
    cataCall(0x00493cf0u,2,cataTeam(),uVar2);
  }
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 != 0) {
    cataCall(0x00491d70u,2,0x5137b,0);
    cataCall(0x00490640u,3,0x4c9664,cataM2_005dd570,cataM2_005dd5b0);
  }
  return;
}

static void cataM2_005dd570(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00492870u,0);
  uVar1 = cataCall(0x004907e0u,1,0x76768e2);
  cataCall(0x00492960u,1,uVar1);
  return;
}

static void cataM2_005dd5b0(void)
{
  uint32_t uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00492ae0u,1,cataShips());
  if ((uVar1 & 2) == 0) {
    cataCall(0x00490640u,3,0x61df215,cataM2_005de270,cataM2_00652ef0);
  }
  return;
}

static void cataM2_005dd5f0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00491d30u,2,0x5137b,1);
  cataCall(0x0049aae0u,2,cataShips(),5);
  uVar1 = cataCall(0x00490bb0u,1,0x4e668f3,cataShips());
  cataCall(0x00495bd0u,2,uVar1);
  cataCall(0x00493490u,1,0x80);
  cataCall(0x00493800u,0);
  cataCall(0x00490640u,3,0xa7c9b35,cataM2_00652ef0,cataM2_005dd730);
  return;
}

static void cataM2_005dd670(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x5137b);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492ce0u,0);
    if (iVar1 == 0) {
      cataCall(0x00490640u,3,0x479e04,cataM2_005ddad0,cataM2_005ddb10);
      return;
    }
    iVar1 = cataCall(0x00492b20u,0);
    if (iVar1 < 0xf) {
      cataCall(0x00490640u,3,0x4937b,cataM2_005ddc60,cataM2_005ddca0);
      return;
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443cca8);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00491e30u,1,0x86a92424);
    if (iVar1 != 0) {
      cataCall(0x00491d30u,2,0x443cca8,1);
      cataCall(0x00490640u,3,0x61df215,cataM2_005de270,cataM2_00652ef0);
    }
  }
  return;
}

static void cataM2_005dd730(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x588674);
  if (iVar1 != 0) {
    cataCall(0x00491d70u,2,0x5137b,0);
    uVar2 = cataCall(0x00490b30u,1,0x68cb840);
    cataCall(0x00493600u,1,uVar2);
    cataCall(0x00490640u,3,0x5b5e4,cataM2_005dd790,cataM2_005dd800);
  }
  return;
}

static void cataM2_005dd790(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x004907e0u,1,0x761a0e5);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  if (iVar2 == 0) {
    cataCall(0x00492870u,0);
    uVar1 = 0x76768e2;
  }
  else {
    cataCall(0x004927e0u,0);
    uVar1 = 0x761a0e5;
  }
  uVar1 = cataCall(0x004907e0u,1,uVar1);
  cataCall(0x00492960u,1,uVar1);
  uVar1 = cataCall(0x00492910u,2,10,0x14);
  cataCall(0x00491f90u,2,0x5b5e4,uVar1);
  return;
}

static void cataM2_005dd800(void)
{
  uint32_t uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00492ae0u,1,cataShips());
  if ((uVar1 & 2) == 0) {
    cataCall(0x00490640u,3,0x469857b,cataM2_005dd870,cataM2_005dda60);
    return;
  }
  iVar2 = cataCall(0x00492060u,1,0x5b5e4);
  if (iVar2 != 0) {
    cataCall(0x00490640u,3,0x5b5e4,cataM2_005dd790,cataM2_005dd800);
  }
  return;
}

static void cataM2_005dd870(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x6896db5);
  cataCall(0x00492d10u,1,uVar1);
  iVar2 = cataCall(0x00492910u,2,0,9);
  if (iVar2 < 7) {
    uVar1 = cataCall(0x004908c0u,1,0x712bfb4);
  }
  else {
    uVar1 = cataCall(0x004909f0u,0);
  }
  // Retail pushes this radius before either branch chooses the search center.
  uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,uVar1,25000);
  uVar1 = cataCall(0x00490bb0u,1,0x6896db5,uVar1);
  cataCall(0x00495a50u,4,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
  uVar1 = cataCall(0x004907e0u,1,0x761a0e5,uVar1);
  iVar2 = cataCall(0x00498630u,2,uVar1);
  if (iVar2 != 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
    uVar1 = cataCall(0x00490bb0u,1,0x6896db5,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  uVar1 = cataCall(0x004907e0u,1,0x712bfb4);
  uVar1 = cataCall(0x00490bb0u,1,0x6896db5,uVar1);
  cataCall(0x00495c40u,2,uVar1);
  uVar1 = cataCall(0x004909f0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x6896db5,uVar1);
  cataCall(0x0049a100u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x2091af3);
  uVar1 = cataCall(0x004907a0u,1,0x761a0e5,uVar1);
  iVar2 = cataCall(0x00494450u,2,uVar1);
  if (iVar2 != 0) {
    uVar1 = cataCall(0x004909f0u,0);
    uVar1 = cataCall(0x00490bb0u,1,0x2091af3,uVar1);
    cataCall(0x0049a100u,2,uVar1);
    uVar1 = cataCall(0x00490bb0u,1,0x2091af3);
    uVar1 = cataCall(0x00490bb0u,1,0x6896db5,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
  iVar2 = cataCall(0x00494510u,1,uVar1);
  if (iVar2 != 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
    uVar1 = cataCall(0x00490bb0u,1,0x6896db5,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  cataCall(0x004927e0u,0);
  uVar1 = cataCall(0x004909f0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x6896db5,uVar1);
  cataCall(0x0049a100u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x6896db5);
  cataCall(0x00492190u,1,uVar1);
  cataCall(0x004920e0u,1,0x469857b);
  uVar1 = cataCall(0x00492910u,2,0x14,0x28);
  cataCall(0x00491f90u,2,0x469857b,uVar1);
  return;
}

static void cataM2_005dda60(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00499870u,1,cataShips());
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x5b5e4,cataM2_005dd790,cataM2_005dd800);
    return;
  }
  iVar1 = cataCall(0x004920a0u,1,0x469857b);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x469857b,cataM2_005dd870,cataM2_005dda60);
  }
  return;
}

static void cataM2_005ddad0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00492910u,2,0x28,0x3c);
  cataCall(0x00491f90u,2,0x479e04,uVar1);
  cataCall(0x00491d70u,2,0x5137b,1);
  return;
}

static void cataM2_005ddb10(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492060u,1,0x479e04);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492120u,1,0xaa768c5);
    if (iVar1 == 0) goto LAB_005ddc2d;
  }
  cataCall(0x004920e0u,1,0x479e04);
  cataCall(0x00495eb0u,0);
  uVar2 = cataCall(0x004907e0u,1,0x76768e2,1);
  uVar2 = cataCall(0x00492910u,2,5,8,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x9379e54,cataScriptText(0x0088b3ec),uVar2);
  cataCall(0x00499c70u,5,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x9379e54,5);
  cataCall(0x0049aae0u,2,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x9379e54);
  cataCall(0x00493cf0u,2,cataTeam(),uVar2);
  uVar2 = cataCall(0x004907e0u,1,0x76768e2,1);
  uVar2 = cataCall(0x00492910u,2,1,3,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x9379e54,cataScriptText(0x0088b3c4),uVar2);
  cataCall(0x00499c70u,5,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x9379e54,5);
  cataCall(0x0049aae0u,2,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x9379e54);
  cataCall(0x00493cf0u,2,cataTeam(),uVar2);
LAB_005ddc2d:
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 != 0) {
    cataCall(0x00491d70u,2,0x5137b,0);
    cataCall(0x00490640u,3,0x5b5e4,cataM2_005dd790,cataM2_005dd800);
  }
  return;
}

static void cataM2_005ddc60(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00491d70u,2,0x5137b,1);
  uVar1 = cataCall(0x004907a0u,1,0x76768e2);
  cataCall(0x00494630u,1,uVar1);
  return;
}

static void cataM2_005ddca0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00494940u,0);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x506a278,cataM2_00608010,cataM2_005ddce0);
  }
  return;
}

static void cataM2_005ddce0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 != 0) {
    cataCall(0x00491d70u,2,0x5137b,0);
    cataCall(0x00490640u,3,0x5b5e4,cataM2_005dd790,cataM2_005dd800);
  }
  return;
}

static void cataM2_005ddd30(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00491d30u,2,0x5137b,1);
  cataCall(0x0049aae0u,2,cataShips(),5);
  uVar1 = cataCall(0x00490bb0u,1,0x4e668f3,cataShips());
  cataCall(0x00495bd0u,2,uVar1);
  cataCall(0x00493490u,1,0x80);
  cataCall(0x00493800u,0);
  cataCall(0x00490640u,3,0xa7c9b35,cataM2_00652ef0,cataM2_005dde90);
  return;
}

static void cataM2_005dddb0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x5137b);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492120u,1,0x4c66683);
    if (iVar1 != 0) {
      cataCall(0x00490640u,3,0x469857b,cataM2_005ddef0,cataM2_005ddff0);
      return;
    }
    iVar1 = cataCall(0x00492ce0u,0);
    if (iVar1 == 0) {
      cataCall(0x00490640u,3,0x479e04,cataM2_005de050,cataM2_005de090);
      return;
    }
    iVar1 = cataCall(0x00492b20u,0);
    if (iVar1 < 0x19) {
      cataCall(0x00490640u,3,0x4937b,cataM2_005ddc60,cataM2_005de1e0);
      return;
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443cca9);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00491e30u,1,0x86a92424);
    if (iVar1 != 0) {
      cataCall(0x00491d30u,2,0x443cca9,1);
      cataCall(0x00490640u,3,0x61df215,cataM2_005de270,cataM2_00652ef0);
    }
  }
  return;
}

static void cataM2_005dde90(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x588674);
  if (iVar1 != 0) {
    uVar2 = cataCall(0x00490b30u,1,0x68cb840);
    cataCall(0x00493600u,1,uVar2);
    cataCall(0x00491d70u,2,0x5137b,0);
    cataCall(0x00490640u,3,0x469857b,cataM2_005ddef0,cataM2_005ddff0);
  }
  return;
}

static void cataM2_005ddef0(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x004909f0u,0,20000);
  uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  iVar2 = cataCall(0x00495a50u,4,uVar1);
  if (iVar2 == 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x4a2a29);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
  iVar2 = cataCall(0x00494510u,1,uVar1);
  if (iVar2 != 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  uVar1 = cataCall(0x004909f0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x0049a100u,2,uVar1);
  cataCall(0x00492780u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492190u,1,uVar1);
  cataCall(0x004920e0u,1,0x469857b);
  uVar1 = cataCall(0x00492910u,2,0x14,0x28);
  cataCall(0x00491f90u,2,0x469857b,uVar1);
  return;
}

static void cataM2_005ddff0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00499870u,1,cataShips());
  if ((iVar1 == 0) && (iVar1 = cataCall(0x00492060u,1,0x469857b), iVar1 == 0)) {
    return;
  }
  cataCall(0x004920e0u,1,0x469857b);
  cataCall(0x00490640u,3,0x469857b,cataM2_005ddef0,cataM2_005ddff0);
  return;
}

static void cataM2_005de050(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00491d70u,2,0x5137b,1);
  uVar1 = cataCall(0x00492910u,2,0x28,0x3c);
  cataCall(0x00491f90u,2,0x479e04,uVar1);
  return;
}

static void cataM2_005de090(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492060u,1,0x479e04);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492120u,1,0xaa768c5);
    if (iVar1 == 0) goto LAB_005de1ad;
  }
  cataCall(0x004920e0u,1,0x479e04);
  cataCall(0x00495eb0u,0);
  uVar2 = cataCall(0x004907e0u,1,0x76768e2,1);
  uVar2 = cataCall(0x00492910u,2,5,10,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x9379e54,cataScriptText(0x0088b3ec),uVar2);
  cataCall(0x00499c70u,5,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x9379e54,5);
  cataCall(0x0049aae0u,2,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x9379e54);
  cataCall(0x00493cf0u,2,cataTeam(),uVar2);
  uVar2 = cataCall(0x004907e0u,1,0x76768e2,1);
  uVar2 = cataCall(0x00492910u,2,1,4,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x9379e54,cataScriptText(0x0088b3c4),uVar2);
  cataCall(0x00499c70u,5,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x9379e54,5);
  cataCall(0x0049aae0u,2,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x9379e54);
  cataCall(0x00493cf0u,2,cataTeam(),uVar2);
LAB_005de1ad:
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 != 0) {
    cataCall(0x00491d70u,2,0x5137b,0);
    cataCall(0x00490640u,3,0x469857b,cataM2_005ddef0,cataM2_005ddff0);
  }
  return;
}

static void cataM2_005de1e0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00494940u,0);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x506a278,cataM2_00608010,cataM2_005de220);
  }
  return;
}

static void cataM2_005de220(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 != 0) {
    cataCall(0x00491d70u,2,0x5137b,1);
    cataCall(0x00490640u,3,0x469857b,cataM2_005ddef0,cataM2_005ddff0);
  }
  return;
}

static void cataM2_005de270(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00499570u,1,cataShips());
  if (iVar1 == 0) {
    cataCall(0x00498270u,1,cataShips());
    return;
  }
  uVar2 = cataCall(0x004907a0u,1,0x61df215,cataShips());
  cataCall(0x00493cf0u,2,uVar2);
  cataCall(0x00490640u,3,0x53a0c,cataM2_0065f6f0,cataM2_00652ef0);
  return;
}

static void cataM2_005de2e0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x0049aae0u,2,cataShips(),5);
  cataCall(0x00491d30u,2,0x5137b,1);
  cataCall(0x00498c10u,1,cataScriptText(0x0089069c));
  uVar1 = cataCall(0x00490bb0u,1,0x4e668f3,cataShips());
  cataCall(0x00495bd0u,2,uVar1);
  cataCall(0x00498c70u,1,cataShips());
  uVar1 = cataCall(0x004907e0u,1,0x76768e2);
  cataCall(0x0049a460u,1,uVar1);
  cataCall(0x00493490u,1,0x80);
  cataCall(0x00493800u,0);
  cataCall(0x00490640u,3,0xa7c9b35,cataM2_00652ef0,cataM2_005de440);
  return;
}

static void cataM2_005de380(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x5137b);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492120u,1,0x469857b);
    if (iVar1 != 0) {
      cataCall(0x00490640u,3,0x761a5e5,cataM2_005de5a0,cataM2_005de5e0);
      return;
    }
    iVar1 = cataCall(0x00492ce0u,0);
    if (iVar1 == 0) {
      cataCall(0x00490640u,3,0x479e04,cataM2_005de870,cataM2_005de920);
      return;
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443ccaa);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00491e30u,1,0x86a92424);
    if (iVar1 != 0) {
      cataCall(0x00491d30u,2,0x443ccaa,1);
      cataCall(0x00490640u,3,0x61df215,cataM2_005dea60,cataM2_00652ef0);
    }
  }
  return;
}

static void cataM2_005de440(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x588674);
  if (iVar1 != 0) {
    cataCall(0x00491d70u,2,0x5137b,0);
    uVar2 = cataCall(0x00490b30u,1,0x68cb840);
    cataCall(0x00493600u,1,uVar2);
    cataCall(0x00490640u,3,0x5b5e4,cataM2_005de4a0,cataM2_005de4e0);
  }
  return;
}

static void cataM2_005de4a0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00492840u,0);
  uVar1 = cataCall(0x00492910u,2,5,0x1e);
  cataCall(0x00491f90u,2,0x761a5e5,uVar1);
  return;
}

static void cataM2_005de4e0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004920a0u,1,0x761a5e5);
  if (iVar1 != 0) {
    cataCall(0x00491d70u,2,0x506a278,0);
    cataCall(0x00490640u,3,0x58be878,cataM2_005de530,cataM2_00652ef0);
  }
  return;
}

static void cataM2_005de530(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00492910u,2,0,9);
  cataCall(0x00491d30u,2,0x58be878,uVar1);
  iVar2 = cataCall(0x00491e30u,1,0x58be878);
  if (iVar2 < 7) {
    cataCall(0x00490640u,3,0x761a5e5,cataM2_005de5a0,cataM2_005de5e0);
    return;
  }
  cataCall(0x00490640u,3,0x5b5e4,cataM2_005de4a0,cataM2_005de4e0);
  return;
}

static void cataM2_005de5a0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x004927b0u,0);
  uVar1 = cataCall(0x004907e0u,1,0x712bfb4);
  cataCall(0x00499330u,1,uVar1);
  return;
}

static void cataM2_005de5e0(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x004907e0u,1,0x712bfb4);
  uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  iVar2 = cataCall(0x00495180u,3,uVar1);
  if (iVar2 != 0) {
    cataCall(0x00490640u,3,0x469857b,cataM2_005de650,cataM2_005de810);
  }
  return;
}

static void cataM2_005de650(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x9668ae7);
  uVar1 = cataCall(0x00490bb0u,1,0x6da2da9,uVar1);
  iVar2 = cataCall(0x0049a790u,2,uVar1);
  if (iVar2 == 0) {
    uVar1 = cataCall(0x004908c0u,1,0x712bfb4,5000);
    uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,uVar1);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495a50u,4,uVar1);
    uVar1 = cataCall(0x004907e0u,1,0x712bfb4);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495c40u,2,uVar1);
    uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
    iVar2 = cataCall(0x00494510u,1,uVar1);
    if (iVar2 != 0) {
      uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
      uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
      uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
      iVar2 = cataCall(0x00495410u,3,uVar1);
      if (iVar2 != 0) {
        uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
        uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
        cataCall(0x00495bd0u,2,uVar1);
      }
    }
  }
  else {
    uVar1 = cataCall(0x004907e0u,1,0x712bfb4);
    uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,uVar1);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495180u,3,uVar1);
  }
  uVar1 = cataCall(0x004909f0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x0049a100u,2,uVar1);
  cataCall(0x004927b0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  if (iVar2 != 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
    cataCall(0x00492190u,1,uVar1);
    uVar1 = cataCall(0x00492910u,2,5,0xf);
    cataCall(0x00491f90u,2,0x469857b,uVar1);
    return;
  }
  cataCall(0x00495eb0u,0);
  cataCall(0x00490640u,3,0x761a5e5,cataM2_005de5a0,cataM2_005de5e0);
  return;
}

static void cataM2_005de810(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492060u,1,0x469857b);
  if ((iVar1 == 0) && (iVar1 = cataCall(0x00499870u,1,cataShips()), iVar1 == 0)) {
    return;
  }
  cataCall(0x00490640u,3,0x469857b,cataM2_005de650,cataM2_005de810);
  return;
}

static void cataM2_005de870(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  cataCall(0x00491d70u,2,0x5137b,1);
  iVar1 = cataCall(0x00491e30u,1,0x443ccab);
  if (iVar1 == 0) {
    cataCall(0x00491d30u,2,0x443ccab,1);
    cataCall(0x00491d30u,2,0x50eaa3,5);
  }
  iVar1 = cataCall(0x00491e30u,1,0x50eaa3);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x004907a0u,1,0x76768e2,0x1f6fd13);
    cataCall(0x00494a00u,2,uVar2);
    cataCall(0x00490640u,3,0x53a0c,cataM2_0065f6f0,cataM2_00652ef0);
    return;
  }
  uVar2 = cataCall(0x00492910u,2,0x28,0x3c);
  cataCall(0x00491f90u,2,0x479e04,uVar2);
  return;
}

static void cataM2_005de920(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004920a0u,1,0x479e04);
  if (iVar1 != 0) {
    uVar2 = cataCall(0x004907e0u,1,0x76768e2);
    iVar1 = cataCall(0x00492d20u,1,uVar2);
    if (iVar1 != 0) {
      uVar2 = cataCall(0x004907e0u,1,0x76768e2,1);
      uVar2 = cataCall(0x00491e30u,1,0x9379e54,uVar2);
      uVar2 = cataCall(0x00490bb0u,1,0x9379e54,cataScriptText(0x0088b3a0),uVar2);
      cataCall(0x00499c70u,5,uVar2);
      uVar2 = cataCall(0x00490bb0u,1,0x9379e54,5);
      cataCall(0x0049aae0u,2,uVar2);
      uVar2 = cataCall(0x00490bb0u,1,0x9379e54);
      cataCall(0x00493cf0u,2,cataTeam(),uVar2);
    }
  }
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 == 0) {
    return;
  }
  uVar2 = cataCall(0x004907e0u,1,0xdbdde91);
  iVar1 = cataCall(0x00492d20u,1,uVar2);
  if (iVar1 == 0) {
    uVar2 = 0xdbdde91;
  }
  else {
    uVar2 = cataCall(0x004907e0u,1,0xdbdde92);
    iVar1 = cataCall(0x00492d20u,1,uVar2);
    if (iVar1 != 0) goto LAB_005dea2a;
    uVar2 = 0xdbdde92;
  }
  uVar2 = cataCall(0x004907a0u,1,uVar2,0xaa768c5);
  cataCall(0x00494a00u,2,uVar2);
LAB_005dea2a:
  cataCall(0x00491df0u,1,0x50eaa3);
  cataCall(0x00491d70u,2,0x5137b,0);
  cataCall(0x00490640u,3,0x5b5e4,cataM2_005de4a0,cataM2_005de4e0);
  return;
}

static void cataM2_005dea60(void)
{
  
  cataScriptScope(0);
  cataCall(0x00498270u,1,cataShips());
  cataCall(0x00490640u,3,0x53a0c,cataM2_0065f6f0,cataM2_00652ef0);
  return;
}

static void cataM2_005deaa0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x4e668f3,cataShips());
  cataCall(0x00495bd0u,2,uVar1);
  cataCall(0x00491d30u,2,0x5137b,0);
  return;
}

static void cataM2_005deae0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x5137b);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492ce0u,0);
    if (iVar1 != 0) {
      cataCall(0x00490640u,3,0x469857b,cataM2_005deb30,cataM2_005deba0);
    }
  }
  return;
}

static void cataM2_005deb30(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00491d70u,2,0x5137b,1);
  uVar1 = cataCall(0x004909f0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,uVar1);
  cataCall(0x0049a100u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x4a2a29);
  cataCall(0x00492190u,1,uVar1);
  uVar1 = cataCall(0x00492910u,2,0x1e,0x3c);
  cataCall(0x00491f90u,2,0x469857b,uVar1);
  return;
}

static void cataM2_005deba0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492ce0u,0);
  if (iVar1 == 0) {
    cataCall(0x00490640u,3,0x53a0c,cataM2_00652ef0,cataM2_00652ef0);
    return;
  }
  iVar1 = cataCall(0x00499870u,1,cataShips());
  if ((iVar1 == 0) && (iVar1 = cataCall(0x00492060u,1,0x469857b), iVar1 == 0)) {
    return;
  }
  cataCall(0x00490640u,3,0x469857b,cataM2_005deb30,cataM2_005deba0);
  return;
}

static void cataM2_005dec20(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x0049aae0u,2,cataShips(),0);
  cataCall(0x00493490u,1,0x80);
  cataCall(0x00493800u,0);
  cataCall(0x00498dd0u,4,0x75ebfb4,0,0,0);
  uVar1 = cataCall(0x00490b30u,1,0x8970502,0xd,10);
  uVar1 = cataCall(0x00490820u,1,0xcff5860,uVar1);
  uVar1 = cataCall(0x00490b30u,1,0x75ebfb4,uVar1);
  cataCall(0x00498f30u,5,uVar1);
  uVar1 = cataCall(0x00490b30u,1,0x75ebfb4);
  cataCall(0x00493600u,1,uVar1);
  cataCall(0x00493490u,1,0x40000);
  cataCall(0x00490640u,3,0x5b5e4,cataM2_005decd0,cataM2_005ded00);
  return;
}

static void cataM2_005decd0(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491f90u,2,0xea51295,0xf);
  return;
}

static void cataM2_005ded00(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004920a0u,1,0xea51295);
  if (iVar1 != 0) {
    cataCall(0x004977a0u,1,cataShips());
  }
  iVar1 = cataCall(0x00492120u,1,0x588674);
  if (iVar1 != 0) {
    cataCall(0x00497800u,1,cataShips());
    uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x471a631);
    cataCall(0x00494a00u,2,uVar2);
    cataCall(0x00490640u,3,0x4ae24,cataM2_00652ef0,cataM2_005ded90);
  }
  return;
}

static void cataM2_005ded90(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00499570u,1,cataShips());
  if (iVar1 != 0) {
    iVar1 = cataCall(0x00491e30u,1,0x443ccac);
    if (iVar1 == 0) {
      cataCall(0x00491d30u,2,0x443ccac,1);
      cataCall(0x00497ed0u,1,cataShips());
      cataCall(0x00494160u,1,0);
    }
    cataCall(0x00490640u,3,0x761a5e5,cataM2_005dee10,cataM2_005dee40);
  }
  return;
}

static void cataM2_005dee10(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491f90u,2,0x16e0f33,100);
  return;
}

static void cataM2_005dee40(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x443ccad);
  if ((iVar1 == 0) && (iVar1 = cataCall(0x004920a0u,1,0x16e0f33), iVar1 != 0)) {
    cataCall(0x00491d30u,2,0x443ccad,1);
    uVar2 = cataCall(0x004907a0u,1,0x4e668f3,0x588674);
    cataCall(0x00494a00u,2,uVar2);
  }
  iVar1 = cataCall(0x00498cb0u,2,cataScriptText(0x0088b230),0);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492ce0u,0);
    if (iVar1 != 0) {
      return;
    }
    uVar2 = 0x4a38954;
  }
  else {
    uVar2 = 0x927ec34;
  }
  uVar2 = cataCall(0x004907a0u,1,0xa8a880c,uVar2);
  cataCall(0x00494a00u,2,uVar2);
  cataCall(0x00490640u,3,0x53a0c,cataM2_00652ef0,cataM2_00652ef0);
  return;
}

static void cataM2_005def00(void)
{
  int iVar2;
  
  cataScriptScope(0);
  cataCall(0x0049aae0u,2,cataShips(),4);
  cataCall(0x00491d30u,2,0x5137b,0);
  const int initialXSign = cataCall(0x00492910u,2,-1,1);
  cataCall(0x00491d30u,2,0x441,initialXSign);
  const int initialYSign = cataCall(0x00492910u,2,-1,1);
  cataCall(0x00491d30u,2,0x442,initialYSign);
  iVar2 = cataCall(0x00491e30u,1,0x441);
  if (iVar2 == 0) {
    iVar2 = cataCall(0x00492910u,2,1,2);
    if (iVar2 == 1) {
      cataCall(0x00491db0u,1,0x441);
    }
    else {
      cataCall(0x00491df0u,1,0x441);
    }
  }
  iVar2 = cataCall(0x00491e30u,1,0x442);
  if (iVar2 == 0) {
    iVar2 = cataCall(0x00492910u,2,1,2);
    if (iVar2 == 1) {
      cataCall(0x00491db0u,1,0x442);
    }
    else {
      cataCall(0x00491df0u,1,0x442);
    }
  }
  // The retail caller pushes Z and signed Y before drawing signed X.
  const int z = cataCall(0x00492910u,2,-6000,6000);
  const int yDistance = cataCall(0x00492910u,2,55000,65000);
  const int ySign = cataCall(0x00491e30u,1,0x442);
  const int xDistance = cataCall(0x00492910u,2,55000,65000);
  const int xSign = cataCall(0x00491e30u,1,0x441);
  cataCall(0x00498dd0u,4,0x8970502,xDistance * xSign,yDistance * ySign,z);
  cataCall(0x00491e70u,1,0x441);
  cataCall(0x00491e70u,1,0x442);
  cataCall(0x004984c0u,2,cataShips(),1);
  cataCall(0x00493490u,1,0x80);
  cataCall(0x00493800u,0);
  cataCall(0x00493600u,1,cataCall(0x00490b30u,1,0x8970502));
  cataCall(0x00493aa0u,1,0x14);
  cataCall(0x00493a10u,2,cataShips(),0);
  cataCall(0x0049a2d0u,2,cataShips(),1);
  cataCall(0x004938f0u,2,cataShips(),1);
  cataCall(0x00490640u,3,0x4ae24,cataM2_00652ef0,cataM2_005df130);
  return;
}

static void cataM2_005df0c0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x5137b);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492ce0u,0);
    if (iVar1 == 0) {
      uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x5138954);
      cataCall(0x00494a00u,2,uVar2);
      cataCall(0x00490640u,3,0x53a0c,cataM2_005df3b0,cataM2_005df3e0);
    }
  }
  return;
}

static void cataM2_005df130(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x443ccae);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00499570u,1,cataShips());
    if (iVar1 == 1) {
      cataCall(0x00491d30u,2,0x443ccae,1);
      cataCall(0x00491d30u,2,0x849928e4,1);
      cataCall(0x00494160u,1,0);
      cataCall(0x00497ed0u,1,cataShips());
      uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x2e80a02);
      cataCall(0x00494a00u,2,uVar2);
    }
  }
  iVar1 = cataCall(0x00492120u,1,0x56a45e2);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x56a45e2,cataM2_00652ef0,cataM2_005df1f0);
  }
  return;
}

static void cataM2_005df1f0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x83617ea8);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492b90u,0);
    if (0x4a < iVar1) {
      cataCall(0x00493aa0u,1,0x4b);
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443ccaf);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492b90u,0);
    if (0x27 < iVar1) {
      cataCall(0x00491d30u,2,0x443ccaf,1);
      uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x471a633);
      cataCall(0x00494a00u,2,uVar2);
      cataCall(0x0049a2d0u,2,cataShips(),0);
      cataCall(0x00493940u,1,cataShips());
    }
  }
  iVar1 = cataCall(0x00492b90u,0);
  if (iVar1 == 0x5a) {
    cataCall(0x00493a10u,2,cataShips(),1);
    cataCall(0x00491d30u,2,0x5137b,1);
    cataCall(0x004984c0u,2,cataShips(),1);
    uVar2 = cataCall(0x004907a0u,1,0x98ddc43,0xa40f2f4);
    cataCall(0x00494a00u,2,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x712bfb4,0x588674);
    cataCall(0x00494a00u,2,uVar2);
    cataCall(0x00490640u,3,0x5b5e4,cataM2_00652ef0,cataM2_005df320);
  }
  return;
}

static void cataM2_005df320(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x443ccb0);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492120u,1,0x47a89);
    if (iVar1 != 0) {
      cataCall(0x00491d30u,2,0x443ccb0,1);
      cataCall(0x00491f90u,2,0xa7c9b35,6);
    }
  }
  iVar1 = cataCall(0x004920a0u,1,0xa7c9b35);
  if (iVar1 != 0) {
    cataCall(0x00493800u,0);
    cataCall(0x00490640u,3,0x53a0c,cataM2_005df3b0,cataM2_005df3e0);
  }
  return;
}

static void cataM2_005df3b0(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491f90u,2,0x53a0c,10);
  return;
}

static void cataM2_005df3e0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004920a0u,1,0x53a0c);
  if (iVar1 != 0) {
    cataCall(0x00497850u,1,cataShips());
  }
  return;
}

static void cataM2_005df420(void)
{
  
  cataScriptScope(0);
  cataCall(0x004984c0u,2,cataShips(),1);
  cataCall(0x00498c10u,1,cataScriptText(0x00890560));
  cataCall(0x00498c70u,1,cataShips());
  cataCall(0x00497fd0u,3,cataShips(),cataScriptText(0x0088b630),0);
  cataCall(0x0049aae0u,2,cataShips(),5);
  cataCall(0x00493490u,1,0x80);
  cataCall(0x00493800u,0);
  cataCall(0x00490640u,3,0xa7c9b35,cataM2_00652ef0,cataM2_005df4b0);
  return;
}

static void cataM2_005df4b0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x588674);
  if (iVar1 != 0) {
    cataCall(0x00498dd0u,4,0x51e1d3,0,0,0);
    uVar2 = cataCall(0x004908c0u,1,0x893b502,45000);
    uVar2 = cataCall(0x00490b30u,1,0x51e1d3,uVar2);
    cataCall(0x004991d0u,3,uVar2);
    uVar2 = cataCall(0x00490b30u,1,0x51e1d3);
    cataCall(0x00493600u,1,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x489849,0x588674);
    cataCall(0x00494a00u,2,uVar2);
    cataCall(0x00490640u,3,0x4617a78,cataM2_005df560,cataM2_005df5a0);
  }
  return;
}

static void cataM2_005df560(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00492780u,0);
  uVar1 = cataCall(0x004907e0u,1,0x893b502);
  cataCall(0x004924e0u,1,uVar1);
  return;
}

static void cataM2_005df5a0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x443ccb1);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x004998c0u,0);
    if (iVar1 != 0) {
      cataCall(0x00491d30u,2,0x443ccb1,1);
      uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0xa6345b7);
      cataCall(0x00494a00u,2,uVar2);
    }
  }
  iVar1 = cataCall(0x00492120u,1,0x4617a78);
  if (iVar1 != 0) {
    cataCall(0x00497000u,1,0x14);
    uVar2 = cataCall(0x004907a0u,1,0x617a7d1,0x588674);
    cataCall(0x00494a00u,2,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x489849,0x4617a78);
    cataCall(0x00494a00u,2,uVar2);
    cataCall(0x00491d30u,2,0x83617ea8,1);
    cataCall(0x00498080u,1,cataShips());
    cataCall(0x004984c0u,2,cataShips(),0);
    cataCall(0x00490640u,3,0x469857b,cataM2_005df690,cataM2_005df7c0);
  }
  return;
}

static void cataM2_005df690(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x004909f0u,0,10000);
  uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495a50u,4,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x893b502);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495bd0u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
  iVar2 = cataCall(0x00494510u,1,uVar1);
  if (iVar2 != 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  if (iVar2 == 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x4a2a29);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  uVar1 = cataCall(0x004909f0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x0049a100u,2,uVar1);
  cataCall(0x00492780u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492190u,1,uVar1);
  uVar1 = cataCall(0x00492910u,2,10,0x1e);
  cataCall(0x00491f90u,2,0x469857b,uVar1);
  return;
}

static void cataM2_005df7c0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492ce0u,0);
  if (iVar1 == 0) {
    cataCall(0x00490640u,3,0x53a0c,cataM2_00652ef0,cataM2_00652ef0);
    return;
  }
  iVar1 = cataCall(0x00499870u,1,cataShips());
  if ((iVar1 == 0) && (iVar1 = cataCall(0x004920a0u,1,0x469857b), iVar1 == 0)) {
    return;
  }
  cataCall(0x00490640u,3,0x469857b,cataM2_005df690,cataM2_005df7c0);
  return;
}

static void cataM2_005df840(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  cataCall(0x0049aae0u,2,cataShips(),5);
  cataCall(0x00491d30u,2,0x5137b,1);
  iVar1 = cataCall(0x00493b90u,0);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x004907a0u,1,0x617a7d1);
    iVar1 = cataCall(0x00493de0u,1,uVar2);
    if (iVar1 != 0) {
      cataCall(0x00490640u,3,0x53a0c,cataM2_005e01b0,cataM2_00652ef0);
      return;
    }
  }
  else if (iVar1 != 1) goto LAB_005df8dd;
  uVar2 = cataCall(0x004907a0u,1,0x617a7d3);
  iVar1 = cataCall(0x00493de0u,1,uVar2);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x53a0c,cataM2_005e01b0,cataM2_00652ef0);
    return;
  }
LAB_005df8dd:
  cataCall(0x00490640u,3,0x479e04,cataM2_005e3410,cataM2_005df950);
  return;
}

static void cataM2_005df900(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x5137b);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492ce0u,0);
    if (iVar1 == 0) {
      cataCall(0x00490640u,3,0x53a0c,cataM2_005e01b0,cataM2_00652ef0);
    }
  }
  return;
}

static void cataM2_005df950(void)
{
  int iVar1;
  CataValue uVar2;
  int iVar3;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x588674);
  if (iVar1 != 0) {
    iVar1 = cataCall(0x00491e30u,1,0x443ccb2);
    if (iVar1 == 0) {
      uVar2 = cataCall(0x004907a0u,1,0x617a7d1);
      iVar1 = cataCall(0x00493de0u,1,uVar2);
      if (iVar1 != 0) {
        cataCall(0x00491d30u,2,0x443ccb2,1);
        uVar2 = cataCall(0x004907a0u,1,0x617a7d2,0x588674);
        cataCall(0x00494a00u,2,uVar2);
        uVar2 = cataCall(0x004907a0u,1,0x617a7d3,0x588674);
        cataCall(0x00494a00u,2,uVar2);
      }
    }
    uVar2 = cataCall(0x004907a0u,1,0x617a7d1);
    iVar1 = cataCall(0x00493de0u,1,uVar2);
    if (iVar1 == 0) {
      uVar2 = cataCall(0x004907a0u,1,0x617a7d2);
      iVar1 = cataCall(0x00493de0u,1,uVar2);
      if (iVar1 == 0) {
        uVar2 = cataCall(0x00490820u,1,0xcff5860,20000);
        uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
        iVar1 = cataCall(0x00495a50u,4,uVar2);
        iVar3 = cataCall(0x00493b90u,0);
        cataCall(0x00491d30u,2,0x9379e54,iVar1 / (7 - iVar3));
        iVar1 = cataCall(0x00491e30u,1,0x9379e54);
        if (iVar1 < 3) {
          cataCall(0x00491d70u,2,0x9379e54,3);
        }
        uVar2 = cataCall(0x00490bb0u,1,0x9379e54);
        cataCall(0x00492d10u,1,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x4c34,1);
        uVar2 = cataCall(0x00491e30u,1,0x9379e54,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x379e5c1,cataScriptText(0x0088b3ec),uVar2);
        cataCall(0x00499c70u,5,uVar2);
        iVar1 = cataCall(0x00491e30u,1,0x9379e54);
        iVar1 = (int)(iVar1 + (iVar1 >> 0x1f & 3U)) >> 2;
      }
      else {
        uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,cataScriptText(0x0088b940));
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
        cataCall(0x00495690u,3,uVar2);
        uVar2 = cataCall(0x004908c0u,1,0x893b502,20000);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
        iVar1 = cataCall(0x00495a50u,4,uVar2);
        iVar3 = cataCall(0x00493b90u,0);
        cataCall(0x00491d30u,2,0x9379e54,iVar1 / (7 - iVar3));
        iVar1 = cataCall(0x00491e30u,1,0x9379e54);
        if (iVar1 < 3) {
          cataCall(0x00491d70u,2,0x9379e54,3);
        }
        uVar2 = cataCall(0x00490bb0u,1,0x9379e54);
        cataCall(0x00492d10u,1,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x4c34,1);
        uVar2 = cataCall(0x00491e30u,1,0x9379e54,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x379e5c1,cataScriptText(0x0088b3ec),uVar2);
        cataCall(0x00499c70u,5,uVar2);
        iVar1 = cataCall(0x00491e30u,1,0x9379e54);
        iVar1 = iVar1 / 3;
      }
    }
    else {
      uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,cataScriptText(0x0088afd0));
      uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
      cataCall(0x00495640u,3,uVar2);
      uVar2 = cataCall(0x004908c0u,1,0x893b502,20000);
      uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
      uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
      iVar1 = cataCall(0x00495a50u,4,uVar2);
      iVar3 = cataCall(0x00493b90u,0);
      cataCall(0x00491d30u,2,0x9379e54,iVar1 / (7 - iVar3));
      iVar1 = cataCall(0x00491e30u,1,0x9379e54);
      if (iVar1 < 3) {
        cataCall(0x00491d70u,2,0x9379e54,3);
      }
      uVar2 = cataCall(0x00490bb0u,1,0x9379e54);
      cataCall(0x00492d10u,1,uVar2);
      uVar2 = cataCall(0x00490bb0u,1,0x4c34,1);
      uVar2 = cataCall(0x00491e30u,1,0x9379e54,uVar2);
      uVar2 = cataCall(0x00490bb0u,1,0x379e5c1,cataScriptText(0x0088b3ec),uVar2);
      cataCall(0x00499c70u,5,uVar2);
      iVar1 = cataCall(0x00491e30u,1,0x9379e54);
      iVar1 = iVar1 / 3;
    }
    // Each retail branch leaves the gate selection and owner on the stack.
    const CataValue gates = cataCall(0x00490bb0u,1,0x4c34);
    const CataValue reinforcements = cataCall(0x00490bb0u,1,0x379e5c2);
    cataCall(0x00499c70u,5,reinforcements,cataScriptText(0x0088b3c4),iVar1,gates,1);
    uVar2 = cataCall(0x00490bb0u,1,0x379e5c1);
    uVar2 = cataCall(0x00490bb0u,1,0x9379e54,uVar2);
    cataCall(0x00495bd0u,2,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x379e5c2);
    uVar2 = cataCall(0x00490bb0u,1,0x9379e54,uVar2);
    cataCall(0x00495bd0u,2,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x9379e54,5);
    cataCall(0x0049aae0u,2,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x9379e54);
    cataCall(0x00493cf0u,2,cataTeam(),uVar2);
  }
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 != 0) {
    iVar1 = cataCall(0x00492ce0u,0);
    if (iVar1 < 0xb) {
      cataCall(0x00492780u,0);
    }
    else {
      cataCall(0x004927e0u,0);
    }
    cataCall(0x00490640u,3,0xa7c9b35,cataM2_005dfe10,cataM2_005dfe40);
  }
  return;
}

static void cataM2_005dfe10(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491f90u,2,0xa7c9b35,10);
  return;
}

static void cataM2_005dfe40(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004920a0u,1,0xa7c9b35);
  if (iVar1 != 0) {
    cataCall(0x00491d70u,2,0x5137b,0);
    cataCall(0x00493490u,1,0x80);
    cataCall(0x00493800u,0);
    uVar2 = cataCall(0x004908c0u,1,0x893b502,30000);
    cataCall(0x00493630u,2,uVar2);
    cataCall(0x00490640u,3,0x469857b,cataM2_005dfec0,cataM2_005e00c0);
  }
  return;
}

static void cataM2_005dfec0(void)
{
  CataValue uVar1;
  int iVar2;
  CataValue uVar3;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x617a7d1);
  iVar2 = cataCall(0x00493de0u,1,uVar1);
  if (iVar2 == 0) {
    uVar1 = cataCall(0x004907a0u,1,0x617a7d2);
    iVar2 = cataCall(0x00493de0u,1,uVar1);
    if (iVar2 == 0) {
      uVar1 = cataCall(0x00490bb0u,1,0xcff5860);
      uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
      cataCall(0x00495bd0u,2,uVar1);
      goto LAB_005dffa6;
    }
    uVar1 = cataCall(0x004909f0u,0,30000);
    uVar3 = 0x4a2a29;
  }
  else {
    uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,cataScriptText(0x0088b940));
    uVar1 = cataCall(0x00490bb0u,1,0x8c46ff2,uVar1);
    cataCall(0x00495690u,3,uVar1);
    uVar1 = cataCall(0x004909f0u,0,30000);
    uVar3 = 0x8c46ff2;
  }
  uVar1 = cataCall(0x00490bb0u,1,uVar3,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495a50u,4,uVar1);
LAB_005dffa6:
  uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
  iVar2 = cataCall(0x00494510u,1,uVar1);
  if (iVar2 != 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  if (iVar2 == 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x4a2a29);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  uVar1 = cataCall(0x004909f0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x0049a100u,2,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x893b502);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495c40u,2,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x893b502);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495bd0u,2,uVar1);
  iVar2 = cataCall(0x00492ce0u,0);
  if (iVar2 < 0xb) {
    cataCall(0x00492780u,0);
  }
  else {
    cataCall(0x004927e0u,0);
  }
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492190u,1,uVar1);
  uVar1 = cataCall(0x00492910u,2,5,0x14);
  cataCall(0x00491f90u,2,0x469857b,uVar1);
  return;
}

static void cataM2_005e00c0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00499870u,1,cataShips());
  if ((iVar1 == 0) && (iVar1 = cataCall(0x00492060u,1,0x469857b), iVar1 == 0)) {
    return;
  }
  cataCall(0x004920e0u,1,0x469857b);
  cataCall(0x00490640u,3,0x469857b,cataM2_005dfec0,cataM2_005e00c0);
  return;
}

static void cataM2_005e0120(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00492910u,2,0x14,0x3c);
  cataCall(0x00491f90u,2,0x5b5e4,uVar1);
  return;
}

static void cataM2_005e0150(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004920a0u,1,0x5b5e4);
  if (iVar1 != 0) {
    cataCall(0x00494a00u,2,cataTeam(),0x588674);
    cataCall(0x00490640u,3,0x479e04,cataM2_005e3410,cataM2_005df950);
  }
  return;
}

static void cataM2_005e01b0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  cataCall(0x00491d70u,2,0x5137b,1);
  iVar1 = cataCall(0x00491e30u,1,0x443ccb3);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x004907a0u,1,0x617a7d1);
    iVar1 = cataCall(0x00493de0u,1,uVar2);
    if (iVar1 == 0) {
      cataCall(0x00491d30u,2,0x443ccb3,1);
      cataCall(0x00490640u,3,0x5b5e4,cataM2_005e0120,cataM2_005e0150);
    }
  }
  return;
}

static void cataM2_005e0220(void)
{
  
  cataScriptScope(0);
  cataCall(0x0049aae0u,2,cataShips(),5);
  cataCall(0x004984c0u,2,cataShips(),1);
  cataCall(0x00491d30u,2,0x5137b,1);
  cataCall(0x00493490u,1,0x80);
  cataCall(0x00493800u,0);
  cataCall(0x00490640u,3,0xa7c9b35,cataM2_00652ef0,cataM2_005e02e0);
  return;
}

static void cataM2_005e0290(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x5137b);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492ce0u,0);
    if (iVar1 == 0) {
      cataCall(0x00490640u,3,0x53a0c,cataM2_0065f6f0,cataM2_00652ef0);
    }
  }
  return;
}

static void cataM2_005e02e0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x588674);
  if (iVar1 != 0) {
    cataCall(0x00498dd0u,4,0x489849,0,0,0);
    uVar2 = cataCall(0x00490b30u,1,0x51e1d3,0xf,10);
    uVar2 = cataCall(0x00490b30u,1,0x8970502,uVar2);
    uVar2 = cataCall(0x00490b30u,1,0x489849,uVar2);
    cataCall(0x00498f30u,5,uVar2);
    uVar2 = cataCall(0x00490b30u,1,0x489849);
    cataCall(0x00493600u,1,uVar2);
    cataCall(0x00491d70u,2,0x5137b,0);
    cataCall(0x00490640u,3,0x8a63464,cataM2_005e0390,cataM2_005e03d0);
  }
  return;
}

static void cataM2_005e0390(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00492840u,0);
  uVar1 = cataCall(0x004907e0u,1,0x596d859);
  cataCall(0x004924e0u,1,uVar1);
  return;
}

static void cataM2_005e03d0(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
  iVar2 = cataCall(0x00494510u,1,uVar1);
  if ((iVar2 == 0) && (iVar2 = cataCall(0x00492120u,1,0x4617a78), iVar2 == 0)) {
    return;
  }
  cataCall(0x004984c0u,2,cataShips(),0);
  cataCall(0x00495eb0u,0);
  cataCall(0x00490640u,3,0x469857b,cataM2_005e0440,cataM2_005e0540);
  return;
}

static void cataM2_005e0440(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x004909f0u,0,30000);
  uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  iVar2 = cataCall(0x00495a50u,4,uVar1);
  if (iVar2 == 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x4a2a29);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
  iVar2 = cataCall(0x00494510u,1,uVar1);
  if (iVar2 != 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x857bce3);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
  }
  uVar1 = cataCall(0x004909f0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x0049a100u,2,uVar1);
  cataCall(0x00492840u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492190u,1,uVar1);
  cataCall(0x004920e0u,1,0x469857b);
  uVar1 = cataCall(0x00492910u,2,10,0x14);
  cataCall(0x00491f90u,2,0x469857b,uVar1);
  return;
}

static void cataM2_005e0540(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00499870u,1,cataShips());
  if ((iVar1 == 0) && (iVar1 = cataCall(0x00492060u,1,0x469857b), iVar1 == 0)) {
    return;
  }
  cataCall(0x004920e0u,1,0x469857b);
  cataCall(0x00490640u,3,0x469857b,cataM2_005e0440,cataM2_005e0540);
  return;
}

static void cataM2_005e05a0(void)
{
  CataValue uVar1;
  
  cataScriptScope(1);
  cataCall(0x00497000u,1,1);
  uVar1 = cataCall(0x00490bb0u,1,0xcff5860);
  cataCall(0x00498790u,1,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0xa8a880c);
  cataCall(0x00490660u,4,0xa8a880c,cataM2_005db770,cataM2_0062ca80,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x98ddc43);
  cataCall(0x00490660u,4,0x98ddc43,cataM2_0062ca80,cataM2_005dcbd0,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x4e668f3);
  cataCall(0x00490660u,4,0x4e668f3,cataM2_0062ca80,cataM2_005dcf10,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x76768e2);
  cataCall(0x00490660u,4,0x7676de2,cataM2_005dd040,cataM2_005dd0a0,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x61df215);
  cataCall(0x00490660u,4,0x61df215,cataM2_005deaa0,cataM2_005deae0,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0xdbdde93);
  cataCall(0x00490660u,4,0x4c66683,cataM2_005ddd30,cataM2_005dddb0,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x761a0e5);
  cataCall(0x00490660u,4,0x761a5e5,cataM2_005de2e0,cataM2_005de380,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0xdbdde91);
  cataCall(0x00490660u,4,0x5896df5,cataM2_005dd5f0,cataM2_005dd670,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0xdbdde92);
  cataCall(0x00490660u,4,0x5896df5,cataM2_005dd5f0,cataM2_005dd670,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x4697680,5);
  cataCall(0x0049aae0u,2,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x4697681,5);
  cataCall(0x0049aae0u,2,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x4697682,5);
  cataCall(0x0049aae0u,2,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x4697683,5);
  cataCall(0x0049aae0u,2,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x4697684,5);
  cataCall(0x0049aae0u,2,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x4697685,5);
  cataCall(0x0049aae0u,2,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x4697680);
  cataCall(0x00490660u,4,0x4697443,cataM2_005db2a0,cataM2_005db3c0,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x4697681);
  cataCall(0x00490660u,4,0x4697443,cataM2_005db2a0,cataM2_005db3c0,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x4697682);
  cataCall(0x00490660u,4,0x4697443,cataM2_005db2a0,cataM2_005db3c0,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x4697683);
  cataCall(0x00490660u,4,0x4697443,cataM2_005db2a0,cataM2_005db3c0,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x4697684);
  cataCall(0x00490660u,4,0x4697443,cataM2_005db2a0,cataM2_005db3c0,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x4697685);
  cataCall(0x00490660u,4,0x4697443,cataM2_005db2a0,cataM2_005db3c0,uVar1);
  uVar1 = cataCall(0x00493b90u,0);
  switch(uVar1) {
  case 0:
    cataCall(0x00491f90u,2,0x4ce34,0x78);
    uVar1 = cataCall(0x004907e0u,1,0x4697681);
    cataCall(0x00498270u,1,uVar1);
    uVar1 = cataCall(0x004907e0u,1,0x4697682);
    cataCall(0x00498270u,1,uVar1);
    uVar1 = cataCall(0x004907e0u,1,0x4697683);
    cataCall(0x00498270u,1,uVar1);
    uVar1 = cataCall(0x004907e0u,1,0x4697684);
    cataCall(0x00498270u,1,uVar1);
    uVar1 = cataCall(0x004907e0u,1,0x4697685);
    cataCall(0x00498270u,1,uVar1);
    break;
  case 1:
    cataCall(0x00491f90u,2,0x4ce34,0xb4);
    uVar1 = cataCall(0x004907e0u,1,0x4697683);
    cataCall(0x00498270u,1,uVar1);
    uVar1 = cataCall(0x004907e0u,1,0x4697684);
    cataCall(0x00498270u,1,uVar1);
    uVar1 = cataCall(0x004907e0u,1,0x4697685);
    cataCall(0x00498270u,1,uVar1);
    break;
  case 2:
    cataCall(0x00491f90u,2,0x4ce34,0x168);
    uVar1 = cataCall(0x004907e0u,1,0x4697684);
    cataCall(0x00498270u,1,uVar1);
    uVar1 = cataCall(0x004907e0u,1,0x4697685);
    cataCall(0x00498270u,1,uVar1);
    break;
  case 3:
    uVar1 = cataCall(0x004907e0u,1,0x4697685);
    cataCall(0x00498270u,1,uVar1);
  }
  uVar1 = cataCall(0x004907a0u,1,0x893b502);
  cataCall(0x00490660u,4,0x8970502,cataM2_005def00,cataM2_005df0c0,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x712bfb4);
  cataCall(0x00490660u,4,0x75ebfb4,cataM2_005dec20,cataM2_0062ca80,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x596d859);
  cataCall(0x00490660u,4,0x51e1d3,cataM2_005df420,cataM2_0062ca80,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x54c9664);
  cataCall(0x00490660u,4,0xe1ae0b4,cataM2_005dd370,cataM2_005dd3d0,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x617a7d1);
  cataCall(0x00490660u,4,0x4617a78,cataM2_005df840,cataM2_005df900,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x617a7d2);
  cataCall(0x00490660u,4,0x4617a78,cataM2_005df840,cataM2_005df900,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x617a7d3);
  cataCall(0x00490660u,4,0x4617a78,cataM2_005df840,cataM2_005df900,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x489849);
  cataCall(0x00490660u,4,0x489849,cataM2_005e0220,cataM2_005e0290,uVar1);
  cataCall(0x00496e20u,2,1,30000);
  cataCall(0x00496cf0u,1,cataScriptText(0x00890688));
  cataCall(0x00496cf0u,1,cataScriptText(0x0089056c));
  cataCall(0x00496cf0u,1,cataScriptText(0x0089069c));
  cataCall(0x00496cf0u,1,cataScriptText(0x008906ac));
  cataCall(0x00497f10u,2,0,cataScriptText(0x0088b9f4));
  cataCall(0x00497f10u,2,0,cataScriptText(0x0088b9e8));
  cataCall(0x00497f10u,2,0,cataScriptText(0x0088b9d0));
  cataCall(0x00497f10u,2,0,cataScriptText(0x0088b940));
  cataCall(0x00497f10u,2,0,cataScriptText(0x0088b9b8));
  cataCall(0x00497f70u,2,0,3);
  cataCall(0x00497f10u,2,1,cataScriptText(0x0088b3ec));
  cataCall(0x00497f10u,2,1,cataScriptText(0x0088b3c4));
  cataCall(0x00497f70u,2,1,1);
  cataCall(0x00497f10u,2,2,cataScriptText(0x0088b074));
  cataCall(0x004980c0u,3,cataScriptText(0x0088b88c),cataScriptText(0x0088b0cc),1);
  cataCall(0x004980c0u,3,cataScriptText(0x0088b8f8),cataScriptText(0x0088b0cc),0);
  cataCall(0x004980c0u,3,cataScriptText(0x0088b8cc),cataScriptText(0x0088b0cc),0);
  cataCall(0x004980c0u,3,cataScriptText(0x0088b8a4),cataScriptText(0x0088b0cc),0);
  cataCall(0x004980c0u,3,cataScriptText(0x0088b8b8),cataScriptText(0x0088b0cc),0);
  cataCall(0x00493f40u,0);
  cataCall(0x00498230u,0);
  cataCall(0x00491f90u,2,0x6381513,1);
  cataCall(0x00491f90u,2,0xbe10ca3,1);
  uVar1 = cataCall(0x00490bb0u,1,0x4c34);
  uVar1 = cataCall(0x00490af0u,1,0x4c34,uVar1);
  cataCall(0x004995e0u,2,uVar1);
  cataCall(0x00497710u,2,8000,cataScriptLocalizedText(0x008c84a4));
  return;
}

static void cataM2_005e0bb0(void)
{
  int iVar1;
  CataValue uVar2;
  CataValue uVar3;
  
  cataScriptScope(1);
  iVar1 = cataCall(0x00492060u,1,0x6381513);
  if (iVar1 != 0) {
    uVar2 = cataCall(0x00490bb0u,1,0x117ca33);
    uVar2 = cataCall(0x00490af0u,1,0x7ec6887,uVar2);
    cataCall(0x00492db0u,2,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x117ca33);
    uVar2 = cataCall(0x00490bb0u,1,0x6da2da9,uVar2);
    cataCall(0x00495620u,2,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x117ca33);
    uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,uVar2);
    cataCall(0x00495560u,2,uVar2);
    uVar2 = cataCall(0x004907e0u,1,0x712bfb4);
    uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,uVar2);
    cataCall(0x00495c40u,2,uVar2);
    uVar2 = cataCall(0x00492910u,2,5,10);
    cataCall(0x00491f00u,2,0x6381513,uVar2);
    cataCall(0x00491f50u,1,0x6381513);
  }
  iVar1 = cataCall(0x00492060u,1,0xbe10ca3);
  if (iVar1 != 0) {
    iVar1 = cataCall(0x00491e30u,1,0x1a8a423);
    if (iVar1 == 0) {
      uVar2 = cataCall(0x00490bb0u,1,0x1a8a423);
      cataCall(0x00492d60u,1,uVar2);
      cataCall(0x00491f00u,2,0xbe10ca3,3);
      cataCall(0x00491f50u,1,0xbe10ca3);
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443ccb4);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x00490bb0u,1,0x1a8a423);
    iVar1 = cataCall(0x00499570u,1,uVar2);
    if (iVar1 != 0) {
      cataCall(0x00491d30u,2,0x443ccb4,1);
      cataCall(0x00491d30u,2,0x1a8a423,1);
      uVar2 = cataCall(0x00490bb0u,1,0x1a8a423);
      cataCall(0x00492d10u,1,uVar2);
      uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x1a8a423);
      cataCall(0x00494a00u,2,uVar2);
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x1a8a423);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x00490bb0u,1,0x1a8a423);
    cataCall(0x00492d10u,1,uVar2);
  }
  iVar1 = cataCall(0x00491e30u,1,0x87defce4);
  if (iVar1 != 0) {
    uVar2 = cataCall(0x00490bb0u,1,0xcff5860);
    iVar1 = cataCall(0x00492d20u,1,uVar2);
    if (iVar1 != 0) {
      uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,cataScriptText(0x0088b88c));
      uVar2 = cataCall(0x00490bb0u,1,0x7ca3a49,uVar2);
      iVar1 = cataCall(0x00495690u,3,uVar2);
      if (iVar1 == 0) {
        iVar1 = cataCall(0x00491e30u,1,0x98b21d9);
        if (iVar1 == 0) goto LAB_005e0e31;
        cataCall(0x00491d70u,2,0x98b21d9,0);
        uVar3 = 0x9502d35;
        uVar2 = 0xa8a880c;
      }
      else {
        iVar1 = cataCall(0x00491e30u,1,0x98b21d9);
        if (iVar1 != 0) goto LAB_005e0e31;
        uVar2 = cataCall(0x00490bb0u,1,0x7ca3a49);
        iVar1 = cataCall(0x00493400u,1,uVar2);
        if (iVar1 != 0) goto LAB_005e0e31;
        cataCall(0x00491d70u,2,0x98b21d9,1);
        uVar3 = 0xfa02d75;
        uVar2 = 0x98ddc43;
      }
      uVar2 = cataCall(0x004907a0u,1,uVar2,uVar3);
      cataCall(0x00494a00u,2,uVar2);
    }
  }
LAB_005e0e31:
  iVar1 = cataCall(0x00491e30u,1,0x443ccb5);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00491e30u,1,0x87defce4);
    if (iVar1 != 0) {
      cataCall(0x00491d30u,2,0x443ccb5,1);
      uVar2 = cataCall(0x00490bb0u,1,0x117ca33);
      uVar2 = cataCall(0x00490bb0u,1,0x5506da2,uVar2);
      cataCall(0x004954d0u,2,uVar2);
      uVar2 = cataCall(0x00490bb0u,1,0x5506da2);
      uVar2 = cataCall(0x00490bb0u,1,0x5506da2,uVar2);
      cataCall(0x004952b0u,2,uVar2);
      uVar2 = cataCall(0x00490bb0u,1,0xcff5860);
      uVar2 = cataCall(0x00490bb0u,1,0x4e37d85,uVar2);
      iVar1 = cataCall(0x0049ad80u,2,uVar2);
      if (iVar1 != 0) {
        uVar2 = cataCall(0x00490bb0u,1,0x4e37d85);
        uVar2 = cataCall(0x00490bb0u,1,0x5506da2,uVar2);
        cataCall(0x00495bd0u,2,uVar2);
      }
      iVar1 = cataCall(0x00493b90u,0);
      if (iVar1 < 2) {
        iVar1 = cataCall(0x00496e00u,1,0);
        if (iVar1 < 500) {
          uVar2 = cataCall(0x00490bb0u,1,0x5506da2,cataScriptText(0x0088b940));
          uVar2 = cataCall(0x00490bb0u,1,0x5c46fa2,uVar2);
          iVar1 = cataCall(0x00495690u,3,uVar2);
          if (iVar1 == 0) {
            uVar2 = cataCall(0x00490bb0u,1,0xcff5860,0);
            uVar2 = cataCall(0x00490bb0u,1,0x9379e54,cataScriptText(0x0088b940),1,uVar2);
            cataCall(0x00499c70u,5,uVar2);
          }
        }
      }
      cataCall(0x00491d30u,2,0x98b21d9,1);
      cataCall(0x00497290u,2,2,cataScriptLocalizedText(0x008c84a4));
      uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x93100e4);
      cataCall(0x00494a00u,2,uVar2);
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443ccb6);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00491e30u,1,0x8f624235);
    if (iVar1 != 0) {
      uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,cataScriptText(0x0088b8b8));
      uVar2 = cataCall(0x00490bb0u,1,0x69e2def,uVar2);
      iVar1 = cataCall(0x00495690u,3,uVar2);
      if (iVar1 != 0) {
        uVar2 = cataCall(0x00490bb0u,1,0x69e2def);
        iVar1 = cataCall(0x00493400u,1,uVar2);
        if (iVar1 == 0) {
          cataCall(0x00491d30u,2,0x443ccb6,1);
          uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x479e14);
          cataCall(0x00494a00u,2,uVar2);
          uVar2 = cataCall(0x004907a0u,1,0x98ddc43,0x479e14);
          cataCall(0x00494a00u,2,uVar2);
        }
      }
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x8d8b9e42);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x00490bb0u,1,0x4697443);
    iVar1 = cataCall(0x00499570u,1,uVar2);
    if (iVar1 != 0) {
      cataCall(0x00491d30u,2,0x8d8b9e42,1);
      uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0xd8bd352);
      cataCall(0x00494a00u,2,uVar2);
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443ccb7);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x004967b0u,1,cataScriptText(0x0088b8b8));
    if (iVar1 != 0) {
      cataCall(0x00491d30u,2,0x443ccb7,1);
      uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x67ae1d2);
      cataCall(0x00494a00u,2,uVar2);
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443ccb8);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00491e30u,1,0x849928e4);
    if (iVar1 == 0) {
      iVar1 = cataCall(0x004920a0u,1,0x4ce34);
      if (iVar1 != 0) {
        cataCall(0x00491d30u,2,0x443ccb8,1);
        uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x4ce34);
        cataCall(0x00494a00u,2,uVar2);
      }
    }
  }
  return;
}

static void cataM2_005e3410(void)
{
  
  cataScriptScope(0);
  cataCall(0x00495eb0u,0);
  return;
}

static void cataM2_005f4730(void)
{
  
  cataScriptScope(0);
  cataCall(0x00497b80u,0);
  return;
}

static void cataM2_00608010(void)
{
  
  cataScriptScope(0);
  cataCall(0x004947c0u,0);
  return;
}

static void cataM2_00613940(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491d70u,2,0x858db5e4,0);
  return;
}

static void cataM2_00624aa0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00497b00u,0);
  if (iVar1 != 0) {
    uVar2 = cataCall(0x004907a0u,1,0x98ddc43,0xd2247a4);
    cataCall(0x00494a00u,2,uVar2);
    cataCall(0x00490640u,3,0x53a0c,cataM2_00652ef0,cataM2_00652ef0);
  }
  return;
}

static void cataM2_0062ca80(void)
{
  
  cataScriptScope(0);
  return;
}

static void cataM2_00652ef0(void)
{
  
  cataScriptScope(0);
  return;
}

static void cataM2_0065f6f0(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491d70u,2,0x5137b,1);
  return;
}
extern "C" void cataMission02Init(){cataM2_005e05a0();}
extern "C" void cataMission02Watch(){cataM2_005e0bb0();}
extern "C" const void **cataMission02FunctionTable(){
    static const void *functions[]={
        reinterpret_cast<const void*>(&cataM2_005db2a0),
        reinterpret_cast<const void*>(&cataM2_005db3c0),
        reinterpret_cast<const void*>(&cataM2_005db480),
        reinterpret_cast<const void*>(&cataM2_005db590),
        reinterpret_cast<const void*>(&cataM2_005db5d0),
        reinterpret_cast<const void*>(&cataM2_005db6f0),
        reinterpret_cast<const void*>(&cataM2_005db770),
        reinterpret_cast<const void*>(&cataM2_005db830),
        reinterpret_cast<const void*>(&cataM2_005dba70),
        reinterpret_cast<const void*>(&cataM2_005dbac0),
        reinterpret_cast<const void*>(&cataM2_005dcbd0),
        reinterpret_cast<const void*>(&cataM2_005dcf10),
        reinterpret_cast<const void*>(&cataM2_005dd040),
        reinterpret_cast<const void*>(&cataM2_005dd0a0),
        reinterpret_cast<const void*>(&cataM2_005dd1b0),
        reinterpret_cast<const void*>(&cataM2_005dd250),
        reinterpret_cast<const void*>(&cataM2_005dd280),
        reinterpret_cast<const void*>(&cataM2_005dd2d0),
        reinterpret_cast<const void*>(&cataM2_005dd370),
        reinterpret_cast<const void*>(&cataM2_005dd3d0),
        reinterpret_cast<const void*>(&cataM2_005dd4a0),
        reinterpret_cast<const void*>(&cataM2_005dd570),
        reinterpret_cast<const void*>(&cataM2_005dd5b0),
        reinterpret_cast<const void*>(&cataM2_005dd5f0),
        reinterpret_cast<const void*>(&cataM2_005dd670),
        reinterpret_cast<const void*>(&cataM2_005dd730),
        reinterpret_cast<const void*>(&cataM2_005dd790),
        reinterpret_cast<const void*>(&cataM2_005dd800),
        reinterpret_cast<const void*>(&cataM2_005dd870),
        reinterpret_cast<const void*>(&cataM2_005dda60),
        reinterpret_cast<const void*>(&cataM2_005ddad0),
        reinterpret_cast<const void*>(&cataM2_005ddb10),
        reinterpret_cast<const void*>(&cataM2_005ddc60),
        reinterpret_cast<const void*>(&cataM2_005ddca0),
        reinterpret_cast<const void*>(&cataM2_005ddce0),
        reinterpret_cast<const void*>(&cataM2_005ddd30),
        reinterpret_cast<const void*>(&cataM2_005dddb0),
        reinterpret_cast<const void*>(&cataM2_005dde90),
        reinterpret_cast<const void*>(&cataM2_005ddef0),
        reinterpret_cast<const void*>(&cataM2_005ddff0),
        reinterpret_cast<const void*>(&cataM2_005de050),
        reinterpret_cast<const void*>(&cataM2_005de090),
        reinterpret_cast<const void*>(&cataM2_005de1e0),
        reinterpret_cast<const void*>(&cataM2_005de220),
        reinterpret_cast<const void*>(&cataM2_005de270),
        reinterpret_cast<const void*>(&cataM2_005de2e0),
        reinterpret_cast<const void*>(&cataM2_005de380),
        reinterpret_cast<const void*>(&cataM2_005de440),
        reinterpret_cast<const void*>(&cataM2_005de4a0),
        reinterpret_cast<const void*>(&cataM2_005de4e0),
        reinterpret_cast<const void*>(&cataM2_005de530),
        reinterpret_cast<const void*>(&cataM2_005de5a0),
        reinterpret_cast<const void*>(&cataM2_005de5e0),
        reinterpret_cast<const void*>(&cataM2_005de650),
        reinterpret_cast<const void*>(&cataM2_005de810),
        reinterpret_cast<const void*>(&cataM2_005de870),
        reinterpret_cast<const void*>(&cataM2_005de920),
        reinterpret_cast<const void*>(&cataM2_005dea60),
        reinterpret_cast<const void*>(&cataM2_005deaa0),
        reinterpret_cast<const void*>(&cataM2_005deae0),
        reinterpret_cast<const void*>(&cataM2_005deb30),
        reinterpret_cast<const void*>(&cataM2_005deba0),
        reinterpret_cast<const void*>(&cataM2_005dec20),
        reinterpret_cast<const void*>(&cataM2_005decd0),
        reinterpret_cast<const void*>(&cataM2_005ded00),
        reinterpret_cast<const void*>(&cataM2_005ded90),
        reinterpret_cast<const void*>(&cataM2_005dee10),
        reinterpret_cast<const void*>(&cataM2_005dee40),
        reinterpret_cast<const void*>(&cataM2_005def00),
        reinterpret_cast<const void*>(&cataM2_005df0c0),
        reinterpret_cast<const void*>(&cataM2_005df130),
        reinterpret_cast<const void*>(&cataM2_005df1f0),
        reinterpret_cast<const void*>(&cataM2_005df320),
        reinterpret_cast<const void*>(&cataM2_005df3b0),
        reinterpret_cast<const void*>(&cataM2_005df3e0),
        reinterpret_cast<const void*>(&cataM2_005df420),
        reinterpret_cast<const void*>(&cataM2_005df4b0),
        reinterpret_cast<const void*>(&cataM2_005df560),
        reinterpret_cast<const void*>(&cataM2_005df5a0),
        reinterpret_cast<const void*>(&cataM2_005df690),
        reinterpret_cast<const void*>(&cataM2_005df7c0),
        reinterpret_cast<const void*>(&cataM2_005df840),
        reinterpret_cast<const void*>(&cataM2_005df900),
        reinterpret_cast<const void*>(&cataM2_005df950),
        reinterpret_cast<const void*>(&cataM2_005dfe10),
        reinterpret_cast<const void*>(&cataM2_005dfe40),
        reinterpret_cast<const void*>(&cataM2_005dfec0),
        reinterpret_cast<const void*>(&cataM2_005e00c0),
        reinterpret_cast<const void*>(&cataM2_005e0120),
        reinterpret_cast<const void*>(&cataM2_005e0150),
        reinterpret_cast<const void*>(&cataM2_005e01b0),
        reinterpret_cast<const void*>(&cataM2_005e0220),
        reinterpret_cast<const void*>(&cataM2_005e0290),
        reinterpret_cast<const void*>(&cataM2_005e02e0),
        reinterpret_cast<const void*>(&cataM2_005e0390),
        reinterpret_cast<const void*>(&cataM2_005e03d0),
        reinterpret_cast<const void*>(&cataM2_005e0440),
        reinterpret_cast<const void*>(&cataM2_005e0540),
        reinterpret_cast<const void*>(&cataM2_005e05a0),
        reinterpret_cast<const void*>(&cataM2_005e0bb0),
        reinterpret_cast<const void*>(&cataM2_005e3410),
        reinterpret_cast<const void*>(&cataM2_005f4730),
        reinterpret_cast<const void*>(&cataM2_00608010),
        reinterpret_cast<const void*>(&cataM2_00613940),
        reinterpret_cast<const void*>(&cataM2_00624aa0),
        reinterpret_cast<const void*>(&cataM2_0062ca80),
        reinterpret_cast<const void*>(&cataM2_00652ef0),
        reinterpret_cast<const void*>(&cataM2_0065f6f0),
        nullptr};
    return functions;
}
