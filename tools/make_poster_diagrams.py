#!/usr/bin/env python3
from pathlib import Path
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

OUT_DIR = Path(__file__).resolve().parents[1] / "docs" / "diagrams"
OUT_DIR.mkdir(parents=True, exist_ok=True)


def box(ax, x, y, w, h, title, lines, fc="#f8fafc", ec="#334155", title_color="#0f172a"):
    patch = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.03",
                           linewidth=1.8, edgecolor=ec, facecolor=fc)
    ax.add_patch(patch)
    ax.text(x + 0.02 * w, y + h - 0.12 * h, title, fontsize=12, fontweight="bold", color=title_color, va="top")
    t = "\n".join(lines)
    ax.text(x + 0.02 * w, y + h - 0.26 * h, t, fontsize=10, color="#1e293b", va="top", linespacing=1.35)


def arrow(ax, x1, y1, x2, y2, label="", color="#2563eb", rad=0.0):
    style = f"arc3,rad={rad}"
    arr = FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>", mutation_scale=14,
                          linewidth=2.1, color=color, connectionstyle=style)
    ax.add_patch(arr)
    if label:
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        ax.text(mx, my + 0.02, label, fontsize=9, color=color, ha="center", va="bottom", fontweight="bold")


def make_architecture():
    fig, ax = plt.subplots(figsize=(14, 8), dpi=180)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    ax.set_title("Battleship System Architecture", fontsize=18, fontweight="bold", pad=20)

    box(ax, 0.04, 0.62, 0.24, 0.26, "Input Layer", [
        "Joystick X (A0)",
        "Joystick Y (A1)",
        "Joystick Button (A2)",
        "Debounce + Edge Detection"
    ], fc="#ecfeff", ec="#0e7490")

    box(ax, 0.37, 0.52, 0.26, 0.38, "Arduino Game Engine", [
        "State Machine:",
        "INTRO -> P1_PLACE -> P2_PLACE",
        "-> P1_TURN <-> P2_TURN -> GAME_OVER",
        "",
        "Board Data + Rules",
        "Hit/Miss Resolution",
        "Turn Switching + Win Detection",
        "LED Render Mapping"
    ], fc="#eff6ff", ec="#1d4ed8")

    box(ax, 0.72, 0.62, 0.24, 0.26, "Display Layer", [
        "MAX7219 8x32 Matrix",
        "Ship / Hit / Miss Patterns",
        "Cursor + Turn Indicators",
        "Start/Win Screens"
    ], fc="#f0fdf4", ec="#15803d")

    box(ax, 0.72, 0.20, 0.24, 0.20, "Optional Audio Layer", [
        "Buzzer / Speaker",
        "Hit/Miss/Win Events"
    ], fc="#fff7ed", ec="#c2410c")

    box(ax, 0.04, 0.20, 0.24, 0.20, "Power + Wiring", [
        "5V + GND Shared",
        "SPI: D11 D10 D13",
        "Analog: A0 A1 A2"
    ], fc="#fdf4ff", ec="#a21caf")

    arrow(ax, 0.28, 0.75, 0.37, 0.75, "Control Input", color="#0e7490")
    arrow(ax, 0.63, 0.75, 0.72, 0.75, "Frame Data", color="#15803d")
    arrow(ax, 0.63, 0.42, 0.72, 0.30, "Event Signals", color="#c2410c", rad=-0.07)
    arrow(ax, 0.28, 0.30, 0.37, 0.52, "Electrical I/O", color="#a21caf", rad=0.08)

    ax.text(0.5, 0.04, "ECE 5 Winter 2026 • Battleship Project", ha="center", fontsize=10, color="#334155")

    svg = OUT_DIR / "battleship_architecture.svg"
    png = OUT_DIR / "battleship_architecture.png"
    fig.savefig(svg, bbox_inches="tight")
    fig.savefig(png, bbox_inches="tight")
    plt.close(fig)


