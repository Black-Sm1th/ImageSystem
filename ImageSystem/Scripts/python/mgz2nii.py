import nibabel as nib
import os
import argparse
import sys

# 设置标准输出编码为UTF-8，避免Windows下的编码问题
if sys.platform == 'win32':
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

def mgz_to_nii(mgz_path, output_nii_path):
    # 检查输入文件是否存在
    if not os.path.exists(mgz_path):
        raise FileNotFoundError(f"Input file not found: {mgz_path}")
    
    # 读取 .mgz
    mgz_img = nib.load(mgz_path)
    # 保存为 .nii.gz
    nib.save(mgz_img, output_nii_path)
    print(f"Conversion completed: {mgz_path} -> {output_nii_path}")

if __name__ == "__main__":
    # 创建参数解析器
    parser = argparse.ArgumentParser(description="Convert FreeSurfer .mgz file to .nii.gz format")
    
    # 添加输入文件路径参数
    parser.add_argument(
        "mgz_path",
        type=str,
        help="Input .mgz file path (e.g., /data/aseg.mgz)"
    )
    
    # 添加输出文件路径参数
    parser.add_argument(
        "output_nii_path",
        type=str,
        help="Output .nii.gz file path (e.g., /output/aseg.nii.gz)"
    )
    
    # 解析参数
    args = parser.parse_args()
    
    # 创建输出目录（如果不存在）
    output_dir = os.path.dirname(args.output_nii_path)
    if output_dir:  # 防止空路径（当前目录）
        os.makedirs(output_dir, exist_ok=True)
    
    # 调用转换函数
    try:
        mgz_to_nii(args.mgz_path, args.output_nii_path)
        sys.exit(0)  # 显式返回成功状态码
    except Exception as e:
        print(f"Conversion failed: {e}", file=sys.stderr)
        sys.exit(1)  # 显式返回失败状态码