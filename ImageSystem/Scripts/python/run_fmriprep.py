#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
import subprocess
import re
import shutil
import glob
import argparse
import platform
from pathlib import Path

# ===========================
# 0. Dependency Check
# ===========================
try:
    from dcm2bids.dcm2niix_gen import Dcm2niixGen
    from dcm2bids.utils.io import write_txt
    from dcm2bids.utils.scaffold import bids_starter_kit
except ImportError:
    print("Error: 'dcm2bids' package is missing.")
    print("Please install it using: pip install dcm2bids")
    sys.exit(1)

# ===========================
# 1. Platform Helpers
# ===========================

def get_docker_cmd():
    """Returns the appropriate docker command list based on OS."""
    if platform.system().lower() == 'windows':
        return ['docker']
    return ['sudo', 'docker']

def is_in_directory(filepath, directory):
    """Check if filepath is inside directory."""
    return os.path.realpath(filepath).startswith(os.path.realpath(directory) + os.sep)

# ===========================
# 2. BIDS Conversion Logic
# ===========================

def dicom2bids(dicoms_dir: str, bids_dir: str):
    """
    Main function to orchestrate DICOM to BIDS conversion.
    Source Logic:
    """
    print(f"--- Starting DICOM to BIDS conversion ---")
    print(f"Source: {dicoms_dir}")
    print(f"Target: {bids_dir}")
    
    # 1. Create Scaffold
    dcm2bids_scaffold(bids_dir)
    
    # 2. Run dcm2niix via wrapper
    dicom2bids_helper(dicom_dir=dicoms_dir, bids_dir=bids_dir)
    
    # 3. Filter and organize files
    for modality in ["T1w", "T2w", "task-rest_bold"]:
        dicom2bids_filter(bids_dir, modality)
    
    return bids_dir

def dcm2bids_scaffold(bids_dir: str):
    """Creates the BIDS directory structure and default files."""
    for _ in ["code", "derivatives", "sourcedata"]:
        os.makedirs(os.path.join(bids_dir, _), exist_ok=True)
    
    # Write standard BIDS files using dcm2bids templates
    write_txt(os.path.join(bids_dir, "CHANGES"), bids_starter_kit.CHANGES)
    write_txt(os.path.join(bids_dir, "dataset_description.json"),
              bids_starter_kit.dataset_description.replace("BIDS_VERSION", "v1.8.0"))
    write_txt(os.path.join(bids_dir, "participants.json"), bids_starter_kit.participants_json)
    write_txt(os.path.join(bids_dir, "participants.tsv"), bids_starter_kit.participants_tsv)
    write_txt(os.path.join(bids_dir, ".bidsignore"), "tmp_dcm2bids")
    write_txt(os.path.join(bids_dir, "README"), bids_starter_kit.README)
    
    # Reset sub-01 directory
    sub_dir = os.path.join(bids_dir, "sub-01")
    if os.path.exists(sub_dir):
        shutil.rmtree(sub_dir)
    os.makedirs(sub_dir, exist_ok=True)
    os.makedirs(os.path.join(sub_dir, "anat"), exist_ok=True)
    os.makedirs(os.path.join(sub_dir, "func"), exist_ok=True)

def dicom2bids_helper(dicom_dir: str, bids_dir: str):
    """
    Runs dcm2niix to convert DICOMs to temporary NIfTI files.
    NOTE: Requires dcm2niix to be in system PATH.
    """
    helper_dir = os.path.join(bids_dir, "tmp_dcm2bids", "helper")
    # Ensure helper dir exists (though dcm2bids usually handles it)
    os.makedirs(helper_dir, exist_ok=True)
    
    try:
        app = Dcm2niixGen(dicom_dirs=[dicom_dir], bids_dir=Path(helper_dir), helper=True)
        app.run(force=True)
    except Exception as e:
        print(f"Error running dcm2niix: {e}")
        print("Ensure 'dcm2niix' is installed and in your system PATH.")
        raise e
    return bids_dir

