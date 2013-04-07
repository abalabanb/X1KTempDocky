/*
** X1kTemp.docky
** (c) 2013 Alexandre Balaban <alexandre -(@)- balaban -(.)- fr>
**
** Based upon readtemp (c) 2013 by Thomas Frieden
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
** INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
** CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
** ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
**
*/


#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/timer.h>
#include <expansion/pci.h>

#include "dockybase.h"
#include "readtemp.h"

/* **********************************************************************
 * SMBus driver part
 */

/* Register */
#define smbus_mtxfifo			0x00
#define smbus_mrxfifo			0x04
#define smbus_mcnt				0x08
#define smbus_xfsta				0x0c
#define smbus_saddr				0x10
#define smbus_smsta				0x14
#define smbus_imask				0x18
#define smbus_ctl				0x1c
#define smbus_stxfifo			0x20
#define smbus_srxfifo			0x24

/* Bit definitions */
#define smbus_mtxfifo_read		(1L << 10)
#define smbus_mtxfifo_stop		(1L << 9)
#define smbus_mtxfifo_start		(1L << 8)
#define smbus_mtxfifo_rd_slave	1

#define smbus_mrxfifo_empty		(1L << 8)

#define smbus_ctl_msw_mask		0x07ff0000
#define smbus_ctl_msw_shift		16
#define smbus_ctl_msw(x)		(((x) << smbus_ctl_msw_shift) & smbus_ctl_msw_mask)
#define smbus_ctl_mrr			(1L << 10)
#define smbus_ctl_mtr			(1L << 9)
#define smbus_ctl_ujm			(1L << 8)
#define smbus_ctl_clk_mask		0x000000ff
#define smbus_ctl_clk_shift		0
#define smbus_ctl_clk(x)		(((x) << smbus_ctl_clk_shift) & smbus_ctl_clk_mask)

#define smbus_smsta_xen			(1L << 27)
#define smbus_smsta_mto			(1L << 23)
#define smbus_smsta_mta			(1L << 22)
#define smbus_smsta_mtn			(1L << 21)

#define smbus_def_clk			84


static void pci_write_long_little(volatile uint32 *addr, uint32 x)
{
    asm volatile ("stwbrx %0, 0, %1; eieio" :: "r" (x), "r" (addr));

}

static uint32 pci_read_long_little(volatile uint32 *addr)
{
        volatile uint32 x;

        asm volatile ("sync; lwbrx %0, 0, %1" : "=r" (x) : "r" (addr));

        return x;
}

static void __write_chan(smbus_channel_t *chan, uint32 offset, uint32 val)
{
	//chan->pci->OutLong(offset, val);
	pci_write_long_little((uint32 *)(chan->base+offset), val);
}


static uint32 __read_chan(smbus_channel_t *chan, uint32 offset)
{
	//return chan->pci->InLong(offset);
	return pci_read_long_little((uint32 *)(chan->base+offset));
}


void smbus_clear(smbus_channel_t *chan)
{
	/* Simply write the status again to clear all error bits */
	uint32 status = __read_chan(chan, smbus_smsta);
	__write_chan(chan, smbus_smsta, status);
}


void smbus_init(smbus_channel_t *chan)
{
	/* Set frequence to 100 KHz, and reset fifos */
	__write_chan(chan, smbus_ctl, 
			smbus_ctl_clk(smbus_def_clk) | smbus_ctl_mrr | smbus_ctl_mtr);
	smbus_clear(chan);
}


#define eclto64(ecv) ((uint64)ecv.ev_hi << 32 | ecv.ev_lo)

