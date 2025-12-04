
#include "include/vmc_groups.h"
#include <string.h>
// Structure to hold a single Memory Card Group and its associated Title IDs
typedef struct
{
    const char *groupId;   // The group ID string (e.g., "XEBP_100.01")
    const char **titleIds; // Pointer to an array of title ID strings
    size_t titleCount;     // Number of title IDs in the 'titleIds' array
} MemoryCardGroup;

// --- Embedded Dictionary Data ---

static const char *titles_XEBP_100_01[] = {
    "SCES_532.86",
    "SCUS_974.29",
    "SLES_532.86",
    "SCED_529.52",
    "SCES_524.60",
    "SCKA_200.40",
    "SCUS_973.30",
    "SLES_524.60",
    "SCUS_975.16",
    "SCPS_150.21",
    "SCPS_150.57",
    "SCES_516.08",
    "SLES_516.08",
    "SCKA_200.10",
    "SCUS_972.65",
    "PAPX_902.23",
    "SCES_503.61",
    "SCUS_971.24",
    "SCKA_200.60",
    "SCUS_974.65",
    "SCES_532.85",
    "SLES_532.85",
    "SCPS_151.00",
    "SCAJ_201.57",
    "SCPS_193.21",
    "SCPS_193.28"};
static const size_t count_XEBP_100_01 = sizeof(titles_XEBP_100_01) / sizeof(titles_XEBP_100_01[0]);

static const char *titles_XEBP_100_02[] = {
    "SCES_524.56",
    "SCKA_200.37",
    "SCPS_150.84",
    "SLES_524.56",
    "SCAJ_201.09",
    "SCPS_193.09",
    "SCKA_200.11",
    "SCPS_150.56",
    "SCPS_193.17",
    "SCUS_972.68",
    "SCUS_975.13",
    "PBPX_955.16",
    "SCED_510.75",
    "SCES_509.16",
    "SCKA_201.20",
    "SCPS_150.37",
    "SCUS_971.99",
    "SLES_509.16"};
static const size_t count_XEBP_100_02 = sizeof(titles_XEBP_100_02) / sizeof(titles_XEBP_100_02[0]);

static const char *titles_XEBP_100_03[] = {
    "SCES_554.96",
    "SCUS_976.23",
    "SCPS_151.20",
    "SCUS_976.15"};
static const size_t count_XEBP_100_03 = sizeof(titles_XEBP_100_03) / sizeof(titles_XEBP_100_03[0]);

static const char *titles_XEBP_100_04[] = {
    "SCES_502.94",
    "PBPX_956.01",
    "SCAJ_300.06",
    "SCAJ_300.07",
    "SCES_517.19",
    "SCKA_300.01",
    "SCPS_170.01",
    "SCUS_973.28",
    "SLES_517.19",
    "PBPX_955.23",
    "PBPX_955.24",
    "SCAJ_200.66",
    "SCES_524.38",
    "SCPS_150.55",
    "SLES_524.38",
    "SCPS_193.04",
    "PBPX_955.02",
    "SCPS_150.09",
    "SCPS_550.07",
    "SCUS_971.02",
    "SCUS_975.12",
    "SCPS_720.01",
    "PBPX_955.03",
    "SCPS_150.10",
    "SCPS_550.05",
    "SCPS_192.08",
    "SCES_508.58",
    "SCPS_559.02",
    "SCPS_559.03",
    "SCPS_560.05",
    "SCKA_200.29"};
static const size_t count_XEBP_100_04 = sizeof(titles_XEBP_100_04) / sizeof(titles_XEBP_100_04[0]);

static const char *titles_XEBP_100_05[] = {
    "SCES_503.82",
    "SLES_503.82",
    "SLPM_650.51",
    "SLUS_202.28",
    "SCES_511.56",
    "SLES_511.56",
    "SCES_514.34",
    "SLES_514.34",
    "SLPM_652.57",
    "SLUS_206.22",
    "SLPM_660.18",
    "SLPM_656.22"};
static const size_t count_XEBP_100_05 = sizeof(titles_XEBP_100_05) / sizeof(titles_XEBP_100_05[0]);

static const char *titles_XEBP_100_06[] = {
    "SCES_504.90",
    "SCES_504.91",
    "SCES_504.92",
    "SCES_504.93",
    "SCES_504.94",
    "SLES_504.90",
    "SLES_504.91",
    "SLES_504.92",
    "SLES_504.93",
    "SLES_504.94",
    "SLPS_250.50",
    "SLUS_203.12",
    "SLKA_252.14",
    "SLPM_666.77",
    "SLPM_661.24",
    "SLPM_651.15",
    "SLPM_675.13",
    "SLPS_250.88",
    "SCAJ_250.12",
    "SCES_518.15",
    "SCES_518.16",
    "SCES_518.17",
    "SCES_518.18",
    "SCES_518.19",
    "SLAJ_250.12",
    "SLES_518.15",
    "SLES_518.16",
    "SLES_518.17",
    "SLES_518.18",
    "SLES_518.19",
    "SLKA_251.44",
    "SLPS_252.50",
    "SLUS_206.72",
    "SLPM_666.78",
    "SLPM_661.25",
    "SCAJ_200.68",
    "SLPM_654.78"};
static const size_t count_XEBP_100_06 = sizeof(titles_XEBP_100_06) / sizeof(titles_XEBP_100_06[0]);

static const char *titles_XEBP_100_07[] = {
    "SCAJ_200.99",
    "SCCS_400.05",
    "SCED_508.44",
    "SCES_507.60",
    "SCPS_110.03",
    "SCPS_550.01",
    "SCPS_560.01",
    "SCUS_971.13",
    "SLES_507.60",
    "SCKA_200.28",
    "SCPS_191.03",
    "SCPS_191.51",
    "SCAJ_201.96",
    "SCAJ_201.46",
    "SCES_533.26",
    "SCUS_974.72",
    "SLES_533.26"};
static const size_t count_XEBP_100_07 = sizeof(titles_XEBP_100_07) / sizeof(titles_XEBP_100_07[0]);

static const char *titles_XEBP_100_08[] = {
    "SLKA_250.80",
    "SCES_522.37",
    "SLES_522.37",
    "SLUS_202.67",
    "SCPS_550.29",
    "SLPS_251.21",
    "SLPS_732.30",
    "SLKA_251.38",
    "SCES_524.67",
    "SLES_524.67",
    "SLUS_205.62",
    "SLPS_732.31",
    "SCPS_550.42",
    "SLPS_251.43",
    "SLKA_251.45",
    "SCES_524.69",
    "SLES_524.69",
    "SLUS_205.63",
    "SCAJ_200.04",
    "SLPS_251.58",
    "SLPS_732.32",
    "SCES_524.68",
    "SLES_524.68",
    "SLUS_205.64",
    "SLKA_251.74",
    "SLPS_732.33",
    "SCAJ_200.24",
    "SLPS_252.02",
    "SLUS_212.58",
    "SLUS_291.99",
    "SLPS_256.51",
    "SLPS_732.59",
    "SLPS_257.55",
    "SLPS_256.55",
    "SLPS_732.66",
    "SLUS_214.88",
    "SLPS_256.56",
    "SLPS_732.67",
    "SLUS_214.89",
    "SLPS_256.52",
    "SLPS_257.56",
    "SLUS_214.80",
    "SLPS_255.27",
    "SLPM_685.21"};
