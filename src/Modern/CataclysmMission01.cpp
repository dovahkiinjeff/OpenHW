/* Ported from our 2026-10-08 Ghidra recovery. Do not import raw retail
   pointers or process-layout casts here. Stable labels below identify recovered
   callbacks for save/load; original function addresses are never executed. */
#include "CataclysmScriptRuntime.h"
#include "CataclysmScriptIds.h"
#include <cstdint>
static void cataM1_005d6fe0();
static void cataM1_005d7050();
static void cataM1_005d78d0();
static void cataM1_005d7960();
static void cataM1_005d7990();
static void cataM1_005d7a10();
static void cataM1_005d7a40();
static void cataM1_005d7aa0();
static void cataM1_005d7b80();
static void cataM1_005d7be0();
static void cataM1_005d7c20();
static void cataM1_005d7c90();
static void cataM1_005d7cc0();
static void cataM1_005d7d10();
static void cataM1_005d7d40();
static void cataM1_005d7d90();
static void cataM1_005d7df0();
static void cataM1_005d7e30();
static void cataM1_005d7e80();
static void cataM1_005d7f60();
static void cataM1_005d7f90();
static void cataM1_005d7fd0();
static void cataM1_005d8010();
static void cataM1_005d80f0();
static void cataM1_005d8130();
static void cataM1_005d8180();
static void cataM1_005d8220();
static void cataM1_005d82e0();
static void cataM1_005d8340();
static void cataM1_005d8370();
static void cataM1_005d83b0();
static void cataM1_005d8400();
static void cataM1_005d84a0();
static void cataM1_005d8510();
static void cataM1_005d8580();
static void cataM1_005d85c0();
static void cataM1_005d85f0();
static void cataM1_005d8630();
static void cataM1_005d8660();
static void cataM1_005d86c0();
static void cataM1_005d8730();
static void cataM1_005d8750();
static void cataM1_005d87f0();
static void cataM1_005d8880();
static void cataM1_005d88f0();
static void cataM1_005d8930();
static void cataM1_005d8980();
static void cataM1_005d89d0();
static void cataM1_005d8a60();
static void cataM1_005d8bd0();
static void cataM1_005d8c30();
static void cataM1_005d8d20();
static void cataM1_005d8e80();
static void cataM1_005d8ef0();
static void cataM1_005d8f60();
static void cataM1_005d8fa0();
static void cataM1_005d9020();
static void cataM1_005d9050();
static void cataM1_005d90d0();
static void cataM1_005d9180();
static void cataM1_005d92a0();
static void cataM1_005d92d0();
static void cataM1_005d9310();
static void cataM1_005d9380();
static void cataM1_005d93f0();
static void cataM1_005d9430();
static void cataM1_005d9490();
static void cataM1_005d94e0();
static void cataM1_005d9600();
static void cataM1_005d9780();
static void cataM1_005d97b0();
static void cataM1_005d9800();
static void cataM1_005d9870();
static void cataM1_005d98f0();
static void cataM1_005d9930();
static void cataM1_005d9980();
static void cataM1_005d9a20();
static void cataM1_005d9af0();
static void cataM1_005d9b20();
static void cataM1_005da060();
static void cataM1_005da210();
static void cataM1_005da2c0();
static void cataM1_005db0b0();
static void cataM1_005db0f0();
static void cataM1_005db150();
static void cataM1_005f4730();
static void cataM1_00613940();
static void cataM1_0062ca80();
static void cataM1_00652ef0();

static void cataM1_005d6fe0(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491d30u,2,0xe364124,0);
  cataCall(0x00491d30u,2,0x50f0154,0);
  cataCall(0x00491d30u,2,0x99a7f04,0);
  cataCall(0x00491d30u,2,0xb5ee704,0);
  cataCall(0x00491d30u,2,0x8c7ac44,0);
  cataCall(0x00491d30u,2,0xd4d4d34,0);
  cataCall(0x00491d30u,2,0x83f97b5,0);
  return;
}

static void cataM1_005d7050(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0xd4d4d34);
  if (iVar1 != 0) {
    iVar1 = cataCall(0x00491e30u,1,0x83f97b5);
    if (iVar1 == 1) {
      iVar1 = cataCall(0x00491e30u,1,0xe364124);
      if (iVar1 == 1) {
        cataCall(0x00491db0u,1,0x83f97b5);
      }
    }
    iVar1 = cataCall(0x00491e30u,1,0x83f97b5);
    if (iVar1 == 2) {
      iVar1 = cataCall(0x00491e30u,1,0x50f0154);
      if (iVar1 == 1) {
        cataCall(0x00491db0u,1,0x83f97b5);
      }
    }
    iVar1 = cataCall(0x00491e30u,1,0x83f97b5);
    if (iVar1 == 3) {
      iVar1 = cataCall(0x00491e30u,1,0x99a7f04);
      if (iVar1 == 1) {
        cataCall(0x00491db0u,1,0x83f97b5);
      }
    }
    iVar1 = cataCall(0x00491e30u,1,0x83f97b5);
    if (iVar1 == 4) {
      iVar1 = cataCall(0x00491e30u,1,0xb5ee704);
      if (iVar1 == 1) {
        cataCall(0x00491db0u,1,0x83f97b5);
      }
    }
    iVar1 = cataCall(0x00491e30u,1,0x83f97b5);
    if (iVar1 == 5) {
      iVar1 = cataCall(0x00491e30u,1,0x8c7ac44);
      if (iVar1 == 1) {
        cataCall(0x00491db0u,1,0x83f97b5);
      }
    }
    iVar1 = cataCall(0x00491e30u,1,0x443cca1);
    if (iVar1 == 0) {
      iVar1 = cataCall(0x00491e30u,1,0x83f97b5);
      if (iVar1 == 1) {
        cataCall(0x00491d30u,2,0x443cca1,1);
        uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x588674);
        cataCall(0x00494a00u,2,uVar2);
      }
    }
    iVar1 = cataCall(0x00491e30u,1,0x443cca2);
    if (iVar1 == 0) {
      iVar1 = cataCall(0x00491e30u,1,0x83f97b5);
      if (iVar1 == 2) {
        cataCall(0x00491d30u,2,0x443cca2,1);
        uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x939002e);
        cataCall(0x00494a00u,2,uVar2);
      }
    }
    iVar1 = cataCall(0x00491e30u,1,0x443cca3);
    if (iVar1 == 0) {
      iVar1 = cataCall(0x00491e30u,1,0x83f97b5);
      if (iVar1 == 3) {
        cataCall(0x00491d30u,2,0x443cca3,1);
        uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x4d89963);
        cataCall(0x00494a00u,2,uVar2);
      }
    }
    iVar1 = cataCall(0x00491e30u,1,0x443cca4);
    if (iVar1 == 0) {
      iVar1 = cataCall(0x00491e30u,1,0x83f97b5);
      if (iVar1 == 4) {
        cataCall(0x00491d30u,2,0x443cca4,1);
        uVar2 = cataCall(0x004907a0u,1,0xb25c8d1,0x852cf18);
        cataCall(0x00494a00u,2,uVar2);
        uVar2 = cataCall(0x004907a0u,1,0x6a0d9b1,0x852cf18);
        cataCall(0x00494a00u,2,uVar2);
        uVar2 = cataCall(0x004907a0u,1,0xebc0bc1,0x852cf18);
        cataCall(0x00494a00u,2,uVar2);
        uVar2 = cataCall(0x004907a0u,1,0xeff5151,0x852cf18);
        cataCall(0x00494a00u,2,uVar2);
        uVar2 = cataCall(0x004907a0u,1,0xeff5152,0x852cf18);
        cataCall(0x00494a00u,2,uVar2);
      }
    }
    iVar1 = cataCall(0x00491e30u,1,0x443cca5);
    if (iVar1 == 0) {
      iVar1 = cataCall(0x00491e30u,1,0x83f97b5);
      if (iVar1 == 5) {
        cataCall(0x00491d30u,2,0x443cca5,1);
        uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0xf92dd92);
        cataCall(0x00494a00u,2,uVar2);
      }
    }
    iVar1 = cataCall(0x00491e30u,1,0x443cca6);
    if (iVar1 == 0) {
      iVar1 = cataCall(0x00491e30u,1,0x83f97b5);
      if (iVar1 == 6) {
        cataCall(0x00491d30u,2,0x443cca6,1);
        uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x5c43485);
        cataCall(0x00494a00u,2,uVar2);
      }
    }
    cataCall(0x00491d70u,2,0xd4d4d34,0);
    cataCall(0x004964e0u,1,cataScriptText(0x008c83b8));
  }
  iVar1 = cataCall(0x00492120u,1,0xde8950a);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00492120u,1,0x3b1a6a);
    if (iVar1 == 0) {
      iVar1 = cataCall(0x00492120u,1,0x96dfbfa);
      if (iVar1 != 0) {
        cataCall(0x00494e70u,3,cataScriptText(0x008c8384),
                     cataScriptLocalizedText(0x008c7f1c),
                     cataScriptLocalizedText(0x008c7f28));
        uVar2 = 0xcbd4dc3;
        goto LAB_005d7401;
      }
      iVar1 = cataCall(0x00492120u,1,0x3184ce6);
      if (iVar1 == 0) {
        iVar1 = cataCall(0x00492120u,1,0xd99b1ba);
        if (iVar1 != 0) {
          cataCall(0x00494e70u,3,cataScriptText(0x008c8370),
                       cataScriptLocalizedText(0x008c7f34),
                       cataScriptLocalizedText(0x008c7f40));
          uVar2 = 0x518e0d2;
          goto LAB_005d7401;
        }
        iVar1 = cataCall(0x00492120u,1,0xb0f775a);
        if (iVar1 == 0) {
          iVar1 = cataCall(0x00492120u,1,0xb315ffa);
          if (iVar1 != 0) {
            cataCall(0x004d6480u,0);
            cataCall(0x00494e70u,3,cataScriptText(0x008c7f90),cataScriptLocalizedText(0x008c7f64),
                         cataScriptLocalizedText(0x008c7f70));
            cataCall(0x00497290u,2,1,cataScriptLocalizedText(0x008c7ee0));
          }
        }
        else {
          cataCall(0x00494e70u,3,cataScriptText(0x008c8358),
                       cataScriptLocalizedText(0x008c7f4c),
                       cataScriptLocalizedText(0x008c7f58));
          uVar2 = cataCall(0x00490bb0u,1,0xcb402f8);
          cataCall(0x004963e0u,2,cataScriptText(0x008c83b8),uVar2);
        }
      }
      else {
        cataCall(0x00496cf0u,1,cataScriptText(0x0089056c));
      }
    }
    else {
      cataCall(0x00494e70u,3,cataScriptText(0x008c8398),
                   cataScriptLocalizedText(0x008c7f04),
                   cataScriptLocalizedText(0x008c7f10));
      uVar2 = cataCall(0x00490af0u,1,0xd56e434);
      cataCall(0x004964b0u,2,cataScriptText(0x008c83b8),uVar2);
    }
  }
  else {
    cataCall(0x00494e70u,3,cataScriptText(0x008c83a8),cataScriptLocalizedText(0x008c7eec),
                 cataScriptLocalizedText(0x008c7ef8));
    uVar2 = 0x4fcebd3;
LAB_005d7401:
    uVar2 = cataCall(0x004907e0u,1,uVar2);
    cataCall(0x004963e0u,2,cataScriptText(0x008c83b8),uVar2);
  }
  iVar1 = cataCall(0x00491e30u,1,0x443cca7);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00491e30u,1,0x87defce4);
    if (iVar1 == 1) {
      cataCall(0x00491d30u,2,0x443cca7,1);
      cataCall(0x00491d70u,2,0x83f97b5,1);
      cataCall(0x00491d70u,2,0xd4d4d34,1);
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443cca8);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x004907e0u,1,0x4fcebd3);
    iVar1 = cataCall(0x00492d20u,1,uVar2);
    if (iVar1 != 0) {
      uVar2 = cataCall(0x004907e0u,1,0xd788163);
      iVar1 = cataCall(0x00492d20u,1,uVar2);
      if (iVar1 != 0) goto LAB_005d7667;
    }
    cataCall(0x00491d30u,2,0x443cca8,1);
    uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x2d73454);
    cataCall(0x00494a00u,2,uVar2);
    cataCall(0x00491d70u,2,0xe364124,1);
    cataCall(0x00494f60u,1,cataScriptText(0x008c83a8));
    cataCall(0x00491d70u,2,0xd4d4d34,1);
  }