int smbus_waitready(struct DockyData *dd, smbus_channel_t *chan)
{
    struct DockyBase *db = dd->Base;
	uint32 status = 0;
	struct EClockVal ecv;
	uint32 eclk = ITimer->ReadEClock(&ecv);
	
	uint64 end = eclto64(ecv) + eclk;
	
	for (;;)
	{
		status = __read_chan(chan, smbus_smsta);
		if (status & smbus_smsta_xen)
			break;
		
		ITimer->ReadEClock(&ecv);
		uint64 cur = eclto64(ecv);
		if (cur > end)
		{
#ifndef NDEBUG
			IExec->DebugPrintF("[smbus_waitready] smbus stuck\n");
#endif
			__write_chan(chan, smbus_smsta, status);
			return -1;
		}
	}
	
	if (status & (smbus_smsta_mto | smbus_smsta_mta | smbus_smsta_mtn))
	{
#ifndef NDEBUG
		if (status & (smbus_smsta_mto | smbus_smsta_mta))
			IExec->DebugPrintF("[smbus_waitready] unexpected SMBUS status: 0x%08x\n", status);
#endif
		__write_chan(chan, smbus_ctl, 
				smbus_ctl_clk(smbus_def_clk) | smbus_ctl_mrr | smbus_ctl_mtr);
		
		__write_chan(chan, smbus_smsta,
				(status & (smbus_smsta_mto | smbus_smsta_mta | smbus_smsta_mtn | smbus_smsta_xen)));

		return -1;
	}
	
	__write_chan(chan, smbus_smsta, smbus_smsta_xen);
}


int smbus_write(struct DockyData *dd, smbus_channel_t *chan, uint8 slave, uint8 *buffer, int32 length)
{
    struct DockyBase *db = dd->Base;
	smbus_clear(chan);
	
	__write_chan(chan, smbus_mtxfifo, (((uint32)slave) << 1) | smbus_mtxfifo_start);
	
	while (length > 1)
	{
		__write_chan(chan, smbus_mtxfifo, *buffer);
		buffer++;
		length--;
	}
	
	__write_chan(chan, smbus_mtxfifo, (*buffer) | smbus_mtxfifo_stop);
	
	return smbus_waitready(dd, chan);
}


int smbus_read(struct DockyData *dd, smbus_channel_t *chan, uint8 slave, uint8 *buffer, int32 length)
{
    struct DockyBase *db = dd->Base;
	uint32 x;
	
	smbus_clear(chan);
	
	__write_chan(chan, smbus_mtxfifo, 
			( ((uint32)slave) << 1) | smbus_mtxfifo_start | smbus_mtxfifo_rd_slave);
	__write_chan(chan, smbus_mtxfifo,
			length | smbus_mtxfifo_stop | smbus_mtxfifo_read);
	
	if (smbus_waitready(dd, chan) < 0)
		return -1;
	
	while (length > 0)
	{
		x = __read_chan(chan, smbus_mrxfifo);
		if (x & smbus_mrxfifo_empty)
		{
#ifndef NDEBUG
			IExec->DebugPrintF("[smbus_read] fifo empty\n");
#endif
		}
		
		*buffer++  = (uint8)x;
		length--;
	}
	
	return 0;
}


BOOL smbus_startup(struct DockyBase *db)
{
    BOOL bStarted = FALSE;

	ExpansionLib = IExec->OpenLibrary("expansion.library", 0);
	IPCI = (struct PCIIFace *)IExec->GetInterface(ExpansionLib, "pci", 1, TAG_DONE);
	
	int idx = 0;
	db->smbus_numchannels = 0;
	
	while (idx >= 0 && idx < SMBUS_MAXCHANNELS)
	{
		db->channels[idx].pci = IPCI->FindDeviceTags(
				FDT_VendorID,		0x1959,
				FDT_DeviceID,		0xA003,
				FDT_Index,			idx,
			TAG_DONE);
		
		if (db->channels[idx].pci)
		{
#ifndef NDEBUG
			struct PCIResourceRange *range = db->channels[idx].pci->GetResourceRange(0);
			IExec->DebugPrintF("SMBus %d has I/O range %p\n", idx, range->BaseAddress);
			db->channels[idx].base = range->BaseAddress | 0xfc800000;
			db->channels[idx].pci->FreeResourceRange(range);
#endif
			db->channels[idx].pci->SetEndian(PCI_MODE_LITTLE_ENDIAN);
			smbus_init(&db->channels[idx]);
			idx++;
			db->smbus_numchannels++;
		}
		else
			idx = SMBUS_MAXCHANNELS + 1;
	}
	
#ifndef NDEBUG
	IExec->DebugPrintF("Found %d smbus channels\n", db->smbus_numchannels);
#endif

    if(0 < db->smbus_numchannels)
    {
    	/* Open timer.device (add error checking) */
    	db->TimerPort = (struct MsgPort *)IExec->AllocSysObjectTags(ASOT_PORT, TAG_DONE);
    	db->TimerRequest = (struct TimeRequest *)IExec->AllocSysObjectTags(ASOT_IOREQUEST,
    			ASOIOR_ReplyPort,	db->TimerPort,
    			TAG_DONE);
    	
    	if (IExec->OpenDevice("timer.device", UNIT_ECLOCK, (struct IORequest *)db->TimerRequest, 0) == 0)
    	{
    		ITimer = (struct TimerIFace *)IExec->GetInterface((struct Library *)db->TimerRequest->Request.io_Device, "main", 1, NULL);
    	    bStarted = TRUE;
    	}

	}

#ifndef NDEBUG
    if(!bStarted)
    	IExec->DebugPrintF("smbus NOT started\n");
#endif

    return bStarted;
}

