#ifndef READTEMP_H
#define READTEMP_H
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

/// smbus part
typedef struct smbus_channel
{
	struct PCIDevice *pci;
	uint32 base;
} smbus_channel_t;

#define SMBUS_MAXCHANNELS		5

BOOL smbus_startup(struct DockyBase *db);
void smbus_shutdown(struct DockyBase *db);
///

/// tmp423 part
typedef struct tmp423_device
{
	smbus_channel_t *chan;
	uint8 smbus_addr;
} tmp423_device_t;

int tmp423_read_all_temps(struct DockyData *dd);
///

#endif
