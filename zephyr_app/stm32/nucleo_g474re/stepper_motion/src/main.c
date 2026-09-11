/*
 * STM32G474RE Nucleo + ADI/Trinamic TMC5160 Motion Controller
 * FUYU FSK30 Auto-Reversing Stall-Sensing Sweep Controller (StallGuard2)
 */

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/stepper/stepper.h>
#include <zephyr/drivers/stepper/stepper_ctrl.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include "../drivers/stepper/adi_tmc/tmc51xx/tmc51xx.h"

LOG_MODULE_REGISTER(stepper_motion, LOG_LEVEL_INF);

/* 
 * FUYU FSK30 (T12 Lead Screw - 12mm lead per revolution):
 * 200 full steps/rev * 256 usteps = 51,200 usteps/rev
 * Resolution: 51,200 / 12 mm = 4,267 usteps/mm
 */
#define USTEPS_PER_MM        4267

/* Target large stroke so motor sweeps until it hits the end stop */
#define MAX_FORWARD_TRAVEL   (350 * USTEPS_PER_MM)   /* 350 mm forward search */
#define MAX_REVERSE_TRAVEL   (-350 * USTEPS_PER_MM)  /* 350 mm reverse search */

#define TURNAROUND_DWELL_MS  300       /* Settling pause at turnaround */
#define POLL_INTERVAL_MS     50

static const struct device *stepper_driver = DEVICE_DT_GET(DT_ALIAS(stepper_driver));
static const struct device *stepper_ctrl   = DEVICE_DT_GET(DT_ALIAS(stepper_ctrl));
static const struct device *tmc_dev        = DEVICE_DT_GET(DT_NODELABEL(tmc5160));

static void dump_tmc5160_status(void)
{
	uint32_t gconf = 0, gstat = 0, rampstat = 0, drv_status = 0;
	uint32_t vactual = 0, vmax = 0, xactual = 0, xtarget = 0, rampmode = 0;

	tmc51xx_read(tmc_dev, 0x00, &gconf);
	tmc51xx_read(tmc_dev, 0x01, &gstat);
	tmc51xx_read(tmc_dev, 0x20, &rampmode);
	tmc51xx_read(tmc_dev, 0x21, &xactual);
	tmc51xx_read(tmc_dev, 0x22, &vactual);
	tmc51xx_read(tmc_dev, 0x27, &vmax);
	tmc51xx_read(tmc_dev, 0x2D, &xtarget);
	tmc51xx_read(tmc_dev, 0x35, &rampstat);
	tmc51xx_read(tmc_dev, 0x6F, &drv_status);

	LOG_INF("--- TMC5160 Diagnostics ---");
	LOG_INF("  GCONF=0x%08X  GSTAT=0x%08X  RAMPMODE=%u", gconf, gstat, rampmode);
	LOG_INF("  VMAX=%u  VACTUAL=%d  XACTUAL=%d  XTARGET=%d", vmax, (int32_t)vactual, (int32_t)xactual, (int32_t)xtarget);
	LOG_INF("  RAMPSTAT=0x%08X  DRV_STATUS=0x%08X", rampstat, drv_status);
}

