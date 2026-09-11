#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/stepper/stepper.h>
#include <zephyr/drivers/stepper/stepper_ctrl.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(stepper_tmc5160, LOG_LEVEL_INF);

static const struct device *const stepper_driver = DEVICE_DT_GET(DT_ALIAS(stepper_driver));
static const struct device *const stepper_ctrl = DEVICE_DT_GET(DT_ALIAS(stepper_ctrl));

int main(void)
{
	if (!device_is_ready(stepper_driver)) {
		LOG_ERR("Stepper driver %s not ready", stepper_driver->name);
		return 0;
	}

	if (!device_is_ready(stepper_ctrl)) {
		LOG_ERR("Stepper controller %s not ready", stepper_ctrl->name);
		return 0;
	}

	LOG_INF("TMC5160A up over SPI1. Drive it from the shell, e.g.:");
	LOG_INF("  stepper enable %s", stepper_driver->name);
	LOG_INF("  stepper_ctrl move_by %s 3200", stepper_ctrl->name);
	LOG_INF("  stepper_ctrl move_to %s 0", stepper_ctrl->name);
	LOG_INF("  stepper_ctrl run %s 1", stepper_ctrl->name);
	LOG_INF("  stepper_ctrl stop %s", stepper_ctrl->name);
	LOG_INF("  stepper disable %s", stepper_driver->name);

	return 0;
}