static const size_t count_XEBP_100_08 = sizeof(titles_XEBP_100_08) / sizeof(titles_XEBP_100_08[0]);

static const char *titles_XEBP_100_09[] = {
    "SLUS_211.68",
    "SCES_537.55",
    "SLES_537.55",
    "SLKA_253.28",
    "SCES_521.18",
    "SLES_521.18",
    "SLPM_654.44",
    "SLUS_207.33",
    "SLKA_250.82",
    "SLPM_663.25",
    "SLPM_654.06"};
static const size_t count_XEBP_100_09 = sizeof(titles_XEBP_100_09) / sizeof(titles_XEBP_100_09[0]);

static const char *titles_XEBP_100_10[] = {
    "SCES_531.94",
    "SLES_531.94",
    "SLUS_210.83",
    "SLPS_204.23",
    "SCES_542.21",
    "SLES_542.21",
    "SLPM_665.72",
    "SLUS_214.09"};
static const size_t count_XEBP_100_10 = sizeof(titles_XEBP_100_10) / sizeof(titles_XEBP_100_10[0]);

static const char *titles_XEBP_100_11[] = {
    "SCES_515.89",
    "SLES_515.89",
    "SLUS_207.65",
    "SCES_533.19",
    "SLES_533.19",
    "SLUS_209.84"};
static const size_t count_XEBP_100_11 = sizeof(titles_XEBP_100_11) / sizeof(titles_XEBP_100_11[0]);

static const char *titles_XEBP_100_12[] = {
    "SLKA_252.65",
    "SLPM_658.80",
    "SLUS_209.64",
    "SCES_530.38",
    "SLES_530.38",
    "SCES_541.86",
    "SLES_541.86",
    "SLUS_213.61",
    "SLPM_742.68",
    "SLPM_661.60",
    "SLPM_742.42"};
static const size_t count_XEBP_100_12 = sizeof(titles_XEBP_100_12) / sizeof(titles_XEBP_100_12[0]);

static const char *titles_XEBP_100_13[] = {
    "SCES_537.69",
    "SLES_537.69",
    "SLUS_212.45",
    "SCES_529.13",
    "SLES_529.13",
    "SLUS_209.79",
    "SLPM_742.13",
    "SLPM_656.00",
    "SLPM_655.99"};
static const size_t count_XEBP_100_13 = sizeof(titles_XEBP_100_13) / sizeof(titles_XEBP_100_13[0]);

static const char *titles_XEBP_100_14[] = {
    "SCES_532.00",
    "SLES_532.00",
    "SLUS_212.27",
    "SLPS_255.60",
    "SLUS_214.41",
    "SCES_541.64",
    "SLES_541.64",
    "SLPS_256.90"};
static const size_t count_XEBP_100_14 = sizeof(titles_XEBP_100_14) / sizeof(titles_XEBP_100_14[0]);

static const char *titles_XEBP_100_15[] = {
    "SCES_512.33",
    "SLES_512.33",
    "SLPS_251.74",
    "SLUS_205.91",
    "SCES_518.39",
    "SLES_518.39",
    "SLPS_253.30",
    "SLUS_207.79",
    "SLPM_685.13",
    "SCES_527.30",
    "SLES_527.30",
    "SLUS_209.98",
    "SCES_533.46",
    "SLES_533.46",
    "SLUS_211.23"};
static const size_t count_XEBP_100_15 = sizeof(titles_XEBP_100_15) / sizeof(titles_XEBP_100_15[0]);

static const char *titles_XEBP_100_16[] = {
    "SLPM_622.06",
    "SLUS_207.58",
    "SLPS_201.06",
    "SLPS_201.05",
    "SLPM_622.07",
    "SLPM_622.08",
    "SLUS_207.59",
    "SLPM_624.40"};
static const size_t count_XEBP_100_16 = sizeof(titles_XEBP_100_16) / sizeof(titles_XEBP_100_16[0]);

static const char *titles_XEBP_100_17[] = {
    "SLUS_204.69",
    "SLPS_290.02",
    "SLPS_739.01",
    "SLPS_290.01",
    "SLPS_290.05",
    "SCPS_559.01",
    "SCAJ_300.01",
    "SCAJ_200.86",
    "SLPS_253.68",
    "SCAJ_200.87",
    "SLPS_253.69",
    "SLPS_732.24",
    "SLPS_732.25",
    "SLPS_253.66",
    "SLPS_253.67",
    "SLES_820.34",
    "SLES_820.35",
    "SLUS_208.92",
    "SLUS_211.33",
    "SLUS_213.89",
    "SLUS_214.17",
    "SCAJ_201.79",
    "SLPS_256.40",
    "SCAJ_201.80",
    "SLPS_256.41"};
static const size_t count_XEBP_100_17 = sizeof(titles_XEBP_100_17) / sizeof(titles_XEBP_100_17[0]);

static const char *titles_XEBP_100_18[] = {
    "SLES_550.18",
    "SLUS_215.69",
    "SCKA_200.99",
    "SLPM_664.45",
    "SLES_553.54",
    "SLUS_216.21",
    "SLPM_666.89",
    "SLPM_837.70"};
static const size_t count_XEBP_100_18 = sizeof(titles_XEBP_100_18) / sizeof(titles_XEBP_100_18[0]);

static const char *titles_XEBP_100_19[] = {
    "SCES_543.08",
    "SLES_543.08",
    "SLPM_660.31",
    "SLUS_211.94",
    "SLPM_742.44",
    "SLES_548.92",
    "SLPM_666.63",
    "SLUS_216.31"};
static const size_t count_XEBP_100_19 = sizeof(titles_XEBP_100_19) / sizeof(titles_XEBP_100_19[0]);

static const char *titles_XEBP_100_20[] = {
    "SCES_534.58",
    "SLES_534.58",
    "SLUS_209.74",
    "SCAJ_200.95",
    "SLPM_655.97",
    "SLPM_663.72",
    "SCES_545.55",
    "SLES_545.55",
    "SLUS_211.52",
    "SCAJ_201.20",
    "SLPM_657.95",
    "SLPM_663.73"};
static const size_t count_XEBP_100_20 = sizeof(titles_XEBP_100_20) / sizeof(titles_XEBP_100_20[0]);

static const char *titles_XEBP_100_21[] = {
    "SLPM_621.54",
    "SLPM_652.77",
    "SLPM_653.58",
    "SLPM_624.27",
    "SLPM_657.75",
    "SLPM_662.42",
    "SLPM_666.09",
    "SLPM_669.30",
    "SLPM_550.90",
    "SLPM_663.13",
    "SLPM_663.14"};
static const size_t count_XEBP_100_21 = sizeof(titles_XEBP_100_21) / sizeof(titles_XEBP_100_21[0]);

static const char *titles_XEBP_100_22[] = {
    "SCUS_971.04",
    "SCES_502.93",
    "SLES_502.93",
    "SCUS_972.11",
    "SLES_518.14",
    "SCUS_975.10",
    "SCUS_973.69",
    "SCUS_974.05",
    "SLES_537.54",
    "SCES_537.54",
    "SCUS_975.14",
    "SLUS_974.05"};
static const size_t count_XEBP_100_22 = sizeof(titles_XEBP_100_22) / sizeof(titles_XEBP_100_22[0]);

static const char *titles_XEBP_100_23[] = {
    "SCES_529.42",
    "SLES_529.42",
    "SLUS_210.29",
    "SCES_537.17",
    "SLES_537.17",
    "SLUS_213.55"};
