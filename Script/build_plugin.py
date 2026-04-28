#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
AnimatedTexturePlugin - UAT BuildPlugin 打包脚本 (Python 版)
用途：通过 UAT 将插件编译打包到指定输出目录
日志：构建日志保存到 Saved/BuildLogs/ 目录，每次构建创建独立子目录
"""

import os
import sys
import shutil
import subprocess
from datetime import datetime

# ============================================================================
#  配置区
# ============================================================================
UE_ROOT = r"C:\Program Files\Epic Games\UE_5.3"
PLUGIN_FILE = r"E:\MyGitHub\UAnimatedTexture5\Plugins\AnimatedTexturePlugin\AnimatedTexturePlugin.uplugin"
PACKAGE_OUTPUT = r"D:\Stage\AnimatedTexturePlugin"
PROJECT_ROOT = r"E:\MyGitHub\UAnimatedTexture5"
LOG_ROOT = os.path.join(PROJECT_ROOT, "Saved", "BuildLogs")

UAT_BAT = os.path.join(UE_ROOT, "Engine", "Build", "BatchFiles", "RunUAT.bat")

# 保留的历史构建子目录数量上限
KEEP_COUNT = 10


# ============================================================================
#  功能函数
# ============================================================================

def cleanup_old_builds(log_root: str, keep_count: int) -> None:
    """清理历史构建子目录，仅保留最近 keep_count 个。"""
    if not os.path.isdir(log_root):
        return

    # 扫描以 build_ 开头的子目录
    build_dirs = [
        d for d in os.listdir(log_root)
        if os.path.isdir(os.path.join(log_root, d)) and d.startswith("build_")
    ]

    # 按目录名排序（时间戳命名天然有序），降序排列
    build_dirs.sort(reverse=True)

    # 删除超出保留数量的旧目录
    for old_dir in build_dirs[keep_count:]:
        old_path = os.path.join(log_root, old_dir)
        print(f"[CLEANUP] 删除旧构建日志: {old_dir}")
        shutil.rmtree(old_path, ignore_errors=True)


def run_build(build_dir: str) -> int:
    """执行 UAT BuildPlugin 构建，将输出重定向到日志文件，返回退出码。"""
    log_file_path = os.path.join(build_dir, "build.log")

    cmd = [
        UAT_BAT,
        "BuildPlugin",
        f"-Plugin={PLUGIN_FILE}",
        f"-Package={PACKAGE_OUTPUT}",
        "-CreateSubFolder",
        "-nocompile",
        "-nocompileuat",
    ]

    with open(log_file_path, "w", encoding="utf-8", errors="replace") as log_file:
        result = subprocess.run(
            cmd,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            shell=True,  # 需要 shell=True 来正确调用 .bat 文件
        )

    return result.returncode


def write_result_file(build_dir: str, exit_code: int) -> None:
    """生成构建结果状态文件 result.txt。"""
    result_path = os.path.join(build_dir, "result.txt")
    if exit_code == 0:
        status_line = "SUCCESS ExitCode=0"
    else:
        status_line = f"FAILED ExitCode={exit_code}"

    with open(result_path, "w", encoding="utf-8") as f:
        f.write(status_line + "\n")


def extract_errors(build_dir: str) -> None:
    """从 build.log 中提取包含 error 关键字的行，写入 errors.log（仅构建失败时调用）。"""
    log_file_path = os.path.join(build_dir, "build.log")
    errors_file_path = os.path.join(build_dir, "errors.log")

    error_lines = []
    try:
        with open(log_file_path, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                if "error" in line.lower():
                    error_lines.append(line)
    except FileNotFoundError:
        pass

    with open(errors_file_path, "w", encoding="utf-8") as f:
        f.writelines(error_lines)


def copy_latest_files(log_root: str, build_dir: str, build_failed: bool) -> None:
    """将本次构建的关键文件复制为 latest 快捷文件。"""
    # 复制 build.log -> build_latest.log
    build_log = os.path.join(build_dir, "build.log")
    if os.path.isfile(build_log):
        shutil.copy2(build_log, os.path.join(log_root, "build_latest.log"))

    # 复制 result.txt -> build_latest_result.txt
    result_txt = os.path.join(build_dir, "result.txt")
    if os.path.isfile(result_txt):
        shutil.copy2(result_txt, os.path.join(log_root, "build_latest_result.txt"))

    latest_errors = os.path.join(log_root, "build_latest_errors.log")
    if build_failed:
        # 构建失败：复制错误摘要
        errors_log = os.path.join(build_dir, "errors.log")
        if os.path.isfile(errors_log):
            shutil.copy2(errors_log, latest_errors)
    else:
        # 构建成功：删除旧的错误摘要（避免残留上次失败的信息）
        if os.path.isfile(latest_errors):
            os.remove(latest_errors)


def print_banner(build_dir: str) -> None:
    """打印构建开始前的配置信息。"""
    log_file_path = os.path.join(build_dir, "build.log")
    print("=" * 60)
    print(f"  UE Engine  : {UE_ROOT}")
    print(f"  Plugin     : {PLUGIN_FILE}")
    print(f"  Output     : {PACKAGE_OUTPUT}")
    print(f"  Log Dir    : {build_dir}")
    print("=" * 60)
    print()
    print("[INFO] 构建开始，输出将重定向到日志文件...")
    print(f"[INFO] 日志路径: {log_file_path}")
    print()


def print_result(build_dir: str, exit_code: int) -> None:
    """打印构建完成后的结果信息。"""
    print()
    print("=" * 60)
    if exit_code == 0:
        print("  [SUCCESS] BuildPlugin 构建成功！")
    else:
        print(f"  [FAILED] BuildPlugin 构建失败！ExitCode={exit_code}")
        print()
        print("  --- 错误摘要 ---")
        errors_file = os.path.join(build_dir, "errors.log")
        if os.path.isfile(errors_file):
            try:
                with open(errors_file, "r", encoding="utf-8", errors="replace") as f:
                    content = f.read().strip()
                if content:
                    print(content)
                else:
                    print("  （未提取到包含 error 关键字的行）")
            except Exception:
                print("  （读取错误摘要文件失败）")
        else:
            print("  （未提取到包含 error 关键字的行）")
        print("  --- 错误摘要结束 ---")

    print()
    print(f"  完整日志: {os.path.join(build_dir, 'build.log')}")
    print(f"  构建状态: {os.path.join(LOG_ROOT, 'build_latest_result.txt')}")
    print("=" * 60)


# ============================================================================
#  主流程
# ============================================================================

def main() -> int:
    # 步骤1：生成时间戳，构造本次构建子目录路径
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    build_dir_name = f"build_{timestamp}"
    build_dir = os.path.join(LOG_ROOT, build_dir_name)

    # 步骤2：创建日志根目录和本次构建子目录
    os.makedirs(build_dir, exist_ok=True)

    # 步骤3：清理历史构建子目录（保留最近 KEEP_COUNT 个）
    cleanup_old_builds(LOG_ROOT, KEEP_COUNT)

    # 步骤4：打印构建配置信息
    print_banner(build_dir)

    # 步骤5：执行构建，输出重定向到日志文件
    exit_code = run_build(build_dir)

    # 步骤6：生成构建结果状态文件
    write_result_file(build_dir, exit_code)

    # 步骤7：生成错误摘要文件（仅构建失败时）
    build_failed = exit_code != 0
    if build_failed:
        extract_errors(build_dir)

    # 步骤8：复制 latest 快捷文件到日志根目录
    copy_latest_files(LOG_ROOT, build_dir, build_failed)

    # 步骤9：控制台输出构建结果
    print_result(build_dir, exit_code)

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
