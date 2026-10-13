"""Build the offline fixture helper with an existing Windows MSVC configuration."""
import argparse
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--vcvars", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build, output = args.build.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    record = next(row for row in json.loads((build / "compile_commands.json").read_text())
                  if row["file"].replace("\\", "/").endswith("tools/gameplay_assembler/src/assembler_cli.cpp"))
    compile_command = record["command"].replace(record["file"].replace("/", "\\"),
                                               str(root / "tools/compile_entity_practice.cpp"))
    compile_command = re.sub(r"/Fo\S+", lambda _: "/Fo" + str(output / "compiler.obj"), compile_command)
    compile_command = re.sub(r"/Fd\S+", lambda _: "/Fd" + str(output / "compiler.pdb"), compile_command)
    commands = subprocess.check_output(["ninja", "-C", str(build), "-t", "commands", "cuexis_gameplay_assemble"], text=True)
    link = next(line for line in commands.splitlines() if "vs_link_exe" in line and "cuexis_gameplay_assemble.exe" in line)
    main_object = "tools\\gameplay_assembler\\CMakeFiles\\cuexis_gameplay_assemble.dir\\src\\main.cpp.obj"
    assert main_object in link
    link = link.replace(main_object, str(output / "compiler.obj"))
    link = link.replace("bin\\cuexis_gameplay_assemble.exe", str(output / "compiler.exe"))
    link = link.replace("bin\\cuexis_gameplay_assemble.pdb", str(output / "link.pdb"))
    link = link.replace("lib\\cuexis_gameplay_assemble.lib", str(output / "compiler.lib"))
    link = link.replace(str(build).replace("\\", "/") + "/bin/cuexis_gameplay_assemble.exe",
                        str(output / "compiler.exe").replace("\\", "/"))
    script = output / "build.cmd"
    script.write_text(f'@echo off\ncall "{args.vcvars.resolve()}" >nul\nif errorlevel 1 exit /b %errorlevel%\n'
                      f'{compile_command}\nif errorlevel 1 exit /b %errorlevel%\n{link}\nexit /b %errorlevel%\n', encoding="ascii")
    result = subprocess.run([str(script)], cwd=build, capture_output=True)
    (output / "build.log").write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f"Fixture helper build failed: {output / 'build.log'}")
    print(output / "compiler.exe")


if __name__ == "__main__":
    main()
