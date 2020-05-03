/*
 * (C) Copyright 2006 OpenMoko, Inc.
 * Author: Harald Welte <laforge@openmoko.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>

#include <nand.h>
#include <asm/arch/s3c24x0_cpu.h>
#include <asm/io.h>

#define S3C2410_NFCONF_EN          (1<<15)
#define S3C2410_NFCONF_512BYTE     (1<<14)
#define S3C2410_NFCONF_4STEP       (1<<13)
#define S3C2410_NFCONF_INITECC     (1<<12)
#define S3C2410_NFCONF_nFCE        (1<<11)
#define S3C2410_NFCONF_TACLS(x)    ((x)<<8)
#define S3C2410_NFCONF_TWRPH0(x)   ((x)<<4)
#define S3C2410_NFCONF_TWRPH1(x)   ((x)<<0)

#define S3C2410_ADDR_NALE 4
#define S3C2410_ADDR_NCLE 8

#ifdef CONFIG_NAND_SPL

/* in the early stage of NAND flash booting, printf() is not available */
#define printf(fmt, args...)

static void nand_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;

	for (i = 0; i < len; i++)
		buf[i] = readb(this->IO_ADDR_R);
}
#endif
/*static void s3c2440_hwcontrol(struct mtd_info *mtd,int cmd,unsigned int ctrl)
{
    struct nand_chip *chip = mtd->priv;        
    struct s3c2440_nand *nand = s3c2440_get_base_nand();   //获取nand寄存器地址
     
    if (ctrl & NAND_CLE)                 // 传输的是命令
        nand->nfcmd=cmd;  
    else                                // 传输的是地址
        nand->nfaddr=cmd;  
}*/

/*ctrl:表示做什么，选中芯片/取消选中，发命令还是发地址
* cmd :命令值或者地址值
*/
static void s3c2440_hwcontrol(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;        
    struct s3c2440_nand *nand = s3c2440_get_base_nand();   //获取nand寄存器地址
     
    if (ctrl & NAND_CLE)  
		 // 传输的是命令	
       writeb(dat,&nand->nfcmd);   
    else if (ctrl & NAND_ALE)     
		 // 传输的是地址
       writeb(dat,&nand->nfaddr);  
}

static int s3c2440_dev_ready(struct mtd_info *mtd)
{
	struct s3c2440_nand *nand = s3c2440_get_base_nand();
	debug("dev_ready\n");
	return readl(&nand->nfstat) & 0x01;
}


/*nand flash  :CE */

static void s3c2440_nand_select(struct mtd_info *mtd, int chipnr)
{
	struct s3c2440_nand *nand = s3c2440_get_base_nand();

	switch (chipnr) {
	case -1:
		/*取消选中*/
		nand->nfcont |=(1<<1);
		break;
	case 0:
		/*选中*/
		nand->nfcont &=~(1<<1);
		break;

	default:
		BUG();
	}
}


int board_nand_init(struct nand_chip *nand)
{
	u_int32_t cfg;
	u_int8_t tacls, twrph0, twrph1;
	struct s3c24x0_clock_power *clk_power = s3c24x0_get_base_clock_power();
	struct s3c2440_nand *nand_reg = s3c2440_get_base_nand();

	debug("board_nand_init()\n");
	
	writel(readl(&clk_power->clkcon) | (1 << 4), &clk_power->clkcon);

	/* initialize hardware */
#if defined(CONFIG_S3C24XX_CUSTOM_NAND_TIMING)
	tacls  = CONFIG_S3C24XX_TACLS;
	twrph0 = CONFIG_S3C24XX_TWRPH0;
	twrph1 =  CONFIG_S3C24XX_TWRPH1;
#else
	tacls = 4;
	twrph0 = 8;
	twrph1 = 8;
#endif
#if 0
	/*适用2410*/
	cfg = S3C2410_NFCONF_EN;
	cfg |= S3C2410_NFCONF_TACLS(tacls - 1);
	cfg |= S3C2410_NFCONF_TWRPH0(twrph0 - 1);
	cfg |= S3C2410_NFCONF_TWRPH1(twrph1 - 1);
#endif
	/*2440的NAND时序设置*/
	cfg = ((tacls-1)<<12)|((twrph0-1)<<8)|((twrph1-1)<<4);
	writel(cfg, &nand_reg->nfconf);
	
	/* 使能NAND Flash控制器, 初始化ECC, 禁止片选 */
	writel((1<<4)|(1<<1)|(1<<0), &nand_reg->nfcont);
	
	/* initialize nand_chip data structure */
	nand->IO_ADDR_R= (void *)&nand_reg->nfdata;
	nand->IO_ADDR_W = (void *)&nand_reg->nfdata;

	nand->select_chip = s3c2440_nand_select;             //设置CE ;


	/* read_buf and write_buf are default */
	/* read_byte and write_byte are default */
#ifdef CONFIG_NAND_SPL
	nand->read_buf = nand_read_buf;
#endif

	/* hwcontrol always must be implemented */
	nand->cmd_ctrl = s3c2440_hwcontrol;

	nand->dev_ready = s3c2440_dev_ready;

#ifdef CONFIG_S3C2410_NAND_HWECC
	nand->ecc.hwctl = s3c2410_nand_enable_hwecc;
	nand->ecc.calculate = s3c2410_nand_calculate_ecc;
	nand->ecc.correct = s3c2410_nand_correct_data;
	nand->ecc.mode = NAND_ECC_HW;
	nand->ecc.size = CONFIG_SYS_NAND_ECCSIZE;
	nand->ecc.bytes = CONFIG_SYS_NAND_ECCBYTES;
#else
	nand->ecc.mode = NAND_ECC_SOFT;
#endif

	nand->ecc.mode = NAND_ECC_SOFT; 			  //使用软件ECC 

#ifdef CONFIG_S3C2410_NAND_BBT
	nand->options = NAND_USE_FLASH_BBT;
#else
	nand->options = 0;
#endif

	debug("end of nand_init\n");

	return 0;
}
