/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General 
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** dos_sb.c
**
** DOS Sound Blaster routines
**
** Note: the information in this file has been gathered from many
**  Internet documents, and from source code written by Ethan Brodsky.
** $Id: dos_sb.c,v 1.4 2000/06/09 15:12:27 matt Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <dos.h>
#include <go32.h>
#include <dpmi.h>
#include "dos_sb.h"
#include "dos_ints.h"

#ifdef DJGPP_USE_NEARPTR
#include <sys/nearptr.h>
#endif /* DJGPP_USE_NEARPTR */


#include "log.h"
//#define log_printf printf


#define  INVALID        0xFFFFFFFF

#define  DSP_RESET      0x06
#define  DSP_READ       0x0A
#define  DSP_WRITE      0x0C
#define  DSP_ACK        0x0E

#define  DSP_GETVERSION 0xE1

#define  DSP_DMA_PAUSE_8BIT   0xD0
#define  DSP_DMA_PAUSE_16BIT  0xD5

#define  LOW_BYTE(x)    (uint8) ((x) & 0xFF)
#define  HIGH_BYTE(x)   (uint8) ((x) >> 8)


/* our all-encompassing Sound Blaster structure */
static struct
{
   uint16 baseio;
   uint16 dspversion;
   uint32 dma, dma16;
   uint32 irq;
   
   uint32 samplerate;
   uint8 bps;
   boolean stereo;
   
   void *buffer;
   uint32 bufsize, halfbuf;
   
   sbmix_t callback;
} sb;

/* information about the current DMA settings */
static boolean dma_autoinit = FALSE;
static volatile int dma_count = 0;
static int dma_length;
static uint16 dma_addrport;
static uint16 dma_ackport;

/* DOS specifics */
static _go32_dpmi_seginfo dos_buffer;
static uint32 dos_offset = 0, dos_page = 0;
static uint32 dos_bufaddr = 0;

/* interrupt / autodetection info */
#define  MAX_IRQS    11

static uint32 irq_intvector = INVALID;
static uint8 pic_rotateport = (uint8) INVALID, pic_maskport = (uint8) INVALID;
static uint8 irq_stopmask = (uint8)  INVALID, irq_startmask = (uint8)  INVALID;
static _go32_dpmi_seginfo old_interrupt;
static _go32_dpmi_seginfo new_interrupt;
static _go32_dpmi_seginfo old_handler[MAX_IRQS];
static _go32_dpmi_seginfo new_handler[MAX_IRQS];
static volatile int irq_hit[MAX_IRQS];
static int irq_mask[MAX_IRQS];

/* global flags */
static boolean sb_initialized = FALSE;
static boolean sb_probed = FALSE;


/* 8 and 16 bit DMA ports */
static const uint8 dma_ports[] =
{
   0x87, 0x83, 0x81, 0x82, (uint8) INVALID, 0x8B, 0x89, 0x8A
};



/*
** Basic DSP routines
*/
static void dsp_write(uint16 baseio, uint8 value)
{
   int timeout = 10000;

   /* wait until DSP is ready... */
   while (timeout-- && (inportb(baseio + DSP_WRITE) & 0x80))
      ; /* loop */
   outportb(baseio + DSP_WRITE, value);
}

static uint8 dsp_read(uint16 baseio)
{
   int timeout = 10000;

   while (timeout-- && (0 == (inportb(baseio + DSP_ACK) & 0x80)))
      ; /* loop */
   return (inportb(baseio + DSP_READ));
}

/* returns TRUE if DSP found and successfully reset, FALSE otherwise */
static boolean dsp_reset(uint16 baseio)
{
   outportb(baseio + DSP_RESET, 1); /* reset command */
   delay(5);                        /* 5 usec delay */
   outportb(baseio + DSP_RESET, 0); /* clear */
   delay(5);                        /* 5 usec delay */

   if (0xAA == dsp_read(baseio))
      return TRUE;

   /* BLEH, we failed */
   return FALSE;
}

/* return DSP version in 8:8 major:minor format */
static uint16 dsp_getversion(uint16 baseio)
{
   uint16 version;

   dsp_write(baseio, DSP_GETVERSION);
   version = dsp_read(baseio) << 8;
   version += dsp_read(baseio);
   return version;
}

