Import("env")
import os
import shutil

project_dir = env.subst("$PROJECT_DIR")
build_dir = env.subst("$BUILD_DIR")

# Set variables for easier access
common_data_dir = os.path.join(project_dir, "data")
merged_data_dir = os.path.join(build_dir, "merged_fs")
config_src_dir = project_dir
target_config_file = env.GetProjectOption("custom_config_file", "config.json")

# Update PlatformIO to use the merged directory
# This tells PIO to build the LittleFS image from 'merged_fs' instead of 'data'
env.Replace(PROJECT_DATA_DIR=merged_data_dir)

# Run the copy logic IMMEDIATELY (inline)
# Executing this now ensures the directory exists before the build system checks for it
print(f"--- Merging filesystem for {env['PIOENV']} ---")
print(f"Target config: {target_config_file}")

# Clean the staging directory to ensure no old files remain
if os.path.exists(merged_data_dir):
    try:
        shutil.rmtree(merged_data_dir)
    except Exception:
        # If deletion fails (e.g. file locking), just proceed
        pass

# Re-create directory
if not os.path.exists(merged_data_dir):
    os.makedirs(merged_data_dir)

# Copy the common data directory
if os.path.exists(common_data_dir):
    from distutils.dir_util import copy_tree
    copy_tree(common_data_dir, merged_data_dir)

# Copy the target config file
src_config = os.path.join(config_src_dir, target_config_file)
dst_config = os.path.join(merged_data_dir, target_config_file)

if os.path.exists(src_config):
    shutil.copy(src_config, dst_config)
    print(f"Copied {target_config_file} to filesystem")
else:
    print(f"WARNING: Config file '{src_config}' not found in firmware root!")