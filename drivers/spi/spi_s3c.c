/*
 * spi_s3c.c - Samsung SOC SPI controller driver.
 * By -- Jaswinder Singh <jassi.brar@samsung.com>
 *
 * Copyright (C) 2009 Samsung Electronics Ltd.
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <asm/dma.h>
#include <plat/spi.h>
#include "spi_s3c.h"


//#define DEBUGSPI

#ifdef DEBUGSPI
#define dbg_printk(x...)  printk(x)

#define dump_regs(sspi)   dump_regs_line(sspi, __func__, __LINE__)
static void dump_regs_line(struct s3cspi_bus *sspi, const char *func, int line)
{
	u32 val;

	printk("[%s:%d] SPI-%d :: ",func, line, sspi->spi_mstinfo->pdev->id );
	val = readl(sspi->regs + S3C_SPI_CH_CFG);
	printk("CHN-%x\t", val);
	val = readl(sspi->regs + S3C_SPI_CLK_CFG);
	printk("CLK-%x\t", val);
	val = readl(sspi->regs + S3C_SPI_MODE_CFG);
	printk("MOD-%x\t", val);
	val = readl(sspi->regs + S3C_SPI_SLAVE_SEL);
	printk("SLVSEL-%x\t", val);
	val = readl(sspi->regs + S3C_SPI_STATUS);
	if(val & SPI_STUS_TX_DONE)
	   printk("TX_done\t");
	if(val & SPI_STUS_TRAILCNT_ZERO)
	   printk("TrailZ\t");
	if(val & SPI_STUS_RX_OVERRUN_ERR)
	   printk("RX_Ovrn\t");
	if(val & SPI_STUS_RX_UNDERRUN_ERR)
	   printk("Rx_Unrn\t");
	if(val & SPI_STUS_TX_OVERRUN_ERR)
	   printk("Tx_Ovrn\t");
	if(val & SPI_STUS_TX_UNDERRUN_ERR)
	   printk("Tx_Unrn\t");
	if(val & SPI_STUS_RX_FIFORDY)
	   printk("Rx_Rdy\t");
	if(val & SPI_STUS_TX_FIFORDY)
	   printk("Tx_Rdy\t");
	printk("Rx/TxLvl=%d,%d \n", RX_FIFO_LVL(val), TX_FIFO_LVL(val));
    //print_symbol("\t(caller %s)\n", (unsigned long) __builtin_return_address(0));
}

static void dump_spidevice_info(struct spi_device *spi)
{
	dbg_printk("Modalias = %s\n", spi->modalias);
	dbg_printk("Slave-%d on Bus-%d\n", spi->chip_select, spi->master->bus_num);
	dbg_printk("max_speed_hz = %d\n", spi->max_speed_hz);
	dbg_printk("bits_per_word = %d\n", spi->bits_per_word);
	dbg_printk("irq = %d\n", spi->irq);
	dbg_printk("Clk Phs = %d\n", spi->mode & SPI_CPHA);
	dbg_printk("Clk Pol = %d\n", spi->mode & SPI_CPOL);
	dbg_printk("ActiveCS = %s\n", (spi->mode & (1<<2)) ? "high" : "low" );
	dbg_printk("Our Mode = %s\n", (spi->mode & SPI_READY)? "Slave" : "Master" );
}

#else

#define dbg_printk(x...)		/**/
#define dump_regs(sspi) 		/**/
#define dump_spidevice_info(spi) 	/**/

#endif

static void __dump_regs_line(struct s3cspi_bus *sspi)
{
	u32 val;

	printk("SPI-%d :: ",sspi->spi_mstinfo->pdev->id );
	val = readl(sspi->regs + S3C_SPI_CH_CFG);
	printk("CHN-%x\t", val);
	val = readl(sspi->regs + S3C_SPI_CLK_CFG);
	printk("CLK-%x\t", val);
	val = readl(sspi->regs + S3C_SPI_MODE_CFG);
	printk("MOD-%x\t", val);
	val = readl(sspi->regs + S3C_SPI_SLAVE_SEL);
	printk("SLVSEL-%x\t", val);
	val = readl(sspi->regs + S3C_SPI_STATUS);
	if(val & SPI_STUS_TX_DONE)
	   printk("TX_done\t");
	if(val & SPI_STUS_TRAILCNT_ZERO)
	   printk("TrailZ\t");
	if(val & SPI_STUS_RX_OVERRUN_ERR)
	   printk("RX_Ovrn\t");
	if(val & SPI_STUS_RX_UNDERRUN_ERR)
	   printk("Rx_Unrn\t");
	if(val & SPI_STUS_TX_OVERRUN_ERR)
	   printk("Tx_Ovrn\t");
	if(val & SPI_STUS_TX_UNDERRUN_ERR)
	   printk("Tx_Unrn\t");
	if(val & SPI_STUS_RX_FIFORDY)
	   printk("Rx_Rdy\t");
	if(val & SPI_STUS_TX_FIFORDY)
	   printk("Tx_Rdy\t");
	printk("Rx/TxLvl=%d,%d\t", RX_FIFO_LVL(val), TX_FIFO_LVL(val));
	val = readl(sspi->regs + S3C_SPI_PACKET_CNT);
	printk("PACKET_CNT-%x\t", val);
	val = readl(sspi->regs + S3C_SPI_FB_CLK);
	printk("FB_CLK-%x\n", val);
}


static struct s3c2410_dma_client s3c_spi_dma_client = {
	.name = "s3cspi-dma",
};

static inline void enable_spidma(struct s3cspi_bus *sspi, struct spi_transfer *xfer)
{
	u32 val;

	val = readl(sspi->regs + S3C_SPI_MODE_CFG);
	val &= ~(SPI_MODE_TXDMA_ON | SPI_MODE_RXDMA_ON);
	if(xfer->tx_buf != NULL)
	   val |= SPI_MODE_TXDMA_ON;
	if(xfer->rx_buf != NULL)
	   val |= SPI_MODE_RXDMA_ON;
	writel(val, sspi->regs + S3C_SPI_MODE_CFG);
}

static inline void flush_dma(struct s3cspi_bus *sspi, struct spi_transfer *xfer)
{
	if(xfer->tx_buf != NULL)
	   s3c2410_dma_ctrl(sspi->tx_dmach, S3C2410_DMAOP_FLUSH);
	if(xfer->rx_buf != NULL)
	   s3c2410_dma_ctrl(sspi->rx_dmach, S3C2410_DMAOP_FLUSH);
}

