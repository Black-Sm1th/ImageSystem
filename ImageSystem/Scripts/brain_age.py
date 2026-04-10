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
import hashlib
import os
import re
import shutil
import subprocess
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Any

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


def extract_patient_metadata_from_input(input_path: Path) -> tuple[str, str]:
    """
    尝试从输入中自动提取姓名与病历号（DICOM Tag: PatientName, PatientID）。
    仅对 DICOM 输入有效；NIfTI 输入返回空字符串。
    """
    try:
        import pydicom
    except Exception:
        return "", ""

    dcm_candidates: list[Path] = []
    if input_path.is_dir():
        dcm_candidates = sorted(input_path.rglob("*.dcm"))
        if not dcm_candidates:
            # 兼容无扩展名 DICOM，尝试取目录下文件
            dcm_candidates = [f for f in input_path.iterdir() if f.is_file()]
    elif input_path.is_file() and input_path.suffix.lower() == ".dcm":
        dcm_candidates = [input_path]

    for dcm_file in dcm_candidates:
        try:
            ds = pydicom.dcmread(dcm_file, stop_before_pixels=True, force=True)
            name = str(getattr(ds, "PatientName", "") or "").strip()
            patient_id = str(getattr(ds, "PatientID", "") or "").strip()
            if name or patient_id:
                return name, patient_id
        except Exception:
            continue

    return "", ""


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


def convert_dicom_to_nifti(
    dicom_dir: Path,
    work_dir: Path,
    dcm2niix_format: str = "%i_%j",
) -> list[Path]:
    """使用 dcm2niix 将 DICOM 转换为 .nii.gz，自动处理压缩的 DICOM。"""
    out_dir = work_dir / "dcm2niix_out"
    out_dir.mkdir(parents=True, exist_ok=True)

    # 先尝试解压 DICOM 文件（如果需要）
    decompressed_dir = work_dir / "decompressed_dicom"
    actual_dicom_dir = decompress_dicom_folder(dicom_dir, decompressed_dir)

    cmd = [
        "dcm2niix",
        "-z",
        "y",  # 压缩输出
        "-f",
        dcm2niix_format,  # 默认使用 %i_%j，避免跨受试者重名冲突
        "-o",
        str(out_dir),
        str(actual_dicom_dir),
    ]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
    except FileNotFoundError as exc:  # pragma: no cover - 环境相关
        raise RuntimeError(
            "未检测到 dcm2niix，请安装后重试：https://github.com/rordenlab/dcm2niix"
        ) from exc

    nifti_files = sorted(out_dir.rglob("*.nii.gz"))

    # 某些情况下 dcm2niix 会因部分序列异常返回非 0，但仍成功输出 NIfTI
    if result.returncode != 0:
        if nifti_files:
            print("  警告: dcm2niix 返回非0，已忽略并继续（检测到可用 .nii.gz 输出）")
            if result.stderr.strip():
                print("  dcm2niix stderr:")
                print(result.stderr.strip())
        else:
            raise RuntimeError(
                f"命令失败: {' '.join(cmd)}\nstdout: {result.stdout}\nstderr: {result.stderr}"
            )

    if not nifti_files:
        raise RuntimeError("dcm2niix 未生成任何 .nii.gz 文件，请检查 DICOM 输入。")

    return nifti_files


def can_read_dicom_file(path: Path) -> bool:
    """尽量判断文件是否为 DICOM（兼容无扩展名且未压缩）。"""
    try:
        import pydicom
        pydicom.dcmread(path, stop_before_pixels=True, force=True)
        return True
    except Exception:
        return False