static const size_t count_XEBP_100_23 = sizeof(titles_XEBP_100_23) / sizeof(titles_XEBP_100_23[0]);

static const char *titles_XEBP_100_24[] = {
    "SCES_517.97",
    "SLES_517.97",
    "SLUS_207.52",
    "SCES_525.81",
    "SLES_525.81",
    "SLUS_210.00",
    "SLUS_210.25",
    "SLUS_207.19",
    "SLUS_209.91",
    "SCES_518.87",
    "SLES_518.87",
    "SLUS_207.57",
    "SLUS_20754",
    "SLUS_20824",
    "SLUS_208.68",
    "SLUS_207.50",
    "SCES_519.53",
    "SCES_519.63",
    "SCES_519.64",
    "SLES_519.53",
    "SLES_519.63",
    "SLES_519.64",
    "SCES_523.74",
    "SLES_523.74",
    "SLPM_655.87",
    "SLUS_209.06",
    "SCES_517.98",
    "SLES_517.98",
    "SLUS_207.56",
    "SLES_520.22"};
static const size_t count_XEBP_100_24 = sizeof(titles_XEBP_100_24) / sizeof(titles_XEBP_100_24[0]);

static const char *titles_XEBP_100_25[] = {
    "SCES_50422",
    "SLES_50422",
    "SLUS_202.63",
    "SLPS_250.79",
    "SLUS_202.41"};
static const size_t count_XEBP_100_25 = sizeof(titles_XEBP_100_25) / sizeof(titles_XEBP_100_25[0]);

static const char *titles_XEBP_100_26[] = {
    "SCES_51154",
    "SLES_51154",
    "SLUS_205.29",
    "SLPS_251.57",
    "SLUS_205.30"};
static const size_t count_XEBP_100_26 = sizeof(titles_XEBP_100_26) / sizeof(titles_XEBP_100_26[0]);

static const char *titles_XEBP_100_27[] = {
    "SLPM_662.04",
    "SLUS_212.13",
    "SLUS_212.14",
    "SCES_529.82",
    "SLES_529.82",
    "SLUS_211.18",
    "SCES_525.84",
    "SCES_525.85",
    "SLAJ_250.53",
    "SLES_525.84",
    "SLES_525.85",
    "SLPM_657.19",
    "SLUS_210.50",
    "SLPM_659.58",
    "SLPM_669.62",
    "SLKA_252.06",
    "SCES_535.06",
    "SCES_535.07",
    "SLES-5350",
    "SLES_535.07",
    "SLPM_661.08",
    "SLUS_212.42",
    "SLKA_253.04",
    "SLAJ_250.66",
    "SLPM_666.52",
    "SLPM_550.04",
    "SCES_538.86",
    "SCES_540.30",
    "SLAJ_250.78",
    "SLES_540.30",
    "SLPM_663.54",
    "SLUS_213.76",
    "SLUS_291.80",
    "SLPM_667.31",
    "SLPM_669.61",
    "SLES_538.86"};
static const size_t count_XEBP_100_27 = sizeof(titles_XEBP_100_27) / sizeof(titles_XEBP_100_27[0]);

static const char *titles_XEBP_100_28[] = {
    "SCES_542.48",
    "SLES_542.48",
    "SLPM_666.00",
    "SLUS_214.76",
    "SLUS_214.77",
    "SLUS_214.59",
    "SLUS_214.07"};
static const size_t count_XEBP_100_28 = sizeof(titles_XEBP_100_28) / sizeof(titles_XEBP_100_28[0]);

static const char *titles_XEBP_100_29[] = {
    "SLPM_668.37",
    "SLUS_216.38",
    "SLUS_216.20"};
static const size_t count_XEBP_100_29 = sizeof(titles_XEBP_100_29) / sizeof(titles_XEBP_100_29[0]);

static const char *titles_XEBP_100_30[] = {
    "SLUS_217.70",
    "SLUS_217.52"};
static const size_t count_XEBP_100_30 = sizeof(titles_XEBP_100_30) / sizeof(titles_XEBP_100_30[0]);

static const char *titles_XEBP_100_31[] = {
    "SLUS_218.93",
    "SLUS_218.92"};
static const size_t count_XEBP_100_31 = sizeof(titles_XEBP_100_31) / sizeof(titles_XEBP_100_31[0]);

static const char *titles_XEBP_100_32[] = {
    "SCES_513.40",
    "SLAJ_250.07",
    "SLES_513.40",
    "SLPM_652.69",
    "SLUS_204.76",
    "SLUS_205.38"};
static const size_t count_XEBP_100_32 = sizeof(titles_XEBP_100_32) / sizeof(titles_XEBP_100_32[0]);

static const char *titles_XEBP_100_33[] = {
    "SCES_513.39",
    "SLES_513.39",
    "SLPM_652.24",
    "SLUS_204.57",
    "SLUS_204.53"};
static const size_t count_XEBP_100_33 = sizeof(titles_XEBP_100_33) / sizeof(titles_XEBP_100_33[0]);

static const char *titles_XEBP_100_34[] = {
    "SCES_520.08",
    "SLES_520.08",
    "SLPS_252.97",
    "SLUS_207.55",
    "SLPM_656.66",
    "SLUS_207.71"};
static const size_t count_XEBP_100_34 = sizeof(titles_XEBP_100_34) / sizeof(titles_XEBP_100_34[0]);

static const char *titles_XEBP_100_35[] = {
    "SCES_516.61",
    "SCES_516.62",
    "SCES_516.63",
    "SCES_516.64",
    "SCES_516.65",
    "SLES_516.61",
    "SLES_516.62",
    "SLES_516.63",
    "SLES_516.64",
    "SLES_516.65",
    "SLUS_206.53",
    "SLKA_253.39",
    "SCES_521.71",
    "SCES_521.72",
    "SCES_521.74",
    "SCES_521.75",
    "SLES_521.71",
    "SLES_521.72",
    "SLES_521.73",
    "SLES_521.74",
    "SLES_521.75",
    "SCES_525.91",
    "SCES_525.92",
    "SCES_525.93",
    "SLES_525.91",
    "SLES_525.92",
    "SLES_525.93",
    "SLUS_209.38",
    "SLKA_253.40",
    "SLKA_254.17"};
static const size_t count_XEBP_100_35 = sizeof(titles_XEBP_100_35) / sizeof(titles_XEBP_100_35[0]);

static const char *titles_XEBP_100_36[] = {
    "SCES_533.39",
    "SLES_533.39",
    "SLUS_211.53",
    "SCES_538.60",
    "SCES_538.61",
    "SCES_538.62",
    "SLES_538.60",
    "SLES_538.61",
    "SLES_538.62",
    "SLUS_212.99",
    "SSCES_540.95",
    "SCES_540.96",
    "SCES_540.97",
    "SLES_540.95",
    "SLES_540.96",
    "SLES_540.97",
    "SLUS_213.98",
    "SLUS_208.79",
    "SLPM_654.23",
    "SLPM_661.72",
    "SLKA_252.66",
    "SLPM_668.13",
    "SLPM_665.23",
    "SLPM_669.71",
    "SLUS_212.02",
    "SLPM_658.91",
    "SLPM_665.60",
    "SLUS_215.84",
    "SLPM_665.49",
    "SLPM_667.01"};