static inline void flush_spi(struct s3cspi_bus *sspi)
{
	u32 val;

	val = readl(sspi->regs + S3C_SPI_CH_CFG);
	val |= SPI_CH_SW_RST;
	//val &= ~SPI_CH_HS_EN;
	//if((sspi->cur_speed > 30000000UL)) /* TODO ??? */
	   val |= SPI_CH_HS_EN;
	writel(val, sspi->regs + S3C_SPI_CH_CFG);

	/* Flush TxFIFO*/
	do{
	   val = readl(sspi->regs + S3C_SPI_STATUS);
	   val = TX_FIFO_LVL(val);
	}while(val);

	/* Flush RxFIFO*/
	val = readl(sspi->regs + S3C_SPI_STATUS);
	val = RX_FIFO_LVL(val);
	while(val){
	   readl(sspi->regs + S3C_SPI_RX_DATA);
	   val = readl(sspi->regs + S3C_SPI_STATUS);
	   val = RX_FIFO_LVL(val);
	}

	val = readl(sspi->regs + S3C_SPI_CH_CFG);
	val &= ~SPI_CH_SW_RST;
	writel(val, sspi->regs + S3C_SPI_CH_CFG);
}

static inline void enable_spichan(struct s3cspi_bus *sspi, struct spi_transfer *xfer)
{
	u32 val;

	val = readl(sspi->regs + S3C_SPI_CH_CFG);
	val &= ~(SPI_CH_RXCH_ON | SPI_CH_TXCH_ON);
	if(xfer->tx_buf != NULL){
	   val |= SPI_CH_TXCH_ON;
	}
	if(xfer->rx_buf != NULL){
	      writel((xfer->len & 0xffff) | SPI_PACKET_CNT_EN, 
			sspi->regs + S3C_SPI_PACKET_CNT); /* XXX TODO Bytes or number of SPI-Words? */
	   val |= SPI_CH_RXCH_ON;
	}
	writel(val, sspi->regs + S3C_SPI_CH_CFG);
}

static inline void enable_spiintr(struct s3cspi_bus *sspi, struct spi_transfer *xfer)
{
	u32 val = 0;

	if(xfer->tx_buf != NULL){
	   val |= SPI_INT_TX_OVERRUN_EN;
	   //val |= SPI_INT_TX_UNDERRUN_EN;
	}
	if(xfer->rx_buf != NULL){
	   val |= (SPI_INT_RX_UNDERRUN_EN | SPI_INT_RX_OVERRUN_EN | SPI_INT_TRAILING_EN);
	}
	writel(val, sspi->regs + S3C_SPI_INT_EN);
}

static inline void enable_spienqueue(struct s3cspi_bus *sspi, struct spi_transfer *xfer)
{
	if(xfer->rx_buf != NULL){
	   sspi->rx_done = BUSY;
	   s3c2410_dma_config(sspi->rx_dmach, sspi->cur_bpw/8);
	   s3c2410_dma_enqueue(sspi->rx_dmach, (void *)sspi, xfer->rx_dma, xfer->len);
	}
	if(xfer->tx_buf != NULL){
	   sspi->tx_done = BUSY;
	   s3c2410_dma_config(sspi->tx_dmach, sspi->cur_bpw/8);
	   s3c2410_dma_enqueue(sspi->tx_dmach, (void *)sspi, xfer->tx_dma, xfer->len);
	}
}

static inline void enable_cs(struct s3cspi_bus *sspi, struct spi_device *spi)
{
	u32 val;
	val = readl(sspi->regs + S3C_SPI_SLAVE_SEL);	
 
    val = readl(sspi->regs + S3C_SPI_SLAVE_SEL); 
    val &=  ~(0x3f<<4);         // clear NSSOUT inactive time;     
 
#if 0
	//val |=  SPI_SLAVE_AUTO;   	   /* Auto Mode Enable */
	//val &= ~SPI_SLAVE_SIG_INACT;   /* Activate CS */
#else
	val &= ~SPI_SLAVE_AUTO;   	 /* Manual Mode Enable */
	val &= ~SPI_SLAVE_SIG_INACT; /* Activate CS */
#endif

    if( !(sspi->cur_mode & SPI_READY) ){
          val |= (0x30&0x3f)<<4;          // set NSSOUT inactive time;      
    }

	writel(val, sspi->regs + S3C_SPI_SLAVE_SEL);
}

static inline void disable_cs(struct s3cspi_bus *sspi, struct spi_device *spi)
{
	u32 val;
   
	val = readl(sspi->regs + S3C_SPI_SLAVE_SEL);

	val &= ~SPI_SLAVE_AUTO; 	/* Manual Mode enable */
	val |= SPI_SLAVE_SIG_INACT; /* InActivate CS */
	writel(val, sspi->regs + S3C_SPI_SLAVE_SEL);
}

static inline void set_polarity(struct s3cspi_bus *sspi)
{
	u32 val;

	val = readl(sspi->regs + S3C_SPI_CH_CFG);
	val &= ~(SPI_CH_SLAVE | SPI_CPOL_L | SPI_CPHA_B);
	if(sspi->cur_mode & SPI_READY){
	   val |= SPI_CH_SLAVE;
	}
	if(sspi->cur_mode & SPI_CPHA){
	   val |= SPI_CPHA_B;
	}
	if(sspi->cur_mode & SPI_CPOL){
	   val |= SPI_CPOL_L;
	}
	writel(val, sspi->regs + S3C_SPI_CH_CFG);
}

static inline void set_clock(struct s3cspi_bus *sspi)
{
	u32 val;
	struct s3c_spi_mstr_info *smi = sspi->spi_mstinfo;

	val = readl(sspi->regs + S3C_SPI_CLK_CFG);
	val &= ~(SPI_CLKSEL_SRCMSK | SPI_ENCLK_ENABLE | 0xff);

	//if( !(sspi->cur_mode & SPI_READY))  // if Master
    {
		val |= SPI_CLKSEL_SRC;
	   	val |= ((smi->spiclck_getrate(smi) / sspi->cur_speed / 2 - 1) << 0);	// PCLK and PSR
	   	val |= SPI_ENCLK_ENABLE;
	}
	writel(val, sspi->regs + S3C_SPI_CLK_CFG);
}

static inline void set_dmachan(struct s3cspi_bus *sspi)
{
	u32 val;

	val = readl(sspi->regs + S3C_SPI_MODE_CFG);
	val &= ~((0x3<<17) | (0x3<<29));
	if(sspi->cur_bpw == 8){
	   val |= SPI_MODE_CH_TSZ_BYTE;
	   val |= SPI_MODE_BUS_TSZ_BYTE;
	}else if(sspi->cur_bpw == 16){
	   val |= SPI_MODE_CH_TSZ_HALFWORD;
	   val |= SPI_MODE_BUS_TSZ_HALFWORD;
	}else if(sspi->cur_bpw == 32){
	   val |= SPI_MODE_CH_TSZ_WORD;
	   val |= SPI_MODE_BUS_TSZ_WORD;
	}else{
	   printk("Invalid Bits/Word!\n");
	}
	val &= ~(SPI_MODE_4BURST | SPI_MODE_TXDMA_ON | SPI_MODE_RXDMA_ON);
	writel(val, sspi->regs + S3C_SPI_MODE_CFG);
}