def assess_case_directory(path: Path) -> tuple[bool, str]:
    """判断目录是否可作为病例目录，并返回原因。"""
    if not path.is_dir():
        return False, "不是目录"

    nii_gz_count = sum(1 for _ in path.rglob("*.nii.gz"))
    nii_count = sum(1 for _ in path.rglob("*.nii"))
    dcm_count = sum(1 for _ in path.rglob("*.dcm"))

    if nii_gz_count > 0 or nii_count > 0:
        return True, f"命中 NIfTI: nii.gz={nii_gz_count}, nii={nii_count}"

    if dcm_count > 0:
        return True, f"命中 DICOM(.dcm): {dcm_count}"

    # 兼容无扩展名 DICOM：抽样若干文件尝试读取
    candidates = [f for f in path.rglob("*") if f.is_file()]
    for f in candidates[:100]:
        if can_read_dicom_file(f):
            return True, "命中无扩展名 DICOM（抽样可读）"

    return False, "未发现 NIfTI/DICOM"


def discover_case_directories(root_dir: Path) -> list[Path]:
    """在总目录下递归自动发现病例目录（优先叶子目录），并打印过滤原因。"""
    if not root_dir.is_dir():
        return []

    all_dirs = [p for p in root_dir.rglob("*") if p.is_dir()]
    # 叶子目录优先，避免父目录吞掉子目录
    all_dirs = sorted(all_dirs, key=lambda p: (-len(p.parts), str(p).lower()))

    cases: list[Path] = []
    skipped: list[tuple[Path, str]] = []

    for d in all_dirs:
        ok, reason = assess_case_directory(d)
        if not ok:
            skipped.append((d, reason))
            continue

        # 若 d 位于已识别病例目录内部，跳过，避免重复
        if any(d.is_relative_to(existing) for existing in cases):
            skipped.append((d, "已被更深层病例目录覆盖"))
            continue

        # 若 d 是已识别病例目录的父目录，移除旧条目，保留更深层（理论上很少触发）
        cases = [c for c in cases if not c.is_relative_to(d)]
        cases.append(d)

    if not cases:
        ok_root, reason_root = assess_case_directory(root_dir)
        if ok_root:
            return [root_dir]
        print(f"自动发现失败: {root_dir} -> {reason_root}")
        return []

    # 打印部分被过滤目录原因，便于排查
    print(f"自动发现共检查目录 {len(all_dirs)} 个，识别病例目录 {len(cases)} 个")
    for p, reason in skipped[:30]:
        print(f"  跳过: {p} -> {reason}")
    if len(skipped) > 30:
        print(f"  ... 其余跳过目录 {len(skipped) - 30} 个")

    return sorted(cases, key=lambda p: str(p).lower())


def gather_nifti_inputs(
    input_path: Path,
    temp_dir: Path,
    dcm2niix_format: str = "%i_%j",
) -> list[Path]:
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
    if not dcm_files:
        # 兼容无扩展名 DICOM（含未压缩）：抽样检查是否可被 pydicom 读取
        candidate_files = [f for f in input_path.rglob("*") if f.is_file()]
        for f in candidate_files[:300]:
            if can_read_dicom_file(f):
                dcm_files = [f]
                break

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
        return convert_dicom_to_nifti(input_path, temp_dir, dcm2niix_format)

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


def file_sha256(path: Path, chunk_size: int = 1024 * 1024) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            data = f.read(chunk_size)
            if not data:
                break
            h.update(data)
    return h.hexdigest()


def deduplicate_nifti_files(nifti_files: list[Path]) -> tuple[list[Path], int]:
    """按文件内容去重，返回 (去重后列表, 去重数量)。"""
    seen: dict[str, Path] = {}
    unique_files: list[Path] = []
    duplicate_count = 0

    for f in nifti_files:
        try:
            key = file_sha256(f)
        except Exception:
            # hash 失败时退化为路径+大小键，尽量不中断流程
            stat = f.stat()
            key = f"fallback:{f.name}:{stat.st_size}"

        if key in seen:
            duplicate_count += 1
            continue

        seen[key] = f
        unique_files.append(f)

    return unique_files, duplicate_count


