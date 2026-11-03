"""Offline single-motor parameter screening; physical acceptance is separate."""
import argparse
import copy
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "hardware/drv8833/arrival-record.template.json"
AXES = ("propulsion", "yaw", "lift")


class InvalidRecord(ValueError):
    pass


def screen(record):
    checks = []

    def add(key, status, detail):
        checks.append({"id": key, "status": status, "detail": detail})

    def obj(value, key):
        if not isinstance(value, dict):
            raise InvalidRecord(f"{key}: expected object")
        return value

    def field(section, key, path, kind="positive"):
        label = f"{path}.{key}"
        if key not in section:
            raise InvalidRecord(f"{label}: missing field; use null for unknown")
        value = section[key]
        if value is None:
            add(label, "pending", "待填写")
            return None
        if kind == "text":
            valid = isinstance(value, str) and bool(value.strip())
        elif kind == "boolean":
            valid = type(value) is bool
        else:
            valid = type(value) in (int, float) and math.isfinite(value)
            valid = valid and (value > 0 if kind == "positive" else value >= 0)
        if not valid:
            raise InvalidRecord(f"{label}: invalid {kind} value")
        return value

    def compare(key, values, condition, detail):
        if any(v is None for v in values):
            add(key, "pending", detail)
        else:
            add(key, "passed" if condition(*values) else "incompatible", detail)

    obj(record, "record")
    if type(record.get("schema_version")) is not int or record["schema_version"] != 1:
        raise InvalidRecord("schema_version must be 1")
    axis = record.get("selected_axis")
    if axis not in AXES:
        raise InvalidRecord("selected_axis must be propulsion, yaw, or lift")
    motors = record.get("motors")
    if not isinstance(motors, list) or len(motors) != 3:
        raise InvalidRecord("motors must contain the three axis records")
    for motor in motors:
        obj(motor, "motor")
    if sorted(str(m.get("axis")) for m in motors) != sorted(AXES):
        raise InvalidRecord("motors must have each axis exactly once")
    motor = next(m for m in motors if m["axis"] == axis)
    sections = {"driver": obj(record.get("driver"), "driver"),
                "motor": motor,
                "test_power": obj(record.get("test_power"), "test_power"),
                "harness": obj(record.get("harness"), "harness")}
    fields = {
        "driver": {"model": "text", "package": "text", "board_id": "text",
                   "rating_vm_min_v": "positive", "rating_vm_max_v": "positive",
                   "rating_ambient_c": "nonnegative", "continuous_per_bridge_a": "positive",
                   "peak_per_bridge_a": "positive", "peak_duration_ms": "positive",
                   "logic_3v3_confirmed": "boolean", "standby_3v3_confirmed": "boolean"},
        "motor": {"model": "text", "type": "text", "vm_min_v": "positive",
                  "vm_max_v": "positive", "no_load_a": "positive", "loaded_a": "positive",
                  "startup_a": "positive", "startup_ms": "positive"},
        "test_power": {"kind": "text", "vm_min_v": "positive", "vm_max_v": "positive",
                       "continuous_a": "positive", "peak_a": "positive",
                       "peak_duration_ms": "positive"},
        "harness": {"connector_type": "text", "continuous_a": "positive", "peak_a": "positive",
                    "peak_duration_ms": "positive", "polarity_checked": "boolean"},
    }
    values = {}
    for name, section in sections.items():
        values[name] = {key: field(section, key, name, kind) for key, kind in fields[name].items()}
        field(section, "source", name, "text")
    d, m, p, h = (values[n] for n in ("driver", "motor", "test_power", "harness"))
    compare("driver_model", [d["model"], d["package"]],
            lambda model, pkg: model.upper() == "DRV8833" and pkg in ("PW", "PWP", "RTY"),
            "确认 DRV8833 完整丝印与封装")
    compare("motor_type", [m["type"]], lambda kind: kind == "two_wire_brushed_dc",
            "两线有刷直流电机")
    compare("motor_current_order", [m["no_load_a"], m["loaded_a"]],
            lambda no_load, loaded: no_load <= loaded,
            "同电压下空载电流不高于带载电流")
    compare("power_kind", [p["kind"]], lambda kind: kind in ("current_limited_supply", "battery"),
            "记录限流电源或已核实的电池")
    if p["kind"] == "battery":
        cells = field(sections["test_power"], "cells", "test_power")
        compare("battery_cells", [cells], lambda value: type(value) is int and value == 1,
                "当前接线方案采用 1S 电池；其他节数重新评审")
        compare("battery_full_charge", [p["vm_max_v"]], lambda value: value >= 4.2,
                "1S 电池按满充 4.2V 核对")
    for name, lo, hi in (("driver", d["rating_vm_min_v"], d["rating_vm_max_v"]),
                         ("motor", m["vm_min_v"], m["vm_max_v"]),
                         ("test_power", p["vm_min_v"], p["vm_max_v"])):
        compare(f"{name}_voltage_order", [lo, hi], lambda a, b: a <= b, "电压下限 ≤ 上限")
    compare("chip_voltage", [p["vm_min_v"], p["vm_max_v"]],
            lambda lo, hi: 2.7 <= lo <= hi <= 10.8, "DRV8833 VM 2.7–10.8V")
    compare("board_current_voltage_conditions",
            [d["rating_vm_min_v"], d["rating_vm_max_v"], p["vm_min_v"], p["vm_max_v"]],
            lambda lo, hi, plo, phi: lo <= plo <= phi <= hi,
            "板级电流能力的数据条件覆盖完整供电范围")
    compare("motor_voltage", [m["vm_min_v"], m["vm_max_v"], p["vm_min_v"], p["vm_max_v"]],
            lambda lo, hi, plo, phi: lo <= plo <= phi <= hi, "电机允许电压覆盖供电范围")
    for key in ("logic_3v3_confirmed", "standby_3v3_confirmed"):
        compare(key, [d[key]], lambda value: value is True, "3.3V 控制电平兼容")
    compare("harness_polarity", [h["polarity_checked"]], lambda value: value is True,
            "成品线束极性与端子逐项核对")
    for name, continuous, peak, duration in (
            ("driver", d["continuous_per_bridge_a"], d["peak_per_bridge_a"], d["peak_duration_ms"]),
            ("test_power", p["continuous_a"], p["peak_a"], p["peak_duration_ms"]),
            ("harness", h["continuous_a"], h["peak_a"], h["peak_duration_ms"])):
        compare(f"{name}_current_order", [continuous, peak], lambda a, b: a <= b,
                "持续额定值 ≤ 峰值额定值")
        compare(f"{name}_working_current", [continuous, m["no_load_a"], m["loaded_a"]],
                lambda cap, idle, loaded: max(idle, loaded) < cap,
                "电机工作电流小于该环节持续能力")
        compare(f"{name}_startup_current", [peak, m["startup_a"], duration, m["startup_ms"]],
                lambda cap, startup, allowed_ms, startup_ms: startup <= cap and startup_ms <= allowed_ms,
                "启动电流与持续时间均在该环节允许范围内")
    states = {c["status"] for c in checks}
    status = "incompatible" if "incompatible" in states else (
        "pending" if "pending" in states else "parameters_complete")
    return {"schema_version": 1, "status": status, "selected_axis": axis,
            "scope": "single_motor_parameter_screening_only",
            "hardware_released": False,
            "next_step": "按单电机台架单完成固件、实物接线、限流电流与温升验收",
            "mass_collection": "到货时统一称量，不参与本次电气参数筛查",
            "checks": checks}