LAB_005d7667:
  iVar1 = cataCall(0x00491e30u,1,0x443cca9);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x004907e0u,1,0xcbd4dc3);
    iVar1 = cataCall(0x00499570u,1,uVar2);
    if (iVar1 != 0) {
      cataCall(0x00491d30u,2,0x443cca9,1);
      uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x6b533a4);
      cataCall(0x00494a00u,2,uVar2);
      cataCall(0x00491d70u,2,0x50f0154,1);
      cataCall(0x00494f60u,1,cataScriptText(0x008c8398));
      cataCall(0x00491d70u,2,0xd4d4d34,1);
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443ccaa);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x004907e0u,1,0xcbd4dc3);
    iVar1 = cataCall(0x00492d20u,1,uVar2);
    if (iVar1 == 0) {
      cataCall(0x00491d30u,2,0x443ccaa,1);
      uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x86cd624);
      cataCall(0x00494a00u,2,uVar2);
      cataCall(0x00491d70u,2,0x99a7f04,1);
      cataCall(0x00494f60u,1,cataScriptText(0x008c8384));
      cataCall(0x00491d70u,2,0xd4d4d34,1);
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443ccab);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x004907e0u,1,0x518e0d2);
    iVar1 = cataCall(0x00492d20u,1,uVar2);
    if (iVar1 == 0) {
      cataCall(0x00491d30u,2,0x443ccab,1);
      uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0xd87e234);
      cataCall(0x00494a00u,2,uVar2);
      cataCall(0x00491d70u,2,0x8c7ac44,1);
      cataCall(0x00494f60u,1,cataScriptText(0x008c8370));
      cataCall(0x00491d70u,2,0xd4d4d34,1);
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443ccac);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x004907e0u,1,0xb25c8d1);
    iVar1 = cataCall(0x00492d20u,1,uVar2);
    if (iVar1 == 0) {
      uVar2 = cataCall(0x004907e0u,1,0x6a0d9b1);
      iVar1 = cataCall(0x00492d20u,1,uVar2);
      if (iVar1 == 0) {
        uVar2 = cataCall(0x004907e0u,1,0xebc0bc1);
        iVar1 = cataCall(0x00492d20u,1,uVar2);
        if (iVar1 == 0) {
          cataCall(0x00491d30u,2,0x443ccac,1);
          uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x12f6cf4);
          cataCall(0x00494a00u,2,uVar2);
          cataCall(0x00491d70u,2,0xb5ee704,1);
          cataCall(0x00494f60u,1,cataScriptText(0x008c8358));
          cataCall(0x00491d70u,2,0xd4d4d34,1);
        }
      }
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443ccad);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x00490bb0u,1,0xca25860);
    iVar1 = cataCall(0x00492d20u,1,uVar2);
    if (iVar1 == 0) {
      iVar1 = cataCall(0x00491e30u,1,0x87defce4);
      if (iVar1 != 0) {
        cataCall(0x00491d30u,2,0x443ccad,1);
        uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0xd2247a4);
        cataCall(0x00494a00u,2,uVar2);
      }
    }
  }
  return;
}

static void cataM1_005d78d0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x443ccae);
  if (iVar1 == 0) {
    cataCall(0x00491d30u,2,0x443ccae,1);
    cataCall(0x00498c70u,1,cataShips());
    cataCall(0x00494160u,1,0);
    cataCall(0x00497ed0u,1,cataShips());
  }
  cataCall(0x00493aa0u,1,0x50);
  cataCall(0x00493980u,2,cataShips(),0x32);
  cataCall(0x00490640u,3,0x633addb,cataM1_005d7960,cataM1_005d7990);
  return;
}

static void cataM1_005d7960(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x004907e0u,1,0x4fcebd3);
  cataCall(0x00492190u,1,uVar1);
  return;
}

static void cataM1_005d7990(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492ce0u,0);
  if (iVar1 == 0) {
    cataCall(0x004949b0u,1,0x339afa2);
    cataCall(0x00490640u,3,0x53a0c,cataM1_00652ef0,cataM1_00652ef0);
    return;
  }
  uVar2 = cataCall(0x004907e0u,1,0x4fcebd3);
  iVar1 = cataCall(0x00492d20u,1,uVar2);
  if (iVar1 == 0) {
    cataCall(0x00490640u,3,0x1ff3a12,cataM1_005d7a10,cataM1_005d7a40);
  }
  return;
}

static void cataM1_005d7a10(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x004907e0u,1,0xbaec012);
  cataCall(0x00492190u,1,uVar1);
  return;
}

static void cataM1_005d7a40(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x004907e0u,1,0xbaec012);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  if (iVar2 == 0) {
    uVar1 = cataCall(0x004907a0u,1,0x39b6c01,0xe2b10e5);
    cataCall(0x00494a00u,2,uVar1);
    cataCall(0x00490640u,3,0x3669d95,cataM1_005d7aa0,cataM1_00652ef0);
  }
  return;
}

static void cataM1_005d7aa0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00493980u,2,cataShips(),10);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  uVar1 = cataCall(0x00490af0u,1,0x90e96b5,uVar1);
  cataCall(0x00492db0u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495620u,2,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x518e0d2);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495c40u,2,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x53ec482);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495c40u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492190u,1,uVar1);
  return;
}

static void cataM1_005d7b80(void)
{
  int iVar1;
  
  cataScriptScope(0);
  cataCall(0x0049aae0u,2,cataShips(),6);
  iVar1 = cataCall(0x00491e30u,1,0x443ccaf);
  if (iVar1 == 0) {
    cataCall(0x00491d30u,2,0x443ccaf,1);
    cataCall(0x00494160u,1,0);
    cataCall(0x00497ed0u,1,cataShips());
  }
  return;
}

static void cataM1_005d7be0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492ce0u,0);
  if (iVar1 == 0) {
    uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0xfd97064);
    cataCall(0x00494a00u,2,uVar2);
  }
  return;
}

static void cataM1_005d7c20(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x443ccb0);
  if (iVar1 == 0) {
    cataCall(0x00491d30u,2,0x443ccb0,1);
    cataCall(0x00494160u,1,0);
    cataCall(0x00497ed0u,1,cataShips());
  }
  cataCall(0x00490640u,3,0x7464cb7,cataM1_005d7c90,cataM1_005d7cc0);
  return;
}

static void cataM1_005d7c90(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490aa0u,1,0xd43c27c);
  cataCall(0x004929d0u,1,uVar1);
  return;
}

static void cataM1_005d7cc0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x852cf18);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x857bcb7,cataM1_005d7d10,cataM1_005d7d40);
  }
  return;
}

static void cataM1_005d7d10(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0xcb402f8);
  cataCall(0x00492190u,1,uVar1);
  return;
}

static void cataM1_005d7d40(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0xcb402f8);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  if (iVar2 == 0) {
    cataCall(0x00490640u,3,0x7464cb7,cataM1_005d7c90,cataM1_005d7cc0);
  }
  return;
}

static void cataM1_005d7d90(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x443ccb1);
  if (iVar1 == 0) {
    cataCall(0x00491d30u,2,0x443ccb1,1);
    cataCall(0x00498c70u,1,cataShips());
    cataCall(0x00494160u,1,0);
    cataCall(0x00497ed0u,1,cataShips());
  }
  return;
}

static void cataM1_005d7df0(void)
{
  
  cataScriptScope(0);
  cataCall(0x00498c70u,1,cataShips());
  cataCall(0x00490640u,3,0x7bd9e7b,cataM1_005d7e30,cataM1_005d7e80);
  return;
}

static void cataM1_005d7e30(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00491f90u,2,0x84e59f2,7);
  cataCall(0x00493980u,2,cataShips(),0x32);
  uVar1 = cataCall(0x004907e0u,1,0xd788163);
  cataCall(0x00492190u,1,uVar1);
  return;
}

static void cataM1_005d7e80(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492060u,1,0x84e59f2);
  if (iVar1 != 0) {
    cataCall(0x00491f00u,2,0x84e59f2,7);
    cataCall(0x00491f50u,1,0x84e59f2);
    uVar2 = cataCall(0x004908c0u,1,0xd788163,10000);
    uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
    cataCall(0x00495a50u,4,uVar2);
    uVar2 = cataCall(0x004909f0u,0);
    uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
    cataCall(0x0049a100u,2,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x36980eb);
    cataCall(0x00492190u,1,uVar2);
  }
  uVar2 = cataCall(0x004907e0u,1,0xd788163);
  iVar1 = cataCall(0x00492d20u,1,uVar2);
  if (iVar1 == 0) {
    cataCall(0x00490640u,3,0x9dbf1e0,cataM1_005d7f60,cataM1_00652ef0);
  }
  return;
}

static void cataM1_005d7f60(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0xca25860);
  cataCall(0x00492190u,1,uVar1);
  return;
}

static void cataM1_005d7f90(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00498c70u,1,cataShips());
  uVar1 = cataCall(0x004907e0u,1,0x2baec02);
  cataCall(0x00492960u,1,uVar1);
  return;
}

static void cataM1_005d7fd0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00498c70u,1,cataShips());
  uVar1 = cataCall(0x00490960u,1,0xd56e434);
  cataCall(0x004941a0u,1,uVar1);
  return;
}