void smbus_shutdown(struct DockyBase *db)
{
	int i;
	for (i = 0; i < db->smbus_numchannels; i++)
	{
		if (db->channels[i].pci)
		{
			IExec->DropInterface((struct Interface *)db->channels[i].pci);
		}
	}
	
	
	IExec->DropInterface((struct Interface *)IPCI);
	IExec->CloseLibrary(ExpansionLib);
	
	IExec->CloseDevice((struct IORequest *)db->TimerRequest);
	IExec->FreeSysObject(ASOT_IOREQUEST, db->TimerRequest);
	IExec->FreeSysObject(ASOT_PORT, db->TimerPort);
}




/* **********************************************************************
 * Temperature reading
 */

/* Registers */
#define tmp423_ltemp_h			0x00
#define tmp423_ltemp_l			0x10
#define tmp423_rtemp1_h			0x01
#define tmp423_rtemp1_l			0x11
#define tmp423_rtemp2_h			0x02
#define tmp423_rtemp2_l			0x12
#define tmp423_rtemp3_h			0x03
#define tmp423_rtemp3_l			0x13
#define tmp423_status			0x08
#define tmp423_cfg1				0x09
#define tmp423_cfg2				0x0a
#define tmp423_vendor_id		0xfe
#define tmp423_device_id		0xff

/* Bits */
#define tmp423_cfg1_ext_range	(1 << 2)

int tmp423_read_reg(struct DockyData *dd, smbus_channel_t *chan, uint8 slave, uint8 reg, uint8* bptr)
{
	uint8 buf[1];
    struct DockyBase *db = dd->Base;

	/* Select register address */
	buf[0] = reg;
	int err= smbus_write(dd, chan, slave, buf, 1);
	if (err < 0)
		return err;
	
	/* Read register */
	return smbus_read(dd, chan, slave, bptr, 1);
}


int tmp423_write_reg(struct DockyData *dd, smbus_channel_t *chan, uint8 slave, uint8 reg, uint8 val)
{
	uint8 buf[2];
	int err;
    struct DockyBase *db = dd->Base;

	/* Set register address and value */
	buf[0] = reg;
	buf[1] = val;
	
	err = smbus_write(dd, chan, slave, buf, 2);
	if (err < 0)
		return err;
	
	return smbus_read(dd, chan, slave, buf, 1);
}


int tmp423_temp_offset64(struct DockyData *dd, tmp423_device_t *dev)
{
	uint8 cfg;
	int rc;
    struct DockyBase *db = dd->Base;
	
	rc = tmp423_read_reg(dd, dev->chan, dev->smbus_addr, tmp423_cfg1, &cfg);
	if (rc == 0)
	{
		if (cfg & tmp423_cfg1_ext_range)
			rc = 1;
		else 
			rc = 0;
	}
	else
	{
#ifndef NDEBUG
		IExec->DebugPrintF("[tmp423_temp_offset64] faild ot read cfg1\n");
#endif
		rc = -1;
	}
	
	return rc;
}


