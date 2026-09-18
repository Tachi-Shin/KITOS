#ifndef DOS_CONFIG_H
#define DOS_CONFIG_H

/* データ領域としてマウントするFAT32パーティション */
#define QEMU_DATA_PARTITION_LBA 62502912ULL
#define RPI4_DATA_PARTITION_LBA 67110912ULL

/* タイマー割込み周期：10ミリ秒 */
#define TIMER_INTERVAL_MS 10U

/* シェルを含めたタスク数 */
#define MAX_TASKS       8U
#define TASK_STACK_SIZE (32U * 1024U)

#define TASK_UNUSED     0
#define TASK_READY      1
#define TASK_RUNNING    2
#define TASK_BLOCKED    3
#define TASK_EXITED     4
#define TASK_STOPPED    5

#endif