static void s3c_spi_config(struct s3cspi_bus *sspi)
{
	/* Set Polarity and Phase */
	set_polarity(sspi);

	/* Set Channel & DMA Mode */
	set_dmachan(sspi);
}

void s3c_spi_dma_txcb(struct s3c2410_dma_chan *chan, void *buf_id, int size, enum s3c2410_dma_buffresult res);
void s3c_spi_dma_rxcb(struct s3c2410_dma_chan *chan, void *buf_id, int size, enum s3c2410_dma_buffresult res);

static int s3c_spi_dma_request(struct s3cspi_bus *sspi)
{
	int ret = -ENODEV;

	if(s3c2410_dma_request(sspi->rx_dmach, &s3c_spi_dma_client, NULL)){
		printk(KERN_ERR "cannot get RxDMA\n");
		ret = -EBUSY;
		goto err1;
	}
	s3c2410_dma_set_buffdone_fn(sspi->rx_dmach, s3c_spi_dma_rxcb);
	s3c2410_dma_devconfig(sspi->rx_dmach, S3C2410_DMASRC_HW, sspi->sfr_phyaddr + S3C_SPI_RX_DATA);
	s3c2410_dma_config(sspi->rx_dmach, sspi->cur_bpw/8);
	s3c2410_dma_setflags(sspi->rx_dmach, S3C2410_DMAF_AUTOSTART);

	if(s3c2410_dma_request(sspi->tx_dmach, &s3c_spi_dma_client, NULL)){
		printk(KERN_ERR "cannot get TxDMA\n");
		ret = -EBUSY;
		goto err2;
	}
	s3c2410_dma_set_buffdone_fn(sspi->tx_dmach, s3c_spi_dma_txcb);
	s3c2410_dma_devconfig(sspi->tx_dmach, S3C2410_DMASRC_MEM, sspi->sfr_phyaddr + S3C_SPI_TX_DATA);
	s3c2410_dma_config(sspi->tx_dmach, sspi->cur_bpw/8);
	s3c2410_dma_setflags(sspi->tx_dmach, S3C2410_DMAF_AUTOSTART);

	return 0;
	
err2:
	s3c2410_dma_free(sspi->rx_dmach, &s3c_spi_dma_client);
	
err1:

	return ret;

}

static void s3c_spi_dma_release(struct s3cspi_bus *sspi)
{
	s3c2410_dma_free(sspi->tx_dmach, &s3c_spi_dma_client);
	s3c2410_dma_free(sspi->rx_dmach, &s3c_spi_dma_client);
}

static void s3c_spi_hwinit(struct s3cspi_bus *sspi, int channel)
{
	unsigned int val;

	writel(SPI_SLAVE_SIG_INACT, sspi->regs + S3C_SPI_SLAVE_SEL);

	/* Disable Interrupts */
	writel(0, sspi->regs + S3C_SPI_INT_EN);

#ifdef CONFIG_CPU_S3C6410
	writel((readl(S3C64XX_SPC_BASE) & ~(3<<28)) | (3<<28), S3C64XX_SPC_BASE);
	writel((readl(S3C64XX_SPC_BASE) & ~(3<<18)) | (3<<18), S3C64XX_SPC_BASE);
#elif defined (CONFIG_CPU_S5P6440)
	writel((readl(S5P64XX_SPC_BASE) & ~(3<<28)) | (3<<28), S5P64XX_SPC_BASE);
	writel((readl(S5P64XX_SPC_BASE) & ~(3<<18)) | (3<<18), S5P64XX_SPC_BASE);
#elif defined (CONFIG_CPU_S5PC100)
	/* How to control drive strength, if we must? */
#endif

	writel(SPI_CLKSEL_SRC, sspi->regs + S3C_SPI_CLK_CFG);
	writel(0, sspi->regs + S3C_SPI_MODE_CFG);
	writel(SPI_SLAVE_SIG_INACT, sspi->regs + S3C_SPI_SLAVE_SEL);
	writel(0, sspi->regs + S3C_SPI_PACKET_CNT);
	writel(readl(sspi->regs + S3C_SPI_PENDING_CLR), sspi->regs + S3C_SPI_PENDING_CLR);
	writel(SPI_FBCLK_0NS, sspi->regs + S3C_SPI_FB_CLK);

	flush_spi(sspi);

	writel(0, sspi->regs + S3C_SPI_SWAP_CFG);
	writel(SPI_FBCLK_9NS, sspi->regs + S3C_SPI_FB_CLK);

	val = readl(sspi->regs + S3C_SPI_MODE_CFG);
	val &= ~(SPI_MAX_TRAILCNT << SPI_TRAILCNT_OFF);
	if(channel == 0)
	   SET_MODECFG(val, 0);
	else 
	   SET_MODECFG(val, 1);
	val |= (SPI_TRAILCNT << SPI_TRAILCNT_OFF);
	writel(val, sspi->regs + S3C_SPI_MODE_CFG);
	
	// default mode SLAVE 
	val = readl(sspi->regs + S3C_SPI_CH_CFG);
	val &= ~(SPI_CH_SLAVE | SPI_CPOL_L | SPI_CPHA_B);
	val |= SPI_CH_SLAVE;
	writel(val, sspi->regs + S3C_SPI_CH_CFG);

}

static irqreturn_t s3c_spi_interrupt(int irq, void *dev_id)
{
	u32 val;
	struct s3cspi_bus *sspi = (struct s3cspi_bus *)dev_id;

	dump_regs(sspi);
	val = readl(sspi->regs + S3C_SPI_PENDING_CLR);
	dbg_printk("%s: PENDING=%x\n", __FUNCTION__, val);
	writel(val, sspi->regs + S3C_SPI_PENDING_CLR);

	/* We get interrupted only for bad news */
	if(sspi->tx_done != PASS){
	   sspi->tx_done = FAIL;
	}
	if(sspi->rx_done != PASS){
	   sspi->rx_done = FAIL;
	}

	/* No need to spinlock, as called in IRQ_DISABLED mode */
	atomic_set(&sspi->state, atomic_read(&sspi->state) | IRQERR);

	complete(&sspi->xfer_completion);

	return IRQ_HANDLED;
}