static const size_t count_XEBP_100_36 = sizeof(titles_XEBP_100_36) / sizeof(titles_XEBP_100_36[0]);

static const char *titles_XEBP_100_37[] = {
    "SCES_525.51",
    "SCES_525.53",
    "SCES_525.55",
    "SLUS_208.78",
    "SLPM_625.35",
    "SLUS_210.80",
    "SLES_530.02",
    "SLES_530.03",
    "SLES_530.04",
    "SCES_531.19",
    "SCES_531.20",
    "SCES_531.21",
    "SLAJ_250.57",
    "SLES_531.19",
    "SLES_531.20",
    "SLES_531.21",
    "SLPM_657.81",
    "SLPM_742.23",
    "SLPM_625.73",
    "SLPM_742.31",
    "KOEI_000.21",
    "SLPM_657.80"};
static const size_t count_XEBP_100_37 = sizeof(titles_XEBP_100_37) / sizeof(titles_XEBP_100_37[0]);

static const char *titles_XEBP_100_38[] = {
    "SCES_543.40",
    "SLES_543.40",
    "SLUS_214.62",
    "SLES_546.24",
    "SLES_551.08",
    "SLUS_217.26",
    "SLES_546.24",
    "SLUS_215.85"};
static const size_t count_XEBP_100_38 = sizeof(titles_XEBP_100_38) / sizeof(titles_XEBP_100_38[0]);

static const char *titles_XEBP_100_39[] = {
    "SCES_548.45",
    "SLES_548.45",
    "SLUS_216.62",
    "SLUS_216.62",
    "SLUS_218.03"};
static const size_t count_XEBP_100_39 = sizeof(titles_XEBP_100_39) / sizeof(titles_XEBP_100_39[0]);

static const char *titles_XEBP_100_40[] = {
    "SCES_504.12",
    "SCES_504.62",
    "SLES_504.12",
    "SLES_504.62",
    "SLES_511.14",
    "SCES_511.14",
    "SCES_519.12",
    "SCES_519.15",
    "SLES_519.12",
    "SLES_519.15",
    "SCES_527.60",
    "SCES_528.00",
    "SLES_527.60",
    "SLES_528.00",
    "SCES_534.44",
    "SCES_535.44",
    "SCES_535.45",
    "SLES_534.44",
    "SLES_535.44",
    "SLES_535.45",
    "SCES_542.03",
    "SCES_542.04",
    "SCES_543.60",
    "SCES_543.61",
    "SCES_543.62",
    "SLES_542.03",
    "SLES_542.04",
    "SLES_543.60",
    "SLES_543.61",
    "SLES_543.62",
    "SLES_549.13",
    "SLUS_216.85",
    "SLES_554.06",
    "SLUS_218.21",
    "SLES_555.87",
    "SLES_556.36",
    "SLES_556.56",
    "SLES_556.66",
    "SLES_556.73",
    "SCES_538.99",
    "SLES_538.99"};
static const size_t count_XEBP_100_40 = sizeof(titles_XEBP_100_40) / sizeof(titles_XEBP_100_40[0]);

static const char *titles_XEBP_100_41[] = {
    "SCES_503.60",
    "SCUS_971.01",
    "SLES_503.60",
    "SCES_512.24",
    "SCUS_971.97",
    "SLES_512.24",
    "SLPM_654.12"};
static const size_t count_XEBP_100_41 = sizeof(titles_XEBP_100_41) / sizeof(titles_XEBP_100_41[0]);

static const char *titles_XEBP_100_42[] = {
    "SCES_527.07",
    "SLES_527.07",
    "SLPM_654.95",
    "SLUS_208.96",
    "SLPM_742.48",
    "SLKA_252.19",
    "SLPM_658.69"};
static const size_t count_XEBP_100_42 = sizeof(titles_XEBP_100_42) / sizeof(titles_XEBP_100_42[0]);

static const char *titles_XEBP_100_43[] = {
    "SLES_522.86",
    "SLES_534.98",
    "SCES_543.76",
    "SCKA_200.96",
    "SLES_543.76",
    "SLUS_212.77",
    "SCES_534.18",
    "SCES_536.95",
    "SLES_534.18",
    "SLES_536.95",
    "SLUS_212.18",
    "SCES_534.94",
    "SCES_534.96",
    "SLES_534.94",
    "SLES_534.96",
    "SLUS_212.52",
    "SLUS_212.84",
    "SLES_536.34"};
static const size_t count_XEBP_100_43 = sizeof(titles_XEBP_100_43) / sizeof(titles_XEBP_100_43[0]);

static const char *titles_XEBP_100_44[] = {
    "SCES_541.63",
    "SLPS_252.93",
    "SLES_541.63",
    "SLUS_213.58",
    "SLPS_732.12",
    "SLPS_253.98",
    "SLPS_732.21",
    "SLES_548.78",
    "SLUS_215.75",
    "SLES_552.37",
    "SLUS_217.27",
    "SLPS_732.51",
    "SLPS_255.89",
    "SLPS_257.68",
    "SLUS_218.62",
    "SLES_556.05",
    "SLPS_258.37"};
static const size_t count_XEBP_100_44 = sizeof(titles_XEBP_100_44) / sizeof(titles_XEBP_100_44[0]);

static const char *titles_XEBP_100_45[] = {
    "SLES_550.04",
    "SCES_527.25",
    "SLES_527.25",
    "SLUS_210.65",
    "SLUS_291.18",
    "SLPM_669.60",
    "SLPM_657.66",
    "SLPM_660.51",
    "SCES_535.57",
    "SCES_535.58",
    "SCES_535.59",
    "SLES_535.57",
    "SLES_535.58",
    "SLES_535.59",
    "SLPM_662.32",
    "SLUS_212.67",
    "SLPM_665.62",
    "SCES_538.57",
    "SLAJ_250.75",
    "SLES_538.57",
    "SLUS_213.51",
    "SLPM_668.69",
    "SCES_543.21",
    "SCES_543.22",
    "SCES_543.23",
    "SCES_543.24",
    "SLAJ_250.91",
    "SLES_543.21",
    "SLES_543.22",
    "SLES_543.23",
    "SLES_543.24",
    "SLPM_666.17",
    "SLUS_214.93",
    "SCES_544.02",
    "SCES_544.92",
    "SCES_544.93",
    "SLES_544.02",
    "SLES_544.92",
    "SLES_544.93",
    "SLUS_214.94",
    "SLPM_669.32",
    "SLUS_216.58",
    "SLES_550.02",
    "SLPM_551.51"};
static const size_t count_XEBP_100_45 = sizeof(titles_XEBP_100_45) / sizeof(titles_XEBP_100_45[0]);

static const char *titles_XEBP_100_46[] = {
    "SLUS_213.95",
    "SLES_541.88",
    "SLES_548.40",
    "SLUS_215.88"};
static const size_t count_XEBP_100_46 = sizeof(titles_XEBP_100_46) / sizeof(titles_XEBP_100_46[0]);

static const char *titles_XEBP_100_47[] = {
    "PAPX_902.34",
    "SCAJ_200.79",
    "SCKA_200.25",
    "SLPS_253.60",
    "SLUS_210.08",
    "SLPS_732.10",
    "SLPS_732.40",
    "SLPS_254.67",
    "SCAJ_201.35",
    "SCKA_200.51",
    "SLPS_732.41",
    "SLUS_212.30"};