static void cataM1_005d8010(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x443ccb2);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00499570u,1,cataShips());
    if (iVar1 != 0) {
      cataCall(0x00491d30u,2,0x443ccb2,1);
      cataCall(0x00495f00u,0);
    }
  }
  iVar1 = cataCall(0x00491e30u,1,0x443ccb3);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00496e40u,0);
    if (iVar1 == 0) {
      iVar1 = cataCall(0x00492ce0u,0);
      if (iVar1 != 0) {
        cataCall(0x00491d30u,2,0x443ccb3,1);
        uVar2 = cataCall(0x004907a0u,1,0xec6a6b1,0x544e6c3);
        cataCall(0x00494a00u,2,uVar2);
        uVar2 = cataCall(0x004907a0u,1,0xec6a6b2,0x544e6c3);
        cataCall(0x00494a00u,2,uVar2);
        uVar2 = cataCall(0x004907a0u,1,0xec6a6b3,0x544e6c3);
        cataCall(0x00494a00u,2,uVar2);
      }
    }
  }
  return;
}

static void cataM1_005d80f0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00498c70u,1,cataShips());
  uVar1 = cataCall(0x004907e0u,1,0xcbd4dc3);
  cataCall(0x00492960u,1,uVar1);
  return;
}

static void cataM1_005d8130(void)
{
  
  cataScriptScope(0);
  cataCall(0x004984c0u,2,cataShips(),1);
  cataCall(0x00498c70u,1,cataShips());
  cataCall(0x00490640u,3,0x569b0c5,cataM1_005d8180,cataM1_005d8220);
  return;
}

static void cataM1_005d8180(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00493980u,2,cataShips(),10);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  uVar1 = cataCall(0x00490af0u,1,0x90e96b5,uVar1);
  cataCall(0x00492db0u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495560u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492190u,1,uVar1);
  cataCall(0x00493aa0u,1,0x28);
  return;
}

static void cataM1_005d8220(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x4b095);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x4b095,cataM1_005d82e0,cataM1_005d8340);
    return;
  }
  uVar2 = cataCall(0x00490bb0u,1,0x5920,5000);
  iVar1 = cataCall(0x00492e90u,2,uVar2);
  if (iVar1 != 0) {
    uVar2 = cataCall(0x00490bb0u,1,0x5920,0);
    uVar2 = cataCall(0x00490bb0u,1,0x5920,uVar2);
    iVar1 = cataCall(0x004985f0u,3,uVar2);
    if (iVar1 != 0) {
      cataCall(0x004984c0u,2,cataShips(),0);
      return;
    }
  }
  cataCall(0x004984c0u,2,cataShips(),1);
  return;
}

static void cataM1_005d82e0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00493980u,2,cataShips(),0x32);
  cataCall(0x00493a10u,2,cataShips(),0);
  cataCall(0x004984c0u,2,cataShips(),0);
  uVar1 = cataCall(0x00490aa0u,1,0x50fd188);
  cataCall(0x004929d0u,1,uVar1);
  return;
}

static void cataM1_005d8340(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492b20u,0);
  if (0x14 < iVar1) {
    cataCall(0x00493aa0u,1,0x14);
  }
  return;
}

static void cataM1_005d8370(void)
{
  
  cataScriptScope(0);
  cataCall(0x00498c70u,1,cataShips());
  cataCall(0x00490640u,3,0x498cf55,cataM1_00652ef0,cataM1_005d83b0);
  return;
}

static void cataM1_005d83b0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x544e6c3);
  if (iVar1 != 0) {
    cataCall(0x004947c0u,0);
    cataCall(0x00490640u,3,0x506a278,cataM1_00652ef0,cataM1_005d8580);
  }
  return;
}

static void cataM1_005d8400(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492ce0u,0);
  if (iVar1 == 0) {
    cataCall(0x00490640u,3,0x4869685,cataM1_005d84a0,cataM1_005d8510);
    return;
  }
  uVar2 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492d10u,1,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0xca25860);
  uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
  cataCall(0x00495bd0u,2,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x36980eb,10000);
  cataCall(0x00492e90u,2,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492190u,1,uVar2);
  return;
}

static void cataM1_005d84a0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00495eb0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x52a7);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x2baec02,1);
  uVar1 = cataCall(0x00493e00u,0,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x52a7,cataScriptText(0x0088b404),uVar1);
  cataCall(0x00499c70u,5,uVar1);
  return;
}

static void cataM1_005d8510(void)
{
  CataValue uVar1;
  int iVar2;
  int iVar3;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x52a7);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  iVar3 = cataCall(0x00493e00u,0);
  if (iVar3 <= iVar2) {
    uVar1 = cataCall(0x00490bb0u,1,0x52a7);
    cataCall(0x00493cf0u,2,cataTeam(),uVar1);
    cataCall(0x00490640u,3,0x506a278,cataM1_00652ef0,cataM1_005d8580);
  }
  return;
}

static void cataM1_005d8580(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x857bcb7,cataM1_00652ef0,cataM1_005d8400);
  }
  return;
}

static void cataM1_005d85c0(void)
{
  
  cataScriptScope(0);
  cataCall(0x00498c70u,1,cataShips());
  return;
}

static void cataM1_005d85f0(void)
{
  
  cataScriptScope(0);
  cataCall(0x00498c70u,1,cataShips());
  cataCall(0x00490640u,3,0x5569d3,cataM1_005d8630,cataM1_005d8660);
  return;
}

static void cataM1_005d8630(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x004907a0u,1,0xbaec012);
  cataCall(0x00494750u,1,uVar1);
  return;
}

static void cataM1_005d8660(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00494940u,0);
  if (iVar1 != 0) {
    uVar2 = cataCall(0x004907a0u,1,0xbaec012,0x82fb2f4);
    cataCall(0x00494a00u,2,uVar2);
    cataCall(0x00490640u,3,0x4e2d3,cataM1_00652ef0,cataM1_005d86c0);
  }
  return;
}

static void cataM1_005d86c0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x339afa2);
  if (iVar1 != 0) {
    cataCall(0x00491f90u,2,0x84e59f2,0xd);
    return;
  }
  iVar1 = cataCall(0x004920a0u,1,0x84e59f2);
  if (iVar1 != 0) {
    cataCall(0x004947c0u,0);
    cataCall(0x00490640u,3,0x506a278,cataM1_00652ef0,cataM1_005d88f0);
  }
  return;
}

static void cataM1_005d8730(void)
{
  
  cataScriptScope(0);
  cataCall(0x00492810u,0);
  return;
}

static void cataM1_005d8750(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492ce0u,0);
  if (iVar1 == 0) {
    cataCall(0x00490640u,3,0x4869685,cataM1_005d87f0,cataM1_005d8880);
    return;
  }
  uVar2 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492d10u,1,uVar2);
  uVar2 = cataCall(0x00490820u,1,0xca25860,10000);
  uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
  cataCall(0x00495a50u,4,uVar2);
  uVar2 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492190u,1,uVar2);
  return;
}

static void cataM1_005d87f0(void)
{
  CataValue uVar1;
  int iVar2;
  const char *pcVar3;
  
  cataScriptScope(0);
  cataCall(0x00495eb0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x52a7);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0xc37e6b1);
  iVar2 = cataCall(0x00493de0u,1,uVar1);
  if (iVar2 == 0) {
    uVar1 = cataCall(0x004907e0u,1,0xbaec012,1);
    uVar1 = cataCall(0x00493e00u,0,uVar1);
    pcVar3 = cataScriptText(0x0088b4b4);
  }
  else {
    uVar1 = cataCall(0x004907e0u,1,0xbaec012,1);
    uVar1 = cataCall(0x00493e00u,0,uVar1);
    pcVar3 = cataScriptText(0x0088b464);
  }
  uVar1 = cataCall(0x00490bb0u,1,0x52a7,pcVar3,uVar1);
  cataCall(0x00499c70u,5,uVar1);
  return;
}

static void cataM1_005d8880(void)
{
  CataValue uVar1;
  int iVar2;
  int iVar3;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x52a7);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  iVar3 = cataCall(0x00493e00u,0);
  if (iVar3 <= iVar2) {
    uVar1 = cataCall(0x00490bb0u,1,0x52a7);
    cataCall(0x00493cf0u,2,cataTeam(),uVar1);
    cataCall(0x00490640u,3,0x506a278,cataM1_00652ef0,cataM1_005d88f0);
  }
  return;
}

static void cataM1_005d88f0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x857bcb7,cataM1_005d8730,cataM1_005d8750);
  }
  return;
}

static void cataM1_005d8930(void)
{
  
  cataScriptScope(0);
  cataCall(0x00498c70u,1,cataShips());
  cataCall(0x004977a0u,1,cataShips());
  cataCall(0x00490640u,3,0x348ce77,cataM1_00652ef0,cataM1_005d8980);
  return;
}

static void cataM1_005d8980(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x852cf18);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x58440c8,cataM1_005d89d0,cataM1_005d8a60);
  }
  return;
}

static void cataM1_005d89d0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00497800u,1,cataShips());
  uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,cataScriptText(0x0088b570));
  uVar1 = cataCall(0x00490bb0u,1,0xb87dad3,uVar1);
  cataCall(0x00495690u,3,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0xb87dad3);
  uVar1 = cataCall(0x00490bb0u,1,0x7893600,uVar1);
  cataCall(0x00498d90u,2,uVar1);
  uVar1 = cataCall(0x00490820u,1,0x7893600);
  cataCall(0x004941a0u,1,uVar1);
  cataCall(0x00491f90u,2,0x8f2a9c2,6);
  return;
}

static void cataM1_005d8a60(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492060u,1,0x8f2a9c2);
  if (iVar1 != 0) {
    uVar2 = cataCall(0x004909f0u,0,10000);
    uVar2 = cataCall(0x00490bb0u,1,0x5296679,uVar2);
    cataCall(0x004931a0u,3,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x5296679);
    uVar2 = cataCall(0x00490bb0u,1,0x5296679,uVar2);
    iVar1 = cataCall(0x00495560u,2,uVar2);
    if (iVar1 == 0) {
      uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,cataScriptText(0x0088b570));
      uVar2 = cataCall(0x00490bb0u,1,0xb87dad3,uVar2);
      iVar1 = cataCall(0x00495690u,3,uVar2);
      if (iVar1 != 0) {
        uVar2 = cataCall(0x00490bb0u,1,0xb87dad3);
        uVar2 = cataCall(0x00490bb0u,1,0x7893600,uVar2);
        cataCall(0x00498d90u,2,uVar2);
        uVar2 = cataCall(0x00490820u,1,0x7893600);
        cataCall(0x004941a0u,1,uVar2);
      }
    }
    else {
      iVar1 = cataCall(0x00491e30u,1,0x443ccb4);
      if (iVar1 == 0) {
        uVar2 = cataCall(0x00490bb0u,1,0x5296679,cataScriptText(0x0088b570));
        uVar2 = cataCall(0x00490bb0u,1,0x53a0c,uVar2);
        iVar1 = cataCall(0x00495690u,3,uVar2);
        if (iVar1 != 0) {
          cataCall(0x00491d30u,2,0x443ccb4,1);
          uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0xf19cfe7);
          cataCall(0x00494a00u,2,uVar2);
        }
      }
      uVar2 = cataCall(0x00490bb0u,1,0x5296679);
      cataCall(0x00492190u,1,uVar2);
    }
    cataCall(0x00491f00u,2,0x8f2a9c2,6);
    cataCall(0x00491f50u,1,0x8f2a9c2);
  }
  return;
}

