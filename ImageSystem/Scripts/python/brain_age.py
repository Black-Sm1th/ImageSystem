"""
一个简易的 Windows 端辅助脚本：
1) 接收一个或多个 DICOM 文件夹或 .nii/.nii.gz 文件/文件夹作为输入；
2) 如有需要，将 DICOM 转为 .nii.gz，并确保符合容器要求的目录结构；
3) 绑定临时工作目录到容器的 /usr/data，调用已构建好的 deepbrain 镜像完成推理；
4) 将输出的年龄预测结果复制到指定位置并打印。

依赖项：
- 已安装 Docker（并已构建/拉取 deepbrain 镜像）；
- 若输入为 DICOM，需要本机可执行的 dcm2niix（https://github.com/rordenlab/dcm2niix）；
- 若 DICOM 为 JPEG 压缩格式，需要 pydicom 及解码库：pip install pydicom pylibjpeg pylibjpeg-libjpeg

用法示例：
python brain_age.py --input E:/my_dcm_folder --output E:/result/Prediction.csv --preprocess
python brain_age.py --input E:/dcm_folder1 E:/dcm_folder2 --output E:/result/Prediction.csv --preprocess
python brain_age.py --input E:/dcm_folder1 E:/dcm_folder2 --ids patient001 patient002 --output E:/result/Prediction.csv --preprocess
"""

import argparse
import csv
import gzip
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

# JPEG 压缩的 DICOM Transfer Syntax UIDs
COMPRESSED_TRANSFER_SYNTAXES = {
    "1.2.840.10008.1.2.4.50",   # JPEG Baseline
    "1.2.840.10008.1.2.4.51",   # JPEG Extended
    "1.2.840.10008.1.2.4.57",   # JPEG Lossless
    "1.2.840.10008.1.2.4.70",   # JPEG Lossless SV1
    "1.2.840.10008.1.2.4.80",   # JPEG-LS Lossless
    "1.2.840.10008.1.2.4.81",   # JPEG-LS Near Lossless
    "1.2.840.10008.1.2.4.90",   # JPEG 2000 Lossless
    "1.2.840.10008.1.2.4.91",   # JPEG 2000
}

DEFAULT_MODEL = Path("model/DBN_model.h5")


def run_cmd(cmd: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess:
    """运行子进程并在失败时抛出异常。"""
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"命令失败: {' '.join(cmd)}\nstdout: {result.stdout}\nstderr: {result.stderr}"
        )
    return result


def ensure_docker_image(image: str) -> None:
    """检查镜像是否存在。"""
    result = subprocess.run(
        ["docker", "image", "inspect", image],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"未找到镜像 {image}，请先构建或 docker pull。docker stderr:\n{result.stderr}"
        )


def compress_nii_to_gz(src: Path, dst_dir: Path) -> Path:
    """将 .nii 压缩为 .nii.gz。"""
    dst = dst_dir / f"{src.stem}.nii.gz"
    with open(src, "rb") as f_src, gzip.open(dst, "wb") as f_dst:
        shutil.copyfileobj(f_src, f_dst)
    return dst


def is_compressed_dicom(dcm_path: Path) -> bool:
    """检查 DICOM 文件是否使用了压缩传输语法。"""
    try:
        import pydicom
        ds = pydicom.dcmread(dcm_path, stop_before_pixels=True)
        transfer_syntax = str(ds.file_meta.TransferSyntaxUID)
        return transfer_syntax in COMPRESSED_TRANSFER_SYNTAXES
    except Exception:
        return False


def decompress_dicom_folder(dicom_dir: Path, output_dir: Path) -> Path:
    """
    将压缩的 DICOM 文件解压到新目录。
    返回解压后的目录路径（如果无需解压则返回原目录）。
    """
    try:
        import pydicom
        from pydicom.uid import ExplicitVRLittleEndian
    except ImportError:
        raise RuntimeError(
            "需要安装 pydicom 及解码库来处理压缩的 DICOM 文件：\n"
            "pip install pydicom pylibjpeg pylibjpeg-libjpeg"
        )

    dcm_files = sorted(dicom_dir.rglob("*.dcm"))
    if not dcm_files:
        # 尝试查找没有扩展名的 DICOM 文件
        dcm_files = [f for f in dicom_dir.iterdir() if f.is_file()]
    
    if not dcm_files:
        return dicom_dir

    # 检查第一个文件是否需要解压
    if not is_compressed_dicom(dcm_files[0]):
        print(f"  DICOM 文件无需解压: {dicom_dir.name}")
        return dicom_dir

    print(f"  检测到压缩的 DICOM 文件，正在解压: {dicom_dir.name}")
    output_dir.mkdir(parents=True, exist_ok=True)
    
    decompressed_count = 0
    for dcm_file in dcm_files:
        try:
            ds = pydicom.dcmread(dcm_file)
            # 解压像素数据
            ds.decompress()
            # 设置为未压缩的传输语法
            ds.file_meta.TransferSyntaxUID = ExplicitVRLittleEndian
            # 保存到新位置
            output_path = output_dir / dcm_file.name
            ds.save_as(output_path)
            decompressed_count += 1
        except Exception as e:
            print(f"  警告: 无法解压文件 {dcm_file.name}: {e}")
            # 尝试直接复制原文件
            shutil.copyfile(dcm_file, output_dir / dcm_file.name)
    
    print(f"  已解压 {decompressed_count}/{len(dcm_files)} 个文件")
    return output_dir