/*
** Brute force autodetection...
*/
/* detect the base IO by attempting to reset the DSP at known addresses */
static uint16 detect_baseio(void)
{
   int i;
   static uint16 port_val[] =
   {
      0x210, 0x220, 0x230, 0x240, 0x250, 0x260, 0x280, (uint16) INVALID
   };

   for (i = 0; (uint16) INVALID != port_val[i]; i++)
   {
      if (dsp_reset(port_val[i]))
         break;
   }

   /* will return INVALID if not found */
   return port_val[i];
}

/* stop all DSP activity */
static void dsp_stop(uint16 baseio)
{
   /* pause 8/16 bit DMA mode digitized sound IO */
   dsp_reset(baseio);
   dsp_write(baseio, DSP_DMA_PAUSE_8BIT);
   dsp_write(baseio, DSP_DMA_PAUSE_16BIT);
}

/* return dma_request for all channels (bit7->dma7... bit0->dma0) */
static uint8 dma_req(void)
{
   return (inportb(0xd0) & 0xF0) | (inportb(0x08) >> 4);
}

/* return number of set bits in byte x */
static int bitcount(uint8 x)
{
   int i, set_count = 0;

   for (i = 0; i < 8; i++)
      if (x & (1 << i))
         set_count++;

   return set_count;
}

/* returns position of lowest bit set in byte x (INVALID if none) */
static int bitpos(uint8 x)
{
   int i;

   for (i = 0; i < 8; i++)
      if (x & (1 << i))
         return i;

   return INVALID;
}

static int find_dma(uint16 baseio, int dma_channel)
{
   int dma_maskout, dma_mask;
   int i;

   dma_maskout = ~0x10; /* initially mask only DMA4 */
   dma_mask = 0; /* dma channels active during audio, minus masked out */

   /* stop DSP activity */
   dsp_stop(baseio);

   /* poll to find out which dma channels are in use */
   for (i = 0; i < 100; i++)
      dma_maskout &= ~dma_req();

   /* program card, see whch channel becomes active */
   if (1 == dma_channel)
   {
      /* 8 bit */
      dsp_write(baseio, 0x14);
      dsp_write(baseio, 0x00);
      dsp_write(baseio, 0x00);
   }
   else
   {
      dsp_write(baseio, 0xB0); /* 16-bit, D/A, S/C, FIFO off */
      dsp_write(baseio, 0x10); /* 16-bit mono signed PCM */
      dsp_write(baseio, 0x00);
      dsp_write(baseio, 0x00);
   }

   /* poll to find out which DMA channels are in use with sound */
   for (i = 0; i < 100; i++)
      dma_mask |= (dma_req() & dma_maskout);

   /* stop all DSP activity */
   dsp_stop(baseio);

   if (bitcount(dma_mask) == 1)
      return bitpos(dma_mask);
   else
      return INVALID;
}

static unsigned int detect_dma(uint16 baseio)
{
   return find_dma(baseio, 1);
}

static unsigned int detect_dma16(uint16 baseio)
{
   return find_dma(baseio, 2);
}

static void dsp_transfer(uint16 baseio, int dma)
{
   /* mask DMA channel */
   outportb(0x0A, 0x04 | dma);

   /* write DMA mode: single-cycle read transfer */
   outportb(0x0B, 0x48 | dma);

   /* clear byte pointer flip-flop */
   outportb(0x0C, 0x00);

   /* one transfer */
   outportb(0x01 + dma * 2, 0x00); /* low */
   outportb(0x01 + dma * 2, 0x00); /* high */

   /* address */
   outportb(0x00 + dma * 2, 0x00);
   outportb(0x00 + dma * 2, 0x00);
   outportb(dma_ports[dma], 0x00);

   /* unmask DMA channel */
   outportb(0x0A, 0x00 | dma);

   /* 8-bit single cycle DMA mode */
   dsp_write(baseio, 0x14);
   dsp_write(baseio, 0x00);
   dsp_write(baseio, 0x00);
}

static void irq2_handler(void)  { irq_hit[2]  = 1; }
END_OF_STATIC_FUNCTION(irq2_handler);
static void irq3_handler(void)  { irq_hit[3]  = 1; }
END_OF_STATIC_FUNCTION(irq3_handler);
static void irq5_handler(void)  { irq_hit[5]  = 1; }
END_OF_STATIC_FUNCTION(irq5_handler);
static void irq7_handler(void)  { irq_hit[7]  = 1; }
END_OF_STATIC_FUNCTION(irq7_handler);
static void irq10_handler(void) { irq_hit[10] = 1; }
END_OF_STATIC_FUNCTION(irq10_handler);