static void cataM1_005d8bd0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  cataCall(0x00493980u,2,cataShips(),10);
  cataCall(0x00494160u,1,0);
  iVar1 = cataCall(0x00492ce0u,0);
  if (0 < iVar1) {
    cataCall(0x00497ed0u,1,cataShips());
  }
  cataCall(0x00490640u,3,0x857bcb7,cataM1_005d8c30,cataM1_005d8d20);
  return;
}

static void cataM1_005d8c30(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  uVar1 = cataCall(0x00490af0u,1,0x90e96b5,uVar1);
  cataCall(0x00492db0u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495620u,2,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x518e0d2);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495c40u,2,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x53ec482);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495c40u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492190u,1,uVar1);
  uVar1 = cataCall(0x00492910u,2,1,7);
  cataCall(0x00491f90u,2,0x84e59f2,uVar1);
  return;
}

static void cataM1_005d8d20(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492ce0u,0);
  if (iVar1 != 0) {
    iVar1 = cataCall(0x004948a0u,0);
    if (iVar1 != 0) {
      iVar1 = cataCall(0x00492060u,1,0x84e59f2);
      if (iVar1 != 0) {
        uVar2 = cataCall(0x00490bb0u,1,0x5920);
        cataCall(0x00492d10u,1,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb);
        cataCall(0x00492d10u,1,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x5920);
        uVar2 = cataCall(0x00490af0u,1,0x90e96b5,uVar2);
        cataCall(0x00492db0u,2,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x5920);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
        cataCall(0x00495620u,2,uVar2);
        uVar2 = cataCall(0x004907e0u,1,0x518e0d2);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
        cataCall(0x00495c40u,2,uVar2);
        uVar2 = cataCall(0x004907e0u,1,0x53ec482);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
        cataCall(0x00495c40u,2,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb);
        cataCall(0x00492190u,1,uVar2);
        cataCall(0x00491f00u,2,0x84e59f2,7);
        cataCall(0x00491f50u,1,0x84e59f2);
      }
    }
    return;
  }
  uVar2 = cataCall(0x004907a0u,1,0xaa19331);
  iVar1 = cataCall(0x00493de0u,1,uVar2);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x4869685,cataM1_005d8e80,cataM1_005d8ef0);
    return;
  }
  cataCall(0x00490640u,3,0x53a0c,cataM1_00652ef0,cataM1_00652ef0);
  return;
}

static void cataM1_005d8e80(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00495eb0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x52a7);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0xc0abd03,0);
  uVar1 = cataCall(0x00493e00u,0,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x52a7,cataScriptText(0x0088b5ac),uVar1);
  cataCall(0x00499c70u,5,uVar1);
  return;
}

static void cataM1_005d8ef0(void)
{
  CataValue uVar1;
  int iVar2;
  int iVar3;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x52a7);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  iVar3 = cataCall(0x00493e00u,0);
  if (iVar3 <= iVar2) {
    uVar1 = cataCall(0x00490bb0u,1,0x52a7);
    cataCall(0x00493cf0u,2,cataTeam(),uVar1);
    cataCall(0x00490640u,3,0x506a278,cataM1_00652ef0,cataM1_005d8f60);
  }
  return;
}

static void cataM1_005d8f60(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x857bcb7,cataM1_005d8c30,cataM1_005d8d20);
  }
  return;
}

static void cataM1_005d8fa0(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  cataCall(0x00498c70u,1,cataShips());
  cataCall(0x00493980u,2,cataShips(),10);
  uVar1 = cataCall(0x004907a0u,1,0x53ec482);
  iVar2 = cataCall(0x00493de0u,1,uVar1);
  if (iVar2 != 0) {
    cataCall(0x00490640u,3,0x4937f94,cataM1_005d9020,cataM1_005d9050);
    return;
  }
  cataCall(0x00490640u,3,0x857bcb7,cataM1_005d90d0,cataM1_005d9180);
  return;
}

static void cataM1_005d9020(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x004907a0u,1,0x6d3ec02);
  cataCall(0x00494750u,1,uVar1);
  return;
}

static void cataM1_005d9050(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0x506a278);
  if (iVar1 != 0) {
    cataCall(0x004947c0u,0);
    uVar2 = cataCall(0x004907a0u,1,0x53ec482);
    iVar1 = cataCall(0x00493de0u,1,uVar2);
    if (iVar1 != 0) {
      cataCall(0x00490640u,3,0xe114982,cataM1_005d92a0,cataM1_005d92d0);
      return;
    }
    cataCall(0x00490640u,3,0x857bcb7,cataM1_005d90d0,cataM1_005d9180);
  }
  return;
}

static void cataM1_005d90d0(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  uVar1 = cataCall(0x00490af0u,1,0x90e96b5,uVar1);
  cataCall(0x00492db0u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495560u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492190u,1,uVar1);
  uVar1 = cataCall(0x00492910u,2,1,7);
  cataCall(0x00491f90u,2,0x84e59f2,uVar1);
  return;
}

static void cataM1_005d9180(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492ce0u,0);
  if (iVar1 != 0) {
    iVar1 = cataCall(0x004948a0u,0);
    if (iVar1 != 0) {
      iVar1 = cataCall(0x00492060u,1,0x84e59f2);
      if (iVar1 != 0) {
        uVar2 = cataCall(0x00490bb0u,1,0x5920);
        cataCall(0x00492d10u,1,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb);
        cataCall(0x00492d10u,1,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x5920);
        uVar2 = cataCall(0x00490af0u,1,0x90e96b5,uVar2);
        cataCall(0x00492db0u,2,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x5920);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
        cataCall(0x00495560u,2,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb);
        cataCall(0x00492190u,1,uVar2);
        cataCall(0x00491f00u,2,0x84e59f2,7);
        cataCall(0x00491f50u,1,0x84e59f2);
      }
    }
    return;
  }
  uVar2 = cataCall(0x004907a0u,1,0x5dce6d1);
  iVar1 = cataCall(0x00493de0u,1,uVar2);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x4869685,cataM1_005d9310,cataM1_005d9380);
    return;
  }
  cataCall(0x00490640u,3,0x53a0c,cataM1_00652ef0,cataM1_00652ef0);
  return;
}

static void cataM1_005d92a0(void)
{
  
  cataScriptScope(0);
  cataCall(0x004939d0u,1,cataShips());
  return;
}

static void cataM1_005d92d0(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 != 0) {
    uVar2 = cataCall(0x004907e0u,1,0x518e0d2);
    cataCall(0x00492960u,1,uVar2);
  }
  return;
}

static void cataM1_005d9310(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00495eb0u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x52a7);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x004907e0u,1,0x6d3ec02,1);
  uVar1 = cataCall(0x00493e00u,0,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x52a7,cataScriptText(0x0088b4b4),uVar1);
  cataCall(0x00499c70u,5,uVar1);
  return;
}

static void cataM1_005d9380(void)
{
  CataValue uVar1;
  int iVar2;
  int iVar3;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x52a7);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  iVar3 = cataCall(0x00493e00u,0);
  if (iVar3 <= iVar2) {
    uVar1 = cataCall(0x00490bb0u,1,0x52a7);
    cataCall(0x00493cf0u,2,cataTeam(),uVar1);
    cataCall(0x00490640u,3,0x506a278,cataM1_00652ef0,cataM1_005d93f0);
  }
  return;
}

static void cataM1_005d93f0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x857bcb7,cataM1_005d90d0,cataM1_005d9180);
  }
  return;
}

static void cataM1_005d9430(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00498c70u,1,cataShips());
  uVar1 = cataCall(0x004907a0u,1,0x6d3ec02);
  cataCall(0x00494750u,1,uVar1);
  cataCall(0x00491d30u,2,0x8a928144,1);
  cataCall(0x00490640u,3,0xc2ee6b2,cataM1_00652ef0,cataM1_005d9490);
  return;
}

static void cataM1_005d9490(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0xcb5e56e);
  if (iVar1 != 0) {
    cataCall(0x004947c0u,0);
    cataCall(0x00490640u,3,0x506a278,cataM1_00652ef0,cataM1_005d9870);
  }
  return;
}

static void cataM1_005d94e0(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  cataCall(0x00492780u,0);
  uVar1 = cataCall(0x00490bb0u,1,0x4a2a29,cataScriptText(0x0088b940));
  uVar1 = cataCall(0x00490bb0u,1,0xc46fa23,uVar1);
  cataCall(0x00495690u,3,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0xc46fa23);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  if (iVar2 == 0) {
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
    cataCall(0x00492d10u,1,uVar1);
    uVar1 = cataCall(0x00490bb0u,1,0x4a2a29);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x00495bd0u,2,uVar1);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x004952b0u,2,uVar1);
    uVar1 = cataCall(0x004909f0u,0);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
    cataCall(0x0049a100u,2,uVar1);
    uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
    cataCall(0x00492190u,1,uVar1);
    cataCall(0x00491f90u,2,0x84e59f2,7);
    return;
  }
  uVar1 = cataCall(0x00490bb0u,1,0xc46fa23);
  cataCall(0x00492190u,1,uVar1);
  cataCall(0x00491f90u,2,0x84e59f2,7);
  return;
}

static void cataM1_005d9600(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492ce0u,0);
  if (iVar1 != 0) {
    iVar1 = cataCall(0x00499870u,1,cataShips());
    if (iVar1 != 0) {
      uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,cataScriptText(0x0088b940));
      uVar2 = cataCall(0x00490bb0u,1,0xc46fa23,uVar2);
      cataCall(0x00495690u,3,uVar2);
      uVar2 = cataCall(0x00490bb0u,1,0xc46fa23);
      iVar1 = cataCall(0x00492d20u,1,uVar2);
      if (iVar1 == 0) {
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb);
        cataCall(0x00492d10u,1,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x4a2a29);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
        cataCall(0x00495bd0u,2,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
        cataCall(0x004952b0u,2,uVar2);
        uVar2 = cataCall(0x004909f0u,0);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb,uVar2);
        cataCall(0x0049a100u,2,uVar2);
        uVar2 = cataCall(0x00490bb0u,1,0x36980eb);
        cataCall(0x00492190u,1,uVar2);
      }
      else {
        uVar2 = cataCall(0x00490bb0u,1,0xc46fa23);
        cataCall(0x00492190u,1,uVar2);
      }
      cataCall(0x00491f00u,2,0x84e59f2,7);
      cataCall(0x00491f50u,1,0x84e59f2);
    }
    return;
  }
  iVar1 = cataCall(0x00491e30u,1,0x8a928144);
  if (iVar1 < 3) {
    cataCall(0x00491db0u,1,0x8a928144);
    cataCall(0x00490640u,3,0x8832365,cataM1_005d9780,cataM1_005d97b0);
    return;
  }
  cataCall(0x00490640u,3,0x53a0c,cataM1_00652ef0,cataM1_00652ef0);
  return;
}