def convert_dicom_to_nifti(dicom_dir: Path, work_dir: Path) -> list[Path]:
    """使用 dcm2niix 将 DICOM 转换为 .nii.gz，自动处理压缩的 DICOM。"""
    out_dir = work_dir / "dcm2niix_out"
    out_dir.mkdir(parents=True, exist_ok=True)
    
    # 先尝试解压 DICOM 文件（如果需要）
    decompressed_dir = work_dir / "decompressed_dicom"
    actual_dicom_dir = decompress_dicom_folder(dicom_dir, decompressed_dir)
    
    try:
        run_cmd(
            [
                "dcm2niix",
                "-z",
                "y",  # 压缩输出
                "-f",
                "%p_%s",  # 文件名：序列名_序列号
                "-o",
                str(out_dir),
                str(actual_dicom_dir),
            ]
        )
    except FileNotFoundError as exc:  # pragma: no cover - 环境相关
        raise RuntimeError(
            "未检测到 dcm2niix，请安装后重试：https://github.com/rordenlab/dcm2niix"
        ) from exc

    nifti_files = sorted(out_dir.rglob("*.nii.gz"))
    if not nifti_files:
        raise RuntimeError("dcm2niix 未生成任何 .nii.gz 文件，请检查 DICOM 输入。")
    return nifti_files


def gather_nifti_inputs(input_path: Path, temp_dir: Path) -> list[Path]:
    """
    将输入转换/收集为 .nii.gz 列表。
    - 单个 .nii.gz 直接拷贝
    - 单个 .nii 先压缩
    - 目录下若有 .nii/.nii.gz，全部拷贝
    - 目录下若含 DICOM（.dcm），调用 dcm2niix 转换
    """
    temp_dir.mkdir(parents=True, exist_ok=True)
    if input_path.is_file():
        if input_path.name.endswith(".nii.gz"):
            dst = temp_dir / input_path.name
            shutil.copyfile(input_path, dst)
            return [dst]
        if input_path.suffix == ".nii":
            return [compress_nii_to_gz(input_path, temp_dir)]
        raise RuntimeError("仅支持 .nii 或 .nii.gz 文件，若为 DICOM 请传入目录。")

    if not input_path.is_dir():
        raise RuntimeError("输入路径不存在，或不是文件/文件夹。")

    nii_gz_files = sorted(input_path.rglob("*.nii.gz"))
    nii_files = sorted(input_path.rglob("*.nii"))
    dcm_files = sorted(input_path.rglob("*.dcm"))

    outputs: list[Path] = []
    if nii_gz_files or nii_files:
        for f in nii_gz_files:
            dst = temp_dir / f.name
            shutil.copyfile(f, dst)
            outputs.append(dst)
        for f in nii_files:
            outputs.append(compress_nii_to_gz(f, temp_dir))
        return outputs

    if dcm_files:
        # 将整个目录视为一个序列集合交给 dcm2niix
        return convert_dicom_to_nifti(input_path, temp_dir)

    raise RuntimeError("目录中未找到 .nii/.nii.gz/.nii 或 .dcm 文件。")


def prepare_workdir(
    nifti_files: list[Path],
    model_path: Path,
    base_dir: Path,
) -> tuple[Path, str]:
    """
    构建容器所需目录结构并返回 (工作目录, 模型文件名)。
    """
    image_dir = base_dir / "ImageData"
    output_dir = base_dir / "Output"
    models_dir = base_dir / "Models"
    image_dir.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(parents=True, exist_ok=True)
    models_dir.mkdir(parents=True, exist_ok=True)

    # 使用字典跟踪文件名，避免重复覆盖
    used_names: dict[str, int] = {}
    for f in nifti_files:
        base_name = f.stem  # 去掉 .nii.gz
        if base_name.endswith(".nii"):
            base_name = base_name[:-4]  # 处理 .nii.gz 的情况
        
        # 检查是否有重复文件名
        if f.name in used_names:
            used_names[f.name] += 1
            # 添加序号后缀来区分
            new_name = f"{base_name}_{used_names[f.name]}.nii.gz"
        else:
            used_names[f.name] = 0
            new_name = f.name
        
        shutil.copyfile(f, image_dir / new_name)

    model_path = model_path.resolve()
    if not model_path.exists():
        raise RuntimeError(f"未找到模型文件: {model_path}")
    model_dst = models_dir / model_path.name
    if model_dst.resolve() != model_path:
        shutil.copyfile(model_path, model_dst)

    return base_dir, model_dst.name