int tmp423_read_temp(struct DockyData *dd, tmp423_device_t *dev, uint8 high, uint8 low, int16 *res)
{
	uint8 h, l;
    struct DockyBase *db = dd->Base;

	int rc = tmp423_read_reg(dd, dev->chan, dev->smbus_addr, high, (uint8 *)&h);
	if (rc < 0)
    {
    #ifndef NDEBUG
    	IExec->DebugPrintF("[read_temp] can't read_reg rc=%ld\n", rc);
    #endif
		return rc;
	}

	rc = tmp423_read_reg(dd, dev->chan, dev->smbus_addr, low, (uint8 *)&l);
	if (rc < 0)
    {
    #ifndef NDEBUG
    	IExec->DebugPrintF("[read_temp] can't read_reg rc=%ld\n", rc);
    #endif
		return rc;
	}
	
	*res = h;
	if (tmp423_temp_offset64(dd, dev) == 1)
		*res -= 64;
	
    if (dd->bUseFahrenheit)
    {
        *res = *res * 9 / 5 + 32;
    }

	return 0;
}


#define TMP423_CHANNEL 	&(db->channels[0])
#define TMP423_ADDR		0x4c

int tmp423_read_all_temps(struct DockyData *dd)
{
    int rc = 0;
    struct DockyBase *db = dd->Base;
    if(NULL == dd->dev.chan)
    {
    	dd->dev.chan = TMP423_CHANNEL;
        dd->dev.smbus_addr = TMP423_ADDR;
	}

    rc = tmp423_read_temp(dd, &dd->dev, tmp423_ltemp_h, tmp423_ltemp_l, &dd->MBTemp[dd->curIdx]);
	if(!rc)
        rc = tmp423_read_temp(dd, &dd->dev, tmp423_rtemp1_h, tmp423_rtemp1_l, &dd->CPUTemp[dd->curIdx]);
	if(!rc)
    	rc = tmp423_read_temp(dd, &dd->dev, tmp423_rtemp2_h, tmp423_rtemp2_l, &dd->Core1Temp[dd->curIdx]);
	if(!rc)
    	rc = tmp423_read_temp(dd, &dd->dev, tmp423_rtemp3_h, tmp423_rtemp3_l, &dd->Core2Temp[dd->curIdx]);

#ifndef NDEBUG
    if(rc)
    	IExec->DebugPrintF("[read_all_temps] returns %ld\n", rc);
#endif

    if(!rc) dd->curIdx = (dd->curIdx+1)%dd->maxIdx;

    return rc;
}

/*int main()
{
	smbus_startup();

	tmp423_device_t dev = {TMP423_CHANNEL, TMP423_ADDR};

	uint8 manufacturer, device;
	tmp423_read_reg(TMP423_CHANNEL, TMP423_ADDR, tmp423_vendor_id, &manufacturer);
	tmp423_read_reg(TMP423_CHANNEL, TMP423_ADDR, tmp423_device_id, &device);

	printf("Reading temperature from tmp423 (0x%02lx, 0x%02lx)\n", (uint32)manufacturer, (uint32)device);

	uint16 local, rem1, rem2, rem3;

	tmp423_read_temp(&dev, tmp423_ltemp_h, tmp423_ltemp_l, &local);
	tmp423_read_temp(&dev, tmp423_rtemp1_h, tmp423_rtemp1_l, &rem1);
	tmp423_read_temp(&dev, tmp423_rtemp2_h, tmp423_rtemp2_l, &rem2);
	tmp423_read_temp(&dev, tmp423_rtemp3_h, tmp423_rtemp3_l, &rem3);

	printf("Local (Mainboard)	:	%d °C\n",	(uint32)local);
	printf("Remote1 (CPU)		:	%d °C\n",	(uint32)rem1);
	printf("Remote2 (Core 0)	:	%d °C\n",	(uint32)rem2);
	printf("Remote3 (Core 1)	:	%d °C\n",	(uint32)rem3);

	smbus_shutdown();
	return 0;
}*/