static void cataM1_005d9780(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491f90u,2,0x8e0c8c2,0x2d);
  return;
}

static void cataM1_005d97b0(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004920a0u,1,0x8e0c8c2);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x4869685,cataM1_005d9310,cataM1_005d9800);
  }
  return;
}

static void cataM1_005d9800(void)
{
  CataValue uVar1;
  int iVar2;
  int iVar3;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0x52a7);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  iVar3 = cataCall(0x00493e00u,0);
  if (iVar3 <= iVar2) {
    uVar1 = cataCall(0x00490bb0u,1,0x52a7);
    cataCall(0x00493cf0u,2,cataTeam(),uVar1);
    cataCall(0x00490640u,3,0x506a278,cataM1_00652ef0,cataM1_005d9870);
  }
  return;
}

static void cataM1_005d9870(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x004948a0u,0);
  if (iVar1 != 0) {
    iVar1 = cataCall(0x00491e30u,1,0x443ccb5);
    if (iVar1 == 0) {
      cataCall(0x00491d30u,2,0x443ccb5,1);
      uVar2 = cataCall(0x004907a0u,1,0xa8a880c,0x00a2ba31u);
      cataCall(0x00494a00u,2,uVar2);
    }
    cataCall(0x00490640u,3,0x857bcb7,cataM1_005d94e0,cataM1_005d9600);
  }
  return;
}

static void cataM1_005d98f0(void)
{
  
  cataScriptScope(0);
  cataCall(0x00498c70u,1,cataShips());
  cataCall(0x00490640u,3,0xe98cf15,cataM1_00652ef0,cataM1_005d9930);
  return;
}

static void cataM1_005d9930(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0xe2b10e5);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x3669d95,cataM1_005d9980,cataM1_00652ef0);
  }
  return;
}

static void cataM1_005d9980(void)
{
  CataValue uVar1;
  
  cataScriptScope(0);
  cataCall(0x00493980u,2,cataShips(),10);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492d10u,1,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  uVar1 = cataCall(0x00490af0u,1,0x90e96b5,uVar1);
  cataCall(0x00492db0u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x5920);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb,uVar1);
  cataCall(0x00495560u,2,uVar1);
  uVar1 = cataCall(0x00490bb0u,1,0x36980eb);
  cataCall(0x00492190u,1,uVar1);
  return;
}

static void cataM1_005d9a20(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491ec0u,1,CATA_MISSION1_BRIEFING_TIMER);
  cataCall(0x00491d30u,2,0xfdbd41e,0);
  cataCall(0x00491d30u,2,0x68087c5,0);
  cataCall(0x00491d30u,2,0xca04b13,0);
  cataCall(0x00491d30u,2,0x32243d3,0);
  cataCall(0x00491d30u,2,0xa4e5ad3,0);
  cataCall(0x00491d30u,2,0xddfdd03,0);
  cataCall(0x00491d30u,2,0xa072543,0);
  cataCall(0x00491d30u,2,0x79d69c2,0);
  cataCall(0x00491d30u,2,0x78e6a4e,1);
  cataCall(0x00496720u,1,cataScriptText(0x008ab8b8));
  cataCall(0x00496720u,1,cataScriptText(0x008c83c0));
  cataCall(0x00491d30u,2,0x4a5e094,0);
  cataCall(0x00490640u,3,0x2e80a01,cataM1_005d9af0,cataM1_005d9b20);
  return;
}

static void cataM1_005d9af0(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491d30u,2,0x8afa30c4,1);
  return;
}

static void cataM1_005d9b20(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00491e30u,1,0x8c641d25);
  if (iVar1 == 1) {
    cataCall(0x00498230u,0);
    // The fleet already exists in OpenHW. Populate both mission selections
    // before any team can check if the Kuun-Lan is lost. The arrival engine
    // sets ShipsHaveArrived when the hyperspace arrival actually completes.
    uVar2 = cataCall(0x00490bb0u,1,0xcff5860);
    cataCall(0x00498790u,1,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0xca25860);
    cataCall(0x00498790u,1,uVar2);
    cataCall(0x00497000u,1,0x24);
    uVar2 = cataCall(0x004907a0u,1,0xc0abd03);
    cataCall(0x00490660u,4,0xc0abd03,cataM1_005d7d90,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xaa19331);
    cataCall(0x00490660u,4,0x2cce515,cataM1_005d8bd0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xcccfbf3);
    cataCall(0x00490660u,4,0x2cce515,cataM1_005d8bd0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x6d3a243);
    cataCall(0x00490660u,4,0x2cce515,cataM1_005d8bd0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xf07bae3);
    cataCall(0x00490660u,4,0x2cce515,cataM1_005d8bd0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x2baec02);
    cataCall(0x00490660u,4,0x2baec02,cataM1_005d85c0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x5dce6d1);
    cataCall(0x00490660u,4,0xa3b4115,cataM1_005d8fa0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xd7ee851);
    cataCall(0x00490660u,4,0xa3b4115,cataM1_005d8fa0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x4e96503);
    cataCall(0x00490660u,4,0xa3b4115,cataM1_005d8fa0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x518e0d2);
    cataCall(0x00490660u,4,0x518e0d2,cataM1_005d8130,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x53ec482);
    cataCall(0x00490660u,4,0xa3b4115,cataM1_005d8fa0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x1a91ed2);
    cataCall(0x00490660u,4,0x47289eb,cataM1_005d8930,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xd788163);
    cataCall(0x00490660u,4,0xd788163,cataM1_005d78d0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x4fcebd3);
    cataCall(0x00490660u,4,0x4fcebd3,cataM1_005d7df0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x4e6b1d9);
    cataCall(0x00490660u,4,0x4e6b1d9,cataM1_005d7b80,cataM1_005d7be0,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xeff5151);
    cataCall(0x00490660u,4,0xeff514,cataM1_005d7c20,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xeff5152);
    cataCall(0x00490660u,4,0xeff514,cataM1_005d7c20,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xb25c8d1);
    cataCall(0x00490660u,4,0x47289eb,cataM1_005d8930,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x6a0d9b1);
    cataCall(0x00490660u,4,0x47289eb,cataM1_005d8930,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xebc0bc1);
    cataCall(0x00490660u,4,0x47289eb,cataM1_005d8930,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x3346335);
    cataCall(0x00490660u,4,0x3346335,cataM1_005d7f90,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xcbd4dc3);
    cataCall(0x00490660u,4,0xcbd4dc3,cataM1_005d7fd0,cataM1_005d8010,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x427eea5);
    cataCall(0x00490660u,4,0x427eea5,cataM1_005d80f0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xcbe2441);
    cataCall(0x00490660u,4,0x5a79103,cataM1_005d85f0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xcbe2442);
    cataCall(0x00490660u,4,0x5a79103,cataM1_005d85f0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xc37e6b1);
    cataCall(0x00490660u,4,0x5a79103,cataM1_005d85f0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xbaec012);
    cataCall(0x00490660u,4,0xbaec012,cataM1_005d85c0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xec6a6b1);
    cataCall(0x00490660u,4,0xdec6a65,cataM1_005d8370,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xec6a6b2);
    cataCall(0x00490660u,4,0xdec6a65,cataM1_005d8370,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xec6a6b3);
    cataCall(0x00490660u,4,0xdec6a65,cataM1_005d8370,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0xa388bd1);
    cataCall(0x00490660u,4,0x54a88a2,cataM1_005d9430,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x004907a0u,1,0x39b6c01);
    cataCall(0x00490660u,4,0x539b6c4,cataM1_005d98f0,cataM1_0062ca80,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0xcb402f8);
    cataCall(0x00492d10u,1,uVar2);
    uVar2 = cataCall(0x004907e0u,1,0xb25c8d1);
    uVar2 = cataCall(0x00490bb0u,1,0xcb402f8,uVar2);
    cataCall(0x00495bd0u,2,uVar2);
    uVar2 = cataCall(0x004907e0u,1,0x6a0d9b1);
    uVar2 = cataCall(0x00490bb0u,1,0xcb402f8,uVar2);
    cataCall(0x00495bd0u,2,uVar2);
    uVar2 = cataCall(0x004907e0u,1,0xebc0bc1);
    uVar2 = cataCall(0x00490bb0u,1,0xcb402f8,uVar2);
    cataCall(0x00495bd0u,2,uVar2);
    cataCall(0x00496d10u,1,cataScriptText(0x00890688));
    cataCall(0x004980c0u,3,cataScriptText(0x0088b8f8),cataScriptText(0x0088b0cc),0);
    cataCall(0x004980c0u,3,cataScriptText(0x0088b8cc),cataScriptText(0x0088b0cc),0);
    cataCall(0x004980c0u,3,cataScriptText(0x0088b8b8),cataScriptText(0x0088b0cc),0);
    cataCall(0x004980c0u,3,cataScriptText(0x0088b8a4),cataScriptText(0x0088b0cc),0);
    cataCall(0x00496e20u,2,0,2000);
    cataCall(0x00496e20u,2,1,100000);
    cataCall(0x00491f90u,2,CATA_MISSION1_TARGET_REFRESH_TIMER,1);
    cataCall(0x00490640u,3,0x1f32b25,cataM1_00613940,cataM1_005da2c0);
  }
  return;
}

static void cataM1_005da060(void)
{
  int iVar1;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00497b00u,0);
  if ((iVar1 == 0) && (iVar1 = cataCall(0x00491e30u,1,0x858db5e4), iVar1 == 0)) {
    return;
  }
  cataCall(0x00491d70u,2,0x858db5e4,0);
  iVar1 = cataCall(0x00491e30u,1,0xa072543);
  if (iVar1 != 0) {
    cataCall(0x00496530u,0);
    cataCall(0x00491d70u,2,0xa072543,0);
  }
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
    iVar1 = cataCall(0x00491e30u,1,0x78e6a4e);
    if (iVar1 == 0) {
      cataCall(0x00496c30u,0,1);
      cataCall(0x00491d70u,2,0x78e6a4e,1);
    }
    cataCall(0x00491d70u,2,0x32243d3,0);
  }
  iVar1 = cataCall(0x00491e30u,1,0xa4e5ad3);
  if (iVar1 != 0) {
    cataCall(0x00496ae0u,0);
    cataCall(0x00491d70u,2,0xa4e5ad3,0);
  }
  iVar1 = cataCall(0x00491e30u,1,0xddfdd03);
  if (iVar1 != 0) {
    cataCall(0x00496ae0u,0);
    cataCall(0x00496ae0u,0);
    cataCall(0x00491d70u,2,0xddfdd03,0);
  }
  iVar1 = cataCall(0x00491e30u,1,0x4a5e094);
  if (iVar1 == 0) {
    cataCall(0x00490640u,3,0xe989af2,cataM1_00652ef0,cataM1_005da210);
    return;
  }
  cataCall(0x00490640u,3,0x4a5dc,cataM1_005db0b0,cataM1_00652ef0);
  return;
}