def pin_list(ax, x, y, title, pins, color):
    w, h = 0.22, 0.62
    patch = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.03",
                           linewidth=1.8, edgecolor=color, facecolor="#ffffff")
    ax.add_patch(patch)
    ax.text(x + 0.02, y + h - 0.05, title, fontsize=12, fontweight="bold", color=color, va="top")
    yy = y + h - 0.12
    coords = {}
    for p in pins:
        ax.text(x + 0.02, yy, p, fontsize=10, color="#0f172a", va="center")
        coords[p] = (x + w, yy)
        yy -= 0.085
    return coords


def make_wiring():
    fig, ax = plt.subplots(figsize=(14, 8), dpi=180)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    ax.set_title("Detailed Wiring Diagram (Arduino Battleship)", fontsize=18, fontweight="bold", pad=20)

    joy_pins = ["VRx -> A0", "VRy -> A1", "SW -> A2", "VCC -> 5V", "GND -> GND"]
    max_pins = ["DIN -> D11", "CS -> D10", "CLK -> D13", "VCC -> 5V", "GND -> GND"]
    ard_pins = ["A0", "A1", "A2", "D11", "D10", "D13", "5V", "GND"]

    joy = pin_list(ax, 0.04, 0.18, "Joystick Module", joy_pins, "#0e7490")
    ard = pin_list(ax, 0.39, 0.18, "Arduino Uno/Nano", ard_pins, "#1d4ed8")
    mx = pin_list(ax, 0.74, 0.18, "MAX7219 8x32", max_pins, "#15803d")

    def connect(src, dst, color):
        (x1, y1), (x2, y2) = src, dst
        a = FancyArrowPatch((x1 + 0.005, y1), (x2 - 0.18, y2), arrowstyle="-", linewidth=2.2,
                            color=color, connectionstyle="arc3,rad=0")
        ax.add_patch(a)

    pin_y = {}
    y0 = 0.18 + 0.62 - 0.12
    for p in ard_pins:
        pin_y[p] = y0
        y0 -= 0.085

    # Joystick to Arduino
    connect(joy["VRx -> A0"], (0.39, pin_y["A0"]), "#0891b2")
    connect(joy["VRy -> A1"], (0.39, pin_y["A1"]), "#0ea5e9")
    connect(joy["SW -> A2"], (0.39, pin_y["A2"]), "#06b6d4")
    connect(joy["VCC -> 5V"], (0.39, pin_y["5V"]), "#dc2626")
    connect(joy["GND -> GND"], (0.39, pin_y["GND"]), "#111827")

    # Arduino to MAX7219
    connect((0.39 + 0.22, pin_y["D11"]), mx["DIN -> D11"], "#f97316")
    connect((0.39 + 0.22, pin_y["D10"]), mx["CS -> D10"], "#ea580c")
    connect((0.39 + 0.22, pin_y["D13"]), mx["CLK -> D13"], "#c2410c")
    connect((0.39 + 0.22, pin_y["5V"]), mx["VCC -> 5V"], "#dc2626")
    connect((0.39 + 0.22, pin_y["GND"]), mx["GND -> GND"], "#111827")

    legend = [
        ("Power (5V)", "#dc2626"),
        ("Ground", "#111827"),
        ("Joystick Signals", "#0891b2"),
        ("SPI Signals", "#ea580c"),
    ]
    lx, ly = 0.39, 0.08
    ax.text(lx, ly + 0.06, "Wire Color Legend", fontsize=11, fontweight="bold", color="#334155")
    for i, (name, col) in enumerate(legend):
        yy = ly - i * 0.03
        ax.plot([lx, lx + 0.05], [yy, yy], color=col, linewidth=3)
        ax.text(lx + 0.06, yy, name, va="center", fontsize=9, color="#334155")

    ax.text(0.5, 0.02, "Pins: Joystick A0/A1/A2 • MAX7219 D11/D10/D13 • Shared 5V/GND", ha="center", fontsize=10, color="#334155")

    svg = OUT_DIR / "battleship_wiring_detailed.svg"
    png = OUT_DIR / "battleship_wiring_detailed.png"
    fig.savefig(svg, bbox_inches="tight")
    fig.savefig(png, bbox_inches="tight")
    plt.close(fig)


def main():
    make_architecture()
    make_wiring()
    print(f"Saved diagrams to: {OUT_DIR}")


if __name__ == "__main__":
    main()
