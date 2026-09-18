// include/arch/arm64/rpi4/gicv2.h
#ifndef _GICV2DEF_H_
#define _GICV2DEF_H_

#define	GICD_BASE 	    0xFF841000UL
#define	GICC_BASE		0xFF842000UL

#define NR_GIC_LOCAL_IRQS  NR_LOCAL_IRQS
#define NR_GIC_SGI         16

#define GICD_CTLR       (0x000)
#define GICD_TYPER      (0x004)
#define GICD_IIDR       (0x008)
#define GICD_IGROUPR    (0x080)
#define GICD_IGROUPRN   (0x0FC)
#define GICD_ISENABLER  (0x100)
#define GICD_ISENABLERN (0x17C)
#define GICD_ICENABLER  (0x180)
#define GICD_ICENABLERN (0x1fC)
#define GICD_ISPENDR    (0x200)
#define GICD_ISPENDRN   (0x27C)
#define GICD_ICPENDR    (0x280)
#define GICD_ICPENDRN   (0x2FC)
#define GICD_ISACTIVER  (0x300)
#define GICD_ISACTIVERN (0x37C)
#define GICD_ICACTIVER  (0x380)
#define GICD_ICACTIVERN (0x3FC)
#define GICD_IPRIORITYR (0x400)
#define GICD_IPRIORITYRN (0x7F8)
#define GICD_ITARGETSR  (0x800)
#define GICD_ITARGETSR7 (0x81C)
#define GICD_ITARGETSR8 (0x820)
#define GICD_ITARGETSRN (0xBF8)
#define GICD_ICFGR      (0xC00)
#define GICD_ICFGR1     (0xC04)
#define GICD_ICFGR2     (0xC08)
#define GICD_ICFGRN     (0xCFC)
#define GICD_NSACR      (0xE00)
#define GICD_NSACRN     (0xEFC)
#define GICD_SGIR       (0xF00)
#define GICD_CPENDSGIR  (0xF10)
#define GICD_CPENDSGIRN (0xF1C)
#define GICD_SPENDSGIR  (0xF20)
#define GICD_SPENDSGIRN (0xF2C)
#define GICD_ICPIDR2    (0xFE8)

#define GICD_SGI_TARGET_LIST_SHIFT   (24)
#define GICD_SGI_TARGET_LIST_MASK    (0x3UL << GICD_SGI_TARGET_LIST_SHIFT)
#define GICD_SGI_TARGET_LIST         (0UL<<GICD_SGI_TARGET_LIST_SHIFT)
#define GICD_SGI_TARGET_LIST_VAL     (0)
#define GICD_SGI_TARGET_OTHERS       (1UL<<GICD_SGI_TARGET_LIST_SHIFT)
#define GICD_SGI_TARGET_OTHERS_VAL   (1)
#define GICD_SGI_TARGET_SELF         (2UL<<GICD_SGI_TARGET_LIST_SHIFT)
#define GICD_SGI_TARGET_SELF_VAL     (2)
#define GICD_SGI_TARGET_SHIFT        (16)
#define GICD_SGI_TARGET_MASK         (0xFFUL<<GICD_SGI_TARGET_SHIFT)
#define GICD_SGI_GROUP1              (1UL<<15)
#define GICD_SGI_INTID_MASK          (0xFUL)

/* GICD_CFGR bits */
#define GICD_CFGR_LEVEL_SENSITIVE   0U
#define GICD_CFGR_EDGE_TRIGGERED    2U

#define GICC_CTLR       (0x0000)
#define GICC_PMR        (0x0004)
#define GICC_BPR        (0x0008)
#define GICC_IAR        (0x000C)
#define GICC_EOIR       (0x0010)
#define GICC_RPR        (0x0014)
#define GICC_HPPIR      (0x0018)
#define GICC_APR        (0x00D0)
#define GICC_NSAPR      (0x00E0)
#define GICC_IIDR       (0x00FC)
#define GICC_DIR        (0x1000)

/* Register bits */
#define GICD_CTL_ENABLE 0x1
#define GICD_CTL_ENABLE_GRP0 0x1
#define GICD_CTL_ENABLE_GRP1 0x2

#define GICD_TYPE_LINES 0x01f
#define GICD_TYPE_CPUS_SHIFT 5
#define GICD_TYPE_CPUS  0x0e0
#define GICD_TYPE_SEC   0x400
#define GICD_TYPER_DVIS (1U << 18)

#define GICC_CTL_ENABLE 0x1
#define GICC_CTL_EOI    (0x1 << 9)

#define GICC_IA_IRQ       0x03ff
#define GICC_IA_CPU_MASK  0x1c00
#define GICC_IA_CPU_SHIFT 10

#define GIC_PRI_LOWEST     0xf0U
#define GIC_PRI_IRQ        0xa0U
#define GIC_PRI_IPI        0x90U /* IPIs must preempt normal interrupts */
#define GIC_PRI_HIGHEST    0x80U /* Higher priorities belong to Secure-World */
#define GIC_PRI_IRQ_ALL    ((GIC_PRI_IRQ << 24) | (GIC_PRI_IRQ << 16) |\
                            (GIC_PRI_IRQ << 8) | GIC_PRI_IRQ)
#define GIC_PRI_IPI_ALL    ((GIC_PRI_IPI << 24) | (GIC_PRI_IPI << 16) |\
                            (GIC_PRI_IPI << 8) | GIC_PRI_IPI)


#define CNTP_CTL_EL0_ENABLE   (1U<<0)

#endif /* _GICV2DEF_H_ */