// This file is used to store parameters for DDR, and/or flash
#include <util.h>
#define DONT_DECLAIRE__SOC_SYMBOLS
#include <dram/memcntlr.h>
#include <cg/cg.h>
#include <uart/uart.h>
#include <plr_sections.h>
#include <symb_define.h>
#include <spi_nand/spi_nand_struct.h>

extern init_table_entry_t start_of_init_func_table, end_of_init_func_table;
extern symbol_table_entry_t start_of_symble_table, end_of_symble_table;
extern unsigned int lma_offset_before_mapped_area;
extern void plr_tlb_miss_isr(void);

#ifndef SECTION_SOC
    #define SECTION_SOC         SECTION_SOC_STRU
#endif 

spi_nand_cmd_info_t   plr_cmd_info SECTION_SDATA;
spi_nand_model_info_t plr_model_info SECTION_SDATA;
spi_nand_flash_info_t   plr_spi_nand_flash_info SECTION_SDATA ={
    ._cmd_info   = &plr_cmd_info,
    ._model_info = &plr_model_info,
};

soc_t _soc SECTION_SOC = {
    .bios={
        .header= {
            .signature=SIGNATURE_PLR_FL,
            .export_symb_list=&start_of_symble_table,
            .end_of_export_symb_list=&end_of_symble_table,
            .init_func_list=&start_of_init_func_table,
            .end_of_init_func_list=&end_of_init_func_table,
        },
        .isr=plr_tlb_miss_isr,
        .size_of_plr_load_firstly=(u32_t)&lma_offset_before_mapped_area,
        .uart_putc=VZERO,
        .uart_getc=VZERO,
        .uart_tstc=VZERO,
        .dcache_writeback_invalidate_all=VZERO,
        .icache_invalidate_all=VZERO,
    },
    .flash_info.spi_nand_info=&plr_spi_nand_flash_info,
};

symb_idefine(boot_storage_type, SF_BOOT_STORAGE_TYPE, BOOT_FROM_SPI_NAND);

/* DDR3 */
const mc_register_set_t ddr3_regset SECTION_PARAMETERS = {
    .dcr.v      = DCRdv,
    .iocr.v     = IOCRdv,
    .tpr0.v     = TPR0dv,
    .tpr1.v     = TPR1dv,
    .tpr2.v     = TPR2dv,
    .tpr3.v     = TPR3dv,
    /* modify MR default value */
    .mr0.v      = DDR3_MR0dv,
    .mr1.v      = DDR3_MR1dv,
    .mr2.v      = DDR3_MR2dv,
    .mr3.v      = DDR3_MR3dv,
};

const mc_cntlr_opt_t ddr3_cntlr_opt SECTION_PARAMETERS = {
    //#include "dram_models/nanya_NT5CC256M16EP-EK.h"
    #include "dram_models/jedec_ddr3_1600.h"
};

const mc_dpi_opt_t ddr3_dpi_opt SECTION_PARAMETERS = {
    .rzq_ext        = 0,    // 1: external, 0: internal R480 enable
    .data_pre       = 1,
    .cmd_grp2_en    = 0,
    .ocd_odt        = { ZP_X_OCD_ODT(37, 60, ZP_D3),
                        ZP_X_OCD_ODT(37, 75, ZP_D3_PD3),
                        0, 0 },
#if 1	// x32 board
    #include "dram_models/board_QAB_2xDDR3_12XSFP.h"
#else
    #include "dram_models/board_QAB_48G_6X10G.h"
#endif
};

/* DDR4 */
const mc_register_set_t ddr4_regset SECTION_PARAMETERS = {
    .dcr.v      = DCRdv,
    .iocr.v     = IOCRdv,
    .tpr0.v     = TPR0dv,
    .tpr1.v     = TPR1dv,
    .tpr2.v     = TPR2dv,
    .tpr3.v     = TPR3dv,
    /* modify MR default value */
    .mr0.v      = DDR4_MR0dv,     // default, DLL on and fast exit, turn off test mode
    .mr1.v      = DDR4_MR1dv,     // dll enable
    .mr2.v      = DDR4_MR2dv,
    .mr3.v      = DDR4_MR3dv,
    .mr4.v      = DDR4_MR4dv,
    .mr5.v      = DDR4_MR5dv,
    .mr6.v      = DDR4_MR6dv,
};

const mc_cntlr_opt_t ddr4_cntlr_opt SECTION_PARAMETERS = {
    #include "dram_models/jedec_ddr4_1600.h"
};

const mc_dpi_opt_t ddr4_dpi_opt SECTION_PARAMETERS = {
    .rzq_ext        = 0,    // 1:external, 0: internal R480 enable
    .data_pre       = 1,
    .cmd_grp2_en    = 0,
    .ocd_odt        = { ZP_X_OCD_ODT(40, 60, ZP_D4_P5_CMD),
                        ZP_X_OCD_ODT(40, 60, ZP_D4_P8_DQ),
                        ZP_X_OCD_ODT(40, 73, ZP_D4_P8_PD3),
                        0 },
    #include "dram_models/board_QAB_DDR4.h"
};

const mc_info_t mi[] SECTION_PARAMETERS = {
	{(mc_cntlr_opt_t *)&ddr3_cntlr_opt,
	 (mc_dpi_opt_t *)&ddr3_dpi_opt,
	 (mc_register_set_t *)&ddr3_regset},
	{(mc_cntlr_opt_t *)&ddr4_cntlr_opt,
	 (mc_dpi_opt_t *)&ddr4_dpi_opt,
	 (mc_register_set_t *)&ddr4_regset},
};

MEMCNTLR_SECTION
const mc_info_t *
proj_mc_info_select(void) {
	u8_t dt = RFLD_PSR0(pin_dramtype);
	if(1==dt) {
		return &mi[1];
	} else {
		return &mi[0];
	}
}

const u32_t uart_baud_rate SECTION_PARAMETERS=0;
const cg_info_t cg_info_proj SECTION_PARAMETERS = {
    .dev_freq ={
        .cpu_mhz  = 1000,
	.mem_mhz  = 800,
        .lx_mhz   = 200,
        .spif_mhz = 25,
    },
};

//symb_fdefine(SF_SYS_UDELAY,    otto_lx_timer_udelay);
//symb_fdefine(SF_SYS_GET_TIMER, otto_lx_timer_get_timer);
symb_pdefine(cg_info_dev_freq, SF_SYS_CG_DEV_FREQ, &(cg_info_query.dev_freq));