static const size_t count_XEBP_100_47 = sizeof(titles_XEBP_100_47) / sizeof(titles_XEBP_100_47[0]);

static const char *titles_XEBP_100_48[] = {
    "SLES_526.74",
    "SCES_525.63",
    "SLES_525.63",
    "SLES_525.59",
    "SCES_525.59",
    "SLES_525.62",
    "SCES_525.62",
    "SLES_525.61",
    "SCES_525.61",
    "SLES_525.60",
    "SCES_525.60"};
static const size_t count_XEBP_100_48 = sizeof(titles_XEBP_100_48) / sizeof(titles_XEBP_100_48[0]);

static const char *titles_XEBP_100_49[] = {
    "SLPM_654.13",
    "SCES_519.14",
    "SLES_519.14",
    "SLUS_206.94",
    "SCES_519.13",
    "SLES_519.13",
    "SLPM_654.11",
    "SLUS_207.10"};
static const size_t count_XEBP_100_49 = sizeof(titles_XEBP_100_49) / sizeof(titles_XEBP_100_49[0]);

static const char *titles_XEBP_100_50[] = {
    "SLPM_669.65",
    "SLPM_669.64"};
static const size_t count_XEBP_100_50 = sizeof(titles_XEBP_100_50) / sizeof(titles_XEBP_100_50[0]);

static const char *titles_XEBP_100_51[] = {
    "SLES_820.42",
    "SLES_820.44",
    "SLES_820.46",
    "SLES_820.48",
    "SLES_820.50",
    "SLES_820.52",
    "SLES_820.43",
    "SLES_820.45",
    "SLES_820.47",
    "SLES_820.49",
    "SLES_820.51",
    "SLES_820.53",
    "SLKA_253.53",
    "SLKA_253.54",
    "SLUS_212.43",
    "SLPM_662.23",
    "SLPM_662.24",
    "SLPM_662.20",
    "SLPM_662.21",
    "SLPM_662.22",
    "SLUS_213.59",
    "SLUS_213.60",
    "SLPM_661.17",
    "SLPM_661.18",
    "SLPM_661.19",
    "SLPM_552.37",
    "SLPM_667.95",
    "SLPM_667.99",
    "SLPM_742.58"};
static const size_t count_XEBP_100_51 = sizeof(titles_XEBP_100_51) / sizeof(titles_XEBP_100_51[0]);

static const char *titles_XEBP_100_52[] = {
    "SLPM_658.70",
    "SLPM_662.58",
    "SLPM_658.71",
    "SLPM_660.62",
    "SLPM_660.63",
    "SLPM_663.29",
    "SLPM_665.92",
    "SLPM_665.93",
    "SLPM_667.04",
    "SLPM_667.05"};
static const size_t count_XEBP_100_52 = sizeof(titles_XEBP_100_52) / sizeof(titles_XEBP_100_52[0]);

static const char *titles_XEBP_100_53[] = {
    "SLUS_213.48",
    "SLKA_253.42",
    "SLPM_661.68",
    "SLPM_742.53",
    "SLPM_742.34",
    "SLES_552.42",
    "SLPM_666.02",
    "SLKA_252.80",
    "SLKA_252.81",
    "SLPM_743.01"};
static const size_t count_XEBP_100_53 = sizeof(titles_XEBP_100_53) / sizeof(titles_XEBP_100_53[0]);

static const char *titles_XEBP_100_54[] = {
    "SCAJ_250.45",
    "SLAJ_250.45",
    "SLPM_655.31",
    "SLKA_252.07",
    "SLPM_662.08",
    "SLAJ_250.34",
    "SLPM_655.15",
    "SLUS_219.27",
    "SLAJ_350.01",
    "SLKA_350.03",
    "SLPM_670.03",
    "SLAJ_350.03"};
static const size_t count_XEBP_100_54 = sizeof(titles_XEBP_100_54) / sizeof(titles_XEBP_100_54[0]);

static const char *titles_XEBP_100_55[] = {
    "SLPS_734.18",
    "SCES_506.77",
    "SCES_508.22",
    "SLES_506.77",
    "SLES_508.22",
    "SLPS_250.41",
    "SLUS_203.47",
    "SLES_820.30",
    "SLES_820.31",
    "SLUS_210.41",
    "SLUS_210.44",
    "SLPS_253.17",
    "SLPS_732.14",
    "SLPM_602.14",
    "SLPM_602.26",
    "SLPM_732.14"};
static const size_t count_XEBP_100_55 = sizeof(titles_XEBP_100_55) / sizeof(titles_XEBP_100_55[0]);

static const char *titles_XEBP_100_56[] = {
    "SCES_500.54",
    "SLES_500.54",
    "SCES_500.71",
    "SLES_500.71",
    "SLPS_200.68",
    "SLUS_200.63",
    "SLPM_621.89",
    "SCES_500.55",
    "SCES_500.61",
    "SLES_500.55",
    "SLES_500.61",
    "SLUS_200.65"};
static const size_t count_XEBP_100_56 = sizeof(titles_XEBP_100_56) / sizeof(titles_XEBP_100_56[0]);

static const char *titles_XEBP_100_57[] = {
    "SLUS_210.39",
    "SLPM_664.98",
    "SCES_526.37",
    "SLES_526.37",
    "SCES_526.38",
    "SLES_526.38",
    "SCES_526.39",
    "SLES_526.39",
    "SCES_530.87",
    "SLES_530.87",
    "SLUS_211.82",
    "SLPM_668.81",
    "SLPM_550.46",
    "SLUS_291.78",
    "SCES_530.88",
    "SLES_530.88",
    "SCES_530.89",
    "SLES_530.89"};
static const size_t count_XEBP_100_57 = sizeof(titles_XEBP_100_57) / sizeof(titles_XEBP_100_57[0]);

static const char *titles_XEBP_100_58[] = {
    "SCES_524.58",
    "SLES_524.58",
    "SCES_544.54",
    "SLES_544.54"};
static const size_t count_XEBP_100_58 = sizeof(titles_XEBP_100_58) / sizeof(titles_XEBP_100_58[0]);

static const char *titles_XEBP_100_59[] = {
    "SLUS_206.66",
    "SLUS_213.97"};
static const size_t count_XEBP_100_59 = sizeof(titles_XEBP_100_59) / sizeof(titles_XEBP_100_59[0]);

static const char *titles_XEBP_100_60[] = {
    "SLPS_202.51",
    "SLPS_202.50",
    "SCAJ_100.01",
    "SLPS_731.03",
    "SLPS_731.03",
    "SLPS_256.08",
    "SLPS_256.07",
    "SLPS_732.54"};
static const size_t count_XEBP_100_60 = sizeof(titles_XEBP_100_60) / sizeof(titles_XEBP_100_60[0]);

static const char *titles_XEBP_100_61[] = {
    "SCES_500.79",
    "SLES_500.79",
    "SLPS_250.07",
    "SLUS_200.14",
    "SCCS_400.11",
    "SCES_509.05",
    "SCPS_550.24",
    "SLES_509.05",
    "SLPS_250.40",
    "SLUS_202.49",
    "SLPS_734.11",
    "SCES_513.99",
    "SCPS_550.14",
    "SLES_513.99",
    "SLPS_251.12",
    "SLUS_204.35",
    "SCAJ_200.11",
    "SLPS_251.69",
    "SLPS_734.20",
    "SLPS_734.17",
    "SLUS_206.44",
    "SLPS_251.69",
    "SLKA_250.41",
    "SLES_522.03"};