static void set_handler(int handler, int irq, int vector)
{
   new_handler[irq].pm_offset = (int) handler;
   new_handler[irq].pm_selector = _go32_my_cs();
   _go32_dpmi_get_protected_mode_interrupt_vector(vector, &old_handler[irq]);
   _go32_dpmi_chain_protected_mode_interrupt_vector(vector, &new_handler[irq]);
}

static void clear_irq_hit(void)
{
   int i;

   for (i = 0; i < MAX_IRQS; i++)
      irq_hit[i] = 0;
}

static int check_hits(uint16 baseio)
{
   int i;
   int irq = INVALID;

   for (i = 0; i < MAX_IRQS; i++)
   {
      if (irq_hit[i] && (0 == irq_mask[i]))
      {
         irq = i;

         /* acknowledge the interrupts! */
         inportb(baseio + 0x0E);
         if (irq > 7)
            outportb(0xA0, 0x20);
         outportb(0x20, 0x20);
      }
   }

   return irq;
}

static unsigned int detect_irq(uint16 baseio, int dma)
{
   int pic1_oldmask, pic2_oldmask;

   unsigned int irq = INVALID;
   int i;

   LOCK_FUNCTION(irq2_handler);
   LOCK_FUNCTION(irq3_handler);
   LOCK_FUNCTION(irq5_handler);
   LOCK_FUNCTION(irq7_handler);
   LOCK_FUNCTION(irq10_handler);
   LOCK_VARIABLE(irq_hit);

   /* install temp handlers */
   set_handler((int) irq2_handler, 2, 0x0A);
   set_handler((int) irq3_handler, 3, 0x0B);
   set_handler((int) irq5_handler, 5, 0x0D);
   set_handler((int) irq7_handler, 7, 0x0F);
   set_handler((int) irq10_handler, 10, 0x72);

   /* save old IRQ mask and unmask IRQs */
   pic1_oldmask = inportb(0x21);
   outportb(0x21, pic1_oldmask & 0x53);
   pic2_oldmask = inportb(0xA1);
   outportb(0xA1, pic1_oldmask & 0xFB);

   clear_irq_hit();

   /* wait to see what interrupts are triggered without sound */
   delay(100);

   /* mask out any interrupts triggered without sound */
   for (i = 0; i < MAX_IRQS; i++)
      irq_mask[i] = irq_hit[i];

   clear_irq_hit();

   /* try to trigger an interrupt using DSP command F2 */
   dsp_write(baseio, 0xF2);

   delay(100);

   /* detect triggered interrupts */
   irq = check_hits(baseio);

   /* if F2 fails to trigger an int, run a short transfer */
   if (INVALID == irq)
   {
      dsp_reset(baseio);
      dsp_transfer(baseio, dma);

      delay(100);

      /* detect triggered interrupts */
      irq = check_hits(baseio);
   }

   /* reset DSP just in case */
   dsp_reset(baseio);

   /* remask IRQs */
   outportb(0x21, pic1_oldmask);
   outportb(0xA1, pic2_oldmask);

   /* uninstall handlers */
   _go32_dpmi_set_protected_mode_interrupt_vector(0x0A, &old_handler[2]);
   _go32_dpmi_set_protected_mode_interrupt_vector(0x0B, &old_handler[3]);
   _go32_dpmi_set_protected_mode_interrupt_vector(0x0D, &old_handler[5]);
   _go32_dpmi_set_protected_mode_interrupt_vector(0x0F, &old_handler[7]);
   _go32_dpmi_set_protected_mode_interrupt_vector(0x72, &old_handler[10]);

   return irq;
}

/* try and detect an SB without environment variables */
static boolean sb_detect(void)
{
   if ((uint16) INVALID == (sb.baseio = detect_baseio()))
      return FALSE;
   if (INVALID == (sb.dma = detect_dma(sb.baseio)))
      return FALSE;
   if (INVALID == (sb.dma16 = detect_dma16(sb.baseio)))
      return FALSE;
   if (INVALID == (sb.irq = detect_irq(sb.baseio, sb.dma)))
      return FALSE;

   return TRUE;
}

