#!/usr/bin/env python3
# pylint: disable=invalid-name,missing-docstring
#
# Copyright 2023 Richard Hughes <richard@hughsie.com>
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Based on generate-man.py from fwupd, without the Jinja2 templating.

import argparse
import sys


def _replace_bookend(line: str, search: str, replace_l: str, replace_r: str) -> str:
    try:
        while line.find(search) != -1:
            it = iter(line.split(search, maxsplit=2))
            line_tmp: str = ""
            for token_before in it:
                line_tmp += token_before
                line_tmp += replace_l
                line_tmp += next(it)  # token_mid
                line_tmp += replace_r
                line_tmp += next(it)  # token_after
            line = line_tmp
    except StopIteration:
        pass
    return line


def _convert_md_to_man(data: str) -> str:
    sections = data.split("\n\n")
    troff_lines: list[str] = []

    # header
    split = sections[0].split(" ", maxsplit=4)
    if split[0] != "%" or split[3] != "|":
        print(
            "no man header detected, expected something like "
            f"'% colormgr(1) 1.4.9 | colormgr man page' and got {sections[0]}"
        )
        sys.exit(1)
    man_cmd = split[1][:-3]
    man_sect = int(split[1][-2:-1])
    troff_lines.append(f'.TH "{man_cmd}" "{man_sect}" "" {split[2]} "{split[4]}"')
    troff_lines.append(".hy")  # hyphenate

    # content
    in_name_section = False
    for section in sections[1:]:
        lines = section.split("\n")
        sectkind: str = ".PP"  # begin a new paragraph

        # convert markdown headers to section headers
        if lines[-1].startswith("##"):
            lines = [lines[-1].strip("#").strip()]
            sectkind = ".SH"
            # track if we're entering the NAME section
            in_name_section = lines[0].upper() == "NAME"

        # join long lines
        line = ""
        for line_tmp in lines:
            if not line_tmp:
                continue
            line_tmp = line_tmp.removeprefix("| ")
            if line_tmp.startswith("* "):
                line += ".IP \\[bu] 2\n"
                line += line_tmp[2:]
                continue
            line_tmp = line_tmp.replace("\\-", "-")
            line += line_tmp
            if line_tmp.endswith("."):
                line += " "
            line += " "

        # make bold and italic, and add smart quotes
        line = _replace_bookend(line, "**", "\\f[B]", "\\f[R]")
        line = _replace_bookend(line, "`", "\\f[B]", "\\f[R]")
        line = _replace_bookend(line, "*", "\\f[I]", "\\f[R]")
        line = _replace_bookend(line, '"', "“", "”")

        # in the NAME section, replace em dash with troff hyphen for whatis compatibility
        if in_name_section and sectkind != ".SH":
            line = line.replace("—", "\\-")

        # section headers must be on same line for apropos/whatis compatibility
        if sectkind == ".SH":
            troff_lines.append(f"{sectkind} {line.strip()}")
        else:
            troff_lines.append(sectkind)
            troff_lines.append(line)

    # success
    return "\n".join(troff_lines) + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output")
    parser.add_argument(
        "-r", "--replace", action="append", nargs=2, metavar=("symbol", "version")
    )
    args, argv = parser.parse_known_args()
    if len(argv) != 1:
        print(f"usage: {sys.argv[0]} MARKDOWN [-o TROFF]\n")
        sys.exit(1)

    with open(argv[0], encoding="utf-8") as f:
        rendered = f.read()
    for key, value in args.replace or []:
        rendered = rendered.replace("{{" + key + "}}", value)
    out = _convert_md_to_man(rendered)

    # success
    if args.output:
        with open(args.output, "wb") as f:
            f.write(out.encode())
    else:
        print(out)
