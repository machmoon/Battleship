#!/usr/bin/env python3
import argparse
import random
import time
from dataclasses import dataclass
from typing import List, Optional, Tuple

import pygame

try:
    import serial
    from serial.tools import list_ports
except Exception:
    serial = None
    list_ports = None


GRID_W = 32
GRID_H = 8
FPS = 60

MOVE_REPEAT_MS = 135
BULLET_STEP_MS = 90
MAX_BULLETS = 24
MAX_MINES = 4
START_LIVES = 3

DIR_DX = [0, 1, 1, 1, 0, -1, -1, -1]
DIR_DY = [-1, -1, 0, 1, 1, 1, 0, -1]


@dataclass
class Bullet:
    x: int
    y: int
    dx: int
    dy: int
    from_player: bool


@dataclass
class Tank:
    x: int
    y: int
    alive: bool = True


class ControllerInput:
    def __init__(self, port: Optional[str], baud: int, debug_serial: bool = False, force_controller: bool = False):
        self.port = port
        self.baud = baud
        self.debug_serial = debug_serial
        self.force_controller = force_controller

        self.ser = None
        self.last_connect_try = 0.0
        self.serial_buf = ""
        self.last_packet_at = 0.0
        self.packet_count = 0
        self.last_error = ""

        self.joy_x = 512
        self.joy_y = 512
        self.shoot_down = False
        self.mine_down = False
        self.shoot_edge = False
        self.mine_edge = False
        self.aim_dir = 2

        self._kbd_shoot_last = False
        self._kbd_mine_last = False

    def auto_port(self) -> Optional[str]:
        if list_ports is None:
            return None
        for p in list_ports.comports():
            dev = (p.device or "").lower()
            if "usbmodem" in dev or "usbserial" in dev or "ttyacm" in dev:
                return p.device
        return None

    def _alt_port_name(self, p: str) -> str:
        if "/cu." in p:
            return p.replace("/cu.", "/tty.")
        if "/tty." in p:
            return p.replace("/tty.", "/cu.")
        return p

    def maybe_connect(self):
        if serial is None or self.ser is not None:
            return

        now = time.time()
        if now - self.last_connect_try < 1.2:
            return
        self.last_connect_try = now

        target = self.port or self.auto_port()
        if not target:
            return

        try_ports = [target]
        alt = self._alt_port_name(target)
        if alt != target:
            try_ports.append(alt)

        for dev in try_ports:
            try:
                self.ser = serial.Serial(dev, self.baud, timeout=0.03)
                self.ser.reset_input_buffer()
                self.port = dev
                self.serial_buf = ""
                self.last_error = ""
                print(f"Controller connected: {dev}")
                return
            except Exception as e:
                self.last_error = str(e)
                if self.debug_serial:
                    print(f"Serial connect failed on {dev}: {e}")
                self.ser = None

    def _close_serial(self):
        if self.ser is None:
            return
        try:
            self.ser.close()
        except Exception:
            pass
        self.ser = None

    def _read_serial_lines(self):
        if self.ser is None:
            return
        try:
            raw = self.ser.read(self.ser.in_waiting or 1)
            if not raw:
                return
            self.serial_buf += raw.decode(errors="ignore")
            while "\n" in self.serial_buf:
                line, self.serial_buf = self.serial_buf.split("\n", 1)
                line = line.strip()
                if self.debug_serial and line:
                    print(f"SERIAL: {line}")
                self._parse_line(line)
        except Exception as e:
            self.last_error = str(e)
            if self.debug_serial:
                print(f"Serial read error: {e}")
            self._close_serial()

    def _parse_line(self, line: str):
        if not line.startswith("C,"):
            return

        parts = line.split(",")
        if len(parts) != 8:
            return

        try:
            self.joy_x = int(parts[1])
            self.joy_y = int(parts[2])
            self.shoot_down = parts[3] == "1"
            self.mine_down = parts[4] == "1"
            self.shoot_edge = self.shoot_edge or (parts[5] == "1")
            self.mine_edge = self.mine_edge or (parts[6] == "1")
            self.aim_dir = max(0, min(7, int(parts[7])))
            self.last_packet_at = time.time()
            self.packet_count += 1
        except ValueError:
            return

    def update(self):
        self.shoot_edge = False
        self.mine_edge = False

        self.maybe_connect()
        self._read_serial_lines()

        # If serial connected but stale for long enough, reopen and try alternate device.
        if self.ser is not None and self.last_packet_at and (time.time() - self.last_packet_at > 2.5):
            if self.debug_serial:
                print("No packets for >2.5s, reconnecting serial...")
            old = self.port
            self._close_serial()
            self.port = self._alt_port_name(old) if old else old

        # Keyboard fallback only when allowed.
        keys = pygame.key.get_pressed()
        serial_stale = (time.time() - self.last_packet_at) > 1.0
        if (self.ser is None or serial_stale) and not self.force_controller:
            self.joy_x = 0 if keys[pygame.K_a] else (1023 if keys[pygame.K_d] else 512)
            self.joy_y = 0 if keys[pygame.K_w] else (1023 if keys[pygame.K_s] else 512)

            dir_map = {
                pygame.K_UP: 0,
                pygame.K_RIGHT: 2,
                pygame.K_DOWN: 4,
                pygame.K_LEFT: 6,
            }
            for key, direction in dir_map.items():
                if keys[key]:
                    self.aim_dir = direction

            shoot_now = keys[pygame.K_SPACE]
            mine_now = keys[pygame.K_m]
            self.shoot_edge = shoot_now and not self._kbd_shoot_last
            self.mine_edge = mine_now and not self._kbd_mine_last
            self.shoot_down = shoot_now
            self.mine_down = mine_now
            self._kbd_shoot_last = shoot_now
            self._kbd_mine_last = mine_now