void s3c_spi_dma_rxcb(struct s3c2410_dma_chan *chan, void *buf_id, int size, enum s3c2410_dma_buffresult res)
{
	struct s3cspi_bus *sspi = (struct s3cspi_bus *)buf_id;

	if(res == S3C2410_RES_OK){
	   sspi->rx_done = PASS;
	   //printk("DmaRx-%d ", size);
	}else{
	   sspi->rx_done = FAIL;
	   printk("DmaAbrtRx-%d\n", size);
   	}

	if(sspi->tx_done != BUSY && !(atomic_read(&sspi->state) & IRQERR)) /* If other done and all OK */
	   complete(&sspi->xfer_completion);
}

void s3c_spi_dma_txcb(struct s3c2410_dma_chan *chan, void *buf_id, int size, enum s3c2410_dma_buffresult res)
{
	struct s3cspi_bus *sspi = (struct s3cspi_bus *)buf_id;

	if(res == S3C2410_RES_OK){
	   sspi->tx_done = PASS;
	   //printk("DmaTx-%d tx done \n", size);
	}else{
	   sspi->tx_done = FAIL;
	   printk("DmaAbrtTx-%d \n", size);
	}

	if(sspi->rx_done != BUSY && !(atomic_read(&sspi->state) & IRQERR)) /* If other done and all OK */
	   complete(&sspi->xfer_completion);
}

static int wait_for_txshiftout(struct s3cspi_bus *sspi, unsigned long t)
{
	unsigned long timeout;

	timeout = jiffies + t;
	while (TX_FIFO_LVL(__raw_readl(sspi->regs + S3C_SPI_STATUS))){
	   if(time_after(jiffies, timeout))
	      return -1;
	   cpu_relax();
	}
	return 0;
}

static int wait_for_xfer(struct s3cspi_bus *sspi, struct spi_transfer *xfer)
{
	int status;
	u32 val;
	//unsigned long start_time, end_time;

	val = msecs_to_jiffies(xfer->len / (sspi->min_speed / 8 / 1000)); /* time to xfer data at min. speed */
	//val += msecs_to_jiffies(10); /* just some more */
	val += msecs_to_jiffies(10*1000); /* just some more : 10 sec*/

    //start_time = jiffies;
    status = wait_for_completion_interruptible_timeout(&sspi->xfer_completion, val);
    //end_time = jiffies;
    //printk(" wait_for_completion_interruptible_timeout(%u) :: delta t %u ( star time %u  end time %u) \n",val, end_time-start_time, start_time, end_time);

	if(status == 0)
	   status = -ETIMEDOUT;
	else if(status == -ERESTARTSYS)
	   status = -EINTR;
	else if((sspi->tx_done != PASS) || (sspi->rx_done != PASS)) /* Some Xfer failed */
	   status = -EIO;
	else
	   status = 0;	/* All OK */

	/*
	 * DmaTx returns after simply writing data in the FIFO,
	 * w/o waiting for real transmission on the bus to finish.
	 * DmaRx returns only after Dma read data from FIFO which
	 * needs bus transmission to finish, so we don't worry if 
	 * Xfer involved Rx alone or with Tx.
	 */
	if(!status && (xfer->rx_buf == NULL)){
	   val = msecs_to_jiffies(xfer->len / (sspi->min_speed / 8 / 1000)); /* Be lenient */
	   val += msecs_to_jiffies(5000); /* 5secs to switch on the Master */
	   status = wait_for_txshiftout(sspi, val);
	   if(status == -1){ /* Time-out */
	      status = -ETIMEDOUT;
	   }else{ /* Still last byte on the bus */
	      status = 0;
	      udelay(1000000 / sspi->cur_speed * 8 + 5); /* time to xfer 1 byte at sspi->cur_speed plus 5 usecs extra */
	   }
	}

	return status;
}

/*  First, allocate from our preallocated DMA buffer.
 *  If preallocated DMA buffers are not big enough,
 *  then allocate new DMA Coherent buffers.
 */
static int s3c_spi_map_xfer(struct s3cspi_bus *sspi, struct spi_transfer *xfer)
{
	struct s3c_spi_mstr_info *smi = sspi->spi_mstinfo;
	struct device *dev = &smi->pdev->dev;

	sspi->rx_tmp = NULL;
	sspi->tx_tmp = NULL;

	if(xfer->len <= S3C_SPI_DMABUF_LEN){
	   if(xfer->rx_buf != NULL){
	      xfer->rx_dma = sspi->rx_dma_phys;
	      sspi->rx_tmp = (void *)sspi->rx_dma_cpu;
	   }
	   if(xfer->tx_buf != NULL){
	      xfer->tx_dma = sspi->tx_dma_phys;
	      sspi->tx_tmp = (void *)sspi->tx_dma_cpu;
	   }
	}else{
	   dbg_printk("%s :If you plan to use this Xfer size often, increase S3C_SPI_DMABUF_LEN\n",__FUNCTION__);
	   if(xfer->rx_buf != NULL){
	      sspi->rx_tmp = dma_alloc_coherent(dev, S3C_SPI_DMABUF_LEN, 
							&xfer->rx_dma, GFP_KERNEL | GFP_DMA);
		if(sspi->rx_tmp == NULL)
		   return -ENOMEM;
	   }
	   if(xfer->tx_buf != NULL){
	      sspi->tx_tmp = dma_alloc_coherent(dev, S3C_SPI_DMABUF_LEN, &xfer->tx_dma, GFP_KERNEL | GFP_DMA);
		if(sspi->tx_tmp == NULL){
		   if(xfer->rx_buf != NULL)
		      dma_free_coherent(dev, S3C_SPI_DMABUF_LEN, sspi->rx_tmp, xfer->rx_dma);
		   return -ENOMEM;
		}
	   }
	}

	if(xfer->tx_buf != NULL)
	   memcpy(sspi->tx_tmp, xfer->tx_buf, xfer->len);

	return 0;
}

