#pragma once
#include "sdkconfig.h"
#ifdef CONFIG_CI_BENCH_50HZ
#define CI_REPORT_PERIOD_US 20000
#define CI_RATE_HZ 50
#else
#define CI_REPORT_PERIOD_US 10000
#define CI_RATE_HZ 100
#endif