static const size_t count_XEBP_100_61 = sizeof(titles_XEBP_100_61) / sizeof(titles_XEBP_100_61[0]);

static const char *titles_XEBP_100_62[] = {
    "SLUS_207.18",
    "SLES_519.50",
    "SLPM_654.31",
    "SLKA_251.22",
    "SLUS_209.17",
    "SLES_529.98",
    "SLPM_657.58",
    "SLKA_252.33",
    "SLES_533.50",
    "SLPM_660.74"};
static const size_t count_XEBP_100_62 = sizeof(titles_XEBP_100_62) / sizeof(titles_XEBP_100_62[0]);

static const char *titles_XEBP_100_63[] = {
    "SLUS_201.52",
    "SLPS_250.52",
    "SCES_504.10",
    "SLUS_208.51",
    "SLPS_254.18",
    "SCKA_200.42",
    "SLUS_213.46",
    "SLPS_256.29",
    "SCKA_200.70"};
static const size_t count_XEBP_100_63 = sizeof(titles_XEBP_100_63) / sizeof(titles_XEBP_100_63[0]);

static const char *titles_XEBP_100_64[] = {
    "SCUS_971.40",
    "SCUS_972.01",
    "SCUS_974.16"};
static const size_t count_XEBP_100_64 = sizeof(titles_XEBP_100_64) / sizeof(titles_XEBP_100_64[0]);

static const char *titles_XEBP_100_65[] = {
    "SLUS_204.59",
    "SCES_514.28",
    "SLPM_652.00",
    "SLKA_250.21",
    "SLUS_208.10",
    "SLES_522.38",
    "SLPM_654.47",
    "SLKA_251.35"};
static const size_t count_XEBP_100_65 = sizeof(titles_XEBP_100_65) / sizeof(titles_XEBP_100_65[0]);

static const char *titles_XEBP_100_66[] = {
    "SLUS_211.65",
    "SCAJ_201.08",
    "SCPS_150.58",
    "SCUS_972.31",
    "SCAJ_200.19",
    "SCAJ_200.38",
    "SCES_519.10",
    "SLES_519.10",
    "SCCS_400.07",
    "PAPX_902.30",
    "PCPX_963.30"};
static const size_t count_XEBP_100_66 = sizeof(titles_XEBP_100_66) / sizeof(titles_XEBP_100_66[0]);

static const char *titles_XEBP_100_67[] = {
    "SCES_541.53",
    "SLES_541.53",
    "SLPM_664.42",
    "SCES_541.51",
    "SLES_541.51"};
static const size_t count_XEBP_100_67 = sizeof(titles_XEBP_100_67) / sizeof(titles_XEBP_100_67[0]);

static const char *titles_XEBP_100_68[] = {
    "SLES_544.60",
    "SLES_545.69"};
static const size_t count_XEBP_100_68 = sizeof(titles_XEBP_100_68) / sizeof(titles_XEBP_100_68[0]);

static const char *titles_XEBP_100_69[] = {
    "SLPS_204.41",
    "SLPS_204.66"};
static const size_t count_XEBP_100_69 = sizeof(titles_XEBP_100_69) / sizeof(titles_XEBP_100_69[0]);

static const char *titles_XEBP_100_70[] = {
    "SCES_533.98",
    "SLES_533.98",
    "SCES_544.61",
    "SLES_544.61"};
static const size_t count_XEBP_100_70 = sizeof(titles_XEBP_100_70) / sizeof(titles_XEBP_100_70[0]);

static const char *titles_XEBP_100_71[] = {
    "SLPM_625.25",
    "SLKA_150.45",
    "SLPM_626.38"};
static const size_t count_XEBP_100_71 = sizeof(titles_XEBP_100_71) / sizeof(titles_XEBP_100_71[0]);

static const char *titles_XEBP_100_72[] = {
    "SLPM_625.25",
    "SLKA_150.45",
    "SLPM_626.38"};
static const size_t count_XEBP_100_72 = sizeof(titles_XEBP_100_71) / sizeof(titles_XEBP_100_72[0]);

static const char *titles_XEBP_100_73[] = {
    "SLPM_651.76",
    "SLPM_652.06"};
static const size_t count_XEBP_100_73 = sizeof(titles_XEBP_100_71) / sizeof(titles_XEBP_100_73[0]);

static const char *titles_XEBP_100_74[] = {
    "SLUS_206.36",
    "SLES_525.31",
    "SLED_524.88",
    "SLES_524.39",
    "SLES_516.93",
    "SLUS_211.89",
    "SLES_535.28",
    "SLES_535.26",
    "SLES_535.27",
    "SLED_536.27",
    "SLES_536.26"};
static const size_t count_XEBP_100_74 = sizeof(titles_XEBP_100_71) / sizeof(titles_XEBP_100_74[0]);

static const char *titles_XEBP_100_75[] = {
    "SLUS_205.65",
    "SLES_523.73",
    "SLES_523.25",
    "SLUS_209.73",
    "SLES_530.39"};
static const size_t count_XEBP_100_75 = sizeof(titles_XEBP_100_71) / sizeof(titles_XEBP_100_75[0]);

static const char *titles_XEBP_000_01[] = {
    "SLES_820.36",
    "SLPS_253.38",
    "SCAJ_200.76",
    "SLUS_209.86",
    "SLKA_252.01",
    "SLPS_732.02",
    "SLPS_253.39",
    "SLES_820.37",
    "SLUS_210.79",
    "SLKA_252.02",
    "SCAJ_200.77",
    "SLPS_732.03",
    "SLUS_212.00",
    "SCKA_200.47",
    "SLPS_254.08",
    "SLES_538.19"};
static const size_t count_XEBP_000_01 = sizeof(titles_XEBP_000_01) / sizeof(titles_XEBP_000_01[0]);

static const char *titles_XEBP_000_02[] = {
    "SLPM_650.86",
    "SLPM_650.87"};
static const size_t count_XEBP_000_02 = sizeof(titles_XEBP_000_02) / sizeof(titles_XEBP_000_02[0]);

static const char *titles_XEBP_000_03[] = {
    "SLPS_200.84",
    "SLPS_200.85"};
static const size_t count_XEBP_000_03 = sizeof(titles_XEBP_000_03) / sizeof(titles_XEBP_000_03[0]);

static const char *titles_XEBP_000_04[] = {
    "SLPM_552.21",
    "SLPM_552.22"};
static const size_t count_XEBP_000_04 = sizeof(titles_XEBP_000_04) / sizeof(titles_XEBP_000_04[0]);

static const char *titles_XEBP_000_05[] = {
    "SCPS_110.19",
    "SCPS_110.20"};
static const size_t count_XEBP_000_05 = sizeof(titles_XEBP_000_05) / sizeof(titles_XEBP_000_05[0]);

static const char *titles_XEBP_000_06[] = {
    "SLES_820.21",
    "SLES_820.20",
    "SLUS_206.97",
    "SLUS_208.54",
    "SLES_820.21",
    "SLPM_655.06",
    "SLPM_655.07",
    "SLPM_657.42",
    "SLPM_657.43",
    "SLPM_655.04",
    "SLPM_655.05"};
static const size_t count_XEBP_000_06 = sizeof(titles_XEBP_000_06) / sizeof(titles_XEBP_000_06[0]);

