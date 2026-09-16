
# Activate the virtual Env

cd /home/user/work/zephyr_dev
source .venv/bin/activate

# Easy Configuration
west build -t menuconfig

west build -b nucleo_n657x0_q -t menuconfig