static void sweep_with_stallguard(int32_t target_usteps, const char *dir_name)
{
	int32_t current_pos = 0;
	uint32_t rampstat = 0, drv_status = 0;
	bool moving = true;
	int ret;

	LOG_INF(">> Sweeping %s (moving towards endstop)...", dir_name);

	ret = stepper_enable(stepper_driver);
	if (ret != 0) {
		LOG_ERR("Failed to enable stepper driver (%d)", ret);
		return;
	}

	/* Clear previous stall event in RAMPSTAT */
	tmc51xx_read(tmc_dev, 0x35, &rampstat);

	/* Command move towards boundary */
	ret = stepper_ctrl_move_to(stepper_ctrl, target_usteps);
	if (ret != 0) {
		LOG_ERR("Failed to command move_to (ret=%d)", ret);
		return;
	}

	/* Wait 500ms to allow motor to ramp up to cruising speed */
	k_msleep(500);

	for (int i = 0; i < 400; i++) {
		k_msleep(POLL_INTERVAL_MS);
		stepper_ctrl_get_actual_position(stepper_ctrl, &current_pos);
		stepper_ctrl_is_moving(stepper_ctrl, &moving);

		/* Read real-time StallGuard load & flags */
		tmc51xx_read(tmc_dev, 0x35, &rampstat);
		tmc51xx_read(tmc_dev, 0x6F, &drv_status);

		uint16_t sg_load = drv_status & 0x3FF;
		bool sg_event = (rampstat & BIT(6)) || (drv_status & BIT(24));

		if (i % 10 == 0) {
			LOG_INF("  Pos: %d mm (usteps: %d) | Load(SG): %u | Moving: %d",
			        current_pos / USTEPS_PER_MM, current_pos, sg_load, moving);
		}

		/* Hardware stall event or stopped moving while in flight */
		if (sg_event || (!moving && i > 5)) {
			LOG_INF(">>> [ENDSTOP DETECTED] Hit physical stop at %d mm (usteps: %d, SG: %u)! Reversing...",
			        current_pos / USTEPS_PER_MM, current_pos, sg_load);
			break;
		}
	}

	/* Stop motion and zero reference at this end */
	stepper_ctrl_stop(stepper_ctrl);
	k_msleep(100);
	stepper_ctrl_set_reference_position(stepper_ctrl, 0);

	if (TURNAROUND_DWELL_MS > 0) {
		k_msleep(TURNAROUND_DWELL_MS);
	}
}

int main(void)
{
	int ret;
	uint32_t cycle = 0;

	/* Power-on stabilization delay */
	k_msleep(1000);

	LOG_INF("=========================================================");
	LOG_INF(" FUYU FSK30 Auto-Reversing Stall-Sensing Sweep Controller");
	LOG_INF("=========================================================");

	if (!device_is_ready(stepper_driver) || !device_is_ready(stepper_ctrl)) {
		LOG_ERR("Stepper devices not ready!");
		return -ENODEV;
	}

	/* Enable motor driver stage */
	ret = stepper_enable(stepper_driver);
	if (ret != 0) {
		LOG_ERR("Failed to enable stepper driver (%d)", ret);
		return ret;
	}
	LOG_INF("Stepper driver stage enabled.");

	/* Configure TCOOLTHRS (0x14) = 0xFFFFF so StallGuard is active at all speeds */
	tmc51xx_write(tmc_dev, 0x14, 0x000FFFFF);

	/* Set StallGuard2 threshold SGT = 12 (high sensitivity for lightweight FSK30 stage) */
	int8_t sgt = 12;
	uint32_t coolconf = ((sgt & 0x7F) << 16) | BIT(24);
	tmc51xx_write(tmc_dev, 0x6D, coolconf);

	/* Configure Hardware StallGuard Stop-on-Stall (SWMODE bit 10) */
	uint32_t swmode = 0;
	tmc51xx_read(tmc_dev, 0x34, &swmode);
	swmode |= BIT(10);  /* sg_stop = 1: hardware auto-stop on stall */
	tmc51xx_write(tmc_dev, 0x34, swmode);

	/* Zero coordinate reference at startup position */
	stepper_ctrl_set_reference_position(stepper_ctrl, 0);

	dump_tmc5160_status();

	k_msleep(500);

	/* Continuous sweep: back and forth between physical rail ends */
	while (1) {
		cycle++;
		LOG_INF("--- Continuous Stall-Sensing Sweep Cycle #%u ---", cycle);

		/* 1. Sweep forward until hitting the far endstop */
		sweep_with_stallguard(MAX_FORWARD_TRAVEL, "FORWARD >>");

		/* 2. Sweep backward until hitting the near endstop */
		sweep_with_stallguard(MAX_REVERSE_TRAVEL, "<< REVERSE");
	}

	return 0;
}