static void cataM1_005da210(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(0);
  iVar1 = cataCall(0x00492120u,1,0xd2247a4);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x4a5dc,cataM1_005db0b0,cataM1_00652ef0);
    return;
  }
  iVar1 = cataCall(0x00491e30u,1,0x79d69c2);
  if (iVar1 != 0) {
    uVar2 = cataCall(0x00491e30u,1,0x79d69c2);
    cataCall(0x00491f00u,2,CATA_MISSION1_BRIEFING_TIMER,uVar2);
    cataCall(0x00491f50u,1,CATA_MISSION1_BRIEFING_TIMER);
    cataCall(0x00491d70u,2,0x79d69c2,0);
  }
  iVar1 = cataCall(0x00492060u,1,CATA_MISSION1_BRIEFING_TIMER);
  if (iVar1 != 0) {
    cataCall(0x00490640u,3,0x1f32b25,cataM1_00613940,cataM1_005da2c0);
  }
  return;
}

static void cataM1_005da2c0(void)
{
  int iVar1;
  uint32_t puVar2;
  int iVar3;
  CataValue uVar4;
  CataValue uVar5;
  
  cataScriptScope(0);
  iVar1 = -1;
  if (cataScriptMessageCount() != 0) {
    iVar1 = cataScriptMessageCount();
  }
  do {
    if (iVar1 < 0) {
      return;
    }
    puVar2 = (uint32_t)cataCall(0x00417ec0u,2,cataTeam(),iVar1);
    if ((uint32_t)0x5c43485 < puVar2) {
      if (puVar2 < (uint32_t)0xb66deb6) {
        if (puVar2 == (uint32_t)0xb66deb5) {
          iVar1 = cataCall(0x00496c80u,1,0);
          if (iVar1 == 0) {
            cataCall(0x00496bc0u,1,100000);
            cataCall(0x00491d70u,2,0x78e6a4e,0);
          }
          cataCall(0x00497a90u,0);
          cataCall(0x00497ad0u,0);
          uVar4 = cataCall(0x004907e0u,1,0x4fcebd3);
          cataCall(0x004963a0u,2,cataScriptText(0x008c83cc),uVar4);
          uVar4 = cataCall(0x004908c0u,1,0x4fcebd3);
          cataCall(0x0049a2a0u,1,uVar4);
          uVar4 = cataCall(0x004907e0u,1,0x4fcebd3,cataScriptText(0x008c8464));
          cataCall(0x00495e50u,2,uVar4);
          cataCall(0x00496eb0u,2,0x100080d9,0);
          puVar2 = (uint32_t)0xb66deb6;
        }
        else {
          if (puVar2 < (uint32_t)0x92dd9c4) {
            if (puVar2 == (uint32_t)0x92dd9c3) {
              uVar4 = cataCall(0x004907a0u,1,0x98ddc43,0xd99b1ba);
              cataCall(0x00494a00u,2,uVar4);
              cataCall(0x00491d70u,2,0x32243d3,1);
              cataCall(0x00491d70u,2,0xfdbd41e,1);
              cataCall(0x00491d70u,2,0x79d69c2,3);
              cataCall(0x00496eb0u,2,0x100020de,0);
              cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
              return;
            }
            if (puVar2 < (uint32_t)0x9055de3) {
              if (puVar2 == (uint32_t)0x9055de2) {
                uVar4 = cataCall(0x004908c0u,1,0x2baec02);
                cataCall(0x0049a2a0u,1,uVar4);
                uVar4 = cataCall(0x004907e0u,1,0x2baec02);
                cataCall(0x004963a0u,2,cataScriptText(0x008c8400),uVar4);
                cataCall(0x00496eb0u,2,0x10002163,0);
                cataCall(0x00494a00u,2,cataTeam(),0x1107414);
                cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
                return;
              }
              if (puVar2 == (uint32_t)0x6b533a4) {
                cataCall(0x00495ea0u,1,cataScriptText(0x008c8454));
                goto LAB_005da600;
              }
              if (puVar2 == (uint32_t)0x86cd624) {
                cataCall(0x00495ea0u,1,cataScriptText(0x008c8414));
                cataCall(0x00496eb0u,2,0x10001162,0);
                cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
                return;
              }
            }
            else if (puVar2 == (uint32_t)0x92dd9c2) {
              iVar1 = cataCall(0x00496c80u,1,0);
              if (iVar1 == 0) {
                cataCall(0x00496bc0u,1,100000);
                cataCall(0x00491d70u,2,0x78e6a4e,0);
              }
              uVar4 = cataCall(0x004908c0u,1,0x518e0d2);
              cataCall(0x0049a2a0u,1,uVar4);
              uVar4 = cataCall(0x004907e0u,1,0x518e0d2,cataScriptText(0x008c83f0));
              cataCall(0x00495e50u,2,uVar4);
              uVar4 = cataCall(0x004907e0u,1,0x518e0d2);
              cataCall(0x004963a0u,2,cataScriptText(0x008c83dc),uVar4);
              cataCall(0x00496eb0u,2,0x100080dd,0);
              uVar4 = cataCall(0x004907a0u,1,0xa8a880c,0x92dd9c3);
              cataCall(0x00494a00u,2,uVar4);
              uVar4 = cataCall(0x004907a0u,1,0x518e0d2,0x4b095);
              cataCall(0x00494a00u,2,uVar4);
              uVar4 = cataCall(0x004907a0u,1,0x53ec482,0x506a278);
              cataCall(0x00494a00u,2,uVar4);
              cataCall(0x00491d70u,2,0xa072543,1);
              cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
              return;
            }
            goto LAB_005da5f0;
          }
          if (puVar2 < (uint32_t)0xb66deb4) {
            if (puVar2 == (uint32_t)0xb66deb3) {
              cataCall(0x00496eb0u,2,0x100080d7,0);
              puVar2 = (uint32_t)0xb66deb4;
            }
            else {
              if (puVar2 == (uint32_t)0x939002e) {
                iVar3 = cataCall(0x00493b90u,0);
                if (iVar3 < 2) {
                  uVar4 = 0x9a64054;
                }
                else {
                  uVar4 = 0x3900261;
                }
                uVar5 = 0xa8a880c;
                goto LAB_005da553;
              }
              if (puVar2 != (uint32_t)0x9a64054) goto LAB_005da5f0;
              cataCall(0x00496eb0u,2,0x1000215f,0);
              puVar2 = (uint32_t)0x3900261;
            }
          }
          else {
            if (puVar2 != (uint32_t)0xb66deb4) goto LAB_005da5f0;
            cataCall(0x00496eb0u,2,0x100010d8,0);
            puVar2 = (uint32_t)0xb66deb5;
          }
        }
LAB_005db02e:
        uVar4 = 0xa8a880c;
LAB_005db033:
        uVar4 = cataCall(0x004907a0u,1,uVar4,puVar2);
        cataCall(0x00494a00u,2,uVar4);
        cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
        return;
      }
      if (puVar2 < (uint32_t)0xf19cfe8) {
        if (puVar2 == (uint32_t)0xf19cfe7) {
          iVar3 = cataCall(0x00491e30u,1,0x443ccb6);
          if (iVar3 == 0) {
            cataCall(0x00491d30u,2,0x443ccb6,1);
            cataCall(0x00497000u,1,0x13);
            cataCall(0x00497580u,1,0x5a);
            cataCall(0x00491d70u,2,0x79d69c2,3);
            cataCall(0x00494a00u,2,cataTeam(),0x19cfe72);
            cataCall(0x00490640u,3,0xe989af2,cataM1_00652ef0,cataM1_005da210);
            return;
          }
          goto LAB_005da600;
        }
        if (puVar2 < (uint32_t)0xd87e235) {
          if (puVar2 == (uint32_t)0xd87e234) {
            cataCall(0x00496eb0u,2,0x10002167,0);
            cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
            return;
          }
          if (puVar2 == (uint32_t)0xb66deb6) {
            uVar4 = cataCall(0x004907a0u,1,0x98ddc43,0xde8950a);
            cataCall(0x00494a00u,2,uVar4);
            cataCall(0x00491d70u,2,0xa072543,1);
            cataCall(0x00496eb0u,2,0x100020da,0);
            cataCall(0x00491d70u,2,0x68087c5,1);
            cataCall(0x00491d70u,2,0xca04b13,1);
            cataCall(0x00491d70u,2,0xfdbd41e,1);
            cataCall(0x00491d70u,2,0x32243d3,1);
            cataCall(0x00491d70u,2,0x79d69c2,0x32);
            cataCall(0x00494a00u,2,cataTeam(),0x00a2ba30u);
            cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
            return;
          }
          if (puVar2 == (uint32_t)0xd2247a4) {
            cataCall(0x00490640u,3,0x4a5dc,cataM1_005db0b0,cataM1_00652ef0);
            return;
          }
        }
        else if (puVar2 == (uint32_t)0xd899602) {
          cataCall(0x00496eb0u,2,0x100020dc,0);
          uVar4 = cataCall(0x004907a0u,1,0x98ddc43,0x96dfbfa);
          cataCall(0x00494a00u,2,uVar4);
          iVar1 = cataCall(0x00496c80u,1,0);
          if (iVar1 == 0) {
            cataCall(0x00496bc0u,1,100000);
            cataCall(0x00491d70u,2,0x78e6a4e,0);
          }
          cataCall(0x00497a90u,0);
          cataCall(0x00497ad0u,0);
          uVar4 = cataCall(0x004908c0u,1,0xcbd4dc3);
          cataCall(0x0049a2a0u,1,uVar4);
          uVar4 = cataCall(0x004907e0u,1,0xcbd4dc3);
          cataCall(0x004963a0u,2,cataScriptText(0x008c8400),uVar4);
          uVar4 = cataCall(0x004907e0u,1,0xcbd4dc3,cataScriptText(0x008c8414));
          cataCall(0x00495e50u,2,uVar4);
          iVar1 = cataCall(0x00493b90u,0);
          if (iVar1 < 2) {
            uVar4 = 0x9055de2;
          }
          else {
            uVar4 = 0x1107414;
          }
          cataCall(0x00494a00u,2,cataTeam(),uVar4);
          cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
          return;
        }
      }
      else {
        if (puVar2 == (uint32_t)0xf92dd92) {
          cataCall(0x00497580u,1,0x5a);
          cataCall(0x00491d70u,2,0x79d69c2,3);
          uVar4 = 0x92dd9c2;
          goto LAB_005db075;
        }
        if (puVar2 == (uint32_t)0xfb66de3) {
          cataCall(0x00496eb0u,2,0x100010d6,0);
          puVar2 = (uint32_t)0xb66deb3;
          goto LAB_005db02e;
        }
        if (puVar2 == (uint32_t)0xfd97064) {
          cataCall(0x00496eb0u,2,0x10002166,0);
          cataCall(0x00491d70u,2,0x4a5e094,1);
          cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
          return;
        }
      }
      goto LAB_005da5f0;
    }
    if (puVar2 == (uint32_t)0x5c43485) {
      cataCall(0x00497000u,1,0);
      cataCall(0x00496eb0u,2,0x20012ca,0);
      puVar2 = (uint32_t)0xb315ffa;
      uVar4 = 0x98ddc43;
      goto LAB_005db033;
    }
    if ((uint32_t)0x12f6cf4 < puVar2) {
      if (puVar2 < (uint32_t)0x3900263) {
        if (puVar2 != (uint32_t)0x3900262) {
          if ((uint32_t)0x3184ce6 < puVar2) {
            if (puVar2 == (uint32_t)0x3900261) {
              cataCall(0x00497580u,1,0x5a);
              cataCall(0x00491d70u,2,0x79d69c2,3);
              uVar4 = 0x3900262;
              goto LAB_005db075;
            }
            goto LAB_005da5f0;
          }
          if (puVar2 == (uint32_t)0x3184ce6) {
            cataCall(0x00496eb0u,2,0x10002164,0);
            uVar4 = 0x3184ce6;
            uVar5 = 0x98ddc43;
LAB_005da553:
            uVar4 = cataCall(0x004907a0u,1,uVar5,uVar4);
            cataCall(0x00494a00u,2,uVar4);
          }
          else {
            if (puVar2 == (uint32_t)0x19cfe72) {
              cataCall(0x00496eb0u,2,0x10002165,0);
              iVar1 = cataCall(0x00496c80u,1,0);
              if (iVar1 == 0) {
                cataCall(0x00496bc0u,1,100000);
                cataCall(0x00491d70u,2,0x78e6a4e,0);
              }
              cataCall(0x00497a90u,0);
              cataCall(0x00497ad0u,0);
              cataCall(0x00491d70u,2,0x68087c5,1);
              cataCall(0x00491d70u,2,0xca04b13,1);
              uVar4 = cataCall(0x00490820u,1,0xcb402f8);
              cataCall(0x0049a2a0u,1,uVar4);
              uVar4 = cataCall(0x00490bb0u,1,0xcb402f8);
              cataCall(0x004963a0u,2,cataScriptText(0x008c8438),uVar4);
              uVar4 = cataCall(0x00490bb0u,1,0xcb402f8,cataScriptText(0x008c8470));
              cataCall(0x00495e50u,2,uVar4);
              cataCall(0x00491d70u,2,0x32243d3,1);
              cataCall(0x00491d70u,2,0xa072543,1);
              cataCall(0x00491d70u,2,0xfdbd41e,1);
              puVar2 = (uint32_t)0xb0f775a;
              uVar4 = 0x98ddc43;
              goto LAB_005db033;
            }
            if (puVar2 != (uint32_t)0x2d73454) goto LAB_005da5f0;
            cataCall(0x00497000u,1,0);
            cataCall(0x00495ea0u,1,cataScriptText(0x008c8464));
          }
          goto LAB_005da600;
        }
        uVar4 = cataCall(0x004907e0u,1,0xd788163);
        iVar1 = cataCall(0x00492d20u,1,uVar4);
        if (iVar1 == 0) {
          cataCall(0x00496eb0u,2,0x10002161,0);
          puVar2 = (uint32_t)0x3900263;
        }
        else {
          cataCall(0x00496eb0u,2,0x10002160,0);
          puVar2 = (uint32_t)0x3900263;
        }
      }
      else {
        if (puVar2 == (uint32_t)0x3900263) {
          uVar4 = cataCall(0x004907a0u,1,0x98ddc43,0x3b1a6a);
          cataCall(0x00494a00u,2,uVar4);
          cataCall(0x00496eb0u,2,0x100080db,0);
          iVar1 = cataCall(0x00496c80u,1,0);
          if (iVar1 == 0) {
            cataCall(0x00496bc0u,1,100000);
            cataCall(0x00491d70u,2,0x78e6a4e,0);
          }
          uVar4 = cataCall(0x00490960u,1,0xd56e434);
          cataCall(0x0049a2a0u,1,uVar4);
          uVar4 = cataCall(0x00490af0u,1,0x58dfa04);
          cataCall(0x00496490u,2,cataScriptText(0x008c8424),uVar4);
          uVar4 = cataCall(0x00490960u,1,0xd56e434,cataScriptText(0x008c8454));
          cataCall(0x00495e80u,2,uVar4);
          cataCall(0x00497a90u,0);
          cataCall(0x00497ad0u,0);
          cataCall(0x00491d70u,2,0x68087c5,1);
          cataCall(0x00491d70u,2,0xca04b13,1);
          cataCall(0x00491d70u,2,0x32243d3,1);
          cataCall(0x00491d70u,2,0xfdbd41e,1);
          cataCall(0x00491d70u,2,0xa072543,1);
          cataCall(0x00491d70u,2,0x79d69c2,3);
          cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
          return;
        }
        if (puVar2 == (uint32_t)0x4d89963) {
          cataCall(0x00497580u,1,0x5a);
          cataCall(0x00491d70u,2,0x79d69c2,3);
          uVar4 = 0xd899602;
LAB_005db075:
          uVar4 = cataCall(0x004907a0u,1,0xa8a880c,uVar4);
          cataCall(0x00494a00u,2,uVar4);
          cataCall(0x00490640u,3,0xe989af2,cataM1_00652ef0,cataM1_005da210);
          return;
        }
        if (puVar2 != (uint32_t)0x5886752) goto LAB_005da5f0;
        cataCall(0x00491d70u,2,0x858db5e4,0);
        cataCall(0x00497000u,1,0x24);
        cataCall(0x00496eb0u,2,0x1000215c,0);
        puVar2 = (uint32_t)0xfb66de3;
      }
      goto LAB_005db02e;
    }
    if (puVar2 == (uint32_t)0x12f6cf4) {
      cataCall(0x00495ea0u,1,cataScriptText(0x008c8470));
      goto LAB_005da600;
    }
    if (0x00a2ba31u < puVar2) {
      if (0x00a2ba34u < puVar2) {
        if (puVar2 == (uint32_t)0x1107414) {
          cataCall(0x00491d70u,2,0x68087c5,1);
          cataCall(0x00491d70u,2,0xca04b13,1);
          cataCall(0x00491d70u,2,0x32243d3,1);
          cataCall(0x00491d70u,2,0xfdbd41e,1);
          cataCall(0x00491d70u,2,0xa072543,1);
          cataCall(0x00491d70u,2,0x79d69c2,0x1e);
          cataCall(0x00494a00u,2,cataTeam(),0x3184ce6);
          cataCall(0x00497710u,2,0xb,cataScriptText(0x0074e524));
          cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
          return;
        }
      }
      else {
        if (puVar2 == 0x00a2ba34u) {
          uVar4 = cataCall(0x00490bb0u,1,0xca25860);
          cataCall(0x00496870u,1,uVar4);
          uVar4 = cataCall(0x00491e30u,1,0xf8bffc5);
          cataCall(0x004961b0u,1,uVar4);
          uVar4 = cataCall(0x00491e30u,1,0x3a1cffe);
          cataCall(0x00496210u,1,uVar4);
          uVar4 = cataCall(0x00491e30u,1,0x3d4ba05);
          cataCall(0x00496260u,1,uVar4);
          goto LAB_005da600;
        }
        if (puVar2 == 0x00a2ba32u) {
          cataCall(0x00496eb0u,2,0x1000215e,0);
          cataCall(0x00491d70u,2,0x68087c5,1);
          cataCall(0x00491d70u,2,0xca04b13,1);
          puVar2 = 0x00a2ba33u;
          goto LAB_005db02e;
        }
        if (puVar2 == 0x00a2ba33u) {
          cataCall(0x00491d70u,2,0x32243d3,1);
          cataCall(0x00491d70u,2,0xfdbd41e,1);
          uVar4 = cataCall(0x004907e0u,1,0xa388bd1);
          cataCall(0x00496870u,1,uVar4);
          cataCall(0x004961b0u,1,0x26);
          cataCall(0x00496210u,1,0xffffffec);
          cataCall(0x00496260u,1,0x1f9);
          cataCall(0x00497710u,2,8000,cataScriptText(0x0074e524));
          cataCall(0x00494a00u,2,cataTeam(),0x00a2ba34u);
          cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
          return;
        }
      }
LAB_005da5f0:
      cataCall(0x00417e10u,3,cataTeam(),cataTeam(),puVar2);
    }
    else {
      if (puVar2 == 0x00a2ba31u) {
        uVar4 = cataCall(0x00496190u,0);
        cataCall(0x00491d30u,2,0xf8bffc5,uVar4);
        uVar4 = cataCall(0x004961f0u,0);
        cataCall(0x00491d30u,2,0x3a1cffe,uVar4);
        uVar4 = cataCall(0x00496250u,0);
        cataCall(0x00491d30u,2,0x3d4ba05,uVar4);
        cataCall(0x00497580u,1,0x5a);
        cataCall(0x00491d70u,2,0x79d69c2,3);
        cataCall(0x00494a00u,2,cataTeam(),0x2a4d46);
        cataCall(0x00490640u,3,0xe989af2,cataM1_00652ef0,cataM1_005da210);
        return;
      }
      if ((uint32_t)0x588674 < puVar2) {
        if (puVar2 == 0x00a2ba30u) {
          uVar4 = 0xcb5e56e;
          uVar5 = 0xa388bd1;
          goto LAB_005da553;
        }
        goto LAB_005da5f0;
      }
      if (puVar2 == (uint32_t)0x588674) {
        cataCall(0x00494a00u,2,cataTeam(),0x5886752);
        cataCall(0x00497290u,2,1,cataScriptLocalizedText(0x008c7ed4));
      }
      else if (puVar2 != (uint32_t)0x0) {
        if (puVar2 == (uint32_t)0x2a4d46) {
          cataCall(0x00491d70u,2,0xa072543,1);
          iVar1 = cataCall(0x00496c80u,1,0);
          if (iVar1 == 0) {
            cataCall(0x00496bc0u,1,100000);
            cataCall(0x00491d70u,2,0x78e6a4e,0);
          }
          cataCall(0x00497a90u,0);
          cataCall(0x00497ad0u,0);
          cataCall(0x00496eb0u,2,0x1000215d,0);
          uVar4 = cataCall(0x004907e0u,1,0xa388bd1);
          cataCall(0x004963a0u,2,cataScriptText(0x008c8444),uVar4);
          uVar4 = cataCall(0x004908c0u,1,0xa388bd1);
          cataCall(0x0049a2a0u,1,uVar4);
          iVar1 = cataCall(0x00493b90u,0);
          if (iVar1 < 2) {
            uVar4 = cataCall(0x004907a0u,1,0xa8a880c,0x00a2ba32u);
            cataCall(0x00494a00u,2,uVar4);
          }
          else {
            uVar4 = cataCall(0x004907a0u,1,0xa8a880c,0x00a2ba33u);
            cataCall(0x00494a00u,2,uVar4);
            cataCall(0x00491d70u,2,0x68087c5,1);
            cataCall(0x00491d70u,2,0xca04b13,1);
          }
          cataCall(0x00490640u,3,0xe8e63cc,cataM1_005f4730,cataM1_005da060);
          return;
        }
        goto LAB_005da5f0;
      }
    }
LAB_005da600:
    if (cataScriptMessageCount() == 0) {
      return;
    }
    if (cataScriptMessageCount() == 0) {
      return;
    }
    iVar1 = iVar1 + -1;
  } while( true );
}