static const char *titles_XEBP_000_07[] = {
    "SLES_820.11",
    "SLPM_652.32",
    "SCCS_400.02",
    "SLUS_204.84",
    "SCCS_400.03",
    "SLUS_206.27",
    "SLES_820.12",
    "SLPM_652.33"};
static const size_t count_XEBP_000_07 = sizeof(titles_XEBP_000_07) / sizeof(titles_XEBP_000_07[0]);

static const char *titles_XEBP_000_08[] = {
    "SLPS_201.40",
    "SLPS_201.41"};
static const size_t count_XEBP_000_08 = sizeof(titles_XEBP_000_08) / sizeof(titles_XEBP_000_08[0]);

static const char *titles_XEBP_000_09[] = {
    "SLPM_650.40",
    "SLPM_650.41",
    "SLPM_650.42",
    "SLPM_650.43"};
static const size_t count_XEBP_000_09 = sizeof(titles_XEBP_000_09) / sizeof(titles_XEBP_000_09[0]);

static const char *titles_XEBP_000_10[] = {
    "SCAJ_201.41",
    "SLPM_659.76",
    "SLPM_859.76",
    "SLUS_213.34",
    "SLUS_213.45"};
static const size_t count_XEBP_000_10 = sizeof(titles_XEBP_000_10) / sizeof(titles_XEBP_000_10[0]);

static const char *titles_XEBP_000_11[] = {
    "SLPM_650.02",
    "SLPM_650.03"};
static const size_t count_XEBP_000_11 = sizeof(titles_XEBP_000_11) / sizeof(titles_XEBP_000_11[0]);

static const char *titles_XEBP_000_12[] = {
    "SLPM_666.02",
    "SLKA_252.80",
    "SLKA_252.81",
    "SLPM_743.01"};
static const size_t count_XEBP_000_12 = sizeof(titles_XEBP_000_12) / sizeof(titles_XEBP_000_12[0]);

static const char *titles_XEBP_000_13[] = {
    "SLES_820.38",
    "SLES_820.39",
    "SLUS_211.80",
    "SLUS_213.62",
    "SCKA_200.86",
    "SCKA_200.87",
    "SLPM_662.75",
    "SLPM_662.76",
    "SLPM_742.51",
    "SLPM_742.32"};
static const size_t count_XEBP_000_13 = sizeof(titles_XEBP_000_13) / sizeof(titles_XEBP_000_13[0]);

static const char *titles_XEBP_000_14[] = {
    "SLAJ_251.04",
    "SLAJ_251.05",
    "SLPM_550.82",
    "SLPM_742.86"};
static const size_t count_XEBP_000_14 = sizeof(titles_XEBP_000_14) / sizeof(titles_XEBP_000_14[0]);

static const char *titles_XEBP_000_15[] = {
    "SLUS_208.06",
    "SLUS_208.07"};
static const size_t count_XEBP_000_15 = sizeof(titles_XEBP_000_15) / sizeof(titles_XEBP_000_15[0]);

static const char *titles_XEBP_000_16[] = {
    "SCPS_550.19",
    "SLES_820.28",
    "SLES_820.29",
    "SLUS_204.88",
    "SLUS_208.91",
    "SLPM_664.78",
    "SLPM_664.79",
    "SCAJ_200.70",
    "SLPM_654.38",
    "SLPM_654.39",
    "SLPM_652.09"};
static const size_t count_XEBP_000_16 = sizeof(titles_XEBP_000_16) / sizeof(titles_XEBP_000_16[0]);

static const char *titles_XEBP_000_17[] = {
    "SLPS_200.18",
    "SLPS_200.19"};
static const size_t count_XEBP_000_17 = sizeof(titles_XEBP_000_17) / sizeof(titles_XEBP_000_17[0]);

static const char *titles_XEBP_000_18[] = {
    "SLUS_208.37",
    "SLUS_209.14"};
static const size_t count_XEBP_000_18 = sizeof(titles_XEBP_000_18) / sizeof(titles_XEBP_000_18[0]);

static const char *titles_XEBP_000_20[] = {
    "SLPM_669.24",
    "SLPM_669.25",
    "SLPM_669.22",
    "SLPM_669.23",
    "SLPM_551.71",
    "SLPM_551.72"};
static const size_t count_XEBP_000_20 = sizeof(titles_XEBP_000_20) / sizeof(titles_XEBP_000_20[0]);

static const char *titles_XEBP_000_21[] = {
    "SLPM_620.49",
    "SLPM_620.50"};
static const size_t count_XEBP_000_21 = sizeof(titles_XEBP_000_21) / sizeof(titles_XEBP_000_21[0]);

static const char *titles_XEBP_000_22[] = {
    "SLPM_667.79",
    "SLPM_667.80"};
static const size_t count_XEBP_000_22 = sizeof(titles_XEBP_000_22) / sizeof(titles_XEBP_000_22[0]);

static const char *titles_XEBP_000_23[] = {
    "SLPM_551.18",
    "SLPM_551.19"};
static const size_t count_XEBP_000_23 = sizeof(titles_XEBP_000_23) / sizeof(titles_XEBP_000_23[0]);