def chunk_list(items: list[Path], chunk_count: int) -> list[list[Path]]:
    if not items:
        return []
    chunk_count = max(1, min(chunk_count, len(items)))
    buckets: list[list[Path]] = [[] for _ in range(chunk_count)]
    for idx, item in enumerate(items):
        buckets[idx % chunk_count].append(item)
    return [b for b in buckets if b]


def run_parallel_containers(
    all_nifti_files: list[Path],
    model_path: Path,
    image: str,
    preprocess: bool,
    threshold: float | None,
    parallel_jobs: int,
    temp_root: Path,
) -> list[dict[str, str]]:
    """按分片并行运行多个 Docker，并合并预测结果。"""
    shards = chunk_list(all_nifti_files, parallel_jobs)
    if not shards:
        return []

    def run_one_shard(shard_idx: int, shard_files: list[Path]) -> list[dict[str, str]]:
        shard_root = temp_root / f"shard_{shard_idx:02d}"
        shard_root.mkdir(parents=True, exist_ok=True)
        workdir, model_name = prepare_workdir(shard_files, model_path, shard_root)
        output_name = f"Prediction_part_{shard_idx:02d}.csv"
        output_csv = run_container(
            workdir=workdir,
            model_name=model_name,
            image=image,
            output_name=output_name,
            preprocess=preprocess,
            threshold=threshold,
        )
        return read_prediction(output_csv)

    merged_rows: list[dict[str, str]] = []
    max_workers = max(1, min(parallel_jobs, len(shards)))
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        future_map = {
            executor.submit(run_one_shard, idx, shard_files): idx
            for idx, shard_files in enumerate(shards)
        }
        for future in as_completed(future_map):
            shard_idx = future_map[future]
            shard_rows = future.result()
            merged_rows.extend(shard_rows)
            print(f"  分片 {shard_idx + 1}/{len(shards)} 完成，结果 {len(shard_rows)} 条")

    return merged_rows


