#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fMRIPrep Batch Runner (Modified for participants.tsv priority)
==============================================================
Directly run fMRIPrep on a pre-converted BIDS dataset.
Prioritizes subjects from participants.tsv if available.

Usage:
    python run_fmriprep.py --bids_dir C:/BIDS_Output --output_dir C:/fMRIPrep_Results --license_file license.txt
    python run_fmriprep.py ... --subjects sub-001 sub-002
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

def run_fmriprep_docker(bids_dir, output_dir,
                        image: str = 'nipreps/fmriprep:latest',
                        fs_license_file: str = None,
                        subjects: list = None,
                        skip_bids_validation: bool = False,
                        anat_only: bool = False,
                        use_syn_sdc: bool = False,
                        ignore_fieldmaps: bool = True,
                        output_spaces: str = 'MNI152NLin2009cAsym:res-2',
                        nthreads: int = None,
                        mem_mb: int = None,
                        low_mem: bool = False,
                        fs_no_reconall: bool = True):
    """
    Run fMRIPrep via Docker with GPU support.
    """
    print("\n" + "=" * 60)
    print("fMRIPrep Docker Runner")
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
        print(f"Image {image} not found locally. Will be pulled automatically (~15GB).")
    
    # Get Docker version for env var
    cmd_ver = get_docker_cmd() + ['version', '--format', '{{.Server.Version}}']
    ret = subprocess.run(cmd_ver, stdout=subprocess.PIPE, text=True)
    docker_version = ret.stdout.strip() if ret.returncode == 0 else "unknown"
    
    # Build command
    command = get_docker_cmd() + [
        'run',
        '--rm',
        '-e', f'DOCKER_VERSION_8395080871={docker_version}'
    ]
    
    # Mounts
    abs_bids = os.path.abspath(bids_dir)
    abs_output = os.path.abspath(output_dir)
    os.makedirs(abs_output, exist_ok=True)
    
    # Work directory
    work_dir = os.path.join(abs_output, 'work')
    os.makedirs(work_dir, exist_ok=True)
    
    command.extend(['-v', f'{abs_bids}:/data:ro'])
    command.extend(['-v', f'{abs_output}:/out'])
    command.extend(['-v', f'{work_dir}:/work'])
    
    if fs_license_file and os.path.exists(fs_license_file):
        abs_license = os.path.abspath(fs_license_file)
        command.extend(['-v', f'{abs_license}:/opt/freesurfer/license.txt:ro'])
    else:
        print("Error: FreeSurfer license file required but not found.")
        print("Get one at: https://surfer.nmr.mgh.harvard.edu/registration.html")
        return 1
    
    # Image & args
    command.append(image)
    command.extend(['/data', '/out', 'participant'])
    command.extend(['-w', '/work'])
    
    # Participant labels
    if subjects:
        # fMRIPrep requires single --participant-label with space-separated values
        labels = [s.replace('sub-', '').strip() for s in subjects]
        command.append('--participant-label')
        command.extend(labels)  # Add all labels as separate arguments after --participant-label
        print(f"Passing to fMRIPrep: --participant-label {' '.join(labels)}")
    else:
        print("No --participant-label -> fMRIPrep will process ALL subjects in /data")
    
    # Optional flags
    if skip_bids_validation:
        command.append('--skip-bids-validation')
    
    if anat_only:
        command.append('--anat-only')
    
    if fs_no_reconall:
        command.append('--fs-no-reconall')
    
    if ignore_fieldmaps:
        command.append('--ignore')
        command.append('fieldmaps')
    
    if use_syn_sdc:
        command.append('--use-syn-sdc')
    
    if output_spaces:
        command.extend(['--output-spaces', output_spaces])
    
    if nthreads:
        command.extend(['--nthreads', str(nthreads)])
    
    if mem_mb:
        command.extend(['--mem-mb', str(mem_mb)])
    
    if low_mem:
        command.append('--low-mem')
    
    # Log path
    log_path = os.path.join(abs_output, "fmriprep-docker.log")
    
    print(f"\nBIDS Input : {abs_bids}")
    print(f"Output     : {abs_output}")
    print(f"Work Dir   : {work_dir}")
    print(f"Log File   : {log_path}")
    print(f"Anat Only  : {anat_only}")
    print(f"FS Reconall: {not fs_no_reconall}")
    print(f"SDC        : {'syn-sdc' if use_syn_sdc else ('fieldmaps' if not ignore_fieldmaps else 'disabled')}")
    if subjects:
        print(f"Subjects   : {', '.join(subjects)} ({len(subjects)})")
    else:
        print("Subjects   : ALL in BIDS directory")
    
    print("\n" + "-" * 60)
    print("Full Docker Command:")
    print(' '.join(command))
    print("-" * 60 + "\n")
    
    print("Starting fMRIPrep... (may take hours per subject)")
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
        description='Run fMRIPrep on a BIDS dataset (prioritizes participants.tsv)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python run_fmriprep.py --bids_dir C:/BIDS --output_dir C:/Results --license_file license.txt
  # -> automatically uses subjects from participants.tsv (if exists)

  python run_fmriprep.py ... --subjects sub-001 sub-002
  # -> overrides TSV, only processes these
  
  python run_fmriprep.py ... --anat_only
  # -> only process anatomical images (faster)
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
    parser.add_argument('--image', default='nipreps/fmriprep:latest',
                        help="Docker image (default: nipreps/fmriprep:latest)")
    parser.add_argument('--subjects', nargs='+', type=str, default=None,
                        help="Specific subjects (overrides TSV)")
    parser.add_argument('--skip_bids_validation', action='store_true',
                        help="Skip BIDS validation")
    parser.add_argument('--anat_only', action='store_true',
                        help="Only process anatomical images")
    parser.add_argument('--fs_reconall', action='store_true',
                        help="Enable FreeSurfer surface reconstruction (slow, disabled by default)")
    parser.add_argument('--use_syn_sdc', action='store_true',
                        help="Use fieldmap-less SyN-based SDC")
    parser.add_argument('--use_fieldmaps', action='store_true',
                        help="Use fieldmaps for SDC (requires fmap/ in BIDS)")
    parser.add_argument('--output_spaces', default='MNI152NLin2009cAsym:res-2',
                        help="Output spaces (default: MNI152NLin2009cAsym:res-2)")
    parser.add_argument('--nthreads', type=int, default=None,
                        help="Maximum number of threads")
    parser.add_argument('--mem_mb', type=int, default=None,
                        help="Maximum memory in MB")
    parser.add_argument('--low_mem', action='store_true',
                        help="Use low memory mode")
    
    args = parser.parse_args()
    
    print("\n" + "#" * 60)
    print("#" + " " * 18 + "fMRIPrep Batch Runner" + " " * 19 + "#")
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
    print(f"  {message}")
    
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
            print(f"  - {s}")
    
    # Step 3: Run
    print("\n[Step 2] Starting fMRIPrep Docker...")
    exit_code = run_fmriprep_docker(
        bids_dir=args.bids_dir,
        output_dir=args.output_dir,
        image=args.image,
        fs_license_file=args.license_file,
        subjects=subjects_to_process,
        skip_bids_validation=args.skip_bids_validation,
        anat_only=args.anat_only,
        use_syn_sdc=args.use_syn_sdc,
        ignore_fieldmaps=not args.use_fieldmaps,
        output_spaces=args.output_spaces,
        nthreads=args.nthreads,
        mem_mb=args.mem_mb,
        low_mem=args.low_mem,
        fs_no_reconall=not args.fs_reconall
    )
    
    if exit_code != 0:
        print(f"\nfMRIPrep exited with code {exit_code}")
        print("Check:", os.path.join(os.path.abspath(args.output_dir), "fmriprep-docker.log"))
        sys.exit(exit_code)
    
    print("\n" + "#" * 60)
    print("#" + " " * 15 + "fMRIPrep Completed Successfully!" + " " * 10 + "#")
    print("#" * 60)
    print(f"\nResults in: {os.path.abspath(args.output_dir)}")
    sys.exit(0)


if __name__ == "__main__":
    main()
