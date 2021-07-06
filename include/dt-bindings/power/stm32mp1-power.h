/* SPDX-License-Identifier: GPL-2.0 or BSD-3-Clause */
/*
 * Copyright (C) 2018-2019, STMicroelectronics - All Rights Reserved
 * Author: Yann Gautier <yann.gautier@st.com> for STMicroelectronics.
 */

#ifndef DT_BINDINGS_STM32MP1_POWER_H
#define DT_BINDINGS_STM32MP1_POWER_H

#define STM32_PM_CSLEEP_RUN			0
#define STM32_PM_CSTOP_ALLOW_STOP		1
#define STM32_PM_CSTOP_ALLOW_LP_STOP		2
#define STM32_PM_CSTOP_ALLOW_LPLV_STOP		3
#define STM32_PM_CSTOP_ALLOW_LPLV_STOP2		4
#define STM32_PM_CSTOP_ALLOW_STANDBY_DDR_SR	5
#define STM32_PM_CSTOP_ALLOW_STANDBY_DDR_OFF	6
#define STM32_PM_SHUTDOWN			7
#define STM32_PM_MAX_SOC_MODE			8

#endif /* DT_BINDINGS_STM32MP1_POWER_H */