def dicom2bids_filter(bids_dir: str, modality: str):
    """
    Filters and moves NIfTI files to their correct BIDS subfolders based on Modality.
    Logic derived from
    """
    helper_dir = os.path.join(bids_dir, "tmp_dcm2bids", "helper")
    # Use glob to find json files
    json_files = glob.glob(os.path.join(helper_dir, "*.json"))
    
    for file in json_files:
        # Check if corresponding NIfTI exists
        nii_file = file.replace("json", "nii.gz")
        if not os.path.exists(nii_file):
            continue
        
        # Logic from source: Skip ROI
        if re.search(r'ROI', file, re.I):
            continue
            
        subdir = ""
        # Filter for Anat
        if modality in ["T1w", "T2w"]:
            pattern = r'_T1' if modality == 'T1w' else r'_T2'
            pattern_3d = r'_3DT1' if modality == 'T1w' else r'_3DT2'
            subdir = "anat"
            if not re.search(pattern_3d, file, re.I) and not re.search(pattern, file, re.I):
                continue
        
        # Filter for Func
        elif modality in ["task-rest_bold"]:
            pattern_bold = r'_bold'
            pattern_rest = r'_rest'
            pattern_exclude = r'fieldmap|SB'
            
            if re.search(pattern_exclude, file, re.I):
                continue
            subdir = "func"
            if not re.search(pattern_bold, file, re.I) and not re.search(pattern_rest, file, re.I):
                continue
        
        # Move files
        dest_json = os.path.join(bids_dir, f"sub-01", subdir, f"sub-01_{modality}.json")
        dest_nii = os.path.join(bids_dir, f"sub-01", subdir, f"sub-01_{modality}.nii.gz")
        
        print(f"Moving: {os.path.basename(file)} -> sub-01/{subdir}")
        shutil.move(file, dest_json)
        shutil.move(nii_file, dest_nii)
        
    return True

# ===========================
# 3. Docker Execution Logic
# ===========================

def check_docker():
    """Verify that docker is installed and accessible."""
    cmd = get_docker_cmd() + ['version']
    try:
        ret = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except OSError as e:
        from errno import ENOENT
        if e.errno == ENOENT:
            return -1
        raise e
    if ret.stderr.startswith(b'Cannot connect to the Docker daemon.'):
        return 0
    return 1

def check_image(image):
    """Check whether image is present on local system."""
    cmd = get_docker_cmd() + ['images', '-q', image]
    ret = subprocess.run(cmd, stdout=subprocess.PIPE)
    return bool(ret.stdout)

def check_memory(image):
    """Check total memory from within a docker container."""
    cmd = get_docker_cmd() + ['run', '--rm', '--entrypoint=free', image, '-m']
    ret = subprocess.run(cmd, stdout=subprocess.PIPE)
    if ret.returncode:
        return -1

    try:
        mem = [
            line.decode().split()[1] for line in ret.stdout.splitlines() if line.startswith(b'Mem:')
        ][0]
        return int(mem)
    except IndexError:
        return -1