/*
** BLASTER environment variable parsing
*/
static boolean get_env_item(char *env, void *ptr, char find, int base)
{
   char *item;
   int value;

   item = strrchr(env, find);
   if (NULL == item)
      return FALSE;

   item++;
   value = strtol(item, NULL, base);

   if (10 == base)
      *(int *) ptr = value;
   else if (16 == base)
      *(uint16 *) ptr = value;

   return TRUE;
}

/* parse the BLASTER environment variable */
static boolean parse_blaster_env(void)
{
   char blaster[256], *penv;

   penv = getenv("BLASTER");

   /* bail out if we can't find it... */
   if (NULL == penv)
      return FALSE;

   /* copy it, normalize case */
   strcpy(blaster, penv);
   strupr(blaster);

   if (FALSE == get_env_item(blaster, &sb.baseio, 'A', 16))
      return FALSE;
   if (FALSE == get_env_item(blaster, &sb.irq, 'I', 10))
      return FALSE;
   if (FALSE == get_env_item(blaster, &sb.dma, 'D', 10))
      return FALSE;
   get_env_item(blaster, &sb.dma16, 'H', 10);

   return TRUE;
}

/* probe */
static boolean sb_probe(void)
{
   boolean success = FALSE;
   int probe_iteration;

   for (probe_iteration = 0; probe_iteration < 2; probe_iteration++)
   {
      if (0 == probe_iteration)
         success = parse_blaster_env();
      else
         success = sb_detect();

      if (FALSE == success)
         continue;

      if (FALSE == dsp_reset(sb.baseio))
      {
         log_printf("could not reset SB DSP: check BLASTER= variable\n");
         return FALSE;
      }
      else
      {
         sb.dspversion = dsp_getversion(sb.baseio);
         sb_probed = TRUE;
         return TRUE;
      }
   }

   return FALSE;
}

/*
** Interrupt handler for 8/16-bit audio 
*/

static void sb_isr(void)
{
   uint32 address;

   if (FALSE == dma_autoinit)
   {
      dsp_write(sb.baseio, 0x14);
      dsp_write(sb.baseio, LOW_BYTE(sb.bufsize - 1));
      dsp_write(sb.baseio, HIGH_BYTE(sb.bufsize - 1));
   }

   dma_count++;

   /* indicate we got the interrupt */
   inportb(dma_ackport);

   /* determine the current playback position */
   address = inportb(dma_addrport);
   address |= (inportb(dma_addrport) << 8);
   address -= dos_offset;

   if (address < sb.bufsize)
   {
      sb.callback(sb.buffer + sb.halfbuf, sb.bufsize);

#ifndef DJGPP_USE_NEARPTR
      /* copy data to DOS memory buffer... bleh! */
      dosmemput(sb.buffer + sb.halfbuf, sb.halfbuf, dos_bufaddr + sb.halfbuf);
#endif /* !DJGPP_USE_NEARPTR */
   }
   else
   {
      sb.callback(sb.buffer, sb.bufsize);

#ifndef DJGPP_USE_NEARPTR
      /* copy data to DOS memory buffer... bleh! */
      dosmemput(sb.buffer, sb.halfbuf, dos_bufaddr);
#endif /* !DJGPP_USE_NEARPTR */
   }

   /* acknowledge interrupt was taken */
   if (sb.irq > 7)
      outportb(0xA0, 0x20);
   outportb(0x20, 0x20);
}
END_OF_STATIC_FUNCTION(sb_isr);

/* install the SB ISR */
static void sb_setisr(int irq, int bps)
{
   /* if your IRQ is greater than 10, you're
   ** going to be screwed, anyway..  but
   ** we'll allow it for the time being
   */
   /*assert(irq < 11);*/

   /* lock variables, routines */
   LOCK_VARIABLE(dma_autoinit);
   LOCK_VARIABLE(dma_count);
   LOCK_VARIABLE(dma_ackport);
   LOCK_VARIABLE(dma_addrport);
   LOCK_VARIABLE(dos_bufaddr);
   LOCK_VARIABLE(sb.baseio);
   LOCK_VARIABLE(sb.bufsize);
   LOCK_VARIABLE(sb.buffer);
   LOCK_VARIABLE(sb.halfbuf);
   LOCK_VARIABLE(sb.irq);
   LOCK_FUNCTION(sb_isr);

   if (16 == sb.bps)
   {
      dma_ackport = sb.baseio + 0x0F;
      dma_addrport = ((sb.dma16 - 4) << 2) + 0xC0;
   }
   else
   {
      dma_ackport = sb.baseio + 0x0E;
      dma_addrport = (sb.dma << 1) + 0x00;
   }

   disable_ints();

   outportb(pic_maskport, inportb(pic_maskport) | irq_stopmask);

   _go32_dpmi_get_protected_mode_interrupt_vector(irq_intvector, &old_interrupt);
   new_interrupt.pm_offset = (int) sb_isr;
   new_interrupt.pm_selector = _go32_my_cs();
   _go32_dpmi_allocate_iret_wrapper(&new_interrupt);
   _go32_dpmi_set_protected_mode_interrupt_vector(irq_intvector, &new_interrupt);

   /* unmask the pic, get things ready to roll */
   outportb(pic_maskport, inportb(pic_maskport) & irq_startmask);

   enable_ints();
}