static void s3c_spi_unmap_xfer(struct s3cspi_bus *sspi, struct spi_transfer *xfer)
{
	struct s3c_spi_mstr_info *smi = sspi->spi_mstinfo;
	struct device *dev = &smi->pdev->dev;

	if((xfer->rx_buf != NULL) && (sspi->rx_tmp != NULL))
	   memcpy(xfer->rx_buf, sspi->rx_tmp, xfer->len);

	if(xfer->len > S3C_SPI_DMABUF_LEN){
	   if(xfer->rx_buf != NULL)
	      dma_free_coherent(dev, S3C_SPI_DMABUF_LEN, sspi->rx_tmp, xfer->rx_dma);
	   if(xfer->tx_buf != NULL)
	      dma_free_coherent(dev, S3C_SPI_DMABUF_LEN, sspi->tx_tmp, xfer->tx_dma);
	}else{
	   sspi->rx_tmp = NULL;
	   sspi->tx_tmp = NULL;
	}

	/* Restore to t/rx_dma pointers */
	if(xfer->rx_buf != NULL)
	   xfer->rx_dma = 0;
	if(xfer->tx_buf != NULL)
	   xfer->tx_dma = 0;
}
#if 0
static void dump_packet(struct s3cspi_bus *sspi, struct spi_transfer *xfer, int length)
{
	int i;
    unsigned char *txbuf=NULL;
    unsigned char *rxbuf=NULL; 
#if 0   // for testing
	if(xfer->tx_buf){
	 	txbuf = sspi->tx_tmp;
     	printk("\n--- SPI sspi->tx_dma Data Tranmit :%d byte ---", xfer->len);
     	for(i=0; i<xfer->len; i++){
        	 if(i%16 == 0) printk("\n");
             printk("%02x ", txbuf[i]);
     	}
	}	
    
	if(xfer->rx_buf){
	 	rxbuf = sspi->rx_tmp;
		printk("\n");
    	printk("\n--- SPI sspi->rx_dma Data Received %d bytes (actual_length %d) ---", xfer->len, length);
    	for(i=0; i<xfer->len; i++){
             if(i%16 == 0) printk("\n");
             printk("%02x ", rxbuf[i]);
    	}
    	printk("\n");
	}
#else	
 //--------
	if(xfer->tx_buf){
	 	txbuf = (unsigned char *)xfer->tx_buf;
     	printk("\n--- SPI-%d xfer->tx_buf Data Tranmit :%d byte ---", sspi->spi_mstinfo->pdev->id,xfer->len);
     	for(i=0; i<xfer->len; i++){
             if(i%16 == 0) printk("\n");
             printk("%02x ", txbuf[i]);
     	}
     	printk("\n");
	}
    mdelay(10);

	if(xfer->rx_buf){
	 	rxbuf = (unsigned char *)xfer->rx_buf;
     	printk("\n--- SPI-%d xfer->rx_buf Data Received %d bytes (actual_length %d) ---", sspi->spi_mstinfo->pdev->id,xfer->len, length);
     	for(i=0; i<xfer->len; i++){
             if(i%16 == 0) printk("\n");
             printk("%02x ", rxbuf[i]);
     	}
     	printk("\n");
	}
    mdelay(10);
 //--------
#endif
}
#endif

static int s3c_spi_request_gdma(struct s3cspi_bus *sspi, struct spi_message *msg)
{
	struct spi_device *spi = msg->spi;

	if(s3c2410_dma_request(sspi->rx_dmach, &s3c_spi_dma_client, NULL)){
		dev_err(&spi->dev, "cannot get RxDMA\n");

		return -EBUSY;
	}
	s3c2410_dma_set_buffdone_fn(sspi->rx_dmach, s3c_spi_dma_rxcb);
	s3c2410_dma_devconfig(sspi->rx_dmach, S3C2410_DMASRC_HW, sspi->sfr_phyaddr + S3C_SPI_RX_DATA);
	s3c2410_dma_config(sspi->rx_dmach, sspi->cur_bpw/8);
	s3c2410_dma_setflags(sspi->rx_dmach, S3C2410_DMAF_AUTOSTART);

	if(s3c2410_dma_request(sspi->tx_dmach, &s3c_spi_dma_client, NULL)){
		dev_err(&spi->dev, "cannot get TxDMA\n");

		s3c2410_dma_free(sspi->rx_dmach, &s3c_spi_dma_client);

		return -EBUSY;
	}
	s3c2410_dma_set_buffdone_fn(sspi->tx_dmach, s3c_spi_dma_txcb);
	s3c2410_dma_devconfig(sspi->tx_dmach, S3C2410_DMASRC_MEM, sspi->sfr_phyaddr + S3C_SPI_TX_DATA);
	s3c2410_dma_config(sspi->tx_dmach, sspi->cur_bpw/8);
	s3c2410_dma_setflags(sspi->tx_dmach, S3C2410_DMAF_AUTOSTART);

	return 0;
}

static void s3c_spi_release_gdma(struct s3cspi_bus *sspi)
{
	s3c2410_dma_free(sspi->rx_dmach, &s3c_spi_dma_client);
	s3c2410_dma_free(sspi->tx_dmach, &s3c_spi_dma_client);
}

