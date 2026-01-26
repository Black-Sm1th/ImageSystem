#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DeepPrep Batch Runner (Modified for participants.tsv priority)
============================================================
Directly run DeepPrep on a pre-converted BIDS dataset.
Prioritizes subjects from participants.tsv if available.

Usage:
    python run_deepprep.py --bids_dir C:/BIDS_Output --output_dir C:/DeepPrep_Results --license_file license.txt
    python run_deepprep.py ... --subjects sub-001 sub-002
"""
import sys
import os
import subprocess
import argparse
import platform
from pathlib import Path

try:
    import pandas as pd
except ImportError:
    pd = None
    print("Warning: pandas not installed. Cannot read participants.tsv automatically.")

# ===========================
# 1. Platform Helpers
# ===========================
def get_docker_cmd():
    """Returns the appropriate docker command list based on OS."""
    if platform.system().lower() == 'windows':
        return ['docker']
    return ['sudo', 'docker']

# ===========================
# 2. BIDS Validation & Subject Scanning
# ===========================
def scan_bids_subjects(bids_dir: str):
    """
    Scan BIDS directory and return list of subject IDs (with 'sub-' prefix).
    """
    bids_path = Path(bids_dir)
    
    if not bids_path.exists():
        print(f"Error: BIDS directory not found: {bids_dir}")
        return []
    
    subjects = sorted([d.name for d in bids_path.iterdir()
                       if d.is_dir() and d.name.startswith('sub-')])
    
    return subjects

def validate_bids_structure(bids_dir: str):
    """
    Basic validation of BIDS structure.
    Returns (is_valid, message, subject_count)
    """
    bids_path = Path(bids_dir)
    
    if not bids_path.exists():
        return False, f"BIDS directory not found: {bids_dir}", 0
    
    desc_file = bids_path / "dataset_description.json"
    if not desc_file.exists():
        print("Warning: dataset_description.json not found (optional but recommended)")
    
    subjects = scan_bids_subjects(bids_dir)
    
    if len(subjects) == 0:
        return False, "No subjects (sub-*) found in BIDS directory", 0
    
    valid_subjects = []
    for sub in subjects:
        sub_path = bids_path / sub
        anat_path = sub_path / "anat"
        
        t1_files = list(anat_path.glob("*_T1w.nii.gz")) if anat_path.exists() else []
        
        if len(t1_files) > 0:
            valid_subjects.append(sub)
        else:
            print(f"Warning: {sub} has no T1w file, will be skipped")
    
    if len(valid_subjects) == 0:
        return False, "No valid subjects with T1w files found", 0
    
    return True, f"Found {len(valid_subjects)} valid subjects with T1w", len(valid_subjects)

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
    if ret.stderr and b'Cannot connect to the Docker daemon' in ret.stderr:
        return 0
    return 1

def check_image(image):
    """Check whether image is present on local system."""
    cmd = get_docker_cmd() + ['images', '-q', image]
    ret = subprocess.run(cmd, stdout=subprocess.PIPE)
    return bool(ret.stdout)

def run_deepprep_docker(bids_dir, output_dir,
                        image: str = 'pbfslab/deepprep:25.1.1.cuda129',
                        fs_license_file: str = None,
                        bold_task_type: str = 'rest',
                        bold_sdc: bool = True,
                        subjects: list = None,
                        skip_bids_validation: bool = False,
                        anat_only: bool = False,
                        bold_only: bool = False,
                        device: str = 'auto',
                        resume: bool = False):
    """
    Run DeepPrep via Docker with GPU support.
    """
    print("\n" + "=" * 60)
    print("DeepPrep Docker Runner")
    print("=" * 60)
    
    # Check Docker
    docker_ok = check_docker()
    if docker_ok < 1:
        if docker_ok == -1:
            print('Error: Docker command not found. Is Docker installed?')
        else:
            print("Error: Cannot connect to Docker daemon. Is it running?")
        return 1
    
    if not check_image(image):
        print(f"Image {image} not found locally. Will be pulled automatically (~10GB).")
    
    # Get Docker version for env var
    cmd_ver = get_docker_cmd() + ['version', '--format', '{{.Server.Version}}']
    ret = subprocess.run(cmd_ver, stdout=subprocess.PIPE, text=True)
    docker_version = ret.stdout.strip() if ret.returncode == 0 else "unknown"
    
    # Build command
    command = get_docker_cmd() + [
        'run',
        '--rm',
        '--gpus', 'all',
        '-e', f'DOCKER_VERSION_8395080871={docker_version}'
    ]
    
    # Mounts
    abs_bids = os.path.abspath(bids_dir)
    abs_output = os.path.abspath(output_dir)
    os.makedirs(abs_output, exist_ok=True)
    
    command.extend(['-v', f'{abs_bids}:/input'])
    command.extend(['-v', f'{abs_output}:/output'])
    
    if fs_license_file and os.path.exists(fs_license_file):
        abs_license = os.path.abspath(fs_license_file)
        command.extend(['-v', f'{abs_license}:/fs_license.txt:ro'])
    else:
        print("Error: FreeSurfer license file required but not found.")
        print("Get one at: https://surfer.nmr.mgh.harvard.edu/registration.html")
        return 1
    
    # Image & args
    command.append(image)
    command.extend(['/input', '/output', 'participant'])
    command.extend(['--fs_license_file', '/fs_license.txt'])
    command.extend(['--bold_task_type', bold_task_type])
    command.extend(['--bold_sdc', str(bold_sdc).lower()])
    command.extend(['--device', device])
    
    if subjects:
        # Remove 'sub-' prefix for DeepPrep
        # DeepPrep expects space-separated IDs as a single string: 'sub-001 sub-002'
        labels = [s.replace('sub-', '').strip() for s in subjects]
        labels_str = ' '.join(labels)
        command.extend(['--participant_label', labels_str])
        print(f"Passing to DeepPrep: --participant_label '{labels_str}'")
    else:
        print("No --participant_label -> DeepPrep will process ALL subjects in /input")
    
    if skip_bids_validation:
        command.append('--skip_bids_validation')
    
    if anat_only:
        command.append('--anat_only')
    
    if bold_only:
        command.append('--bold_only')
    
    if resume:
        command.append('--resume')
    
    # Log path
    log_path = os.path.join(abs_output, "deepprep-docker.log")
    
    print(f"\nBIDS Input : {abs_bids}")
    print(f"Output     : {abs_output}")
    print(f"Log File   : {log_path}")
    print(f"Task Type  : {bold_task_type}")
    print(f"SDC        : {bold_sdc}")
    if subjects:
        print(f"Subjects   : {', '.join(subjects)} ({len(subjects)})")
    else:
        print("Subjects   : ALL in BIDS directory")
    
    print("\n" + "-" * 60)
    print("Full Docker Command:")
    print(' '.join(command))
    print("-" * 60 + "\n")
    
    print("Starting DeepPrep... (may take hours per subject)")
    print("Progress in:", log_path, "\n")
    
    try:
        with open(log_path, "w", encoding='utf-8') as f:
            ret = subprocess.run(command, stdout=f, stderr=subprocess.STDOUT,
                               text=True, encoding='utf-8')
        return ret.returncode
    except KeyboardInterrupt:
        print("\nInterrupted by user!")
        return 130
    except Exception as e:
        print(f"Execution failed: {e}")
        return 1

# ===========================
# 4. Main Entry Point
# ===========================
def main():
    parser = argparse.ArgumentParser(
        description='Run DeepPrep on a BIDS dataset (prioritizes participants.tsv)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python run_deepprep.py --bids_dir C:/BIDS --output_dir C:/Results --license_file license.txt
  # -> automatically uses subjects from participants.tsv (if exists)

  python run_deepprep.py ... --subjects sub-001 sub-002
  # -> overrides TSV, only processes these
        """
    )
    
    # Required
    parser.add_argument('--bids_dir', required=True, type=str,
                        help="BIDS dataset directory")
    parser.add_argument('--output_dir', required=True, type=str,
                        help="Output directory")
    parser.add_argument('--license_file', required=True, type=str,
                        help="FreeSurfer license file path")
    
    # Optional
    parser.add_argument('--image', default='pbfslab/deepprep:25.1.1.cuda129',
                        help="Docker image")
    parser.add_argument('--bold_task_type', default='rest',
                        help="BOLD task type")
    parser.add_argument('--bold_sdc', action='store_true', default=True,
                        help="Enable susceptibility distortion correction (default: True)")
    parser.add_argument('--no_bold_sdc', action='store_true',
                        help="Disable susceptibility distortion correction")
    parser.add_argument('--subjects', nargs='+', type=str, default=None,
                        help="Specific subjects (overrides TSV)")
    parser.add_argument('--skip_bids_validation', action='store_true',
                        help="Skip BIDS validation")
    parser.add_argument('--anat_only', action='store_true',
                        help="Only process anatomical images")
    parser.add_argument('--bold_only', action='store_true',
                        help="Only process functional images (requires Recon files)")
    parser.add_argument('--device', default='auto',
                        help="Device: auto, 0, 1, ..., or cpu (default: auto)")
    parser.add_argument('--resume', action='store_true',
                        help="Resume from last exit point")
    
    args = parser.parse_args()
    
    print("\n" + "#" * 60)
    print("#" + " " * 18 + "DeepPrep Batch Runner" + " " * 19 + "#")
    print("#" * 60 + "\n")
    
    # License check
    if not os.path.exists(args.license_file):
        print(f"Error: License file not found: {args.license_file}")
        sys.exit(1)
    
    # Step 1: Validate BIDS & get all subjects
    print("[Step 1] Validating BIDS dataset...")
    is_valid, message, _ = validate_bids_structure(args.bids_dir)
    if not is_valid:
        print(f"Error: {message}")
        sys.exit(1)
    print(f" {message}")
    
    all_subjects = scan_bids_subjects(args.bids_dir)
    print(f"\nSubjects found in BIDS directory: {len(all_subjects)}")
    
    # Step 2: Determine subjects to process
    participants_file = Path(args.bids_dir) / "participants.tsv"
    use_tsv = False
    tsv_subjects = []
    
    if participants_file.exists() and pd is not None:
        try:
            df = pd.read_csv(participants_file, sep='\t', dtype=str)
            if 'participant_id' in df.columns:
                tsv_subjects = df['participant_id'].dropna().unique().tolist()
                # Normalize format, ensure starts with sub-
                tsv_subjects = [s if s.startswith('sub-') else f"sub-{s}" for s in tsv_subjects]
                use_tsv = True
                print(f"Found participants.tsv -> {len(tsv_subjects)} subjects")
            else:
                print("Warning: participants.tsv has no 'participant_id' column")
        except Exception as e:
            print(f"Warning: Cannot read participants.tsv: {e}")
    else:
        if not participants_file.exists():
            print("participants.tsv not found -> will process all subjects")
        else:
            print("pandas not installed -> cannot read TSV automatically")
    
    # Determine final subject list to process
    if args.subjects:
        subjects_to_process = args.subjects
        print(f"\nUsing command-line --subjects ({len(subjects_to_process)}):")
    elif use_tsv:
        subjects_to_process = tsv_subjects
        print(f"\nUsing subjects from participants.tsv ({len(subjects_to_process)}):")
    else:
        subjects_to_process = None
        print(f"\nNo specific list -> processing ALL {len(all_subjects)} subjects")
    
    if subjects_to_process:
        # Filter out subjects not present in BIDS directory
        valid = [s for s in subjects_to_process if s in all_subjects]
        if len(valid) < len(subjects_to_process):
            print(f"Warning: {len(subjects_to_process)-len(valid)} subjects from list not found in BIDS")
        subjects_to_process = valid
        for s in subjects_to_process:
            print(f" - {s}")
    
    # Determine bold_sdc value (--no_bold_sdc overrides --bold_sdc)
    bold_sdc = not args.no_bold_sdc
    
    # Step 3: Run
    print("\n[Step 2] Starting DeepPrep Docker...")
    exit_code = run_deepprep_docker(
        bids_dir=args.bids_dir,
        output_dir=args.output_dir,
        image=args.image,
        fs_license_file=args.license_file,
        bold_task_type=args.bold_task_type,
        bold_sdc=bold_sdc,
        subjects=subjects_to_process,
        skip_bids_validation=args.skip_bids_validation,
        anat_only=args.anat_only,
        bold_only=args.bold_only,
        device=args.device,
        resume=args.resume
    )
    
    if exit_code != 0:
        print(f"\nDeepPrep exited with code {exit_code}")
        print("Check:", os.path.join(os.path.abspath(args.output_dir), "deepprep-docker.log"))
        sys.exit(exit_code)
    
    print("\n" + "#" * 60)
    print("#" + " " * 15 + "DeepPrep Completed Successfully!" + " " * 10 + "#")
    print("#" * 60)
    print(f"\nResults in: {os.path.abspath(args.output_dir)}")
    sys.exit(0)


if __name__ == "__main__":
    main()