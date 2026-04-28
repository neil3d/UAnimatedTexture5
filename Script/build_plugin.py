#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
AnimatedTexturePlugin - UAT BuildPlugin 多版本打包脚本 (Python 版)
用途：通过 UAT 将插件编译打包到指定输出目录，支持多个 UE 引擎版本顺序构建
日志：构建日志保存到 Saved/BuildLogs/ 目录，按引擎版本分子目录存放
"""

import argparse
import os
import sys
import shutil
import subprocess
import time
import zipfile
from datetime import datetime

# ============================================================================
#  配置区（任务1：多版本引擎配置）
# ============================================================================

# 需要构建的引擎版本列表（按顺序构建）
ENGINE_VERSIONS = ["5.3", "5.4", "5.5", "5.6", "5.7"]

# 引擎安装根目录，各版本路径为 ENGINE_BASE_DIR/UE_{version}
ENGINE_BASE_DIR = r"C:\Program Files\Epic Games"

# 插件 .uplugin 文件路径
PLUGIN_FILE = r"E:\MyGitHub\UAnimatedTexture5\Plugins\AnimatedTexturePlugin\AnimatedTexturePlugin.uplugin"

# 构建产物输出根目录，各版本输出到 STAGE_ROOT/AnimatedTexturePlugin_{version}
STAGE_ROOT = r"D:\Stage"

# 项目根目录
PROJECT_ROOT = r"E:\MyGitHub\UAnimatedTexture5"

# 日志根目录
LOG_ROOT = os.path.join(PROJECT_ROOT, "Saved", "BuildLogs")

# 保留的历史构建批次目录数量上限
KEEP_COUNT = 10

# 错误摘要最大行数
MAX_ERROR_LINES_IN_SUMMARY = 50


# ============================================================================
#  辅助函数
# ============================================================================

def get_ue_root(version: str) -> str:
    """根据版本号拼接引擎根目录路径。"""
    return os.path.join(ENGINE_BASE_DIR, f"UE_{version}")


def get_uat_bat(version: str) -> str:
    """根据版本号拼接 RunUAT.bat 路径。"""
    return os.path.join(get_ue_root(version), "Engine", "Build", "BatchFiles", "RunUAT.bat")


def get_package_output(version: str) -> str:
    """根据版本号拼接构建产物输出目录路径。"""
    return os.path.join(STAGE_ROOT, f"AnimatedTexturePlugin_{version}")


def format_duration(seconds: float) -> str:
    """将秒数格式化为 mm:ss 或 hh:mm:ss 格式。"""
    seconds = int(seconds)
    if seconds < 3600:
        return f"{seconds // 60:02d}:{seconds % 60:02d}"
    return f"{seconds // 3600:02d}:{(seconds % 3600) // 60:02d}:{seconds % 60:02d}"


# ============================================================================
#  任务2：引擎版本预检
# ============================================================================

def validate_engine(version: str) -> tuple:
    """
    预检引擎版本是否可用。

    参数:
        version: 引擎版本号字符串，如 "5.3"

    返回:
        (is_valid, ue_root, uat_bat, reason) 元组
        - is_valid: 是否通过预检
        - ue_root: 引擎根目录路径
        - uat_bat: RunUAT.bat 路径
        - reason: 未通过时的原因说明（通过时为空字符串）
    """
    ue_root = get_ue_root(version)
    uat_bat = get_uat_bat(version)

    if not os.path.isdir(ue_root):
        return (False, ue_root, uat_bat, f"引擎目录不存在: {ue_root}")

    if not os.path.isfile(uat_bat):
        return (False, ue_root, uat_bat, f"RunUAT.bat 不存在: {uat_bat}")

    return (True, ue_root, uat_bat, "")


# ============================================================================
#  任务3 & 4：构建执行（支持版本参数化 + 版本专属日志目录）
# ============================================================================

def run_build(uat_bat: str, plugin_file: str, package_output: str, version_log_dir: str) -> int:
    """
    执行 UAT BuildPlugin 构建，将输出重定向到日志文件，返回退出码。

    参数:
        uat_bat: RunUAT.bat 的完整路径
        plugin_file: .uplugin 文件的完整路径
        package_output: 构建产物输出目录
        version_log_dir: 该版本的日志子目录路径
    """
    # 构建前处理输出目录：已存在则清空，不存在则创建
    if os.path.isdir(package_output):
        print(f"[INFO] 清空已有输出目录: {package_output}")
        shutil.rmtree(package_output, ignore_errors=True)
    os.makedirs(package_output, exist_ok=True)

    log_file_path = os.path.join(version_log_dir, "build.log")

    cmd = [
        uat_bat,
        "BuildPlugin",
        f"-Plugin={plugin_file}",
        f"-Package={package_output}",
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


def write_result_file(version_log_dir: str, exit_code: int) -> None:
    """生成构建结果状态文件 result.txt。"""
    result_path = os.path.join(version_log_dir, "result.txt")
    if exit_code == 0:
        status_line = "SUCCESS ExitCode=0"
    else:
        status_line = f"FAILED ExitCode={exit_code}"

    with open(result_path, "w", encoding="utf-8") as f:
        f.write(status_line + "\n")


def extract_errors(version_log_dir: str) -> list:
    """
    从 build.log 中提取包含 error 关键字的行，写入 errors.log。

    返回提取到的错误行列表（供后续汇总使用）。
    """
    log_file_path = os.path.join(version_log_dir, "build.log")
    errors_file_path = os.path.join(version_log_dir, "errors.log")

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

    return error_lines


# ============================================================================
#  任务5：构建产物清理
# ============================================================================

def cleanup_build_artifacts(package_output: str, version: str) -> None:
    """
    清理构建产物中的中间文件（仅构建成功时调用）。

    删除 Intermediate 目录和 Binaries 下的 .pdb 文件。
    """
    print(f"[CLEANUP] UE {version} - 开始清理构建中间文件...")

    # 1. 删除 Intermediate 目录
    intermediate_dir = os.path.join(package_output, "Intermediate")
    if os.path.isdir(intermediate_dir):
        # 计算目录大小
        dir_size = sum(
            os.path.getsize(os.path.join(dp, f))
            for dp, _, filenames in os.walk(intermediate_dir)
            for f in filenames
        )
        shutil.rmtree(intermediate_dir, ignore_errors=True)
        print(f"[CLEANUP]   已删除 Intermediate 目录 (释放 {dir_size / 1024 / 1024:.1f} MB)")
    else:
        print(f"[CLEANUP]   Intermediate 目录不存在，跳过")

    # 2. 删除 Binaries 下的 .pdb 文件
    binaries_dir = os.path.join(package_output, "Binaries")
    pdb_count = 0
    pdb_size = 0
    if os.path.isdir(binaries_dir):
        for root, dirs, files in os.walk(binaries_dir):
            for f in files:
                if f.lower().endswith(".pdb"):
                    pdb_path = os.path.join(root, f)
                    pdb_size += os.path.getsize(pdb_path)
                    os.remove(pdb_path)
                    pdb_count += 1
                    print(f"[CLEANUP]   已删除 PDB: {os.path.relpath(pdb_path, package_output)}")

    if pdb_count > 0:
        print(f"[CLEANUP]   共删除 {pdb_count} 个 .pdb 文件 (释放 {pdb_size / 1024 / 1024:.1f} MB)")
    else:
        print(f"[CLEANUP]   未发现 .pdb 文件")

    print(f"[CLEANUP] UE {version} - 清理完成")


# ============================================================================
#  任务5b：构建产物打包为 zip
# ============================================================================

def zip_package(package_output: str, version: str) -> str:
    """
    将构建产物目录打包为 .zip 文件（仅构建成功时调用）。

    zip 文件保存在 STAGE_ROOT 下，与构建产物目录同级，
    文件名格式为 AnimatedTexturePlugin_{version}.zip。
    zip 内部保留顶层目录名（如 AnimatedTexturePlugin_5.3/...）。

    参数:
        package_output: 构建产物输出目录的完整路径
        version: 引擎版本号字符串

    返回:
        生成的 zip 文件完整路径
    """
    zip_filename = f"AnimatedTexturePlugin_{version}.zip"
    zip_path = os.path.join(STAGE_ROOT, zip_filename)

    # 如果已有同名 zip 则先删除
    if os.path.isfile(zip_path):
        os.remove(zip_path)

    print(f"[PACKAGE] UE {version} - 开始打包为 {zip_filename} ...")

    # 获取顶层目录名，用于 zip 内部路径
    top_dir_name = os.path.basename(package_output)

    file_count = 0
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for root, dirs, files in os.walk(package_output):
            for f in files:
                abs_path = os.path.join(root, f)
                # zip 内部路径：AnimatedTexturePlugin_{version}/...
                rel_path = os.path.relpath(abs_path, os.path.dirname(package_output))
                zf.write(abs_path, rel_path)
                file_count += 1

    zip_size = os.path.getsize(zip_path)
    print(f"[PACKAGE]   已打包 {file_count} 个文件")
    print(f"[PACKAGE]   zip 文件: {zip_path} ({zip_size / 1024 / 1024:.1f} MB)")
    print(f"[PACKAGE] UE {version} - 打包完成")

    return zip_path


# ============================================================================
#  历史日志清理（保留原有逻辑）
# ============================================================================

def cleanup_old_builds(log_root: str, keep_count: int) -> None:
    """清理历史构建批次目录，仅保留最近 keep_count 个。"""
    if not os.path.isdir(log_root):
        return

    # 扫描以 build_ 开头且包含时间戳的子目录（排除 build_latest_* 文件）
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


# ============================================================================
#  任务6：汇总报告生成
# ============================================================================

def generate_summary(batch_dir: str, results: list, build_timestamp: str) -> None:
    """
    生成构建汇总报告 summary.txt。

    参数:
        batch_dir: 构建批次目录路径
        results: 构建结果列表，每项为 dict:
            {version, status, exit_code, duration, log_dir}
        build_timestamp: 构建开始时间字符串
    """
    summary_path = os.path.join(batch_dir, "summary.txt")

    with open(summary_path, "w", encoding="utf-8") as f:
        f.write("=" * 70 + "\n")
        f.write("  AnimatedTexturePlugin 多版本构建汇总报告\n")
        f.write("=" * 70 + "\n")
        f.write(f"  构建时间  : {build_timestamp}\n")
        f.write(f"  插件路径  : {PLUGIN_FILE}\n")
        f.write(f"  输出根目录: {STAGE_ROOT}\n")
        f.write(f"  引擎版本  : {', '.join(ENGINE_VERSIONS)}\n")
        f.write("=" * 70 + "\n\n")

        # 汇总表格
        f.write(f"{'版本':<10} {'状态':<12} {'退出码':<10} {'耗时':<12} {'输出目录'}\n")
        f.write("-" * 70 + "\n")
        for r in results:
            output_dir = get_package_output(r["version"]) if r["status"] != "SKIPPED" else "N/A"
            f.write(f"UE {r['version']:<7} {r['status']:<12} {str(r['exit_code']):<10} "
                    f"{r['duration']:<12} {output_dir}\n")
        f.write("-" * 70 + "\n\n")

        # 失败版本的错误摘要
        failed_versions = [r for r in results if r["status"] == "FAILED"]
        if failed_versions:
            f.write("=" * 70 + "\n")
            f.write("  失败版本错误摘要\n")
            f.write("=" * 70 + "\n\n")

            for r in failed_versions:
                f.write(f"--- UE {r['version']} 错误摘要 (前 {MAX_ERROR_LINES_IN_SUMMARY} 行) ---\n")
                errors_file = os.path.join(r["log_dir"], "errors.log")
                if os.path.isfile(errors_file):
                    try:
                        with open(errors_file, "r", encoding="utf-8", errors="replace") as ef:
                            lines = ef.readlines()
                        for line in lines[:MAX_ERROR_LINES_IN_SUMMARY]:
                            f.write(line)
                        if len(lines) > MAX_ERROR_LINES_IN_SUMMARY:
                            f.write(f"\n... 共 {len(lines)} 行错误，此处仅显示前 {MAX_ERROR_LINES_IN_SUMMARY} 行 ...\n")
                    except Exception:
                        f.write("  （读取错误日志失败）\n")
                else:
                    f.write("  （未找到错误日志文件）\n")
                f.write(f"--- UE {r['version']} 错误摘要结束 ---\n\n")

                f.write(f"  完整构建日志: {os.path.join(r['log_dir'], 'build.log')}\n")
                f.write(f"  完整错误日志: {errors_file}\n\n")
        else:
            f.write("所有版本均构建成功！\n")


# ============================================================================
#  任务7：多版本 latest 文件
# ============================================================================

def copy_latest_files(log_root: str, batch_dir: str, results: list) -> None:
    """
    将本次构建的关键文件复制为 latest 快捷文件。

    - build_latest_summary.txt: 最新汇总
    - build_latest_UE_{version}_errors.log: 各版本错误日志（仅失败时）
    """
    # 1. 复制 summary.txt -> build_latest_summary.txt
    summary_src = os.path.join(batch_dir, "summary.txt")
    if os.path.isfile(summary_src):
        shutil.copy2(summary_src, os.path.join(log_root, "build_latest_summary.txt"))

    # 2. 处理各版本的错误日志
    for r in results:
        version = r["version"]
        latest_errors = os.path.join(log_root, f"build_latest_UE_{version}_errors.log")

        if r["status"] == "FAILED":
            # 构建失败：复制错误日志
            errors_src = os.path.join(r["log_dir"], "errors.log")
            if os.path.isfile(errors_src):
                shutil.copy2(errors_src, latest_errors)
        else:
            # 构建成功或跳过：删除旧的错误日志（避免残留）
            if os.path.isfile(latest_errors):
                os.remove(latest_errors)

    # 3. 清理不再属于当前版本列表的旧 latest 文件
    for f in os.listdir(log_root):
        if f.startswith("build_latest_UE_") and f.endswith("_errors.log"):
            # 提取版本号：build_latest_UE_5.3_errors.log -> 5.3
            version_part = f.replace("build_latest_UE_", "").replace("_errors.log", "")
            if version_part not in ENGINE_VERSIONS:
                old_file = os.path.join(log_root, f)
                print(f"[CLEANUP] 删除过期的 latest 文件: {f}")
                os.remove(old_file)


# ============================================================================
#  任务8：控制台输出函数
# ============================================================================

def print_banner(batch_dir: str) -> None:
    """打印构建开始前的全局配置信息。"""
    print()
    print("=" * 70)
    print("  AnimatedTexturePlugin 多版本构建")
    print("=" * 70)
    print(f"  插件路径    : {PLUGIN_FILE}")
    print(f"  输出根目录  : {STAGE_ROOT}")
    print(f"  日志批次目录: {batch_dir}")
    print(f"  引擎版本    : {', '.join(ENGINE_VERSIONS)}")
    print()
    print("  各版本输出目录:")
    for ver in ENGINE_VERSIONS:
        print(f"    UE {ver} -> {get_package_output(ver)}")
    print()
    print("  各版本引擎路径:")
    for ver in ENGINE_VERSIONS:
        print(f"    UE {ver} -> {get_ue_root(ver)}")
    print("=" * 70)
    print()


def print_version_start(version: str, ue_root: str, package_output: str, version_log_dir: str) -> None:
    """在每个版本构建开始时输出该版本的配置信息。"""
    print()
    print("-" * 70)
    print(f"  [BUILD] UE {version} 开始构建")
    print("-" * 70)
    print(f"  引擎路径: {ue_root}")
    print(f"  输出目录: {package_output}")
    print(f"  日志目录: {version_log_dir}")
    print(f"  日志文件: {os.path.join(version_log_dir, 'build.log')}")
    print()
    print(f"[INFO] UE {version} - 构建开始，输出将重定向到日志文件...")
    print()


def print_version_result(version: str, exit_code: int, duration: str, version_log_dir: str) -> None:
    """在每个版本构建完成后输出结果信息。"""
    print()
    if exit_code == 0:
        print(f"  [SUCCESS] UE {version} 构建成功！(耗时 {duration})")
    else:
        print(f"  [FAILED] UE {version} 构建失败！ExitCode={exit_code} (耗时 {duration})")
        print()
        print(f"  --- UE {version} 错误摘要 ---")
        errors_file = os.path.join(version_log_dir, "errors.log")
        if os.path.isfile(errors_file):
            try:
                with open(errors_file, "r", encoding="utf-8", errors="replace") as f:
                    content = f.read().strip()
                if content:
                    # 控制台只输出前 30 行错误，避免刷屏
                    lines = content.split("\n")
                    for line in lines[:30]:
                        print(f"  {line}")
                    if len(lines) > 30:
                        print(f"  ... 共 {len(lines)} 行错误，完整日志见下方路径 ...")
                else:
                    print("  （未提取到包含 error 关键字的行）")
            except Exception:
                print("  （读取错误摘要文件失败）")
        else:
            print("  （未提取到包含 error 关键字的行）")
        print(f"  --- UE {version} 错误摘要结束 ---")
        print()
        print(f"  完整构建日志: {os.path.join(version_log_dir, 'build.log')}")
        print(f"  完整错误日志: {errors_file}")
    print()


def print_summary(results: list) -> None:
    """在所有版本构建完成后输出格式化的汇总表格。"""
    print()
    print("=" * 70)
    print("  构建汇总")
    print("=" * 70)
    print()
    print(f"  {'版本':<10} {'状态':<12} {'退出码':<10} {'耗时':<12}")
    print(f"  {'-' * 44}")

    for r in results:
        status_marker = "✓" if r["status"] == "SUCCESS" else ("✗" if r["status"] == "FAILED" else "⊘")
        print(f"  UE {r['version']:<7} {status_marker} {r['status']:<10} "
              f"{str(r['exit_code']):<10} {r['duration']:<12}")

    print()

    # 失败版本的错误日志路径提示
    failed = [r for r in results if r["status"] == "FAILED"]
    if failed:
        print("  失败版本错误日志:")
        for r in failed:
            errors_file = os.path.join(r["log_dir"], "errors.log")
            print(f"    UE {r['version']}: {errors_file}")
        print()

    # 汇总文件路径
    print(f"  汇总报告: {os.path.join(LOG_ROOT, 'build_latest_summary.txt')}")

    # 统计
    success_count = sum(1 for r in results if r["status"] == "SUCCESS")
    failed_count = sum(1 for r in results if r["status"] == "FAILED")
    skipped_count = sum(1 for r in results if r["status"] == "SKIPPED")
    total = len(results)

    print()
    if failed_count == 0 and skipped_count == 0:
        print(f"  全部 {total} 个版本构建成功！")
    else:
        print(f"  共 {total} 个版本: {success_count} 成功, {failed_count} 失败, {skipped_count} 跳过")

    print("=" * 70)
    print()


# ============================================================================
#  任务9：主流程 - 多版本顺序构建循环
# ============================================================================

def parse_args() -> argparse.Namespace:
    """解析命令行参数。"""
    parser = argparse.ArgumentParser(
        description="AnimatedTexturePlugin 多版本构建脚本",
    )
    parser.add_argument(
        "--version",
        type=str,
        default=None,
        metavar="VER",
        help=f"仅构建指定的引擎版本（如 5.3 或 5.4），不指定则构建所有版本: {', '.join(ENGINE_VERSIONS)}",
    )
    return parser.parse_args()


def main() -> int:
    # 步骤0：解析命令行参数，确定本次构建的引擎版本列表
    args = parse_args()
    if args.version is not None:
        if args.version not in ENGINE_VERSIONS:
            print(f"[ERROR] 指定的引擎版本 '{args.version}' 不在配置列表中。")
            print(f"[ERROR] 可用版本: {', '.join(ENGINE_VERSIONS)}")
            return 1
        build_versions = [args.version]
        print(f"[INFO] 命令行指定仅构建 UE {args.version}")
    else:
        build_versions = ENGINE_VERSIONS

    # 步骤1：生成时间戳，构造本次构建批次目录路径
    now = datetime.now()
    timestamp = now.strftime("%Y%m%d_%H%M%S")
    build_timestamp = now.strftime("%Y-%m-%d %H:%M:%S")
    batch_dir_name = f"build_{timestamp}"
    batch_dir = os.path.join(LOG_ROOT, batch_dir_name)

    # 步骤2：创建日志根目录和本次构建批次目录
    os.makedirs(batch_dir, exist_ok=True)

    # 步骤3：确保输出根目录存在
    os.makedirs(STAGE_ROOT, exist_ok=True)

    # 步骤4：清理历史构建批次目录（保留最近 KEEP_COUNT 个）
    cleanup_old_builds(LOG_ROOT, KEEP_COUNT)

    # 步骤5：打印全局构建配置信息
    print_banner(batch_dir)

    # 步骤6：遍历引擎版本，依次构建
    results = []

    for version in build_versions:
        # 6.1 预检引擎版本
        is_valid, ue_root, uat_bat, reason = validate_engine(version)
        if not is_valid:
            print(f"[WARNING] UE {version} 预检未通过，跳过构建: {reason}")
            results.append({
                "version": version,
                "status": "SKIPPED",
                "exit_code": -1,
                "duration": "N/A",
                "log_dir": "",
            })
            continue

        # 6.2 创建版本日志子目录
        version_log_dir = os.path.join(batch_dir, f"UE_{version}")
        os.makedirs(version_log_dir, exist_ok=True)

        # 6.3 输出版本构建开始信息
        package_output = get_package_output(version)
        print_version_start(version, ue_root, package_output, version_log_dir)

        # 6.4 执行构建并计时
        start_time = time.time()
        exit_code = run_build(uat_bat, PLUGIN_FILE, package_output, version_log_dir)
        end_time = time.time()
        duration_seconds = end_time - start_time
        duration_str = format_duration(duration_seconds)

        # 6.5 写入构建结果状态文件
        write_result_file(version_log_dir, exit_code)

        # 6.6 构建失败时提取错误
        build_failed = exit_code != 0
        if build_failed:
            extract_errors(version_log_dir)

        # 6.7 构建成功时清理中间文件并打包
        if not build_failed:
            cleanup_build_artifacts(package_output, version)
            zip_package(package_output, version)

        # 6.8 输出版本构建结果
        print_version_result(version, exit_code, duration_str, version_log_dir)

        # 6.9 记录结果
        status = "FAILED" if build_failed else "SUCCESS"
        results.append({
            "version": version,
            "status": status,
            "exit_code": exit_code,
            "duration": duration_str,
            "log_dir": version_log_dir,
        })

    # 步骤7：生成汇总报告
    generate_summary(batch_dir, results, build_timestamp)

    # 步骤8：复制 latest 快捷文件
    copy_latest_files(LOG_ROOT, batch_dir, results)

    # 步骤9：输出汇总表格
    print_summary(results)

    # 步骤10：返回退出码（全部成功返回 0，任一失败返回 1）
    has_failure = any(r["status"] == "FAILED" for r in results)
    return 1 if has_failure else 0


if __name__ == "__main__":
    sys.exit(main())
