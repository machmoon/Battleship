#!/usr/bin/env python3
from pathlib import Path
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

OUT_DIR = Path(__file__).resolve().parents[1] / "docs" / "diagrams"
OUT_DIR.mkdir(parents=True, exist_ok=True)


def rbox(ax, x, y, w, h, title, lines, ec="#1e3a8a", fc="#eff6ff"):
    p = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.015,rounding_size=0.02",
                       linewidth=1.8, edgecolor=ec, facecolor=fc)
    ax.add_patch(p)
    ax.text(x + 0.015, y + h - 0.04, title, fontsize=11, fontweight="bold", color="#0f172a", va="top")
    ax.text(x + 0.015, y + h - 0.09, "\n".join(lines), fontsize=9, color="#1f2937", va="top", linespacing=1.3)


def arrow(ax, x1, y1, x2, y2, color="#2563eb"):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>", mutation_scale=14,
                                 linewidth=1.8, color=color))


def save(fig, name):
    fig.savefig(OUT_DIR / f"{name}.png", dpi=220, bbox_inches="tight")
    fig.savefig(OUT_DIR / f"{name}.svg", bbox_inches="tight")
    plt.close(fig)


def make_architecture():
    fig, ax = plt.subplots(figsize=(12, 7))
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    ax.set_title("Battleship Console Architecture", fontsize=18, fontweight="bold", pad=14)

    rbox(ax, 0.04, 0.63, 0.24, 0.23, "Input", ["Joystick 1: A0/A1/A5", "Joystick 2: A3/A4/D3", "Edge detection + dead-zone"], ec="#0e7490", fc="#ecfeff")
    rbox(ax, 0.36, 0.50, 0.28, 0.38, "Arduino Game Engine", ["State machine", "Placement / turns / shots", "Hit-miss + win detection", "Board render pipeline"], ec="#1d4ed8", fc="#eff6ff")
    rbox(ax, 0.72, 0.63, 0.24, 0.23, "Display", ["MAX7219 (32x24 logical)", "Board/cursor symbols", "Winner/game-over feedback"], ec="#15803d", fc="#f0fdf4")
    rbox(ax, 0.72, 0.22, 0.24, 0.20, "Audio (Optional)", ["Serial sound bridge", "Menu music + game SFX"], ec="#9a3412", fc="#fff7ed")
    rbox(ax, 0.04, 0.22, 0.24, 0.20, "Power/Wiring", ["SPI: D11/D10/D13", "Shared 5V + GND", "Stable supply"], ec="#7e22ce", fc="#faf5ff")

    arrow(ax, 0.28, 0.74, 0.36, 0.74, "#0e7490")
    arrow(ax, 0.64, 0.72, 0.72, 0.72, "#15803d")
    arrow(ax, 0.64, 0.55, 0.72, 0.32, "#9a3412")
    arrow(ax, 0.28, 0.30, 0.36, 0.52, "#7e22ce")

    ax.text(0.5, 0.04, "ECE 5 • Embedded Game Console", ha="center", fontsize=10, color="#334155")
    save(fig, "poster_architecture_updated")


def make_state_machine():
    fig, ax = plt.subplots(figsize=(12, 6.4))
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    ax.set_title("Battleship State Machine", fontsize=18, fontweight="bold", pad=14)

    states = [
        (0.06, 0.58, 0.15, 0.17, "INTRO"),
        (0.26, 0.58, 0.18, 0.17, "P1_PLACE"),
        (0.48, 0.58, 0.18, 0.17, "P2_PLACE"),
        (0.26, 0.30, 0.18, 0.17, "P1_TURN"),
        (0.48, 0.30, 0.18, 0.17, "P2_TURN"),
        (0.74, 0.44, 0.18, 0.17, "GAME_OVER"),
    ]

    for x, y, w, h, t in states:
        rbox(ax, x, y, w, h, t, [], ec="#334155", fc="#f8fafc")

    arrow(ax, 0.21, 0.665, 0.26, 0.665)
    arrow(ax, 0.44, 0.665, 0.48, 0.665)
    arrow(ax, 0.35, 0.58, 0.35, 0.47)
    arrow(ax, 0.57, 0.58, 0.57, 0.47)
    arrow(ax, 0.44, 0.385, 0.48, 0.385)
    arrow(ax, 0.57, 0.385, 0.44, 0.385)
    arrow(ax, 0.66, 0.665, 0.74, 0.53)
    arrow(ax, 0.66, 0.385, 0.74, 0.53)
    arrow(ax, 0.74, 0.44, 0.21, 0.665)

    ax.text(0.26, 0.77, "Start", fontsize=9)
    ax.text(0.46, 0.77, "P1 done", fontsize=9)
    ax.text(0.33, 0.50, "All ships placed", fontsize=9)
    ax.text(0.52, 0.50, "All ships placed", fontsize=9)
    ax.text(0.42, 0.42, "Shot", fontsize=9)
    ax.text(0.51, 0.34, "Turn swap", fontsize=9)
    ax.text(0.69, 0.60, "All enemy ships sunk", fontsize=9)
    ax.text(0.41, 0.23, "Restart", fontsize=9)

    save(fig, "poster_state_machine_updated")


def make_timeline():
    fig, ax = plt.subplots(figsize=(12, 4.8))
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    ax.set_title("5-Lab Timeline", fontsize=18, fontweight="bold", pad=12)

    labs = [
        "Lab 1\nScope + wiring",
        "Lab 2\nDisplay + joystick",
        "Lab 3\nGame logic",
        "Lab 4\nUI/readability",
        "Lab 5\nTesting + demo prep",
    ]
    xs = [0.08, 0.26, 0.44, 0.62, 0.80]
    ax.plot([0.08, 0.90], [0.5, 0.5], color="#334155", linewidth=2.5)

    for x, label in zip(xs, labs):
        ax.scatter([x], [0.5], s=140, color="#2563eb", zorder=3)
        rbox(ax, x - 0.08, 0.58, 0.16, 0.22, label.split("\n")[0], [label.split("\n")[1]], ec="#1d4ed8", fc="#eff6ff")

    save(fig, "poster_timeline_updated")


def make_testing_table():
    fig, ax = plt.subplots(figsize=(12, 5.6))
    ax.axis("off")
    ax.set_title("Testing Summary", fontsize=18, fontweight="bold", pad=12)

    rows = [
        ["Joystick input", "Pass", "Stable movement + button edges"],
        ["Display mapping", "Pass", "All sections addressed correctly"],
        ["Placement rules", "Pass", "No overlap / bounds enforced"],
        ["Turn switching", "Pass", "Alternates after valid shots"],
        ["Hit/miss logic", "Pass", "Correct board updates"],
        ["Win detection", "Pass", "Game ends at full sink"],
    ]

    table = ax.table(
        cellText=rows,
        colLabels=["Test", "Result", "Notes"],
        loc="center",
        cellLoc="left",
        colLoc="left",
    )
    table.auto_set_font_size(False)
    table.set_fontsize(10)
    table.scale(1, 1.55)

    for (r, c), cell in table.get_celld().items():
        if r == 0:
            cell.set_text_props(fontweight="bold", color="#0f172a")
            cell.set_facecolor("#e2e8f0")
        else:
            cell.set_facecolor("#f8fafc")

    save(fig, "poster_testing_summary")


def main():
    make_architecture()
    make_state_machine()
    make_timeline()
    make_testing_table()
    print(f"Saved assets to {OUT_DIR}")


if __name__ == "__main__":
    main()
