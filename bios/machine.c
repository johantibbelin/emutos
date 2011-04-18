/*
 * machine.c - detection of machine type
 *
 * Copyright (c) 2001 EmuTOS development team.
 *
 * Authors:
 *  LVL     Laurent Vogel
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#include "portab.h"
#include "cookie.h"
#include "machine.h"
#include "biosmem.h"
#include "vectors.h"
#include "nvram.h"
#include "tosvars.h"
#include "country.h"
#include "clock.h"
#include "natfeat.h"
#include "xhdi.h"
#include "string.h"

long cookie_vdo;
long cookie_fdc;
long cookie_snd;
long cookie_mch;
long cookie_swi;
long cookie_frb;


/*
 * test specific hardware features
 */

int has_ste_shifter;
int has_tt_shifter;
int has_videl;
 
/*
 * Tests video capabilities (STEnhanced Shifter, TT Shifter and VIDEL)
 */
static void detect_video(void)
{
 /* test if we have an STe Shifter by testing that register 820d
  * works (put a value, read other reg, read again, and compare)
  */
  volatile BYTE *ste_reg = (BYTE *) 0xffff820d;
  volatile BYTE *other_reg1 = (BYTE *) 0xffff8203;
  volatile WORD *other_reg2 = (WORD *) 0xffff8240;

  has_ste_shifter = 0;  
  if(!check_read_byte((long)ste_reg)) return;
  *ste_reg = 90;
  *other_reg1; /* force register read (really useful ?) */
  if(*ste_reg == 90) {
    *ste_reg = 0;
    *other_reg2; /* force register read (really useful ?) */
    if(*ste_reg == 0) {
      has_ste_shifter = 1;
    } 
  }

  /* test if we have a TT Shifter by testing for TT color palette */
  has_tt_shifter = 0;
  if (check_read_byte(0xffff8400))
    has_tt_shifter = 1;

  /* test if we have Falcon VIDEL by testing for f030_xreg */
  has_videl = 0;
  if (check_read_byte(0xffff8282))
    has_videl = 1;
}

/* vme */

int has_vme;

static void detect_vme(void)
{
  volatile BYTE *vme_mask = (BYTE *) 0xffff8e0d;
  volatile BYTE *sys_mask = (BYTE *) 0xffff8e01;
  
  if(check_read_byte(0xffff8e09)) {
    *vme_mask = 0x40;  /* ??? IRQ3 from VMEBUS/soft */
    *sys_mask = 0x14;  /* ??? set VSYNC and HSYNC */
    has_vme = 1;
  } else {
    has_vme = 0;
  }
}

/* DIP switches */

static void setvalue_swi(void)
{
  cookie_swi = 0x7F;
  if (check_read_byte(0xffff9201)) {
    cookie_swi = (*(volatile WORD *)0xffff9200)>>8;
  }
}

/* video type */

static void setvalue_vdo(void)
{
  if(has_videl) {
    cookie_vdo = 0x00030000L;
  }
  else if(has_tt_shifter) {
    cookie_vdo = 0x00020000L;
  }
  else {
    if(has_ste_shifter) {
      cookie_vdo = 0x00010000L;
    } else {
      cookie_vdo = 0x00000000L;
    } 
  }
}

/* detect ARAnyM */
#define ARANYM_NAME     "aranym"
static int is_it_aranym(void)
{
  return (strncasecmp(machine_name(), ARANYM_NAME, strlen(ARANYM_NAME)) == 0);
}

/* machine type */
static void setvalue_mch(void)
{
  if (is_it_aranym()) {
    cookie_mch = MCH_ARANYM;
  }
  else if(has_videl) {
    cookie_mch = MCH_FALCON;
  }
  else if(has_tt_shifter) {
    cookie_mch = MCH_TT;
  }
  else {
    if(has_ste_shifter) {
      if(has_vme) {
        cookie_mch = MCH_MSTE;
      } else {
        cookie_mch = MCH_STE;
      }
    } else {
      cookie_mch = MCH_ST;
    }
  }
}

/* SND */

static void setvalue_snd(void)
{
  /* always at least a PSG */
  cookie_snd = 1;

  /* Check for DMA sound */
  if (check_read_byte(0xFFFF8903)) {
    cookie_snd |= 2;
  }
}
  

/* FDC */

static void setvalue_fdc(void)
{
  /* LVL - This is what I understood of my search on 
   * comp.sys.atari.st.tech, archives, but I do not claim 
   * this to be fully accurate. 
   */
  if(cookie_swi & 0x40) {
    cookie_fdc = FDC_1ATC;
  } else {
    cookie_fdc = FDC_0ATC;
  } 
}