static void cataM1_005db0b0(void)
{
  CataValue uVar1;
  int iVar2;
  
  cataScriptScope(0);
  uVar1 = cataCall(0x00490bb0u,1,0xca25860);
  iVar2 = cataCall(0x00492d20u,1,uVar1);
  if (iVar2 != 0) {
    cataCall(0x0051be70u,0);
    return;
  }
  return;
}

static void cataM1_005db0f0(void)
{
  CataValue uVar1;
  
  cataScriptScope(1);
  uVar1 = cataCall(0x004907a0u,1,0xa8a880c);
  cataCall(0x00490660u,4,0xa8a880c,cataM1_005d9a20,cataM1_0062ca80,uVar1);
  uVar1 = cataCall(0x004907a0u,1,0x98ddc43);
  cataCall(0x00490660u,4,0x98ddc43,cataM1_005d6fe0,cataM1_005d7050,uVar1);
  return;
}

static void cataM1_005db150(void)
{
  int iVar1;
  CataValue uVar2;
  
  cataScriptScope(1);
  iVar1 = cataCall(0x00492060u,1,CATA_MISSION1_TARGET_REFRESH_TIMER);
  if (iVar1 != 0) {
    uVar2 = cataCall(0x00490bb0u,1,0x117ca33);
    uVar2 = cataCall(0x00490af0u,1,0x7ec6887,uVar2);
    cataCall(0x00492db0u,2,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x117ca33);
    uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,uVar2);
    cataCall(0x00495560u,2,uVar2);
    uVar2 = cataCall(0x00490bb0u,1,0x117ca33);
    uVar2 = cataCall(0x00490bb0u,1,0x6da2da9,uVar2);
    cataCall(0x00495620u,2,uVar2);
    uVar2 = cataCall(0x00492910u,2,8,0xf);
    cataCall(0x00491f00u,2,CATA_MISSION1_TARGET_REFRESH_TIMER,uVar2);
    cataCall(0x00491f50u,1,CATA_MISSION1_TARGET_REFRESH_TIMER);
  }
  iVar1 = cataCall(0x00491e30u,1,0x443ccb7);
  if (iVar1 == 0) {
    iVar1 = cataCall(0x00491e30u,1,0x87defce4);
    if (iVar1 != 0) {
      cataCall(0x00491d30u,2,0x443ccb7,1);
      uVar2 = cataCall(0x00490bb0u,1,0xcff5860);
      cataCall(0x00498790u,1,uVar2);
      uVar2 = cataCall(0x00490bb0u,1,0xca25860);
      cataCall(0x00498790u,1,uVar2);
      uVar2 = cataCall(0x00490bb0u,1,0x117ca33);
      uVar2 = cataCall(0x00490af0u,1,0x7ec6887,uVar2);
      cataCall(0x00492db0u,2,uVar2);
      uVar2 = cataCall(0x00490bb0u,1,0x117ca33);
      uVar2 = cataCall(0x00490bb0u,1,0x4a2a29,uVar2);
      cataCall(0x00495560u,2,uVar2);
    }
  }
  return;
}