def read_prediction(csv_path: Path) -> list[dict[str, str]]:
    """读取预测 CSV，返回行列表。"""
    with open(csv_path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        return list(reader)


def resolve_output_csv_path(output_arg: str) -> Path:
    """将输出参数规范为 CSV 文件路径；若传入目录则自动补 Prediction.csv。"""
    p = Path(output_arg).expanduser().resolve()

    # 已存在且是目录
    if p.exists() and p.is_dir():
        return p / "Prediction.csv"

    # 不存在但像目录（无后缀，如 C:/test）也按目录处理
    if p.suffix.lower() != ".csv":
        return p / "Prediction.csv"

    return p


def normalize_subject_id(raw_id: str) -> str:
    """将容器输出 ID 归一化（去掉并行分片后缀 _数字）。"""
    text = raw_id.strip()
    if not text:
        return text
    return re.sub(r"_\d+$", "", text)


def rewrite_prediction_with_metadata(
    output_csv_path: Path,
    rows: list[dict[str, str]],
    id_to_meta: dict[str, dict[str, str]],
) -> list[dict[str, Any]]:
    """
    将预测结果补充姓名与病历号并重写输出文件。

    输出列：ID, Name, PatientID, Pred_Age
    并按 ID 去重（保留最后一次结果）。
    """
    merged_by_id: dict[str, dict[str, Any]] = {}
    for row in rows:
        raw_pred_id = (row.get("ID") or row.get("id") or row.get("subject") or "").strip()
        pred_id = normalize_subject_id(raw_pred_id)
        pred_age = (row.get("Pred_Age") or row.get("predicted_age") or row.get("prediction") or "").strip()

        meta = id_to_meta.get(pred_id, {})
        if not meta and raw_pred_id:
            # 兼容未归一化键（兜底）
            meta = id_to_meta.get(raw_pred_id, {})

        merged_by_id[pred_id] = {
            "ID": pred_id,
            "Name": meta.get("name", ""),
            "PatientID": meta.get("patient_id", ""),
            "Pred_Age": pred_age,
        }

    merged_rows = list(merged_by_id.values())
    merged_rows.sort(key=lambda x: x.get("ID", ""))

    with open(output_csv_path, "w", newline="", encoding="utf-8-sig") as f:
        writer = csv.DictWriter(f, fieldnames=["ID", "Name", "PatientID", "Pred_Age"])
        writer.writeheader()
        writer.writerows(merged_rows)

    return merged_rows


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
        "--dcm2niix-format",
        default="%i_%j",
        help="dcm2niix 输出命名模板（默认 %i_%j，可减少重名冲突）",
    )
    parser.add_argument(
        "--preprocess",
        action="store_true",
        help="对原始扫描执行去颅骨+配准；原始 DICOM 建议开启",
    )
    parser.add_argument(
        "--parallel-docker",
        type=int,
        default=10,
        help="并行 Docker 数量（默认 10）",
    )
    parser.add_argument(
        "--auto-discover-cases",
        action="store_true",
        help="当 --input 只给一个总目录时，自动发现其下多个病例目录再批量处理",
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

    # 可选：自动发现总目录下多个病例目录
    if args.auto_discover_cases and len(input_paths) == 1 and input_paths[0].is_dir():
        discovered = discover_case_directories(input_paths[0])
        if discovered:
            input_paths = discovered
            print(f"自动发现病例目录 {len(input_paths)} 个")
        else:
            print("未在总目录下发现可处理病例，回退为按原输入路径处理")
    model_path = (
        Path(args.model).expanduser().resolve()
        if args.model
        else DEFAULT_MODEL.resolve()
    )
    output_path = resolve_output_csv_path(args.output)

    # 处理自定义 ID
    if args.ids:
        if len(args.ids) != len(input_paths):
            raise RuntimeError(
                f"--ids 数量 ({len(args.ids)}) 必须与实际处理病例数量 ({len(input_paths)}) 一致"
            )
        custom_ids = args.ids
    else:
        # 默认使用输入文件夹名作为 ID
        custom_ids = [p.name for p in input_paths]

    # 自动从输入 DICOM 中提取姓名与病历号
    id_to_meta: dict[str, dict[str, str]] = {}
    for i, input_path in enumerate(input_paths):
        auto_name, auto_patient_id = extract_patient_metadata_from_input(input_path)
        id_to_meta[custom_ids[i]] = {
            "name": auto_name,
            "patient_id": auto_patient_id,
        }

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
            nifti_files = gather_nifti_inputs(
                input_path,
                input_temp_dir,
                args.dcm2niix_format,
            )
            
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

        unique_nifti_files, duplicate_count = deduplicate_nifti_files(all_nifti_files)
        if duplicate_count > 0:
            print(f"检测到重复输入 {duplicate_count} 个，已按文件内容去重")
        if not unique_nifti_files:
            raise RuntimeError("去重后无可用 NIfTI 文件，请检查输入数据。")

        parallel_jobs = max(1, args.parallel_docker)
        print(f"开始并行推理，Docker 并发数: {parallel_jobs}，样本数: {len(unique_nifti_files)}")

        rows = run_parallel_containers(
            all_nifti_files=unique_nifti_files,
            model_path=model_path,
            image=args.docker_image,
            preprocess=args.preprocess,
            threshold=args.threshold,
            parallel_jobs=parallel_jobs,
            temp_root=temp_root,
        )

        output_path.parent.mkdir(parents=True, exist_ok=True)
        if rows:
            merged_rows = rewrite_prediction_with_metadata(output_path, rows, id_to_meta)
            print("预测结果：")
            for row in merged_rows:
                print(
                    f"ID={row.get('ID')}  Name={row.get('Name')}  "
                    f"PatientID={row.get('PatientID')}  Pred_Age={row.get('Pred_Age')}"
                )
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