def run_container(
    workdir: Path,
    model_name: str,
    image: str,
    output_name: str,
    preprocess: bool,
    threshold: float | None,
) -> Path:
    """调用 deepbrain 容器并返回输出 csv 路径。"""
    cmd = [
        "docker",
        "run",
        "--rm",
        "-v",
        f"{workdir}:/usr/data",
        image,
        "-o",
        output_name,
        "-m",
        model_name,
    ]
    if preprocess:
        cmd.append("-p")
    if threshold is not None:
        cmd.extend(["-t", str(threshold)])

    run_cmd(cmd)
    return workdir / "Output" / output_name


def read_prediction(csv_path: Path) -> list[dict[str, str]]:
    """读取预测 CSV，返回行列表。"""
    with open(csv_path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        return list(reader)


def main() -> None:
    parser = argparse.ArgumentParser(description="DeepBrainNet Docker 推理助手")
    parser.add_argument(
        "--input",
        required=True,
        nargs="+",
        help="一个或多个 DICOM 文件夹，或 .nii/.nii.gz 文件/文件夹路径",
    )
    parser.add_argument(
        "--ids",
        nargs="+",
        default=None,
        help="自定义 ID 列表，与 --input 一一对应（不指定则使用输入文件夹名）",
    )
    parser.add_argument(
        "--output",
        default="Prediction.csv",
        help="输出 CSV 路径（默认 ./Prediction.csv）",
    )
    parser.add_argument(
        "--model",
        default=None,
        help="模型文件路径（默认使用 model/DBN_model.h5）",
    )
    parser.add_argument(
        "--docker-image",
        default="deepbrain",
        help="Docker 镜像名（默认 deepbrain）",
    )
    parser.add_argument(
        "--preprocess",
        action="store_true",
        help="对原始扫描执行去颅骨+配准；原始 DICOM 建议开启",
    )
    parser.add_argument(
        "--threshold",
        type=float,
        default=None,
        help="配准重叠阈值，需搭配 --preprocess 使用（默认容器内部 0.98）",
    )
    parser.add_argument(
        "--keep-workdir",
        action="store_true",
        help="保留临时工作目录（调试用）",
    )

    args = parser.parse_args()

    input_paths = [Path(p).expanduser().resolve() for p in args.input]
    model_path = (
        Path(args.model).expanduser().resolve()
        if args.model
        else DEFAULT_MODEL.resolve()
    )
    output_path = Path(args.output).expanduser().resolve()

    # 处理自定义 ID
    if args.ids:
        if len(args.ids) != len(input_paths):
            raise RuntimeError(
                f"--ids 数量 ({len(args.ids)}) 必须与 --input 数量 ({len(input_paths)}) 一致"
            )
        custom_ids = args.ids
    else:
        # 默认使用输入文件夹名作为 ID
        custom_ids = [p.name for p in input_paths]

    ensure_docker_image(args.docker_image)

    temp_root: Path
    cleanup_needed = False
    if args.keep_workdir:
        temp_root = Path(tempfile.mkdtemp(prefix="dbn_workdir_"))
    else:
        temp_root = Path(tempfile.mkdtemp(prefix="dbn_workdir_"))
        cleanup_needed = True

    try:
        # 收集所有输入路径的 nifti 文件，并使用自定义 ID 作为文件名
        all_nifti_files: list[Path] = []
        for idx, input_path in enumerate(input_paths):
            # 为每个输入路径创建独立的子目录，避免文件名冲突
            input_temp_dir = temp_root / "inputs" / f"input_{idx}"
            nifti_files = gather_nifti_inputs(input_path, input_temp_dir)
            
            # 使用自定义 ID 作为文件名，便于对应原始数据
            subject_id = custom_ids[idx]
            renamed_dir = temp_root / "inputs" / f"renamed_{idx}"
            renamed_dir.mkdir(parents=True, exist_ok=True)
            
            for nii_file in nifti_files:
                # 新文件名格式: {自定义ID}.nii.gz
                new_name = f"{subject_id}.nii.gz"
                new_path = renamed_dir / new_name
                shutil.copyfile(nii_file, new_path)
                all_nifti_files.append(new_path)
                print(f"  [{subject_id}] {input_path.name} -> {new_name}")
        
        if not all_nifti_files:
            raise RuntimeError("未从任何输入路径中找到有效的 NIfTI 或 DICOM 文件。")
        
        nifti_files = all_nifti_files
        workdir, model_name = prepare_workdir(nifti_files, model_path, temp_root)
        output_csv = run_container(
            workdir=workdir,
            model_name=model_name,
            image=args.docker_image,
            output_name=output_path.name,
            preprocess=args.preprocess,
            threshold=args.threshold,
        )

        output_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(output_csv, output_path)

        rows = read_prediction(output_csv)
        if rows:
            print("预测结果：")
            for row in rows:
                print(f"ID={row.get('ID')}  Pred_Age={row.get('Pred_Age')}")
        else:
            print("输出 CSV 为空，请检查输入。")

        print(f"结果已保存至: {output_path}")
    finally:
        if cleanup_needed:
            shutil.rmtree(temp_root, ignore_errors=True)
        else:
            print(f"保留工作目录: {temp_root}")


if __name__ == "__main__":
    main()