class WiiTanksGuiGame:
    def __init__(self):
        pygame.init()

        self.screen = pygame.display.set_mode((0, 0), pygame.FULLSCREEN)
        self.win_w, self.win_h = self.screen.get_size()
        pygame.display.set_caption("Wii Tanks Style")

        self.clock = pygame.time.Clock()
        self.font = pygame.font.SysFont("Arial", 28, bold=True)
        self.medium_font = pygame.font.SysFont("Arial", 22, bold=True)
        self.small_font = pygame.font.SysFont("Arial", 18)

        self.hud_w = max(320, int(self.win_w * 0.22))
        self.cell_size = max(14, min((self.win_h - 90) // GRID_H, (self.win_w - self.hud_w - 70) // GRID_W))
        self.board_w = GRID_W * self.cell_size
        self.board_h = GRID_H * self.cell_size
        self.ox = 24
        self.oy = (self.win_h - self.board_h) // 2

        self.running = True
        self.state = "title"  # title, play, level_clear, game_over, campaign_clear
        self.score = 0
        self.lives = START_LIVES
        self.level_idx = 0

        self.levels = [
            {"enemies": 3, "enemy_step_ms": 360, "enemy_fire_ms": 1450},
            {"enemies": 4, "enemy_step_ms": 300, "enemy_fire_ms": 1200},
            {"enemies": 5, "enemy_step_ms": 250, "enemy_fire_ms": 980},
        ]

        self.walls = [[0 for _ in range(GRID_W)] for _ in range(GRID_H)]
        self.player = Tank(2, 6, True)
        self.enemies: List[Tank] = []
        self.bullets: List[Bullet] = []
        self.mines: List[Tuple[int, int]] = []

        self.last_move_at = 0
        self.last_bullet_at = 0
        self.last_enemy_step_at = 0
        self.last_enemy_fire_at = 0

        self.aim_dir = 2

    def start_new_game(self):
        self.score = 0
        self.lives = START_LIVES
        self.level_idx = 0
        self.reset_level()
        self.state = "play"

    def reset_level(self):
        random.seed(100 + self.level_idx)
        self.walls = [[0 for _ in range(GRID_W)] for _ in range(GRID_H)]

        # Scattered brick walls (durability 2)
        wall_count = 14 + self.level_idx * 3
        for _ in range(wall_count):
            x = random.randint(4, GRID_W - 3)
            y = random.randint(1, GRID_H - 2)
            if (x < 6 and y > 4) or (x > GRID_W - 6 and y < 3):
                continue
            self.walls[y][x] = 2

        self.player = Tank(2, 6, True)

        enemy_positions = [(29, 1), (27, 6), (22, 3), (18, 1), (30, 4), (25, 2)]
        n = self.levels[self.level_idx]["enemies"]
        self.enemies = [Tank(enemy_positions[i][0], enemy_positions[i][1]) for i in range(n)]

        self.bullets = []
        self.mines = []
        self.aim_dir = 2

        now = pygame.time.get_ticks()
        self.last_move_at = now
        self.last_bullet_at = now
        self.last_enemy_step_at = now
        self.last_enemy_fire_at = now

    def in_bounds(self, x: int, y: int) -> bool:
        return 0 <= x < GRID_W and 0 <= y < GRID_H

    def is_wall(self, x: int, y: int) -> bool:
        if not self.in_bounds(x, y):
            return True
        return self.walls[y][x] > 0

    def try_move_player(self, controller: ControllerInput):
        now = pygame.time.get_ticks()
        if now - self.last_move_at < MOVE_REPEAT_MS:
            return

        dx = -1 if controller.joy_x < 320 else (1 if controller.joy_x > 700 else 0)
        dy = -1 if controller.joy_y < 320 else (1 if controller.joy_y > 700 else 0)
        if dx == 0 and dy == 0:
            return

        nx = self.player.x + dx
        ny = self.player.y + dy
        self.last_move_at = now

        if not self.in_bounds(nx, ny) or self.is_wall(nx, ny):
            return

        for e in self.enemies:
            if e.alive and e.x == nx and e.y == ny:
                return

        self.player.x = nx
        self.player.y = ny

    def spawn_bullet(self, x: int, y: int, dx: int, dy: int, from_player: bool):
        if not self.in_bounds(x, y):
            return
        if len(self.bullets) >= MAX_BULLETS:
            return
        self.bullets.append(Bullet(x, y, dx, dy, from_player))

    def fire_player(self, controller: ControllerInput):
        self.aim_dir = controller.aim_dir
        if not controller.shoot_edge:
            return

        dx = DIR_DX[self.aim_dir]
        dy = DIR_DY[self.aim_dir]
        if dx == 0 and dy == 0:
            return

        self.spawn_bullet(self.player.x + dx, self.player.y + dy, dx, dy, True)

    def place_mine(self, controller: ControllerInput):
        if not controller.mine_edge:
            return

        pos = (self.player.x, self.player.y)
        if pos in self.mines:
            return

        if len(self.mines) >= MAX_MINES:
            self.mines.pop(0)
        self.mines.append(pos)

    def hit_cover(self, x: int, y: int):
        if self.in_bounds(x, y) and self.walls[y][x] > 0:
            self.walls[y][x] -= 1

    def explode_at(self, x: int, y: int):
        for yy in range(y - 1, y + 2):
            for xx in range(x - 1, x + 2):
                if self.in_bounds(xx, yy):
                    self.hit_cover(xx, yy)

        for e in self.enemies:
            if e.alive and abs(e.x - x) <= 1 and abs(e.y - y) <= 1:
                e.alive = False
                self.score += 10

        if abs(self.player.x - x) <= 1 and abs(self.player.y - y) <= 1:
            self.lives -= 1
            if self.lives <= 0:
                self.state = "game_over"
            else:
                self.player.x, self.player.y = 2, 6

        self.mines = [(mx, my) for (mx, my) in self.mines if abs(mx - x) > 1 or abs(my - y) > 1]

    def update_bullets(self):
        now = pygame.time.get_ticks()
        if now - self.last_bullet_at < BULLET_STEP_MS:
            return
        self.last_bullet_at = now

        next_bullets: List[Bullet] = []

        for b in self.bullets:
            nx = b.x + b.dx
            ny = b.y + b.dy

            if not self.in_bounds(nx, ny):
                continue

            if self.is_wall(nx, ny):
                self.hit_cover(nx, ny)
                continue

            hit_enemy = False
            for e in self.enemies:
                if e.alive and e.x == nx and e.y == ny:
                    e.alive = False
                    self.score += 10
                    hit_enemy = True
                    break
            if hit_enemy:
                continue

            if not b.from_player and nx == self.player.x and ny == self.player.y:
                self.lives -= 1
                if self.lives <= 0:
                    self.state = "game_over"
                else:
                    self.player.x, self.player.y = 2, 6
                continue

            mine_hit = False
            for (mx, my) in self.mines:
                if nx == mx and ny == my:
                    self.explode_at(mx, my)
                    mine_hit = True
                    break
            if mine_hit:
                continue

            b.x = nx
            b.y = ny
            next_bullets.append(b)

        self.bullets = next_bullets

    def update_enemies(self):
        if self.state != "play":
            return

        now = pygame.time.get_ticks()
        step_ms = self.levels[self.level_idx]["enemy_step_ms"]
        fire_ms = self.levels[self.level_idx]["enemy_fire_ms"]

        if now - self.last_enemy_step_at >= step_ms:
            self.last_enemy_step_at = now
            for e in self.enemies:
                if not e.alive:
                    continue

                dx = 0 if self.player.x == e.x else (1 if self.player.x > e.x else -1)
                dy = 0 if self.player.y == e.y else (1 if self.player.y > e.y else -1)

                options = [(e.x + dx, e.y), (e.x, e.y + dy), (e.x + dx, e.y + dy)]
                random.shuffle(options)

                for (nx, ny) in options:
                    if not self.in_bounds(nx, ny):
                        continue
                    if self.is_wall(nx, ny):
                        continue
                    if nx == self.player.x and ny == self.player.y:
                        continue
                    if any(o.alive and o is not e and o.x == nx and o.y == ny for o in self.enemies):
                        continue
                    e.x, e.y = nx, ny
                    break

        if now - self.last_enemy_fire_at >= fire_ms:
            self.last_enemy_fire_at = now
            for e in self.enemies:
                if not e.alive:
                    continue
                dx = 0 if self.player.x == e.x else (1 if self.player.x > e.x else -1)
                dy = 0 if self.player.y == e.y else (1 if self.player.y > e.y else -1)
                if dx == 0 and dy == 0:
                    continue
                self.spawn_bullet(e.x + dx, e.y + dy, dx, dy, False)

        for (mx, my) in list(self.mines):
            for e in self.enemies:
                if e.alive and e.x == mx and e.y == my:
                    self.explode_at(mx, my)
                    break

        if not any(e.alive for e in self.enemies):
            if self.level_idx + 1 >= len(self.levels):
                self.state = "campaign_clear"
            else:
                self.state = "level_clear"

    def draw_tank(self, x: int, y: int, color: Tuple[int, int, int], turret_dir: int):
        px = self.ox + x * self.cell_size
        py = self.oy + y * self.cell_size

        body = pygame.Rect(px + 4, py + 4, self.cell_size - 8, self.cell_size - 8)
        pygame.draw.rect(self.screen, color, body, border_radius=4)
        pygame.draw.rect(self.screen, (24, 24, 24), body, 1, border_radius=4)

        pygame.draw.rect(self.screen, (45, 45, 45), (px + 2, py + 5, 2, self.cell_size - 10))
        pygame.draw.rect(self.screen, (45, 45, 45), (px + self.cell_size - 4, py + 5, 2, self.cell_size - 10))

        cx = px + self.cell_size // 2
        cy = py + self.cell_size // 2
        pygame.draw.circle(self.screen, (225, 225, 225), (cx, cy), max(3, self.cell_size // 8))
        dx = DIR_DX[turret_dir]
        dy = DIR_DY[turret_dir]
        pygame.draw.line(self.screen, (25, 25, 25), (cx, cy), (cx + dx * (self.cell_size // 3), cy + dy * (self.cell_size // 3)), 3)

    def draw_world(self, controller: ControllerInput):
        self.screen.fill((122, 162, 104))

        for y in range(GRID_H):
            for x in range(GRID_W):
                r = pygame.Rect(self.ox + x * self.cell_size, self.oy + y * self.cell_size, self.cell_size - 1, self.cell_size - 1)
                c = self.walls[y][x]
                if c == 2:
                    color = (182, 121, 76)
                elif c == 1:
                    color = (144, 94, 62)
                else:
                    color = (140, 176, 112)
                pygame.draw.rect(self.screen, color, r)
                edge = (108, 70, 45) if c else (132, 168, 104)
                pygame.draw.rect(self.screen, edge, r, 1)

        for (mx, my) in self.mines:
            cx = self.ox + mx * self.cell_size + self.cell_size // 2
            cy = self.oy + my * self.cell_size + self.cell_size // 2
            pygame.draw.circle(self.screen, (40, 40, 40), (cx, cy), self.cell_size // 4)

        for b in self.bullets:
            cx = self.ox + b.x * self.cell_size + self.cell_size // 2
            cy = self.oy + b.y * self.cell_size + self.cell_size // 2
            color = (248, 238, 175) if b.from_player else (255, 104, 88)
            pygame.draw.circle(self.screen, color, (cx, cy), max(3, self.cell_size // 7))

        for e in self.enemies:
            if e.alive:
                turret = 6 if e.x > self.player.x else 2
                self.draw_tank(e.x, e.y, (210, 90, 75), turret)

        self.draw_tank(self.player.x, self.player.y, (62, 126, 214), self.aim_dir)

        dx = DIR_DX[self.aim_dir]
        dy = DIR_DY[self.aim_dir]
        rx, ry = self.player.x + dx, self.player.y + dy
        k = 0
        while self.in_bounds(rx, ry) and not self.is_wall(rx, ry):
            if k % 2 == 0:
                c = (self.ox + rx * self.cell_size + self.cell_size // 2, self.oy + ry * self.cell_size + self.cell_size // 2)
                pygame.draw.circle(self.screen, (242, 215, 85), c, max(2, self.cell_size // 8))
            rx += dx
            ry += dy
            k += 1

        hud_x = self.ox + self.board_w + 18
        hud_h = self.board_h
        pygame.draw.rect(self.screen, (239, 231, 203), (hud_x, self.oy, self.hud_w - 30, hud_h), border_radius=10)
        pygame.draw.rect(self.screen, (118, 102, 72), (hud_x, self.oy, self.hud_w - 30, hud_h), 2, border_radius=10)

        lines = [
            "WII TANKS",
            f"LEVEL {self.level_idx + 1}/{len(self.levels)}",
            f"SCORE {self.score}",
            f"LIVES {self.lives}",
            f"ENEMIES {sum(1 for e in self.enemies if e.alive)}",
            f"MINES {len(self.mines)}/{MAX_MINES}",
            "",
            "MOVE: JOYSTICK",
            "AIM: MPU6050",
            "SHOOT: D2",
            "MINE: D3",
            "ESC: QUIT",
        ]

        y = self.oy + 14
        for i, text in enumerate(lines):
            f = self.font if i == 0 else self.small_font
            self.screen.blit(f.render(text, True, (58, 62, 48)), (hud_x + 14, y))
            y += 28 if i == 0 else 23

        if controller.ser is None:
            conn = "SERIAL: NOT CONNECTED"
        else:
            age = int((time.time() - controller.last_packet_at) * 1000) if controller.last_packet_at else -1
            conn = f"SERIAL: {controller.port}  PKTS:{controller.packet_count}  AGE:{age}ms"
        self.screen.blit(self.small_font.render(conn, True, (90, 96, 82)), (hud_x + 14, self.oy + hud_h - 24))

        vals = f"JX:{controller.joy_x} JY:{controller.joy_y} S:{int(controller.shoot_down)} M:{int(controller.mine_down)} AIM:{controller.aim_dir}"
        self.screen.blit(self.small_font.render(vals, True, (90, 96, 82)), (hud_x + 14, self.oy + hud_h - 48))

        if controller.last_error:
            err = f"SER ERR: {controller.last_error[:40]}"
            self.screen.blit(self.small_font.render(err, True, (190, 90, 90)), (hud_x + 14, self.oy + hud_h - 72))

    def draw_banner(self, title: str, subtitle: str):
        overlay = pygame.Surface((self.win_w, self.win_h), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 120))
        self.screen.blit(overlay, (0, 0))

        tw = self.font.render(title, True, (255, 245, 200))
        sw = self.small_font.render(subtitle, True, (235, 230, 205))

        self.screen.blit(tw, ((self.win_w - tw.get_width()) // 2, self.win_h // 2 - 34))
        self.screen.blit(sw, ((self.win_w - sw.get_width()) // 2, self.win_h // 2 + 6))

    def draw(self, controller: ControllerInput):
        self.draw_world(controller)

        if self.state == "title":
            self.draw_banner("WII TANKS STYLE", "Press SHOOT (D2) to start")
        elif self.state == "level_clear":
            self.draw_banner("LEVEL CLEAR", "Press SHOOT (D2) for next level")
        elif self.state == "campaign_clear":
            self.draw_banner("MISSION COMPLETE", "Press SHOOT (D2) to play again")
        elif self.state == "game_over":
            self.draw_banner("MISSION FAILED", "Press SHOOT (D2) to retry")

        pygame.display.flip()

    def update(self, controller: ControllerInput):
        if self.state == "title":
            if controller.shoot_edge or controller.mine_edge:
                self.start_new_game()
            return

        if self.state == "level_clear":
            if controller.shoot_edge:
                self.level_idx += 1
                self.reset_level()
                self.state = "play"
            return

        if self.state == "campaign_clear":
            if controller.shoot_edge:
                self.start_new_game()
            return

        if self.state == "game_over":
            if controller.shoot_edge:
                self.start_new_game()
            return

        self.try_move_player(controller)
        self.fire_player(controller)
        self.place_mine(controller)
        self.update_enemies()
        self.update_bullets()

    def run(self, controller: ControllerInput):
        while self.running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        self.running = False
                    elif event.key == pygame.K_RETURN and self.state == "title":
                        self.start_new_game()
                    elif event.key == pygame.K_r:
                        self.start_new_game()

            controller.update()
            self.update(controller)
            self.draw(controller)
            self.clock.tick(FPS)

        pygame.quit()


def main():
    parser = argparse.ArgumentParser(description="Wii Tanks style GUI controlled by Arduino serial input")
    parser.add_argument("--port", default=None, help="Serial port, ex: /dev/cu.usbmodem1101")
    parser.add_argument("--baud", default=115200, type=int)
    parser.add_argument("--debug-serial", action="store_true", help="Print incoming serial lines")
    parser.add_argument("--force-controller", action="store_true", help="Disable keyboard fallback")
    args = parser.parse_args()

    game = WiiTanksGuiGame()
    controller = ControllerInput(
        args.port,
        args.baud,
        debug_serial=args.debug_serial,
        force_controller=args.force_controller,
    )
    game.run(controller)


if __name__ == "__main__":
    main()
