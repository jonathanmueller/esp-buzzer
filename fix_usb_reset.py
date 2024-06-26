# Author : Krzysztof Strehlau [ https://github.com/cziter15 ]
# This is workaround for esp32s3 usb reset issue.
# see https://github.com/espressif/arduino-esp32/issues/6762

Import("env")

esptool_dir = env.PioPlatform().get_package_dir("tool-esptoolpy")

def run_command(command_args):
	print(">>> esptool.py --no-stub " + str(command_args))
	try: env.Execute(f"python {esptool_dir}/esptool.py --no-stub " + command_args)
	except: pass

# Do the job only for esptool
if env["UPLOAD_PROTOCOL"] == "esptool":
	# Define the actions
	# before_upload = lambda *a, **b: run_command("-b 1200 --connect-attempts 1 --before usb_reset --after no_reset read_mac") # just touch
	after_upload = lambda *a, **b: run_command("--before no_reset write_mem 0x6000812C 0") # modify register

	# Bind upload pre and post actions
	# env.AddPreAction("upload", before_upload)
	env.AddPostAction("upload", after_upload)