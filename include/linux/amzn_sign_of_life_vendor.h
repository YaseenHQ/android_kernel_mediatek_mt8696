/*
 * amzn_sign_of_life_rtc.h
 *
 * Copyright 2022 Amazon Technologies, Inc. All Rights Reserved.
 *
 * The code contained herein is licensed under the GNU General Public
 * License Version 2.
 * You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __AMZN_SIGN_OF_LIFE_VENDOR_H
#define __AMZN_SIGN_OF_LIFE_VENDOR_H

/*
 * android boot reason charaters.
 */
#define POWER_KEY_BOOT		"=PowerKey"
#define USB_BOOT		"=usb"
#define WATCHDOG_SW		"=Watchdog"
#define WATCHDOG_HW		"=wdt_hw"
#define TWO_SEC_REBOOT		"=2sec_reboot"
#define POWER_LOSS		"=power_loss"
#define OVER_CUR_PRO		"=ocp"
#define KERNEL_PANIC		"=kernel_panic"


#define lcr_rtc_lock		mtk_rtc_acquire_lock
#define lcr_rtc_unlock		mtk_rtc_release_lock

#define lcr_rtc_write		mtk_rtc_write
#define lcr_rtc_read		mtk_rtc_read
#define lcr_rtc_write_trigger	mtk_rtc_write_trigger_out

/*
 * Reboot reasons of rc_wdt_status
 * These are copied from preloader-mtk/mt8696/platform/mt8696/src/drivers/inc/wdt.h
 */
#define RE_BOOT_REASON_UNKNOW		(0x00)
#define RE_BOOT_BY_WDT_HW		(0x01)
#define RE_BOOT_BY_WDT_SW		(0x02)
#define RE_BOOT_WITH_INTTERUPT		(0x04)
#define RE_BOOT_BY_SPM_THERMAL		(0x08)
#define RE_BOOT_BY_SPM			(0x10)
#define RE_BOOT_BY_THERMAL_DIRECT	(0x20)
#define RE_BOOT_BY_DEBUG		(0x40)
#define RE_BOOT_BY_SECURITY		(0x80)
#define RE_BOOT_ABNORMAL		(0xF0)

/* copy from mt-plat/mboot_params.h */
enum AEE_EXP_TYPE_NUM {
	AEE_EXP_TYPE_HWT = 1,
	AEE_EXP_TYPE_KE = 2,
	AEE_EXP_TYPE_NESTED_PANIC = 3,
	AEE_EXP_TYPE_SMART_RESET = 4,
	AEE_EXP_TYPE_HANG_DETECT = 5,
};

#endif
