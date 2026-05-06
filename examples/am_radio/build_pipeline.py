#!/usr/bin/env python3
"""Generate examples/am_radio/am_radio.piper from the Python bindings.

One-shot: writes the .piper file alongside this script. Re-run after
editing the topology in this script. The .piper is the artifact users
load; this generator is the source of truth that produced it.

Usage:
    python examples/am_radio/build_pipeline.py
"""
import os
import sys

import piper

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "am_radio.piper")


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

    # Stages: source / process / probe. Open-loop DAG, multi-stage is
    # purely pedagogical (see docs/architecture.md).
    g.add_stage(make_stage("source",  0x4E69C6FF))   # blue
    g.add_stage(make_stage("process", 0xFF6060FF))   # red
    g.add_stage(make_stage("probe",   0x2D882DFF))   # green (legible)

    # ---- Sources -----------------------------------------------------
    sin_f      = reg.find("sin_wave<float>")
    constant_f = reg.find("constant<float>")
    add_f      = reg.find("add<float>")
    mul_f      = reg.find("multiply<float>")
    abs_f      = reg.find("abs<float>")
    lp_f       = reg.find("low_pass<float>")
    rng        = reg.find("random")
    probe_f    = reg.find("external_output<float>")

    envelope_gen = g.add_node(sin_f,      "envelope_gen", "source", at(-200,   0))
    carrier_gen  = g.add_node(sin_f,      "carrier_gen",  "source", at(-200, 100))
    noise_gen    = g.add_node(rng,        "noise_gen",    "source", at(-200, 300))
    offset       = g.add_node(constant_f, "offset",       "source", at(-200, 200))

    g.set_attr_value(envelope_gen, "frequency", "5.0")    # 5 Hz envelope
    g.set_attr_value(envelope_gen, "amplitude", "0.5")    # modulation index m
    g.set_attr_value(carrier_gen,  "frequency", "200.0")  # 200 Hz carrier
    g.set_attr_value(carrier_gen,  "amplitude", "1.0")
    g.set_attr_value(offset,       "value",     "1.0")    # the "1" in (1 + m*sin)
    g.set_attr_value(noise_gen,    "min",       "-0.1")
    g.set_attr_value(noise_gen,    "max",       "0.1")

    # ---- Process: modulate, add channel noise, demodulate ------------
    envelope_sum = g.add_node(add_f, "envelope_sum", "process", at(   0,  50))
    modulator    = g.add_node(mul_f, "modulator",    "process", at( 200,  50))
    channel      = g.add_node(add_f, "channel",      "process", at( 400, 100))
    rectifier    = g.add_node(abs_f, "rectifier",    "process", at( 600, 100))
    demod_filter = g.add_node(lp_f,  "demod_filter", "process", at( 800, 100))

    # Cutoff well above the 5 Hz envelope and below the 200 Hz carrier
    # so the lowpass keeps the envelope and rejects the rectified
    # carrier ripple.
    g.set_attr_value(demod_filter, "cutoff", "20.0")

    # ---- Probes (one external_output per measurement point) ----------
    # The host extracts each probe via engine.output<float>(name) using
    # the "name" member; we mirror the node name for clarity.
    def make_probe(node_name, pos):
        nid = g.add_node(probe_f, node_name, "probe", pos)
        g.set_attr_value(nid, "name", node_name)
        return nid

    probe_envelope    = make_probe("probe_envelope",    at(   0, 200))
    probe_carrier     = make_probe("probe_carrier",     at(-100, 250))
    probe_modulated   = make_probe("probe_modulated",   at( 200, 200))
    probe_transmitted = make_probe("probe_transmitted", at( 400, 250))
    probe_rectified   = make_probe("probe_rectified",   at( 600, 250))
    probe_recovered   = make_probe("probe_recovered",   at( 800, 250))

    # ---- Wire it -----------------------------------------------------
    pin = piper.PinRef
    # envelope = 1 + 0.5 * sin(2*pi*5*t)
    g.add_link(pin(envelope_gen, "out"), pin(envelope_sum, "a"), "float")
    g.add_link(pin(offset,       "out"), pin(envelope_sum, "b"), "float")
    # transmitted = envelope * carrier + noise
    g.add_link(pin(envelope_sum, "out"), pin(modulator,    "a"), "float")
    g.add_link(pin(carrier_gen,  "out"), pin(modulator,    "b"), "float")
    g.add_link(pin(modulator,    "out"), pin(channel,      "a"), "float")
    g.add_link(pin(noise_gen,    "out"), pin(channel,      "b"), "float")
    # demodulator: full-wave rectify, then lowpass
    g.add_link(pin(channel,      "out"), pin(rectifier,    "in"), "float")
    g.add_link(pin(rectifier,    "out"), pin(demod_filter, "in"), "float")
    # Probes
    g.add_link(pin(envelope_sum, "out"), pin(probe_envelope,    "in"), "float")
    g.add_link(pin(carrier_gen,  "out"), pin(probe_carrier,     "in"), "float")
    g.add_link(pin(modulator,    "out"), pin(probe_modulated,   "in"), "float")
    g.add_link(pin(channel,      "out"), pin(probe_transmitted, "in"), "float")
    g.add_link(pin(rectifier,    "out"), pin(probe_rectified,   "in"), "float")
    g.add_link(pin(demod_filter, "out"), pin(probe_recovered,   "in"), "float")

    # Default mode: enable everything.
    profile = piper.ModeProfile()
    profile.name = "default"
    profile.per_node = {nid: "enable" for nid in [
        envelope_gen, carrier_gen, noise_gen, offset,
        envelope_sum, modulator, channel, rectifier, demod_filter,
        probe_envelope, probe_carrier, probe_modulated,
        probe_transmitted, probe_rectified, probe_recovered,
    ]}
    g.add_mode_profile(profile)
    g.set_default_mode_name("default")

    text = piper.v2.serialize(g)
    with open(OUT, "w") as f:
        f.write(text)
    print(f"wrote {OUT}: {len(g.nodes())} nodes, {len(g.links())} links")


if __name__ == "__main__":
    sys.exit(main())
