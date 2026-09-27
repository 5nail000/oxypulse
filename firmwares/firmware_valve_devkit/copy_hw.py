Import("env")

from pathlib import Path
import shutil

MAIN_FILES = [
    "ao02.cpp",
    "ao02.h",
    "dps310.cpp",
    "dps310.h",
    "sfm3300.cpp",
    "sfm3300.h",
    "scd41.cpp",
    "scd41.h",
    "i2c_bus.cpp",
    "i2c_bus.h",
    "gpio_controller.cpp",
    "gpio_controller.h",
    "servo_controller.cpp",
    "servo_controller.h",
    "igt_session.cpp",
    "igt_session.h",
    "sensirion_i2c.cpp",
    "sensirion_i2c.h",
]

project_dir = Path(env["PROJECT_DIR"])
main_src = project_dir.parent / "firmware_main_devkit" / "src"
hw = project_dir / "src" / "hw"
hw.mkdir(parents=True, exist_ok=True)

for name in MAIN_FILES:
    src = main_src / name
    if not src.is_file():
        raise FileNotFoundError("valve CU: missing %s" % src)
    shutil.copy2(src, hw / name)

env.Append(CPPPATH=[str(hw)])