void machine_detect(void)
{
  detect_video();
  detect_vme();
  detect_megartc();
  detect_nvram();
}
  
void machine_init(void)
{
#ifdef __mcoldfire__
  cookie_add(COOKIE_COLDFIRE, 0);
#else
  /* this is detected by detect_cpu(), called from processor_init() */
  cookie_add(COOKIE_CPU, mcpu);
#endif

  /* _VDO
   * This cookie represents the revision of the video shifter present. 
   * Currently valid values are: 
   * 0x00000000  ST 
   * 0x00010000  STe 
   * 0x00020000  TT030 
   * 0x00030000  Falcon030 
   */

  setvalue_vdo();
  cookie_add(COOKIE_VDO, cookie_vdo);

#ifndef __mcoldfire__
  /* this is detected by detect_fpu(), called from processor_init() */
  cookie_add(COOKIE_FPU, fputype);
#endif

  /* _MCH */
  setvalue_mch();
  cookie_add(COOKIE_MCH, cookie_mch);

  /* _SWI  On machines that contain internal configuration dip switches, 
   * this value specifies their positions as a bitmap. Dip switches are 
   * generally used to indicate the presence of additional hardware which 
   * will be represented by other cookies.  
   */
  if (cookie_mch == MCH_MSTE || cookie_mch == MCH_TT
      || cookie_mch == MCH_FALCON) {
    setvalue_swi();
    cookie_add(COOKIE_SWI, cookie_swi);
  }

  /* _SND
   * This cookie contains a bitmap of sound features available to the 
   * system as follows:  
   * 0x01 GI Sound Chip (PSG) 
   * 0x02 1 Stereo 8-bit Playback 
   * 0x04 DMA Record (w/XBIOS) 
   * 0x08 16-bit CODEC 
   * 0x10 DSP 
   */
  
  setvalue_snd();
  cookie_add(COOKIE_SND, cookie_snd);

  /* _FRB  This cookie is present when alternative RAM is present. It 
   * points to a 64k buffer that may be used by DMA device drivers to 
   * transfer memory between alternative RAM and ST RAM for DMA operations.  
   */
  if (ramtop > 0) {
    cookie_frb = (long)balloc(64 * 1024UL);
    cookie_add(COOKIE_FRB, cookie_frb);
  }
  else {
    cookie_frb = 0;
  }
   
  /* _FLK  The presence of this cookie indicates that file and record 
   * locking extensions to GEMDOS exist. The value field is a version 
   * number currently undefined.  
   */
  
  /* _IDT This cookie defines the currently configured date and time 
   * format, Bits #0-7 contain the ASCII code of the date separator. 
   * Bits #8-11 contain a value indicating the date display format as 
   * follows:  
   *   0 MM-DD-YY 
   *   1 DD-MM-YY 
   *   2 YY-MM-DD 
   *   3 YY-DD-MM
   * Bits #12-15 contain a value indicating the time format as follows:  
   *   0 12 hour 
   *   1 24 hour
   * Note: The value of this cookie does not affect any of the internal 
   * time functions. It is intended for informational use by applications 
   */

  /* _AKP  This cookie indicates the presence of an Advanced Keyboard 
   * Processor. The high word of this cookie is currently reserved. 
   * The low word indicates the language currently used by TOS for 
   * keyboard interpretation and alerts. 
   */
  
  detect_akp_idt();
  cookie_add(COOKIE_IDT, cookie_idt);
  cookie_add(COOKIE_AKP, cookie_akp);
  
  /* Floppy Drive Controller 
   * Most significant byte means: 
   * 0 - DD (Normal floppy interface)
   * 1 - HD (1.44 MB with 3.5")
   * 2 - ED (2.88 MB with 3.5")
   * the 3 other bytes are the Controller ID:
   * 0 - No information available
   * 'ATC' - Fully compatible interface built in a way that
   * behaves like part of the system.
   */

  setvalue_fdc();
  cookie_add(COOKIE_FDC, cookie_fdc);

  if (has_natfeats()) {
    cookie_add(COOKIE_NATFEAT, (long)&natfeat_cookie);
  }

  create_XHDI_cookie();
}

const char * machine_name(void)
{
  static char buffer[80];
  long bufsize;

  bufsize = nfGetFullName(buffer, sizeof(buffer)-1);
  if (bufsize > 0)
    return buffer;

  switch(cookie_mch) {
  case MCH_ST:
    if(has_megartc) {
      return "MegaST";
    } else {
      return "ST";
    }
  case MCH_STE: return "STe";
  case MCH_MSTE: return "MegaSTe";
  case MCH_TT: return "TT";
  case MCH_FALCON: return "Falcon";
  default: return "unknown";
  } 
}