/* remove SB ISR, restore old */
static void sb_resetisr(int irq)
{
   disable_ints();

   outportb(pic_maskport, inportb(pic_maskport) | irq_stopmask);

   _go32_dpmi_set_protected_mode_interrupt_vector(irq_intvector, &old_interrupt);
   _go32_dpmi_free_iret_wrapper(&new_interrupt);

   enable_ints();
}

/* allocate sound buffers */
static boolean sb_allocate_buffers(int bps, int buf_size, boolean stereo)
{
   int double_bufsize;

   if (16 == bps)
      sb.halfbuf = buf_size * sizeof(uint16);
   else
      sb.halfbuf = buf_size * sizeof(uint8);

   double_bufsize = 2 * sb.halfbuf;

   dos_buffer.size = (double_bufsize + 15) >> 4;
   if (_go32_dpmi_allocate_dos_memory(&dos_buffer))
      return FALSE;

   /* calc linear address */
   dos_bufaddr = dos_buffer.rm_segment << 4;
   if (16 == bps)
   {
      dos_page = (dos_bufaddr >> 16) & 0xFF;
      /* bleh! thanks, creative! */
      dos_offset = (dos_bufaddr >> 1) & 0xFFFF;
   }
   else
   {
      dos_page = (dos_bufaddr >> 16) & 0xFF;
      dos_offset = dos_bufaddr & 0xFFFF;
   }

#ifdef DJGPP_USE_NEARPTR
   sb.buffer = (uint8 *) dos_bufaddr + __djgpp_conventional_base;
#else
   sb.buffer = malloc(double_bufsize);
   if (NULL == sb.buffer)
      return FALSE;
#endif /* DJGPP_USE_NEARPTR */

   /* clear out the buffers */
   if (16 == bps)
      memset(sb.buffer, 0, double_bufsize);
   else
      memset(sb.buffer, 0x80, double_bufsize);

#ifndef DJGPP_USE_NEARPTR
   dosmemput(sb.buffer, double_bufsize, dos_bufaddr);
#endif /* !DJGPP_USE_NEARPTR */

   return TRUE;
}

/* free them buffers */
static void sb_free_buffers(void)
{
   sb.callback = NULL;

   _go32_dpmi_free_dos_memory(&dos_buffer);

#ifndef DJGPP_USE_NEARPTR
   free(sb.buffer);
#endif /* !DJGPP_USE_NEARPTR */

   sb.buffer = NULL;
}

/* get rid of all things SB */
void sb_shutdown(void)
{
   /* don't do this if we don't need to! */
   if (FALSE == sb_initialized)
      return;

   dsp_reset(sb.baseio);

   sb_initialized = FALSE;

   /* free buffer madness */
   sb_free_buffers();

   sb_resetisr(sb.irq);
}