static void handle_msg(struct s3cspi_bus *sspi, struct spi_message *msg)
{
	u8 bpw;
	u32 speed, val;
	int status = 0;
	struct spi_transfer *xfer;
	struct spi_device *spi = msg->spi;
	struct s3c_spi_mstr_info *smi = sspi->spi_mstinfo; 

	dump_spidevice_info(spi);

	/*enable clock of spi*/
	smi->spiclck_en(smi);	

	if( s3c_spi_request_gdma(sspi, msg) )
		goto out;

	/* Write to regs only if necessary */
	if((sspi->cur_speed != spi->max_speed_hz)
		||(sspi->cur_mode != spi->mode)
		||(sspi->cur_bpw != spi->bits_per_word)) {
	   sspi->cur_bpw = spi->bits_per_word;
	   sspi->cur_speed = spi->max_speed_hz;
	   sspi->cur_mode = spi->mode;
	   s3c_spi_config(sspi);
	
	   S3C_SETGPIOPULL(sspi);
}
	list_for_each_entry (xfer, &msg->transfers, transfer_list) {

		if(!msg->is_dma_mapped && s3c_spi_map_xfer(sspi, xfer)){
		   dev_err(&spi->dev, "Xfer: Unable to allocate DMA buffer!\n");
		   status = -ENOMEM;
		   goto out;
		}

		INIT_COMPLETION(sspi->xfer_completion);

		/* Only BPW and Speed may change across transfers */
		bpw = xfer->bits_per_word ? : spi->bits_per_word;
		speed = xfer->speed_hz ? : spi->max_speed_hz;

		if(sspi->cur_bpw != bpw || sspi->cur_speed != speed){
			sspi->cur_bpw = bpw;
			sspi->cur_speed = speed;
			s3c_spi_config(sspi);
	  	}

		/* Pending only which is to be done */
		sspi->rx_done = PASS;
		sspi->tx_done = PASS;
		
		/* Configure Clock */
		set_clock(sspi);

		/* Enable Interrupts */
		enable_spiintr(sspi, xfer);

	    flush_spi(sspi);

		/* Enqueue data on DMA */
		enable_spienqueue(sspi, xfer);

		/* Enable DMA */
		enable_spidma(sspi, xfer);

		/* Enable TX/RX */
		enable_spichan(sspi, xfer);
   		dump_regs(sspi);

		/* Slave Select */
		enable_cs(sspi, spi);
   		//__dump_regs_line(sspi);

		status = wait_for_xfer(sspi, xfer);

		/**************
		 * Block Here *
		 **************/

		if(status == -ETIMEDOUT){
		   dev_err(&spi->dev, "Xfer: Timeout!\n");
		   dump_regs(sspi);
		   /* DMA Disable*/
		   val = readl(sspi->regs + S3C_SPI_MODE_CFG);
		   val &= ~(SPI_MODE_TXDMA_ON | SPI_MODE_RXDMA_ON);
		   writel(val, sspi->regs + S3C_SPI_MODE_CFG);
		   flush_dma(sspi, xfer);
		   flush_spi(sspi);
	  	   s3c_spi_dma_release(sspi);
	       s3c_spi_dma_request(sspi);

		   if(!msg->is_dma_mapped)
		      s3c_spi_unmap_xfer(sspi, xfer);
		   goto out;
		}
		if(status == -EINTR){
		   dev_err(&spi->dev, "Xfer: Interrupted!\n");
		   dump_regs(sspi);
		   /* DMA Disable*/
		   val = readl(sspi->regs + S3C_SPI_MODE_CFG);
		   val &= ~(SPI_MODE_TXDMA_ON | SPI_MODE_RXDMA_ON);
		   writel(val, sspi->regs + S3C_SPI_MODE_CFG);
		   flush_dma(sspi, xfer);
		   flush_spi(sspi);
	  	   s3c_spi_dma_release(sspi);
	       s3c_spi_dma_request(sspi);
		   if(!msg->is_dma_mapped)
		      s3c_spi_unmap_xfer(sspi, xfer);
		   goto out;
		}
		if(status == -EIO){ /* Some Xfer failed */
		   dev_err(&spi->dev, "Xfer: Failed!\n");
		   dump_regs(sspi);
		   /* DMA Disable*/
		   val = readl(sspi->regs + S3C_SPI_MODE_CFG);
		   val &= ~(SPI_MODE_TXDMA_ON | SPI_MODE_RXDMA_ON);
		   writel(val, sspi->regs + S3C_SPI_MODE_CFG);
		   flush_dma(sspi, xfer);
		   flush_spi(sspi);
	  	   s3c_spi_dma_release(sspi);
	       s3c_spi_dma_request(sspi);
		   if(!msg->is_dma_mapped)
		      s3c_spi_unmap_xfer(sspi, xfer);
		   goto out;
		}

		if(xfer->delay_usecs)
		   udelay(xfer->delay_usecs);

		disable_cs(sspi, spi);

		msg->actual_length += xfer->len;

		//dump_packet(sspi,xfer, msg->actual_length);

		if(!msg->is_dma_mapped)
		   s3c_spi_unmap_xfer(sspi, xfer);

	}

out:
	disable_cs(sspi, spi);

	/* Disable Interrupts */
	writel(0, sspi->regs + S3C_SPI_INT_EN);

	/* Tx/Rx Disable */
	val = readl(sspi->regs + S3C_SPI_CH_CFG);
	val &= ~(SPI_CH_RXCH_ON | SPI_CH_TXCH_ON);
	writel(val, sspi->regs + S3C_SPI_CH_CFG);

	/* DMA Disable*/
	val = readl(sspi->regs + S3C_SPI_MODE_CFG);
	val &= ~(SPI_MODE_TXDMA_ON | SPI_MODE_RXDMA_ON);
	writel(val, sspi->regs + S3C_SPI_MODE_CFG);

	/*disable clock of spi*/
	smi->spiclck_dis(smi);

	msg->status = status;
	if(msg->complete)
	   msg->complete(msg->context);

	s3c_spi_release_gdma(sspi);
    dump_regs(sspi);
}

static void s3c_spi_work(struct work_struct *work)
{
	struct s3cspi_bus *sspi = container_of(work, struct s3cspi_bus, work);
	unsigned long flags;

	spin_lock_irqsave(&sspi->lock, flags);
	while(!list_empty(&sspi->queue) && !(atomic_read(&sspi->state) & SUSPND)){
		struct spi_message *msg;

		msg = container_of(sspi->queue.next, struct spi_message, queue);
		list_del_init(&msg->queue);
		atomic_set(&sspi->state, atomic_read(&sspi->state) & ~ERRS); /* Clear every ERROR flag */
		atomic_set(&sspi->state, atomic_read(&sspi->state) | XFERBUSY); /* Set Xfer busy flag */
		spin_unlock_irqrestore(&sspi->lock, flags);

		handle_msg(sspi, msg);

		spin_lock_irqsave(&sspi->lock, flags);
		atomic_set(&sspi->state, atomic_read(&sspi->state) & ~XFERBUSY);
	}
	spin_unlock_irqrestore(&sspi->lock, flags);
}

static void s3c_spi_cleanup(struct spi_device *spi)
{
	dbg_printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
}

static int s3c_spi_transfer(struct spi_device *spi, struct spi_message *msg)
{
	struct spi_master *master = spi->master;
	struct s3cspi_bus *sspi = spi_master_get_devdata(master);
	unsigned long flags;

	spin_lock_irqsave(&sspi->lock, flags);

	if(atomic_read(&sspi->state) & SUSPND){
	   spin_unlock_irqrestore(&sspi->lock, flags);
	   return -ESHUTDOWN;
	}

	msg->actual_length = 0;
	msg->status = -EINPROGRESS;
	list_add_tail(&msg->queue, &sspi->queue);
	queue_work(sspi->workqueue, &sspi->work);

	spin_unlock_irqrestore(&sspi->lock, flags);

	return 0;
}

/* the spi->mode bits understood by this driver: */
#define MODEBITS	(SPI_CPOL | SPI_CPHA | SPI_READY | SPI_CS_HIGH)
/*
 * Here we only check the validity of requested configuration and 
 * save the configuration in a local data-structure.
 * The controller is actually configured only just before
 * we get a message to transfer _and_ if no other message is pending(already configured).
 */
