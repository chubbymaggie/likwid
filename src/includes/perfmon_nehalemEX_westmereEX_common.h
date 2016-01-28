#ifndef PERFMON_NEX_WEX_COMMON
#define PERFMON_NEX_WEX_COMMON

#include <registers.h>

enum nex_wex_mbox_reg_ids {
    ADDR_MATCH = 0,
    ADDR_MASK,
    ZDP,
    DSP,
    ISS,
    PGT,
    MAP,
    PLD,
    THR,
    NUM_MBOX_IDS
};

static uint64_t nex_wex_mbox_regs[2][NUM_MBOX_IDS] = {
    [0] = {
        [ADDR_MATCH] = MSR_M0_PMON_ADDR_MATCH,
        [ADDR_MASK] = MSR_M0_PMON_ADDR_MASK,
        [ZDP] = MSR_M0_PMON_ZDP,
        [DSP] = MSR_M0_PMON_DSP,
        [ISS] = MSR_M0_PMON_ISS,
        [PGT] = MSR_M0_PMON_PGT,
        [MAP] = MSR_M0_PMON_MAP,
        [PLD] = MSR_M0_PMON_PLD,
        [THR] = MSR_M0_PMON_MSC_THR,
    },
    [1] = {
        [ADDR_MATCH] = MSR_M1_PMON_ADDR_MATCH,
        [ADDR_MASK] = MSR_M1_PMON_ADDR_MASK,
        [ZDP] = MSR_M1_PMON_ZDP,
        [DSP] = MSR_M1_PMON_DSP,
        [ISS] = MSR_M1_PMON_ISS,
        [PGT] = MSR_M1_PMON_PGT,
        [MAP] = MSR_M1_PMON_MAP,
        [PLD] = MSR_M1_PMON_PLD,
        [THR] = MSR_M1_PMON_MSC_THR,
    },
};

enum nex_wex_rbox_reg_type {
    IPERF0 = 0,
    IPERF1,
    QLX,
    NUM_RBOX_REG_TYPES
};

static uint64_t nex_wex_rbox_regs[2][NUM_RBOX_REG_TYPES][4] = {
    [0] = {
        [IPERF0] = {
            [0] = MSR_R0_PMON_IPERF0_P0,
            [1] = MSR_R0_PMON_IPERF0_P1,
            [2] = MSR_R0_PMON_IPERF0_P2,
            [3] = MSR_R0_PMON_IPERF0_P3,
        },
        [IPERF1] = {
            [0] = MSR_R0_PMON_IPERF1_P0,
            [1] = MSR_R0_PMON_IPERF1_P1,
            [2] = MSR_R0_PMON_IPERF1_P2,
            [3] = MSR_R0_PMON_IPERF1_P3,
        },
        [QLX] = {
            [0] = MSR_R0_PMON_QLX_P0,
            [1] = MSR_R0_PMON_QLX_P1,
            [2] = MSR_R0_PMON_QLX_P2,
            [3] = MSR_R0_PMON_QLX_P3,
        },
    },
    [1] = {
        [IPERF0] = {
            [0] = MSR_R1_PMON_IPERF0_P0,
            [1] = MSR_R1_PMON_IPERF0_P1,
            [2] = MSR_R1_PMON_IPERF0_P2,
            [3] = MSR_R1_PMON_IPERF0_P3,
        },
        [IPERF1] = {
            [0] = MSR_R1_PMON_IPERF1_P0,
            [1] = MSR_R1_PMON_IPERF1_P1,
            [2] = MSR_R1_PMON_IPERF1_P2,
            [3] = MSR_R1_PMON_IPERF1_P3,
        },
        [QLX] = {
            [0] = MSR_R1_PMON_QLX_P0,
            [1] = MSR_R1_PMON_QLX_P1,
            [2] = MSR_R1_PMON_QLX_P2,
            [3] = MSR_R1_PMON_QLX_P3,
        },
    },
};

#endif