/* initialize sound bastard */
boolean sb_init(int *sample_rate, int *bps, int *buf_size, boolean *stereo)
{
   memset(&sb, 0, sizeof(sb));
   
   /* don't init twice! */
   if (TRUE == sb_initialized)
      return TRUE;

   sb.samplerate = *sample_rate;
   sb.bps = *bps;
   sb.bufsize = *buf_size;
   sb.stereo = *stereo;

   /* don't probe twice */
   if (FALSE == sb_probed)
   {
      if (FALSE == sb_probe())
         return FALSE;
   }

#ifdef NOFRENDO_DEBUG
   log_printf("\nSB DSP version: %d.%d baseio: %X IRQ: %d DMA: %d High: %d\n",
              sb.dspversion >> 8, sb.dspversion & 0xFF,
              sb.baseio, sb.irq, sb.dma, sb.dma16);
#endif /* NOFRENDO_DEBUG */

   /* SB's with DSP < 3.02 can't do higher than 22050 */
   /* NOTE: this doesn't seem to hold true, completely.  More investigation
      is definitely needed.
   */
/*
   if (sb.dspversion < 0x0302 && sample_rate > 22050)
   {
      log_printf("SB DSP version %d.%d - can't do higher than 22050 Hz\n",
                 sb.dspversion >> 8, sb.dspversion & 0xFF);
      sb.samplerate = 22050;
      sb.bufsize /= (*sample_rate / sb.samplerate);
   }
*/
   if (sb.dspversion >= 0x0200)
      dma_autoinit = TRUE;
   else
      dma_autoinit = FALSE;

   if (sb.dspversion < 0x0400 && TRUE == sb.stereo)
      sb.stereo = FALSE;

   if (TRUE == sb.stereo)
      sb.bufsize <<= 1;

   /* sanity check */
   if (16 == sb.bps && (INVALID == sb.dma16))
   {
      log_printf("16-bit DMA channel not available, dropping to 8-bit\n");
      sb.bps = 8;
   }

   /* allocate buffer / DOS memory */
   if (FALSE == sb_allocate_buffers(sb.bps, sb.bufsize, sb.stereo))
      return FALSE;

   if (sb.irq < 8)
   {
      /* PIC 1 */
      irq_intvector = 0x08 + sb.irq;
      pic_rotateport = 0x20;
      pic_maskport = 0x21;
   }
   else
   {
      /* PIC 2 */
      irq_intvector = 0x70 + (sb.irq - 8);
      pic_rotateport = 0xA0;
      pic_maskport = 0xA1;
   }

   irq_stopmask = 1 << (sb.irq % 8);
   irq_startmask = ~irq_stopmask;

   /* set the new IRQ vector! */
   sb_setisr(sb.irq, sb.bps);

   sb_initialized = TRUE;

   /* return the actual values */
   *sample_rate = sb.samplerate;
   *bps = sb.bps;
   *buf_size = sb.bufsize;
   *stereo = sb.stereo;

   return TRUE;
}

void sb_stopoutput(void)
{
   if (sb_initialized)
   {
      if (8 == sb.bps)
      {
         dsp_write(sb.baseio, 0xD0);   /* pause 8-bit DMA */
         dsp_write(sb.baseio, 0xD3);
      }
      else
      {
         dsp_write(sb.baseio, 0xD5);   /* pause 16-bit DMA */
         dsp_write(sb.baseio, 0xDA);
         dsp_write(sb.baseio, 0xD5);
      }
   }
}

/* return time constant for older sound bastards */
static uint8 get_time_constant(int rate)
{
   return ((65536 - (256000000L / rate)) >> 8);
}

static void init_samplerate(int rate)
{
   if (8 == sb.bps && sb.dspversion < 0x400)
   {
      dsp_write(sb.baseio, 0x40);
      dsp_write(sb.baseio, get_time_constant(rate));
   }
   else
   {
      dsp_write(sb.baseio, 0x41);
      dsp_write(sb.baseio, rate >> 8);
      dsp_write(sb.baseio, rate & 0xFF);
   }
}

/* set the sample rate */
void sb_setrate(int rate)
{
   if (8 == sb.bps)
   {
      dsp_write(sb.baseio, 0xD0);   /* pause 8-bit DMA */
      init_samplerate(rate);
      dsp_write(sb.baseio, 0xD4);   /* continue 8-bit DMA */
   }
   else
   {
      dsp_write(sb.baseio, 0xD5);   /* pause 16-bit DMA */
      init_samplerate(rate);
      dsp_write(sb.baseio, 0xD6);   /* continue 16-bit DMA */
   }

   sb.samplerate = rate;
}