static int s3c_spi_setup(struct spi_device *spi)
{
	unsigned long flags;
	unsigned int psr;
	struct spi_message *msg;
	struct s3cspi_bus *sspi = spi_master_get_devdata(spi->master);
	struct s3c_spi_mstr_info *smi = sspi->spi_mstinfo;
	
	/*enable clock for setup*/
	smi->spiclck_en(smi);
	
	spin_lock_irqsave(&sspi->lock, flags);

	list_for_each_entry(msg, &sspi->queue, queue){
	   if(msg->spi == spi){ /* Is some mssg is already queued for this device */
	      dev_err(&spi->dev, "setup: attempt while mssg in queue!\n");
	      spin_unlock_irqrestore(&sspi->lock, flags);
	      return -EBUSY;
	   }
	}

	if(atomic_read(&sspi->state) & SUSPND){
		spin_unlock_irqrestore(&sspi->lock, flags);
		dev_err(&spi->dev, "setup: SPI-%d not active!\n", spi->master->bus_num);
		return -ESHUTDOWN;
	}

	spin_unlock_irqrestore(&sspi->lock, flags);

	if (spi->chip_select > spi->master->num_chipselect) {
		dev_err(&spi->dev, "setup: invalid chipselect %u (%u defined)\n",
				spi->chip_select, spi->master->num_chipselect);
		return -EINVAL;
	}

	spi->bits_per_word = spi->bits_per_word ? : 8;

	if((spi->bits_per_word != 8) && 
			(spi->bits_per_word != 16) && 
			(spi->bits_per_word != 32)){
		dev_err(&spi->dev, "setup: %dbits/wrd not supported!\n", spi->bits_per_word);
		return -EINVAL;
	}

	/* XXX Should we return -EINVAL or tolerate it XXX */
	if(spi->max_speed_hz < sspi->min_speed)
		spi->max_speed_hz = sspi->min_speed;
	if(spi->max_speed_hz > sspi->max_speed)
		spi->max_speed_hz = sspi->max_speed;

	/* Round-off max_speed_hz */
	psr = smi->spiclck_getrate(smi) / spi->max_speed_hz / 2 - 1;
	psr &= 0xff;
	if(spi->max_speed_hz < smi->spiclck_getrate(smi) / 2 / (psr + 1))
	   psr = (psr+1) & 0xff;

	spi->max_speed_hz = smi->spiclck_getrate(smi) / 2 / (psr + 1);

	if (spi->max_speed_hz > sspi->max_speed
			|| spi->max_speed_hz < sspi->min_speed){
		dev_err(&spi->dev, "setup: req speed(%u) out of range[%u-%u]\n", 
				spi->max_speed_hz, sspi->min_speed, sspi->max_speed);
		goto setup_err;	
	}
     
	if (spi->mode & ~MODEBITS) {
		dev_err(&spi->dev, "setup: unsupported mode bits %x\n",	spi->mode & ~MODEBITS);
		goto setup_err;
	}

	disable_cs(sspi, spi);
	
	/*disable clock*/
	smi->spiclck_dis(smi);
	return 0;

setup_err :
	smi->spiclck_dis(smi);
	return -EINVAL;
}

static int __init s3c_spi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct s3cspi_bus *sspi;
	struct s3c_spi_mstr_info *smi;
	int ret = -ENODEV;

	dbg_printk("%s:%s:%d ID=%d\n", __FILE__, __func__, __LINE__, pdev->id);
	master = spi_alloc_master(&pdev->dev, sizeof(struct s3cspi_bus)); /* Allocate contiguous SPI controller */
	if (master == NULL)
		return ret;

	sspi = spi_master_get_devdata(master);
	sspi->master = master;
	platform_set_drvdata(pdev, master);

	sspi->spi_mstinfo = (struct s3c_spi_mstr_info *)pdev->dev.platform_data;
	smi = sspi->spi_mstinfo;
	smi->pdev = pdev;

	INIT_WORK(&sspi->work, s3c_spi_work);
	spin_lock_init(&sspi->lock);
	INIT_LIST_HEAD(&sspi->queue);
	init_completion(&sspi->xfer_completion);

	ret = smi->spiclck_get(smi);
	if(ret){
		dev_err(&pdev->dev, "cannot acquire clock \n");
		ret = -EBUSY;
		goto lb1;
	}
	ret = smi->spiclck_en(smi);
	if(ret){
		dev_err(&pdev->dev, "cannot enable clock \n");
		ret = -EBUSY;
		goto lb2;
	}

	sspi->max_speed = smi->spiclck_getrate(smi) / 2 / (0x0 + 1);
	sspi->min_speed = smi->spiclck_getrate(smi) / 2 / (0xff + 1);

	sspi->cur_bpw = 8;

	/* Get and Map Resources */
	sspi->iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (sspi->iores == NULL) {
		dev_err(&pdev->dev, "cannot find IO resource\n");
		ret = -ENOENT;
		goto lb3;
	}

	sspi->ioarea = request_mem_region(sspi->iores->start, sspi->iores->end - sspi->iores->start + 1, pdev->name);
	if (sspi->ioarea == NULL) {
		dev_err(&pdev->dev, "cannot request IO\n");
		ret = -ENXIO;
		goto lb4;
	}

	sspi->regs = ioremap(sspi->iores->start, sspi->iores->end - sspi->iores->start + 1);
	if (sspi->regs == NULL) {
		dev_err(&pdev->dev, "cannot map IO\n");
		ret = -ENXIO;
		goto lb5;
	}

	sspi->tx_dma_cpu = dma_alloc_coherent(&pdev->dev, S3C_SPI_DMABUF_LEN, &sspi->tx_dma_phys, GFP_KERNEL | GFP_DMA);
	if(sspi->tx_dma_cpu == NULL){
		dev_err(&pdev->dev, "Unable to allocate TX DMA buffers\n");
		ret = -ENOMEM;
		goto lb6;
	}

	sspi->rx_dma_cpu = dma_alloc_coherent(&pdev->dev, S3C_SPI_DMABUF_LEN, &sspi->rx_dma_phys, GFP_KERNEL | GFP_DMA);
	if(sspi->rx_dma_cpu == NULL){
		dev_err(&pdev->dev, "Unable to allocate RX DMA buffers\n");
		ret = -ENOMEM;
		goto lb7;
	}

	sspi->irqres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if(sspi->irqres == NULL){
		dev_err(&pdev->dev, "cannot find IRQ\n");
		ret = -ENOENT;
		goto lb8;
	}

	ret = request_irq(sspi->irqres->start, s3c_spi_interrupt, IRQF_DISABLED,
			pdev->name, sspi);
	if(ret){
		dev_err(&pdev->dev, "cannot acquire IRQ\n");
		ret = -EBUSY;
		goto lb9;
	}
	sspi->workqueue = create_singlethread_workqueue("spi_s3c"/*master->dev.parent->bus_id*/);
	if(!sspi->workqueue){
		dev_err(&pdev->dev, "cannot create workqueue\n");
		ret = -EBUSY;
		goto lb10;
	}


	/* Configure GPIOs */
	if(pdev->id == 0){
		SETUP_SPI(sspi, 0);
        //s3c_gpio_cfgpin(S5PV210_GPB(4), S3C_GPIO_INPUT);
        //s3c_gpio_cfgpin(S5PV210_GPB(5), S3C_GPIO_INPUT);
        //s3c_gpio_cfgpin(S5PV210_GPB(6), S3C_GPIO_INPUT);
        //s3c_gpio_cfgpin(S5PV210_GPB(7), S3C_GPIO_INPUT);

        //s3c_gpio_setpull(S5PV210_GPB(4), S3C_GPIO_PULL_UP);
        //s3c_gpio_setpull(S5PV210_GPB(5), S3C_GPIO_PULL_UP);
        //s3c_gpio_setpull(S5PV210_GPB(6), S3C_GPIO_PULL_UP);
        //s3c_gpio_setpull(S5PV210_GPB(7), S3C_GPIO_PULL_UP);
    }
	else if(pdev->id == 1)
		SETUP_SPI(sspi, 1);
	else if(pdev->id == 2)
		SETUP_SPI_CNTRL2(sspi, 2);
	
	S3C_SETGPIOPULL(sspi);

	/* Aquire DMA Channels move to s3c_spi_request_gdma() */


	/* Setup Deufult Mode */
	s3c_spi_hwinit(sspi, pdev->id);

	master->bus_num = pdev->id;
	master->setup = s3c_spi_setup;
	master->transfer = s3c_spi_transfer;
	master->cleanup = s3c_spi_cleanup;
	master->num_chipselect = sspi->spi_mstinfo->num_slaves;
	master->mode_bits = MODEBITS; 

	if(spi_register_master(master)){
		dev_err(&pdev->dev, "cannot register SPI master\n");
		ret = -EBUSY;
		goto lb11;
	}
	/*enable clock only when transmitting data*/
	smi->spiclck_dis(smi);

	printk("Samsung SoC SPI Driver loaded for Bus SPI-%d with %d Slaves attached\n", pdev->id, master->num_chipselect);
	printk("\tMax,Min-Speed [%d, %d]Hz\n", sspi->max_speed, sspi->min_speed);
	printk("\tIrq=%d\tIOmem=[0x%x-0x%x]\tDMA=[Rx-%d, Tx-%d]\n",
			sspi->irqres->start,
			sspi->iores->end, sspi->iores->start,
			sspi->rx_dmach, sspi->tx_dmach);

	return 0;