static void cataM1_005f4730(void)
{
  
  cataScriptScope(0);
  cataCall(0x00497b80u,0);
  return;
}

static void cataM1_00613940(void)
{
  
  cataScriptScope(0);
  cataCall(0x00491d70u,2,0x858db5e4,0);
  return;
}

static void cataM1_0062ca80(void)
{
  
  cataScriptScope(0);
  return;
}

static void cataM1_00652ef0(void)
{
  
  cataScriptScope(0);
  return;
}
extern "C" void cataMission01Init(){cataM1_005db0f0();}
extern "C" void cataMission01Watch(){cataM1_005db150();}
extern "C" const void **cataMission01FunctionTable(){
    static const void *functions[]={
        reinterpret_cast<const void*>(&cataM1_005d6fe0),
        reinterpret_cast<const void*>(&cataM1_005d7050),
        reinterpret_cast<const void*>(&cataM1_005d78d0),
        reinterpret_cast<const void*>(&cataM1_005d7960),
        reinterpret_cast<const void*>(&cataM1_005d7990),
        reinterpret_cast<const void*>(&cataM1_005d7a10),
        reinterpret_cast<const void*>(&cataM1_005d7a40),
        reinterpret_cast<const void*>(&cataM1_005d7aa0),
        reinterpret_cast<const void*>(&cataM1_005d7b80),
        reinterpret_cast<const void*>(&cataM1_005d7be0),
        reinterpret_cast<const void*>(&cataM1_005d7c20),
        reinterpret_cast<const void*>(&cataM1_005d7c90),
        reinterpret_cast<const void*>(&cataM1_005d7cc0),
        reinterpret_cast<const void*>(&cataM1_005d7d10),
        reinterpret_cast<const void*>(&cataM1_005d7d40),
        reinterpret_cast<const void*>(&cataM1_005d7d90),
        reinterpret_cast<const void*>(&cataM1_005d7df0),
        reinterpret_cast<const void*>(&cataM1_005d7e30),
        reinterpret_cast<const void*>(&cataM1_005d7e80),
        reinterpret_cast<const void*>(&cataM1_005d7f60),
        reinterpret_cast<const void*>(&cataM1_005d7f90),
        reinterpret_cast<const void*>(&cataM1_005d7fd0),
        reinterpret_cast<const void*>(&cataM1_005d8010),
        reinterpret_cast<const void*>(&cataM1_005d80f0),
        reinterpret_cast<const void*>(&cataM1_005d8130),
        reinterpret_cast<const void*>(&cataM1_005d8180),
        reinterpret_cast<const void*>(&cataM1_005d8220),
        reinterpret_cast<const void*>(&cataM1_005d82e0),
        reinterpret_cast<const void*>(&cataM1_005d8340),
        reinterpret_cast<const void*>(&cataM1_005d8370),
        reinterpret_cast<const void*>(&cataM1_005d83b0),
        reinterpret_cast<const void*>(&cataM1_005d8400),
        reinterpret_cast<const void*>(&cataM1_005d84a0),
        reinterpret_cast<const void*>(&cataM1_005d8510),
        reinterpret_cast<const void*>(&cataM1_005d8580),
        reinterpret_cast<const void*>(&cataM1_005d85c0),
        reinterpret_cast<const void*>(&cataM1_005d85f0),
        reinterpret_cast<const void*>(&cataM1_005d8630),
        reinterpret_cast<const void*>(&cataM1_005d8660),
        reinterpret_cast<const void*>(&cataM1_005d86c0),
        reinterpret_cast<const void*>(&cataM1_005d8730),
        reinterpret_cast<const void*>(&cataM1_005d8750),
        reinterpret_cast<const void*>(&cataM1_005d87f0),
        reinterpret_cast<const void*>(&cataM1_005d8880),
        reinterpret_cast<const void*>(&cataM1_005d88f0),
        reinterpret_cast<const void*>(&cataM1_005d8930),
        reinterpret_cast<const void*>(&cataM1_005d8980),
        reinterpret_cast<const void*>(&cataM1_005d89d0),
        reinterpret_cast<const void*>(&cataM1_005d8a60),
        reinterpret_cast<const void*>(&cataM1_005d8bd0),
        reinterpret_cast<const void*>(&cataM1_005d8c30),
        reinterpret_cast<const void*>(&cataM1_005d8d20),
        reinterpret_cast<const void*>(&cataM1_005d8e80),
        reinterpret_cast<const void*>(&cataM1_005d8ef0),
        reinterpret_cast<const void*>(&cataM1_005d8f60),
        reinterpret_cast<const void*>(&cataM1_005d8fa0),
        reinterpret_cast<const void*>(&cataM1_005d9020),
        reinterpret_cast<const void*>(&cataM1_005d9050),
        reinterpret_cast<const void*>(&cataM1_005d90d0),
        reinterpret_cast<const void*>(&cataM1_005d9180),
        reinterpret_cast<const void*>(&cataM1_005d92a0),
        reinterpret_cast<const void*>(&cataM1_005d92d0),
        reinterpret_cast<const void*>(&cataM1_005d9310),
        reinterpret_cast<const void*>(&cataM1_005d9380),
        reinterpret_cast<const void*>(&cataM1_005d93f0),
        reinterpret_cast<const void*>(&cataM1_005d9430),
        reinterpret_cast<const void*>(&cataM1_005d9490),
        reinterpret_cast<const void*>(&cataM1_005d94e0),
        reinterpret_cast<const void*>(&cataM1_005d9600),
        reinterpret_cast<const void*>(&cataM1_005d9780),
        reinterpret_cast<const void*>(&cataM1_005d97b0),
        reinterpret_cast<const void*>(&cataM1_005d9800),
        reinterpret_cast<const void*>(&cataM1_005d9870),
        reinterpret_cast<const void*>(&cataM1_005d98f0),
        reinterpret_cast<const void*>(&cataM1_005d9930),
        reinterpret_cast<const void*>(&cataM1_005d9980),
        reinterpret_cast<const void*>(&cataM1_005d9a20),
        reinterpret_cast<const void*>(&cataM1_005d9af0),
        reinterpret_cast<const void*>(&cataM1_005d9b20),
        reinterpret_cast<const void*>(&cataM1_005da060),
        reinterpret_cast<const void*>(&cataM1_005da210),
        reinterpret_cast<const void*>(&cataM1_005da2c0),
        reinterpret_cast<const void*>(&cataM1_005db0b0),
        reinterpret_cast<const void*>(&cataM1_005db0f0),
        reinterpret_cast<const void*>(&cataM1_005db150),
        reinterpret_cast<const void*>(&cataM1_005f4730),
        reinterpret_cast<const void*>(&cataM1_00613940),
        reinterpret_cast<const void*>(&cataM1_0062ca80),
        reinterpret_cast<const void*>(&cataM1_00652ef0),
        nullptr};
    return functions;
}