def run_docker(bids_dir, output_dir, freesurfer: bool = False, 
               analysis_level: str = "participant", 
               image: str = 'nipreps/fmriprep:latest', 
               fs_license_file: str = None, 
               fs_subjects_dir: str = None, 
               work_dir: str = None,
               container_name: str = "fmriprep_runner"):
    
    print("\n--- Preparing to run Docker ---")
    
    # 1. Check Environment
    check = check_docker()
    if check < 1:
        if check == -1:
            print('Error: Could not find docker command... Is it installed?')
        else:
            print("Error: Make sure you have permission to run 'docker'")
        return 1

    if not check_image(image):
        print(f'Downloading image {image}. This may take a while...')

    mem_total = check_memory(image)
    if mem_total != -1 and mem_total < 8000:
        print('Warning: <8GB of RAM is available within your Docker environment.')

    # 2. Get Docker Version
    cmd_ver = get_docker_cmd() + ['version', '--format', '{{.Server.Version}}']
    ret = subprocess.run(cmd_ver, stdout=subprocess.PIPE)
    docker_version = ret.stdout.decode('ascii').strip() if ret.returncode == 0 else "unknown"

    # 3. Build Command
    # Start with base command (no TTY to avoid "not a TTY" errors in non-interactive envs)
    command = get_docker_cmd() + ['run', '--rm', '--name', container_name, '-e', 'DOCKER_VERSION_8395080871=%s' % docker_version]
    
    # Platform specific: User Mapping
    # Windows does not use -u uid:gid mapping for Docker Desktop usually
    if platform.system().lower() != 'windows':
        try:
            command.extend(['-u', '{}'.format(os.getgid())])
        except AttributeError:
            pass

    # License File
    if fs_license_file and os.path.exists(fs_license_file):
        # Convert to absolute path for Docker mounting
        abs_license = os.path.abspath(fs_license_file)
        command.extend(['-v', '{}:/opt/freesurfer/license.txt:ro'.format(abs_license)])
    elif freesurfer:
        print("Warning: Freesurfer enabled but no license file provided/found.")

    main_args = []
    unknown_args = []
    
    # 4. Mount Directories (Critical for Windows)
    # Use os.path.abspath to ensure C:/... or D:/... format
    if bids_dir:
        abs_bids = os.path.abspath(bids_dir)
        command.extend(['-v', ':'.join((abs_bids, '/data', 'ro'))])
        main_args.append('/data')
        
    if output_dir:
        abs_out = os.path.abspath(output_dir)
        os.makedirs(abs_out, exist_ok=True)
        command.extend(['-v', ':'.join((abs_out, '/out'))])
        main_args.append('/out')
    
    main_args.append(analysis_level)

    if fs_subjects_dir:
        abs_fs = os.path.abspath(fs_subjects_dir)
        command.extend(['-v', '{}:/opt/subjects'.format(abs_fs)])
        unknown_args.extend(['--fs-subjects-dir', '/opt/subjects'])

    if work_dir:
        abs_work = os.path.abspath(work_dir)
        # Check nesting
        if bids_dir and is_in_directory(abs_work, os.path.abspath(bids_dir)):
            print('Error: Working directory cannot be a subdirectory of the input BIDS folder.')
            return 1
        command.extend(['-v', ':'.join((abs_work, '/scratch'))])
        unknown_args.extend(['-w', '/scratch'])

    if not freesurfer:
        unknown_args.append("--fs-no-reconall")

    command.append(image)
    command.extend(main_args)
    command.extend(unknown_args)

    # 5. Execute
    log_path = os.path.join(output_dir, "fmriprep-docker.log")
    print(f"Logging to: {log_path}")
    print('RUNNING COMMAND:', ' '.join(command))

    try:
        # Use utf-8 encoding for Windows log files
        with open(log_path, "w", encoding='utf-8') as file:
            ret = subprocess.run(command, stdout=file, stderr=subprocess.STDOUT, text=True, encoding='utf-8')
        return ret.returncode
    except Exception as e:
        print(f"Execution failed: {e}")
        return 1

# ===========================
# 4. Main Entry Point
# ===========================

def main():
    parser = argparse.ArgumentParser(description='Run fMRIPrep via Docker (Windows/Linux Compatible)')
    
    # Required Arguments
    parser.add_argument('--dicom_dir', required=True, type=str, help="Input directory containing DICOM files")
    parser.add_argument('--bids_dir', required=True, type=str, help="Temporary directory for BIDS structure")
    parser.add_argument('--output_dir', required=True, type=str, help="Final output directory")
    
    # Optional Arguments
    parser.add_argument('--freesurfer', action='store_true', help="Enable Freesurfer surface reconstruction")
    parser.add_argument('--analysis_level', default="participant", type=str)
    parser.add_argument('--image', default='nipreps/fmriprep:latest', type=str)
    parser.add_argument('--fs_license_file', default=None, type=str)
    parser.add_argument('--fs_subjects_dir', default=None, type=str)
    parser.add_argument('--work_dir', default=None, type=str)

    args = parser.parse_args()

    print(f"Received arguments: {args}")

    # Step 1: Convert DICOM to BIDS
    try:
        dicom2bids(args.dicom_dir, args.bids_dir)
    except Exception as e:
        print(f"CRITICAL ERROR during DICOM conversion: {e}")
        sys.exit(1)

    # Step 2: Run Docker
    exit_code = run_docker(
        bids_dir=args.bids_dir,
        output_dir=args.output_dir,
        freesurfer=args.freesurfer,
        analysis_level=args.analysis_level,
        image=args.image,
        fs_license_file=args.fs_license_file,
        fs_subjects_dir=args.fs_subjects_dir,
        work_dir=args.work_dir,
        container_name="fmriprep_runner"
    )
    
    if exit_code != 0:
        print(f"fMRIPrep Docker finished with error code: {exit_code}")
        sys.exit(exit_code)
    
    print("fMRIPrep finished successfully.")
    sys.exit(0)

if __name__ == "__main__":
    main()