lb11:
	destroy_workqueue(sspi->workqueue);
lb10:
	free_irq(sspi->irqres->start, sspi);
lb9:
lb8:
	dma_free_coherent(&pdev->dev, S3C_SPI_DMABUF_LEN, sspi->rx_dma_cpu, sspi->rx_dma_phys);
lb7:
	dma_free_coherent(&pdev->dev, S3C_SPI_DMABUF_LEN, sspi->tx_dma_cpu, sspi->tx_dma_phys);
lb6:
	iounmap((void *) sspi->regs);
lb5:
	release_mem_region(sspi->iores->start, sspi->iores->end - sspi->iores->start + 1);
lb4:
lb3:
	smi->spiclck_dis(smi);
lb2:
	smi->spiclck_put(smi);
lb1:
	platform_set_drvdata(pdev, NULL);
	spi_master_put(master);

	return ret;
}

static int __exit s3c_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = spi_master_get(platform_get_drvdata(pdev));
	struct s3cspi_bus *sspi = spi_master_get_devdata(master);
	struct s3c_spi_mstr_info *smi = sspi->spi_mstinfo;

	/* move to s3c_spi_release_gdma() for s3c2410_dma_free() */
	
	spi_unregister_master(master);
	destroy_workqueue(sspi->workqueue);
	free_irq(sspi->irqres->start, sspi);
	dma_free_coherent(&pdev->dev, S3C_SPI_DMABUF_LEN, sspi->rx_dma_cpu, sspi->rx_dma_phys);
	dma_free_coherent(&pdev->dev, S3C_SPI_DMABUF_LEN, sspi->tx_dma_cpu, sspi->tx_dma_phys);
	iounmap((void *) sspi->regs);
	release_mem_region(sspi->iores->start, sspi->iores->end - sspi->iores->start + 1);
	smi->spiclck_dis(smi);
	smi->spiclck_put(smi);
	platform_set_drvdata(pdev, NULL);
	spi_master_put(master);

	return 0;
}

#if defined(CONFIG_PM)
static int s3c_spi_suspend(struct platform_device *pdev, pm_message_t state)
{
	//int i;
	unsigned long flags;
	//struct s3c_spi_pdata *spd;  have to delete
	struct spi_master *master = spi_master_get(platform_get_drvdata(pdev));
	struct s3cspi_bus *sspi = spi_master_get_devdata(master);
	struct s3c_spi_mstr_info *smi = sspi->spi_mstinfo;

	/* Release DMA Channels move to s3c_spi_release_gdma() */

	spin_lock_irqsave(&sspi->lock, flags);
	atomic_set(&sspi->state, atomic_read(&sspi->state) | SUSPND);
	spin_unlock_irqrestore(&sspi->lock, flags);

	while(atomic_read(&sspi->state) & XFERBUSY)
	   msleep(10);

	/* Disable the clock */
	smi->spiclck_dis(smi);
	sspi->cur_speed = 0; /* Output Clock is stopped */

	/* Set GPIOs in least power consuming state */
	S3C_UNSETGPIOPULL(sspi);

	return 0;
}

static int s3c_spi_resume(struct platform_device *pdev)
{
	int val;
	unsigned long flags;
	//struct s3c_spi_pdata *spd;  have to delete
	struct spi_master *master = spi_master_get(platform_get_drvdata(pdev));
	struct s3cspi_bus *sspi = spi_master_get_devdata(master);
	struct s3c_spi_mstr_info *smi = sspi->spi_mstinfo;

	S3C_SETGPIOPULL(sspi);

	/* Enable the clock */
	smi->spiclck_en(smi);

	s3c_spi_hwinit(sspi, pdev->id);

	spin_lock_irqsave(&sspi->lock, flags);
	atomic_set(&sspi->state, atomic_read(&sspi->state) & ~SUSPND);
	spin_unlock_irqrestore(&sspi->lock, flags);

	/* Aquire DMA Channels */
	/* move to s3c_spi_request_gdma() */

	return 0;
}
#else
#define s3c_spi_suspend	NULL
#define s3c_spi_resume	NULL
#endif /* CONFIG_PM */

static struct platform_driver s3c_spi_driver = {
	.driver = {
		.name	= "s3c-spi",
		.owner = THIS_MODULE,
		.bus    = &platform_bus_type,
	},
	.suspend = s3c_spi_suspend,
	.resume = s3c_spi_resume,
};

static int __init s3c_spi_init(void)
{
	dbg_printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
	return platform_driver_probe(&s3c_spi_driver, s3c_spi_probe);
}
module_init(s3c_spi_init);

static void __exit s3c_spi_exit(void)
{
	platform_driver_unregister(&s3c_spi_driver);
}
module_exit(s3c_spi_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jaswinder Singh Brar <jassi.brar@samsung.com>");
MODULE_DESCRIPTION("Samsung SOC SPI Controller");
MODULE_ALIAS("platform:s3c-spi");
