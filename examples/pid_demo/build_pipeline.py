#!/usr/bin/env python3
"""Generate examples/pid_demo/pid_demo.piper.

Closed-loop PID demo with mode-keyed gain banks. The host (the
pid_demo binary) drives a square-wave setpoint via external_input
"command" and switches modes every second to show how each gain bank
shapes the closed-loop response.

Topology:

    [control]                                          [plant]
    external_input<float> "command" ─► pid.setpoint
    preset3<float> "kp_bank"        ─► pid.kp
    preset3<float> "ki_bank"        ─► pid.ki
    preset3<float> "kd_bank"        ─► pid.kd
                                      pid.measured ◄── plant.out  (cross-stage, n-1)
                                      pid.out      ──► plant.in   (low_pass, fc=2 Hz)

    [probe]
    probe_command, probe_pid_out, probe_measured

Modes (label0/1/2 = tight/loose/bypass on every preset3):

    tight  : kp=5,   ki=2,   kd=0.1
    loose  : kp=1,   ki=0.2, kd=0
    bypass : kp=0.1, ki=0,   kd=0    (very weak P, plant dominates)

The Pid step takes the derivative on the measurement (not the error),
so a step setpoint does not kick the controller; a nonzero kd just
adds damping against the plant's velocity.

Re-run after editing this file:

    cmake --build build --target piper_py -j
    PYTHONPATH=build/py_bindings python examples/pid_demo/build_pipeline.py
"""
import os
import sys

import piper

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pid_demo.piper")


def at(x, y):
    return piper.Point(x, y)


def make_stage(name, color):
    s = piper.Stage()
    s.name  = name
    s.color = piper.rgba(color)
    return s


def main():
    reg = piper.NodeRegistry()
    piper.register_builtin_nodes(reg)

    g = piper.Graph()

    # Stage order matters: control -> plant -> probe. plant runs after
    # control, so pid.measured reads plant.out from the previous tick
    # (the n-1 break that resolves the closed-loop cycle).
    g.add_stage(make_stage("control", 0x4E69C6FF))   # blue
    g.add_stage(make_stage("plant",   0xFF6060FF))   # red
    g.add_stage(make_stage("probe",   0x2D882DFF))   # green

    ext_in_f  = reg.find("external_input<float>")
    ext_out_f = reg.find("external_output<float>")
    pid_f     = reg.find("pid<float>")
    preset3_f = reg.find("preset3<float>")
    lp_f      = reg.find("low_pass<float>")

    # ---- control stage ----------------------------------------------
    command = g.add_node(ext_in_f, "command", "control", at(-300, 0))
    g.set_attr_value(command, "name", "command")  # external API name

    def make_gain_bank(name, pos, tight, loose, bypass):
        nid = g.add_node(preset3_f, name, "control", pos)
        g.set_attr_value(nid, "label0", "tight")
        g.set_attr_value(nid, "value0", str(tight))
        g.set_attr_value(nid, "label1", "loose")
        g.set_attr_value(nid, "value1", str(loose))
        g.set_attr_value(nid, "label2", "bypass")
        g.set_attr_value(nid, "value2", str(bypass))
        return nid

    kp_bank = make_gain_bank("kp_bank", at(-300,  100), 5.0, 1.0, 0.1)
    ki_bank = make_gain_bank("ki_bank", at(-300,  200), 2.0, 0.2, 0.0)
    kd_bank = make_gain_bank("kd_bank", at(-300,  300), 0.1, 0.0, 0.0)

    pid = g.add_node(pid_f, "pid", "control", at(0, 100))

    # ---- plant stage ------------------------------------------------
    plant = g.add_node(lp_f, "plant", "plant", at(300, 100))
    g.set_attr_value(plant, "cutoff", "2.0")  # 2 Hz LP -- slow enough that PID matters

    # ---- probe stage ------------------------------------------------
    # Each probe's "name" member is what the host passes to
    # engine.output<float>(name); we mirror the node name for clarity.
    def make_probe(node_name, pos):
        nid = g.add_node(ext_out_f, node_name, "probe", pos)
        g.set_attr_value(nid, "name", node_name)
        return nid

    probe_command  = make_probe("probe_command",  at(  0, -50))
    probe_pid_out  = make_probe("probe_pid_out",  at(  0,  20))
    probe_measured = make_probe("probe_measured", at(300,   0))

    # ---- wires -------------------------------------------------------
    pin = piper.PinRef
    # PID inputs
    g.add_link(pin(command, "out"), pin(pid, "setpoint"), "float")
    g.add_link(pin(plant,   "out"), pin(pid, "measured"), "float")  # cross-stage feedback
    g.add_link(pin(kp_bank, "out"), pin(pid, "kp"),       "float")
    g.add_link(pin(ki_bank, "out"), pin(pid, "ki"),       "float")
    g.add_link(pin(kd_bank, "out"), pin(pid, "kd"),       "float")
    # PID -> plant
    g.add_link(pin(pid,   "out"), pin(plant, "in"), "float")
    # Probes
    g.add_link(pin(command, "out"), pin(probe_command,  "in"), "float")
    g.add_link(pin(pid,     "out"), pin(probe_pid_out,  "in"), "float")
    g.add_link(pin(plant,   "out"), pin(probe_measured, "in"), "float")

    # Modes: each profile labels every preset3 with the same string so
    # tight/loose/bypass switch all three gains atomically.
    for mode_name in ("tight", "loose", "bypass"):
        mp = piper.ModeProfile()
        mp.name = mode_name
        mp.per_node = {
            kp_bank: mode_name,
            ki_bank: mode_name,
            kd_bank: mode_name,
        }
        g.add_mode_profile(mp)
    g.set_default_mode_name("tight")

    text = piper.v2.serialize(g)
    with open(OUT, "w") as f:
        f.write(text)
    print(f"wrote {OUT}: {len(g.nodes())} nodes, {len(g.links())} links, "
          f"{len(g.stages())} stages, {len(g.mode_profiles())} modes")


if __name__ == "__main__":
    sys.exit(main())