def self_test():
    template = json.loads(TEMPLATE.read_text(encoding="utf-8"))
    assert screen(template)["status"] == "pending"
    example = copy.deepcopy(template)
    example["driver"] = dict(model="DRV8833", package="PWP", board_id="FICTIONAL TEST ONLY",
                             rating_vm_min_v=3.0, rating_vm_max_v=4.2, rating_ambient_c=25,
                             continuous_per_bridge_a=0.8, peak_per_bridge_a=1.2, peak_duration_ms=100,
                             logic_3v3_confirmed=True, standby_3v3_confirmed=True, source="test fixture")
    example["motors"][0].update(model="FICTIONAL TEST ONLY", type="two_wire_brushed_dc",
                                vm_min_v=3.0, vm_max_v=4.2, no_load_a=0.1, loaded_a=0.3,
                                startup_a=0.7, startup_ms=50, source="test fixture")
    example["test_power"] = dict(kind="current_limited_supply", cells=None, vm_min_v=3.0,
                                 vm_max_v=4.2, continuous_a=0.9, peak_a=1.0,
                                 peak_duration_ms=100, source="test fixture")
    example["harness"] = dict(connector_type="test fixture", continuous_a=1.0, peak_a=1.5,
                              peak_duration_ms=100, polarity_checked=True, source="test fixture")
    result = screen(example)
    assert result["status"] == "parameters_complete" and result["hardware_released"] is False
    cases = [("motor", "vm_max_v", 4.0), ("motor", "loaded_a", 0.05),
             ("driver", "rating_vm_min_v", 5.0),
             ("driver", "continuous_per_bridge_a", 0.2), ("driver", "peak_duration_ms", 20),
             ("harness", "peak_a", 0.6), ("driver", "logic_3v3_confirmed", False)]
    for section, key, value in cases:
        record = copy.deepcopy(example)
        target = record["motors"][0] if section == "motor" else record[section]
        target[key] = value
        assert screen(record)["status"] == "incompatible", (section, key)
    missing_source = copy.deepcopy(example)
    missing_source["driver"]["source"] = None
    assert screen(missing_source)["status"] == "pending"
    for value in (True, -1, float("nan"), "0.8"):
        invalid = copy.deepcopy(example)
        invalid["driver"]["continuous_per_bridge_a"] = value
        try:
            screen(invalid)
        except InvalidRecord:
            pass
        else:
            raise AssertionError("Invalid numeric value accepted")
    battery = copy.deepcopy(example)
    battery["test_power"].update(kind="battery", cells=2, vm_max_v=8.4)
    assert screen(battery)["status"] == "incompatible"
    invalid = copy.deepcopy(example)
    invalid["motors"][2]["axis"] = "propulsion"
    try:
        screen(invalid)
    except InvalidRecord:
        pass
    else:
        raise AssertionError("Duplicate axis accepted")
    print("arrival parameter screening: 16 scenarios passed (all fixtures fictional)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record", type=Path, default=TEMPLATE)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    try:
        result = screen(json.loads(args.record.read_text(encoding="utf-8-sig")))
    except (OSError, ValueError) as error:
        result = {"status": "invalid_record", "hardware_released": False, "error": str(error)}
    raw = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        if args.output.resolve() == args.record.resolve():
            parser.error("output must differ from the input record")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(raw, encoding="utf-8")
    print(json.dumps({"status": result["status"], "hardware_released": False,
                      "output": str(args.output) if args.output else None,
                      "checks": len(result.get("checks", [])),
                      "error": result.get("error")}, ensure_ascii=False) if args.output else raw, end="\n")
    return {"parameters_complete": 0, "pending": 2}.get(result["status"], 1)


if __name__ == "__main__":
    raise SystemExit(main())
