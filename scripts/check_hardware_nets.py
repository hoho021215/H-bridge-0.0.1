#!/usr/bin/env python3
"""Compare the supplied KiCad schematic connectivity with PCB pad net groups.

Standard library only. This is a design-specific logical check, not KiCad DRC.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys
from collections import defaultdict


EXPECTED_COMMON_PADS = 178
MCU_ASSIGNMENT = {
    "J1.1": "PC0 / TIM1_CH1 / A_H",
    "J1.3": "PA7 / TIM1_CH1N / A_L",
    "J1.4": "PC1 / TIM1_CH2 / B_H",
    "J1.6": "PE10 / TIM1_CH2N / B_L",
    "Currentsensor1.1": "PA0 / ADC1_IN1",
}
EXPECTED_NETS = {
    "J1.1": "PWMA", "J1.2": "3.3GND", "J1.3": "PWMA~",
    "J1.4": "PWMB", "J1.5": "3.3GND", "J1.6": "PWMB~",
    "Currentsensor1.1": "STM32_ADC", "Currentsensor1.2": "3.3GND",
    "Motorout1.1": "OUT+", "Motorout1.2": "OUT-",
    "DCV1.1": "DCV", "DCV1.2": "15GND",
    "15V1.1": "+15V", "15V1.2": "15GND",
    "3.3V1.1": "+3.3V", "3.3V1.2": "3.3GND",
    "IC1.5": "+3.3V", "IC2.5": "+3.3V",
    "IC1.6": "unconnected-(IC1-DT-Pad6)",
    "IC2.6": "unconnected-(IC2-DT-Pad6)",
    "IC3.1": "Current_Sensor", "IC3.2": "Current_Sensor",
    "IC3.3": "OUT+", "IC3.4": "OUT+",
    "IC3.5": "3.3GND", "IC3.8": "+3.3V",
}


def parse_sexpr(path):
    tokens = iter(re.findall(r'"(?:\\.|[^"\\])*"|[()]|[^\s()]+',
                             path.read_text(encoding="utf-8")))

    def read(token):
        if token == "(":
            result = []
            for child in tokens:
                if child == ")":
                    return result
                result.append(read(child))
            raise ValueError(f"Unclosed S-expression: {path}")
        if token == ")":
            raise ValueError(f"Unexpected closing parenthesis: {path}")
        return json.loads(token) if token.startswith('"') else token

    result = read(next(tokens))
    if next(tokens, None) is not None:
        raise ValueError(f"Unexpected trailing content: {path}")
    return result


def children(node, tag):
    return [item for item in node[1:]
            if isinstance(item, list) and item and item[0] == tag]


def first(node, tag, default=None):
    matches = children(node, tag)
    return matches[0] if matches else default


def point(values):
    return tuple(round(float(value), 4) for value in values[:2])


class Groups:
    def __init__(self, points):
        self.parent = {item: item for item in points}

    def root(self, item):
        while self.parent[item] != item:
            self.parent[item] = self.parent[self.parent[item]]
            item = self.parent[item]
        return item

    def join(self, left, right):
        self.parent[self.root(left)] = self.root(right)


def on_segment(p, a, b):
    cross = (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])
    return (abs(cross) < 0.00001
            and min(a[0], b[0]) - 0.00001 <= p[0] <= max(a[0], b[0]) + 0.00001
            and min(a[1], b[1]) - 0.00001 <= p[1] <= max(a[1], b[1]) + 0.00001)


def schematic_groups(schematic):
    if schematic[0] != "kicad_sch" or children(schematic, "sheet"):
        raise ValueError("Only a single-sheet kicad_sch is supported by this check")
    if children(schematic, "bus") or children(schematic, "bus_entry"):
        raise ValueError("Bus connectivity is outside this check's scope")
    libraries = {item[1]: item for item in children(first(schematic, "lib_symbols"), "symbol")}
    pins, points, labels, segments = {}, set(), [], []
    no_connect = {point(first(item, "at")[1:]) for item in children(schematic, "no_connect")}
    for symbol in children(schematic, "symbol"):
        properties = {item[1]: item[2] for item in children(symbol, "property")}
        reference = properties["Reference"]
        library_id = first(symbol, "lib_id")[1]
        sx, sy, degrees = map(float, first(symbol, "at")[1:])
        angle = math.radians(degrees)
        unit = int(first(symbol, "unit", ["unit", "1"])[1])
        mirror = first(symbol, "mirror", ["mirror", ""])[1]
        for part in children(libraries[library_id], "symbol"):
            part_unit = int(part[1].rsplit("_", 2)[-2])
            if part_unit not in (0, unit):
                continue
            for pin in children(part, "pin"):
                x, y = map(float, first(pin, "at")[1:3])
                if mirror == "x":
                    y = -y
                elif mirror == "y":
                    x = -x
                location = point((sx + math.cos(angle) * x - math.sin(angle) * y,
                                  sy - math.sin(angle) * x - math.cos(angle) * y))
                number = first(pin, "number")[1]
                key = f"{reference}.{number}"
                if key in pins and pins[key] != location:
                    raise ValueError(f"Multiple positions for schematic pin {key}")
                pins[key] = location
                points.add(location)
                # This design renames Earth/GND symbols to 3.3GND/15GND.
                if library_id.startswith("power:") and library_id != "power:PWR_FLAG":
                    labels.append((location, properties["Value"]))
    for tag in ("label", "global_label", "hierarchical_label"):
        for label in children(schematic, tag):
            labels.append((point(first(label, "at")[1:]), label[1]))
    for wire in children(schematic, "wire"):
        xy = children(first(wire, "pts"), "xy")
        a, b = point(xy[0][1:]), point(xy[1][1:])
        segments.append((a, b))
        points.update((a, b))
    points.update(point(first(item, "at")[1:]) for item in children(schematic, "junction"))
    points.update(location for location, _ in labels)
    groups = Groups(points)
    # Wire endpoints, pins, labels and explicit junctions connect on a segment.
    # Two crossing lines without one of those points do not create a junction.
    for a, b in segments:
        for location in points:
            if on_segment(location, a, b):
                groups.join(a, location)
    label_locations = {}
    for location, name in labels:
        if name in label_locations:
            groups.join(location, label_locations[name])
        else:
            label_locations[name] = location
    pin_groups = {pin: groups.root(location) for pin, location in pins.items()}
    group_labels = defaultdict(set)
    for location, name in labels:
        group_labels[groups.root(location)].add(name)
    nc_pins = {pin for pin, location in pins.items() if location in no_connect}
    return pin_groups, group_labels, nc_pins


def pcb_pads(board):
    if board[0] != "kicad_pcb":
        raise ValueError("Expected kicad_pcb input")
    pads, values = {}, {}
    for footprint in children(board, "footprint"):
        properties = {item[1]: item[2] for item in children(footprint, "property")}
        reference = properties.get("Reference", "")
        if not reference or "*" in reference:
            continue
        values[reference] = properties.get("Value", "")
        for pad in children(footprint, "pad"):
            net = first(pad, "net")
            if not pad[1] or net is None:
                continue
            key = f"{reference}.{pad[1]}"
            if key in pads and pads[key] != net[-1]:
                raise ValueError(f"Conflicting PCB nets for {key}")
            pads[key] = net[-1]
    return pads, values


def check(schematic_path, pcb_path):
    schematic, board = parse_sexpr(schematic_path), parse_sexpr(pcb_path)
    sch_pins, sch_labels, nc_pins = schematic_groups(schematic)
    pads, values = pcb_pads(board)
    shared = sorted(sch_pins.keys() & pads.keys())
    errors = []
    if len(shared) != EXPECTED_COMMON_PADS:
        errors.append(f"Expected {EXPECTED_COMMON_PADS} common pads, got {len(shared)}")
    sch_to_pcb, pcb_to_sch = defaultdict(set), defaultdict(set)
    for pin in shared:
        sch_to_pcb[sch_pins[pin]].add(pads[pin])
        pcb_to_sch[pads[pin]].add(sch_pins[pin])
    for group, nets in sch_to_pcb.items():
        if len(nets) != 1:
            members = [pin for pin in shared if sch_pins[pin] == group]
            errors.append(f"Schematic-connected pins split across PCB nets: {members}: {sorted(nets)}")
    for net, groups in pcb_to_sch.items():
        if len(groups) != 1:
            members = [pin for pin in shared if pads[pin] == net]
            errors.append(f"PCB net joins separate schematic groups: {net}: {members}")
    for pin, expected in EXPECTED_NETS.items():
        if pads.get(pin) != expected:
            errors.append(f"Wiring contract: {pin} expected {expected!r}, got {pads.get(pin)!r}")
    for pin in ("IC1.6", "IC2.6"):
        if pin not in nc_pins:
            errors.append(f"Expected explicit schematic no-connect on DT pin {pin}")
    if values.get("IC3") != "TMCS1108A3BQDRQ1":
        errors.append(f"Expected A3B sensor Value, got {values.get('IC3')!r}")
    report_pins = [pin for pin in EXPECTED_NETS if pin.startswith(
        ("J1.", "Currentsensor1.", "Motorout1.", "DCV1.", "15V1.", "3.3V1."))]
    mapping = [{"pad": pin, "net": pads.get(pin),
                "schematic_labels": sorted(sch_labels[sch_pins[pin]]) if pin in sch_pins else [],
                "proposed_mcu": MCU_ASSIGNMENT.get(pin)} for pin in report_pins]
    return {"status": "FAIL" if errors else "PASS", "common_pads": len(shared),
            "schematic_version": first(schematic, "version")[1],
            "pcb_version": first(board, "version")[1],
            "sha256": {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in (schematic_path, pcb_path)},
            "pinmap": mapping, "errors": errors,
            "scope": "Logical connectivity only; no routing DRC, assembly or waveform verification."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    repository = Path(__file__).resolve().parent.parent
    parser.add_argument("--schematic", type=Path, default=repository / "hardware/Hbridge.kicad_sch")
    parser.add_argument("--pcb", type=Path, default=repository / "hardware/Hbridge.kicad_pcb")
    parser.add_argument("--json", action="store_true", help="Print a machine-readable report")
    args = parser.parse_args()
    try:
        report = check(args.schematic, args.pcb)
    except (OSError, ValueError, KeyError, IndexError, TypeError, StopIteration) as error:
        print(f"FAIL: cannot check hardware inputs: {error}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print(f"{report['status']}: {report['common_pads']} common pads; schematic/PCB connectivity check")
        for entry in report["pinmap"]:
            assignment = f" -> {entry['proposed_mcu']}" if entry["proposed_mcu"] else ""
            print(f"  {entry['pad']:20s} {str(entry['net']):12s}{assignment}")
        for error in report["errors"]:
            print(f"  ERROR: {error}")
        print(report["scope"])
    return 1 if report["errors"] else 0


if __name__ == "__main__":
    sys.exit(main())
