// arch/arm64/armv8def.h
#ifndef _ARMV8DEF_H_
#define _ARMV8DEF_H_

#define DAIF_F  0x1U
#define DAIF_I  0x2U
#define DAIF_A  0x4U
#define DAIF_D  0x8U

#define PSR_MODE_EL0t	0x00000000
#define PSR_MODE_EL1t	0x00000004
#define PSR_MODE_EL1h	0x00000005
#define PSR_MODE_EL2t	0x00000008
#define PSR_MODE_EL2h	0x00000009
#define PSR_MODE_EL3t	0x0000000c
#define PSR_MODE_EL3h	0x0000000d
#define PSR_MODE_MASK	0x0000000f

#define PSR_MODE32_BIT     (1<<4)
#define PSR_MODE64_BIT     (0<<4)
#define PSR_F_BIT	(DAIF_F << 6)
#define PSR_I_BIT	(DAIF_I << 6)
#define PSR_A_BIT	(DAIF_A << 6)
#define PSR_D_BIT	(DAIF_D << 6)
#define PSR_SSBS_BIT	0x00001000
#define PSR_PAN_BIT	0x00400000
#define PSR_UAO_BIT	0x00800000
#define PSR_V_BIT	0x10000000
#define PSR_C_BIT	0x20000000
#define PSR_Z_BIT	0x40000000
#define PSR_N_BIT	0x80000000

#define HCR_EL2_TGE  (1 << 27)
#define HCR_EL2_RW   (1 << 31)
#define HCR_EL2_E2H  (1 << 34)

/* SCR_EL3 レジスタのビット定義 */
#define SCR_EL3_NS   (1 << 0)   /* Non-secure bit */
#define SCR_EL3_IRQ  (1 << 1)   /* IRQ routing control */
#define SCR_EL3_FIQ  (1 << 2)   /* FIQ routing control */
#define SCR_EL3_EA   (1 << 3)   /* External abort routing control */
#define SCR_EL3_RW   (1 << 10)  /* Execution state control for lower EL */

#endif  /* _ARMV8DEF_H_ */