static const MemoryCardGroup allMemoryCardGroups[] = {
    {"XEBP_100.01", titles_XEBP_100_01, count_XEBP_100_01},
    {"XEBP_100.02", titles_XEBP_100_02, count_XEBP_100_02},
    {"XEBP_100.03", titles_XEBP_100_03, count_XEBP_100_03},
    {"XEBP_100.04", titles_XEBP_100_04, count_XEBP_100_04},
    {"XEBP_100.05", titles_XEBP_100_05, count_XEBP_100_05},
    {"XEBP_100.06", titles_XEBP_100_06, count_XEBP_100_06},
    {"XEBP_100.07", titles_XEBP_100_07, count_XEBP_100_07},
    {"XEBP_100.08", titles_XEBP_100_08, count_XEBP_100_08},
    {"XEBP_100.09", titles_XEBP_100_09, count_XEBP_100_09},
    {"XEBP_100.10", titles_XEBP_100_10, count_XEBP_100_10},
    {"XEBP_100.11", titles_XEBP_100_11, count_XEBP_100_11},
    {"XEBP_100.12", titles_XEBP_100_12, count_XEBP_100_12},
    {"XEBP_100.13", titles_XEBP_100_13, count_XEBP_100_13},
    {"XEBP_100.14", titles_XEBP_100_14, count_XEBP_100_14},
    {"XEBP_100.15", titles_XEBP_100_15, count_XEBP_100_15},
    {"XEBP_100.16", titles_XEBP_100_16, count_XEBP_100_16},
    {"XEBP_100.17", titles_XEBP_100_17, count_XEBP_100_17},
    {"XEBP_100.18", titles_XEBP_100_18, count_XEBP_100_18},
    {"XEBP_100.19", titles_XEBP_100_19, count_XEBP_100_19},
    {"XEBP_100.20", titles_XEBP_100_20, count_XEBP_100_20},
    {"XEBP_100.21", titles_XEBP_100_21, count_XEBP_100_21},
    {"XEBP_100.22", titles_XEBP_100_22, count_XEBP_100_22},
    {"XEBP_100.23", titles_XEBP_100_23, count_XEBP_100_23},
    {"XEBP_100.24", titles_XEBP_100_24, count_XEBP_100_24},
    {"XEBP_100.25", titles_XEBP_100_25, count_XEBP_100_25},
    {"XEBP_100.26", titles_XEBP_100_26, count_XEBP_100_26},
    {"XEBP_100.27", titles_XEBP_100_27, count_XEBP_100_27},
    {"XEBP_100.28", titles_XEBP_100_28, count_XEBP_100_28},
    {"XEBP_100.29", titles_XEBP_100_29, count_XEBP_100_29},
    {"XEBP_100.30", titles_XEBP_100_30, count_XEBP_100_30},
    {"XEBP_100.31", titles_XEBP_100_31, count_XEBP_100_31},
    {"XEBP_100.32", titles_XEBP_100_32, count_XEBP_100_32},
    {"XEBP_100.33", titles_XEBP_100_33, count_XEBP_100_33},
    {"XEBP_100.34", titles_XEBP_100_34, count_XEBP_100_34},
    {"XEBP_100.35", titles_XEBP_100_35, count_XEBP_100_35},
    {"XEBP_100.36", titles_XEBP_100_36, count_XEBP_100_36},
    {"XEBP_100.37", titles_XEBP_100_37, count_XEBP_100_37},
    {"XEBP_100.38", titles_XEBP_100_38, count_XEBP_100_38},
    {"XEBP_100.39", titles_XEBP_100_39, count_XEBP_100_39},
    {"XEBP_100.40", titles_XEBP_100_40, count_XEBP_100_40},
    {"XEBP_100.41", titles_XEBP_100_41, count_XEBP_100_41},
    {"XEBP_100.42", titles_XEBP_100_42, count_XEBP_100_42},
    {"XEBP_100.43", titles_XEBP_100_43, count_XEBP_100_43},
    {"XEBP_100.44", titles_XEBP_100_44, count_XEBP_100_44},
    {"XEBP_100.45", titles_XEBP_100_45, count_XEBP_100_45},
    {"XEBP_100.46", titles_XEBP_100_46, count_XEBP_100_46},
    {"XEBP_100.47", titles_XEBP_100_47, count_XEBP_100_47},
    {"XEBP_100.48", titles_XEBP_100_48, count_XEBP_100_48},
    {"XEBP_100.49", titles_XEBP_100_49, count_XEBP_100_49},
    {"XEBP_100.50", titles_XEBP_100_50, count_XEBP_100_50},
    {"XEBP_100.51", titles_XEBP_100_51, count_XEBP_100_51},
    {"XEBP_100.52", titles_XEBP_100_52, count_XEBP_100_52},
    {"XEBP_100.53", titles_XEBP_100_53, count_XEBP_100_53},
    {"XEBP_100.54", titles_XEBP_100_54, count_XEBP_100_54},
    {"XEBP_100.55", titles_XEBP_100_55, count_XEBP_100_55},
    {"XEBP_100.56", titles_XEBP_100_56, count_XEBP_100_56},
    {"XEBP_100.57", titles_XEBP_100_57, count_XEBP_100_57},
    {"XEBP_100.58", titles_XEBP_100_58, count_XEBP_100_58},
    {"XEBP_100.59", titles_XEBP_100_59, count_XEBP_100_59},
    {"XEBP_100.60", titles_XEBP_100_60, count_XEBP_100_60},
    {"XEBP_100.61", titles_XEBP_100_61, count_XEBP_100_61},
    {"XEBP_100.62", titles_XEBP_100_62, count_XEBP_100_62},
    {"XEBP_100.63", titles_XEBP_100_63, count_XEBP_100_63},
    {"XEBP_100.64", titles_XEBP_100_64, count_XEBP_100_64},
    {"XEBP_100.65", titles_XEBP_100_65, count_XEBP_100_65},
    {"XEBP_100.66", titles_XEBP_100_66, count_XEBP_100_66},
    {"XEBP_100.67", titles_XEBP_100_67, count_XEBP_100_67},
    {"XEBP_100.68", titles_XEBP_100_68, count_XEBP_100_68},
    {"XEBP_100.69", titles_XEBP_100_69, count_XEBP_100_69},
    {"XEBP_100.70", titles_XEBP_100_70, count_XEBP_100_70},
    {"XEBP_100.71", titles_XEBP_100_71, count_XEBP_100_71},
    {"XEBP_100.72", titles_XEBP_100_72, count_XEBP_100_72},
    {"XEBP_100.73", titles_XEBP_100_73, count_XEBP_100_73},
    {"XEBP_100.74", titles_XEBP_100_74, count_XEBP_100_74},
    {"XEBP_100.75", titles_XEBP_100_75, count_XEBP_100_75},
    {"XEBP_000.01", titles_XEBP_000_01, count_XEBP_000_01}, //Group 76
    {"XEBP_000.02", titles_XEBP_000_02, count_XEBP_000_02},
    {"XEBP_000.03", titles_XEBP_000_03, count_XEBP_000_03},
    {"XEBP_000.04", titles_XEBP_000_04, count_XEBP_000_04},
    {"XEBP_000.05", titles_XEBP_000_05, count_XEBP_000_05},
    {"XEBP_000.06", titles_XEBP_000_06, count_XEBP_000_06},
    {"XEBP_000.07", titles_XEBP_000_07, count_XEBP_000_07},
    {"XEBP_000.08", titles_XEBP_000_08, count_XEBP_000_08},
    {"XEBP_000.09", titles_XEBP_000_09, count_XEBP_000_09},
    {"XEBP_000.10", titles_XEBP_000_10, count_XEBP_000_10},
    {"XEBP_000.11", titles_XEBP_000_11, count_XEBP_000_11},
    {"XEBP_000.12", titles_XEBP_000_12, count_XEBP_000_12},
    {"XEBP_000.13", titles_XEBP_000_13, count_XEBP_000_13},
    {"XEBP_000.14", titles_XEBP_000_14, count_XEBP_000_14},
    {"XEBP_000.15", titles_XEBP_000_15, count_XEBP_000_15},
    {"XEBP_000.16", titles_XEBP_000_16, count_XEBP_000_16},
    {"XEBP_000.17", titles_XEBP_000_17, count_XEBP_000_17},
    {"XEBP_000.18", titles_XEBP_000_18, count_XEBP_000_18},
    {"XEBP_000.20", titles_XEBP_000_20, count_XEBP_000_20},
    {"XEBP_000.21", titles_XEBP_000_21, count_XEBP_000_21},
    {"XEBP_000.22", titles_XEBP_000_22, count_XEBP_000_22},
    {"XEBP_000.23", titles_XEBP_000_23, count_XEBP_000_23}};

static const size_t NUM_MEMORY_CARD_GROUPS = sizeof(allMemoryCardGroups) / sizeof(allMemoryCardGroups[0]);
const char *getGroupIdForTitleId(const char *titleId)
{
    if (titleId == NULL) {
        return NULL; // Handle NULL input gracefully
    }

    // Iterate through each main MemoryCardGroup
    for (size_t i = 0; i < NUM_MEMORY_CARD_GROUPS; ++i) {
        const MemoryCardGroup *currentGroup = &allMemoryCardGroups[i];

        // Iterate through the Title IDs within the current group
        for (size_t j = 0; j < currentGroup->titleCount; ++j) {
            // Compare the input titleId with each title in the current group
            if (strcmp(titleId, currentGroup->titleIds[j]) == 0) {
                // Found a match, return the Group ID
                return currentGroup->groupId;
            }
        }
    }

    // If no matching Group ID was found for the given Title ID, return the original Title ID
    return titleId;
}