/* start SB DMA transfer */
static void start_transfer(void)
{
   int dma_maskport, dma_clrptrport;
   int dma_startmask, dma_stopmask;
   int dma_modeport, dma_countport, dma_pageport;
   uint8 dma_mode, start_command, mode_command;

   dma_count = 0;

   dma_length = sb.bufsize << 1;

   if (16 == sb.bps)
   {
      dma_maskport = 0xD4;
      dma_clrptrport = 0xD8;
      dma_modeport = 0xD6;
      dma_countport = 0xC2 + 4 * (sb.dma16 - 4);
      dma_pageport = dma_ports[sb.dma16];
      dma_stopmask = (sb.dma16 - 4) + 0x04;
      dma_startmask = (sb.dma16 - 4) + 0x00;

      if (dma_autoinit)
         dma_mode = (sb.dma16 - 4) + 0x58;
      else
         dma_mode = (sb.dma16 - 4) + 0x48;

      if (TRUE == dma_autoinit)
         start_command = 0xB6;   /* autoinit 16 bit DMA */
      else
         start_command = 0xB0;
   }
   else
   {
      dma_maskport = 0x0A;
      dma_clrptrport = 0x0C;
      dma_modeport = 0x0B;
      dma_countport = 0x01 + 2 * sb.dma;
      dma_pageport = dma_ports[sb.dma];
      dma_stopmask = sb.dma + 0x04;
      dma_startmask = sb.dma + 0x00;
      dma_mode = sb.dma + 0x58;

      if (TRUE == dma_autoinit)
         start_command = 0xC6;   /* autoinit 8 bit DMA */
      else
         start_command = 0xC0;
   }

   /* set signed/unsigned */
   if (8 == sb.bps)
      mode_command = 0;       /* signed */
   else
      mode_command = 0x10;    /* unsigned */

   if (TRUE == sb.stereo)
      mode_command |= 0x20;   /* OR in the stereo bit */

   outportb(dma_maskport, dma_stopmask);
   outportb(dma_clrptrport, 0x00);
   outportb(dma_modeport, dma_mode);
   outportb(dma_addrport, LOW_BYTE(dos_offset));
   outportb(dma_addrport, HIGH_BYTE(dos_offset));
   outportb(dma_countport, LOW_BYTE(dma_length - 1));
   outportb(dma_countport, HIGH_BYTE(dma_length - 1));
   outportb(dma_pageport, dos_page);
   outportb(dma_maskport, dma_startmask);

   init_samplerate(sb.samplerate);

   /* start things going */
   if (16 == sb.bps)
   {
      dsp_write(sb.baseio, start_command);
      dsp_write(sb.baseio, mode_command);
      dsp_write(sb.baseio, LOW_BYTE(sb.bufsize - 1));
      dsp_write(sb.baseio, HIGH_BYTE(sb.bufsize - 1));
   }
   else
   {
      /* turn on speaker */
      dsp_write(sb.baseio, 0xD1);

      if (TRUE == dma_autoinit)
      {
         if (sb.dspversion < 0x0400)
         {
            dsp_write(sb.baseio, 0x48); /* set buffer size */
            dsp_write(sb.baseio, LOW_BYTE(sb.bufsize - 1));
            dsp_write(sb.baseio, HIGH_BYTE(sb.bufsize - 1));
            dsp_write(sb.baseio, 0x1C);
/*            dsp_write(sb.baseio, 0x90); */
         }
         else /* better support for SB 16 */
         {
            dsp_write(sb.baseio, start_command);
            dsp_write(sb.baseio, mode_command);
            dsp_write(sb.baseio, LOW_BYTE(sb.bufsize - 1));
            dsp_write(sb.baseio, HIGH_BYTE(sb.bufsize - 1));
         }
      }
      else
      {
         dsp_write(sb.baseio, 0x14);
         dsp_write(sb.baseio, LOW_BYTE(sb.bufsize - 1));
         dsp_write(sb.baseio, HIGH_BYTE(sb.bufsize - 1));
      }
   }
}

/* start playing the output buffer */
int sb_startoutput(sbmix_t fillbuf)
{
   int count;

   /* make sure we really should be here... */
   if (FALSE == sb_initialized || NULL == fillbuf)
      return -1;

   sb_stopoutput();

   sb.callback = fillbuf;

   start_transfer();

   /* ensure interrupts are really firing... */
   count = clock();

   while (((clock() - count) < CLOCKS_PER_SEC / 2) && (dma_count < 2))
      ; /* loop */

   if (dma_count < 2)
   {
      if (TRUE == dma_autoinit)
      {
         log_printf("Autoinit DMA failed, trying standard mode.\n");
         dsp_reset(sb.baseio);
         dma_autoinit = FALSE;
         return (sb_startoutput(fillbuf));
      }
      else
      {
         log_printf("Standard DMA mode failed, sound will not be heard.\n");
         return -1;
      }
   }
   else
      return 0;
}

/*
** $Log: dos_sb.c,v $
** Revision 1.4  2000/06/09 15:12:27  matt
** initial revision
**
*/
