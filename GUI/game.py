import math
import random
import threading
import time
from collections import deque

import pygame
import pygame.gfxdraw
import serial
import serial.tools.list_ports


BAUD_RATE = 38400
BOARD_SIZE = 8
CELL_SIZE = 60

# Початкові розміри вікна
WIDTH, HEIGHT = 600, 820
OFFSET_X = (WIDTH - BOARD_SIZE * CELL_SIZE) // 2
OFFSET_Y = 160

BG_COLOR = (18, 22, 40)
TEXT_COLOR = (245, 245, 255)
ACCENT = (90, 170, 255)
PANEL_COLOR = (25, 28, 55)

BTN_COLOR = (25, 23, 38)

COLORS = {
    0: (0, 0, 0),
    1: (255, 40, 40),
    2: (0, 120, 255),
    3: (0, 220, 90),
    4: (255, 225, 0),
    5: (180, 0, 255),
    6: (255, 140, 0)
}


def get_ports():
    result = []
    for p in serial.tools.list_ports.comports():
        desc = str(p.description).lower()
        manu = str(p.manufacturer).lower()
        is_board = (
            any(x in desc for x in [
                "stm", "stlink", "st-link",
                "ch340", "cp210", "ftdi", "uart"
            ]) or
            any(x in manu for x in [
                "stmicroelectronics", "wch",
                "silicon labs", "ftdi"
            ])
        )
        result.append({
            "device": p.device,
            "is_board": is_board
        })
    return result


class BgParticle:
    def __init__(self):
        self.reset(random_y=True)

    def reset(self, random_y=False):
        self.x = random.uniform(0, WIDTH)
        if random_y:
            self.y = random.uniform(0, HEIGHT)
        else:
            self.y = HEIGHT + random.uniform(20, 100)
        self.radius = random.uniform(2, 6)
        self.base_alpha = random.randint(20, 80)
        self.alpha = self.base_alpha
        self.speed_y = random.uniform(-1.5, -0.4)
        self.angle = random.uniform(0, math.pi * 2)
        self.rot_speed = random.uniform(-0.04, 0.04)
        self.amp = random.uniform(0.5, 2.0)
        self.popping = False
        self.pop_timer = 0

    def update(self):
        if not self.popping:
            self.y += self.speed_y
            self.angle += self.rot_speed
            self.x += math.cos(self.angle) * self.amp

            if self.y < -20:
                self.reset()

            # Шанс що кулька лусне
            if random.random() < 0.002:
                self.popping = True
                self.pop_timer = 15
        else:
            self.radius += 1.2
            self.alpha -= (self.base_alpha / 15)
            self.pop_timer -= 1
            if self.pop_timer <= 0:
                self.reset()

    def draw(self, surface):
        if self.alpha > 0:
            surf = pygame.Surface(
                (int(self.radius * 2), int(self.radius * 2)),
                pygame.SRCALPHA
            )
            pygame.draw.circle(
                surf,
                (255, 255, 255, int(self.alpha)),
                (int(self.radius), int(self.radius)),
                int(self.radius)
            )
            surface.blit(
                surf,
                (int(self.x - self.radius), int(self.y - self.radius))
            )


class Particle:
    def __init__(self, x, y, color):
        angle = random.uniform(0, 2 * math.pi)
        speed = random.uniform(2, 6)
        self.x = x
        self.y = y
        self.vx = math.cos(angle) * speed
        self.vy = math.sin(angle) * speed
        self.life = 30
        self.color = color

    def update(self):
        self.x += self.vx
        self.y += self.vy
        self.vy += 0.25
        self.life -= 1

    def draw(self, surface):
        if self.life > 0:
            pygame.draw.circle(
                surface, self.color, (int(self.x), int(self.y)), 3
            )


class FloatingText:
    def __init__(self, x, y, text, color, font):
        self.x = x
        self.y = y
        self.text = text
        self.color = color
        self.font = font
        self.life = 255
        self.vx = random.uniform(-0.8, 0.8)
        self.vy = random.uniform(-2.5, -1.5)

    def update(self):
        self.x += self.vx
        self.y += self.vy
        self.life -= 4.5

    def draw(self, surface):
        if self.life > 0:
            alpha = max(0, int(self.life))
            text_surf = self.font.render(
                self.text, True, self.color
            ).convert_alpha()
            shadow_surf = self.font.render(
                self.text, True, (0, 0, 0)
            ).convert_alpha()
            text_surf.set_alpha(alpha)
            shadow_surf.set_alpha(alpha)
            rect = text_surf.get_rect(center=(int(self.x), int(self.y)))
            surface.blit(shadow_surf, (rect.x + 2, rect.y + 2))
            surface.blit(text_surf, rect)


class AnimCell:
    def __init__(self, r, c):
        self.r = r
        self.c = c
        self.color = 0
        self.x = OFFSET_X + c * CELL_SIZE + CELL_SIZE // 2
        self.y = OFFSET_Y + r * CELL_SIZE + CELL_SIZE // 2
        self.target_y = self.y
        self.scale = 1.0

    def sync(self, new_color):
        if self.color != new_color:
            old = self.color
            self.color = new_color
            if old == 0 and new_color != 0:
                self.y = self.target_y - CELL_SIZE
            return old
        return None

    def update_position(self, new_r):
        self.r = new_r
        self.target_y = OFFSET_Y + new_r * CELL_SIZE + CELL_SIZE // 2

    def update(self):
        self.y += (self.target_y - self.y) * 0.45
        if self.scale < 1.0:
            self.scale += 0.1
            if self.scale > 1.0:
                self.scale = 1.0

    def draw(self, surface):
        if self.color == 0:
            return
        base = COLORS[self.color]
        radius = int((CELL_SIZE // 2 - 8) * self.scale)
        cx = int(self.x)
        cy = int(self.y)
        pygame.gfxdraw.filled_circle(surface, cx, cy, radius, base)
        pygame.gfxdraw.aacircle(surface, cx, cy, radius, base)


class Match3Game:
    def __init__(self):
        pygame.init()

        self.windowed_w = 600
        self.windowed_h = 820
        self.is_fullscreen = False

        self.screen = pygame.display.set_mode(
            (WIDTH, HEIGHT), pygame.RESIZABLE
        )
        pygame.display.set_caption("MATCH - 3")
        self.clock = pygame.time.Clock()

        self.font_title = pygame.font.SysFont("Segoe UI", 56, bold=True)
        self.font_big = pygame.font.SysFont("Segoe UI", 36, bold=True)
        self.font_small = pygame.font.SysFont("Segoe UI", 18, bold=True)
        self.font_hint = pygame.font.SysFont("Segoe UI", 18)

        self.board = [
            [AnimCell(r, c) for c in range(BOARD_SIZE)]
            for r in range(BOARD_SIZE)
        ]
        self.particles = []
        self.floating_texts = []
        self.bg_particles = [BgParticle() for _ in range(60)]

        self.state = "MENU"
        self.menu_action_target = ""

        self.player_name = ""
        self.score = 0
        self.score_per_ball = 10
        self.selected = None
        self.busy = False
        self.running = True
        self.exiting_game = False

        self.current_slot = None
        self.last_action_time = time.time()
        self.hint_cells = None
        self.pending_explosions = set()
        self.pending_swap = None
        self.received_0x16_during_busy = False

        self.info_msg = ""
        self.info_timer = 0
        self.info_color = (255, 255, 0)

        self.best_scores = {}
        self.temp_leaderboard_names = {}

        self.slot_names = ["EMPTY", "EMPTY", "EMPTY"]
        self.temp_slots = {i: ['\x00'] * 12 for i in range(3)}

        self.update_layout()

        self.available_ports = get_ports()
        self.selected_port_index = 0
        self.connected = False
        self.ser = None
        self.queue = deque()
        self.lock = threading.Lock()
        self.last_rx_time = 0

        self.reader_thread = threading.Thread(
            target=self.reader_loop, daemon=True
        )
        self.reader_thread.start()

        pygame.time.set_timer(pygame.USEREVENT + 1, 1000)

    def clean_text(self, text):
        if isinstance(text, list):
            s = "".join(text)
        else:
            s = str(text)

        if '\x00' in s:
            s = s[:s.index('\x00')]
        if chr(255) in s:
            s = s[:s.index(chr(255))]

        s = "".join(c for c in s if c.isprintable())
        s = s.rstrip('?')
        return s.strip()

    def show_msg(self, text, duration=120, color=(255, 255, 0)):
        self.info_msg = text
        self.info_timer = duration
        self.info_color = color

    def update_layout(self):
        global OFFSET_X, OFFSET_Y
        OFFSET_X = (WIDTH - BOARD_SIZE * CELL_SIZE) // 2
        OFFSET_Y = (HEIGHT - BOARD_SIZE * CELL_SIZE) // 2 - 30

        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                cell = self.board[r][c]
                cell.x = OFFSET_X + c * CELL_SIZE + CELL_SIZE // 2
                cell.target_y = OFFSET_Y + r * CELL_SIZE + CELL_SIZE // 2
                if abs(cell.y - cell.target_y) < 2 or cell.color == 0:
                    cell.y = cell.target_y

        panel_y = HEIGHT - 70
        self.rect_left = pygame.Rect(
            WIDTH // 2 - 280, panel_y + 20, 40, 35
        )
        self.rect_right = pygame.Rect(
            WIDTH // 2 + 40, panel_y + 20, 40, 35
        )
        self.rect_conn = pygame.Rect(
            WIDTH // 2 + 100, panel_y + 15, 150, 40
        )

    def toggle_fullscreen(self):
        global WIDTH, HEIGHT
        self.is_fullscreen = not self.is_fullscreen
        if self.is_fullscreen:
            info = pygame.display.Info()
            WIDTH, HEIGHT = info.current_w, info.current_h
            self.screen = pygame.display.set_mode(
                (WIDTH, HEIGHT), pygame.FULLSCREEN
            )
        else:
            WIDTH, HEIGHT = self.windowed_w, self.windowed_h
            self.screen = pygame.display.set_mode(
                (WIDTH, HEIGHT), pygame.RESIZABLE
            )
        self.update_layout()

    def crc8(self, data):
        crc = 0
        for b in data:
            crc ^= b
            for _ in range(8):
                crc = (crc << 1) ^ 0x07 if crc & 0x80 else crc << 1
                crc &= 0xFF
        return crc

    def send(self, cmd, b1=0, b2=0, b3=0, b4=0):
        if not self.connected:
            return
        try:
            p = bytearray([cmd, b1, b2, b3, b4])
            p.append(self.crc8(p))
            self.ser.write(p)
        except Exception:
            self.show_msg("ERROR: DATA SEND FAILED!", 120, (255, 60, 60))
            self.disconnect()

    def send_player_name(self):
        padded = self.player_name.ljust(15, '\x00')
        for i in range(5):
            chunk = padded[i * 3: i * 3 + 3]
            self.send(0x20, i, ord(chunk[0]), ord(chunk[1]), ord(chunk[2]))
            time.sleep(0.05)

    def reader_loop(self):
        while self.running:
            if self.connected and self.ser and self.ser.is_open:
                try:
                    if self.ser.in_waiting >= 6:
                        raw = self.ser.read(6)
                        if len(raw) == 6 and self.crc8(raw[:5]) == raw[5]:
                            with self.lock:
                                self.queue.append(raw)
                            self.last_rx_time = time.time()
                except Exception:
                    if self.connected:
                        self.show_msg(
                            "ERROR: MCU CABLE DISCONNECTED!",
                            180, (255, 60, 60)
                        )
                        self.disconnect()
            time.sleep(0.01)

    def sync_board_data(self, delay=0.0):
        if delay > 0:
            time.sleep(delay)
        if not self.connected:
            return

        self.temp_slots = {i: ['\x00'] * 12 for i in range(3)}
        self.temp_leaderboard_names.clear()

        for i in range(3):
            self.send(0x32, i, 0, 0, 0)
            time.sleep(0.05)

        self.send(0x40, 0, 0, 0, 0)

    def task_save_slot(self, slot_idx):
        self.exiting_game = True
        self.send_player_name()
        time.sleep(0.1)
        self.send(0x30, slot_idx)
        time.sleep(0.3)
        self.send(0x12, 0xFF)
        time.sleep(0.3)
        self.sync_board_data()
        self.exiting_game = False

    def task_load_slot(self, slot_idx):
        self.exiting_game = True
        self.send_player_name()
        time.sleep(0.1)
        self.send(0x31, slot_idx)
        time.sleep(0.2)
        self.send(0x15)
        self.exiting_game = False

    def safe_exit_to_menu(self):
        self.exiting_game = True
        self.send_player_name()
        time.sleep(0.1)

        if self.current_slot is not None:
            self.send(0x30, self.current_slot)
            self.show_msg(
                f"SAVING TO SLOT {self.current_slot + 1}...",
                120, (100, 255, 255)
            )
            time.sleep(0.4)

        self.send(0x12, 0xFF)
        time.sleep(0.4)

        self.sync_board_data()
        self.exiting_game = False
        self.state = "MENU"

    def connect(self):
        if self.connected or not self.available_ports:
            return
        try:
            port = self.available_ports[self.selected_port_index]["device"]
            self.ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
            self.connected = True
            self.last_rx_time = time.time()
            self.show_msg("CONNECTED TO " + port, 120, (100, 255, 100))

            for r in range(BOARD_SIZE):
                for c in range(BOARD_SIZE):
                    self.board[r][c].color = 0

            threading.Thread(
                target=self.sync_board_data, args=(1.0,), daemon=True
            ).start()
        except Exception:
            self.connected = False
            self.show_msg("CONNECTION FAILED!", 120, (255, 60, 60))

    def disconnect(self):
        try:
            if self.ser:
                self.ser.close()
        except Exception:
            pass
        self.connected = False
        self.busy = False
        self.state = "MENU"

    def start_new_game(self):
        self.score = 0
        self.busy = False
        self.selected = None
        self.pending_explosions.clear()
        self.pending_swap = None
        self.last_action_time = time.time()
        self.hint_cells = None
        self.floating_texts.clear()

        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                self.board[r][c].color = 0

        self.send_player_name()
        self.send(0x10)
        self.state = "PLAYING"

    def create_explosion(self, x, y, color):
        for _ in range(18):
            self.particles.append(Particle(x, y, color))

    def check_match_at(self, r, c):
        color = self.board[r][c].color
        if color == 0:
            return False

        count_h = 1
        for i in range(c - 1, -1, -1):
            if self.board[r][i].color == color:
                count_h += 1
            else:
                break
        for i in range(c + 1, BOARD_SIZE):
            if self.board[r][i].color == color:
                count_h += 1
            else:
                break
        if count_h >= 3:
            return True

        count_v = 1
        for i in range(r - 1, -1, -1):
            if self.board[i][c].color == color:
                count_v += 1
            else:
                break
        for i in range(r + 1, BOARD_SIZE):
            if self.board[i][c].color == color:
                count_v += 1
            else:
                break
        if count_v >= 3:
            return True

        return False

    def find_possible_move(self):
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                if c < BOARD_SIZE - 1:
                    self.board[r][c].color, self.board[r][c + 1].color = (
                        self.board[r][c + 1].color,
                        self.board[r][c].color
                    )
                    match_found = (
                        self.check_match_at(r, c) or
                        self.check_match_at(r, c + 1)
                    )
                    self.board[r][c].color, self.board[r][c + 1].color = (
                        self.board[r][c + 1].color,
                        self.board[r][c].color
                    )
                    if match_found:
                        return ((r, c), (r, c + 1))

                if r < BOARD_SIZE - 1:
                    self.board[r][c].color, self.board[r + 1][c].color = (
                        self.board[r + 1][c].color,
                        self.board[r][c].color
                    )
                    match_found = (
                        self.check_match_at(r, c) or
                        self.check_match_at(r + 1, c)
                    )
                    self.board[r][c].color, self.board[r + 1][c].color = (
                        self.board[r + 1][c].color,
                        self.board[r][c].color
                    )
                    if match_found:
                        return ((r, c), (r + 1, c))
        return None

    def detect_and_save_matches(self):
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE - 2):
                color = self.board[r][c].color
                if (color != 0 and
                        self.board[r][c + 1].color == color and
                        self.board[r][c + 2].color == color):
                    self.pending_explosions.update(
                        [(r, c), (r, c + 1), (r, c + 2)]
                    )
        for c in range(BOARD_SIZE):
            for r in range(BOARD_SIZE - 2):
                color = self.board[r][c].color
                if (color != 0 and
                        self.board[r + 1][c].color == color and
                        self.board[r + 2][c].color == color):
                    self.pending_explosions.update(
                        [(r, c), (r + 1, c), (r + 2, c)]
                    )

        count = len(self.pending_explosions)
        if count == 3:
            self.score_per_ball = 10
        elif count == 4:
            self.score_per_ball = 15
        elif count >= 5:
            self.score_per_ball = 20
        else:
            self.score_per_ball = 10

    def process_uart(self):
        with self.lock:
            while self.queue:
                p = self.queue.popleft()
                cmd = p[0]

                if cmd == 0x16:
                    self.received_0x16_during_busy = True
                    r, c, color = p[1], p[2], p[3]
                    if 0 <= r < BOARD_SIZE and 0 <= c < BOARD_SIZE:
                        if color == 0:
                            self.detect_and_save_matches()
                        cell = self.board[r][c]
                        old_color = cell.sync(color)

                        if color == 0 and old_color and old_color != 0:
                            if (r, c) in self.pending_explosions:
                                self.create_explosion(
                                    cell.x, cell.y, COLORS[old_color]
                                )
                                text_str = f"+{self.score_per_ball}"
                                ft = FloatingText(
                                    cell.x, cell.y - 15, text_str,
                                    COLORS[old_color], self.font_small
                                )
                                self.floating_texts.append(ft)
                                self.pending_explosions.discard((r, c))
                        if color != 0:
                            self.pending_explosions.discard((r, c))
                        cell.update_position(r)

                elif cmd == 0x11:
                    self.busy = False
                    if self.pending_swap and not self.received_0x16_during_busy:
                        (r1, c1), (r2, c2), color1, color2 = self.pending_swap
                        self.board[r1][c1].color = color1
                        self.board[r2][c2].color = color2
                    self.pending_swap = None

                elif cmd == 0x15:
                    self.score = (
                        (p[1] << 24) | (p[2] << 16) | (p[3] << 8) | p[4]
                    )
                    clean_name = self.clean_text(self.player_name)
                    if clean_name and clean_name != "EMPTY" and self.score > 0:
                        if self.score > self.best_scores.get(clean_name, 0):
                            self.best_scores[clean_name] = self.score

                elif cmd == 0x30:
                    pass

                elif cmd == 0x31:
                    if p[4] == 0xEE:
                        self.show_msg(
                            "ERROR: SLOT IS EMPTY!", 120, (255, 60, 60)
                        )
                        self.current_slot = None
                    else:
                        self.show_msg("GAME LOADED!", 120, (100, 255, 100))
                        self.last_action_time = time.time()
                        self.hint_cells = None
                        self.floating_texts.clear()
                        self.state = "PLAYING"

                elif cmd in [0x33, 0x34, 0x35, 0x36]:
                    slot = p[1]
                    if slot < 3:
                        chars = [chr(c) for c in p[2:5]]
                        if cmd == 0x33:
                            self.temp_slots[slot][0:3] = chars
                        elif cmd == 0x34:
                            self.temp_slots[slot][3:6] = chars
                        elif cmd == 0x35:
                            self.temp_slots[slot][6:9] = chars
                        elif cmd == 0x36:
                            self.temp_slots[slot][9:12] = chars

                elif cmd == 0x32:
                    slot = p[1]
                    status = p[4]
                    if slot < 3:
                        if status == 0xAA:
                            clean_name = self.clean_text(
                                self.temp_slots[slot]
                            )
                            self.slot_names[slot] = (
                                clean_name if clean_name else "EMPTY"
                            )
                        else:
                            self.slot_names[slot] = "EMPTY"

                elif cmd in [0x41, 0x43, 0x44, 0x45]:
                    idx = p[1]
                    if idx not in self.temp_leaderboard_names:
                        self.temp_leaderboard_names[idx] = ['\x00'] * 10

                    if cmd == 0x41:
                        self.temp_leaderboard_names[idx][0:3] = [
                            chr(c) for c in p[2:5]
                        ]
                    elif cmd == 0x43:
                        self.temp_leaderboard_names[idx][3:6] = [
                            chr(c) for c in p[2:5]
                        ]
                    elif cmd == 0x44:
                        self.temp_leaderboard_names[idx][6:9] = [
                            chr(c) for c in p[2:5]
                        ]
                    elif cmd == 0x45:
                        self.temp_leaderboard_names[idx][9] = chr(p[2])

                elif cmd == 0x42:
                    idx = p[1]
                    if idx < 5:
                        score = (p[3] << 8) | p[4]
                        raw_name = self.temp_leaderboard_names.get(
                            idx, ['\x00'] * 10
                        )
                        clean_name = self.clean_text(raw_name)

                        if clean_name and clean_name != "EMPTY" and score > 0:
                            if score > self.best_scores.get(clean_name, 0):
                                self.best_scores[clean_name] = score

        if self.connected and time.time() - self.last_rx_time > 3.0:
            self.show_msg("ERROR: MCU NOT RESPONDING!", 180, (255, 60, 60))
            self.disconnect()

    def is_board_stable(self):
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                cell = self.board[r][c]
                if cell.color == 0 or abs(cell.y - cell.target_y) > 1.0:
                    return False
        return True

    def handle_keydown(self, key, unicode):
        if self.state == "INPUT_NAME":
            if key == pygame.K_BACKSPACE:
                self.player_name = self.player_name[:-1]
            elif key == pygame.K_RETURN:
                if len(self.player_name) > 0:
                    if self.menu_action_target == "NEW":
                        threading.Thread(
                            target=self.sync_board_data, daemon=True
                        ).start()
                        self.state = "SLOTS"
            else:
                is_valid = len(unicode) > 0 and unicode.isprintable()
                if is_valid and len(self.player_name) < 15:
                    if ord(unicode) < 128:
                        self.player_name += unicode

    def click(self, pos):
        if self.exiting_game:
            return

        self.last_action_time = time.time()
        self.hint_cells = None

        if self.rect_left.collidepoint(pos):
            self.available_ports = get_ports()
            if self.available_ports:
                self.selected_port_index = (
                    (self.selected_port_index - 1) % len(self.available_ports)
                )
            return
        elif self.rect_right.collidepoint(pos):
            self.available_ports = get_ports()
            if self.available_ports:
                self.selected_port_index = (
                    (self.selected_port_index + 1) % len(self.available_ports)
                )
            return
        elif self.rect_conn.collidepoint(pos):
            if self.connected:
                self.disconnect()
            else:
                self.connect()
            return

        if not self.connected:
            self.show_msg("ERROR: MCU NOT CONNECTED!", 120, (255, 60, 60))
            return

        if self.state == "MENU":
            cy = HEIGHT // 2 - 100
            if pygame.Rect(WIDTH // 2 - 130, cy, 260, 50).collidepoint(pos):
                self.menu_action_target = "NEW"
                self.state = "INPUT_NAME"
            elif pygame.Rect(
                WIDTH // 2 - 130, cy + 70, 260, 50
            ).collidepoint(pos):
                self.menu_action_target = "LOAD"
                threading.Thread(
                    target=self.sync_board_data, daemon=True
                ).start()
                self.state = "SLOTS"
            elif pygame.Rect(
                WIDTH // 2 - 130, cy + 140, 260, 50
            ).collidepoint(pos):
                threading.Thread(
                    target=self.sync_board_data, daemon=True
                ).start()
                self.state = "LEADERBOARD"

        elif self.state == "INPUT_NAME":
            if pygame.Rect(
                WIDTH // 2 - 130, HEIGHT // 2 + 80, 260, 50
            ).collidepoint(pos):
                if len(self.player_name) > 0:
                    if self.menu_action_target == "NEW":
                        threading.Thread(
                            target=self.sync_board_data, daemon=True
                        ).start()
                        self.state = "SLOTS"
            elif pygame.Rect(
                WIDTH // 2 - 130, HEIGHT // 2 + 150, 260, 50
            ).collidepoint(pos):
                self.state = "MENU"

        elif self.state == "SLOTS":
            cy = HEIGHT // 2 - 80
            for i in range(3):
                if pygame.Rect(
                    WIDTH // 2 - 150, cy + i * 70, 300, 50
                ).collidepoint(pos):
                    if self.menu_action_target == "NEW":
                        self.current_slot = i
                        self.start_new_game()
                    elif self.menu_action_target == "LOAD":
                        if self.slot_names[i] != "EMPTY":
                            self.player_name = self.slot_names[i]
                            self.current_slot = i

                            for r in range(BOARD_SIZE):
                                for c in range(BOARD_SIZE):
                                    self.board[r][c].color = 0
                            self.score = 0

                            self.task_load_slot(i)
                            self.state = "PLAYING"

            if pygame.Rect(
                WIDTH // 2 - 130, cy + 210, 260, 50
            ).collidepoint(pos):
                self.state = "MENU"

        elif self.state == "LEADERBOARD":
            if pygame.Rect(
                WIDTH // 2 - 130, HEIGHT - 180, 260, 50
            ).collidepoint(pos):
                self.state = "MENU"

        elif self.state == "PLAYING":
            menu_btn_rect = pygame.Rect(WIDTH - 120, 10, 100, 40)

            if menu_btn_rect.collidepoint(pos):
                threading.Thread(
                    target=self.safe_exit_to_menu, daemon=True
                ).start()
                return

            if self.busy or not self.connected or not self.is_board_stable():
                self.selected = None
                return

            c = (pos[0] - OFFSET_X) // CELL_SIZE
            r = (pos[1] - OFFSET_Y) // CELL_SIZE

            if not (0 <= r < BOARD_SIZE and 0 <= c < BOARD_SIZE):
                return

            if self.selected is None:
                self.selected = (r, c)
            else:
                r1, c1 = self.selected
                if abs(r1 - r) + abs(c1 - c) == 1:
                    self.busy = True
                    self.received_0x16_during_busy = False

                    c1_color = self.board[r1][c1].color
                    c2_color = self.board[r][c].color

                    self.board[r1][c1].color = c2_color
                    self.board[r][c].color = c1_color

                    self.pending_explosions.clear()
                    self.detect_and_save_matches()

                    if len(self.pending_explosions) > 0:
                        self.pending_swap = None
                    else:
                        self.pending_swap = (
                            (r1, c1), (r, c), c1_color, c2_color
                        )

                    self.send(0x11, r1, c1, r, c)
                self.selected = None

    def draw_button(
        self, surface, rect, label, base_color, font, hover=False
    ):
        bg_color = (25, 23, 38)
        text_color = (245, 245, 255)
        border_radius = 12

        if hover:
            neon_color = (190, 40, 255)
            border_color = neon_color

            glow_padding = 20
            glow_surf = pygame.Surface(
                (rect.w + glow_padding * 2, rect.h + glow_padding * 2),
                pygame.SRCALPHA
            )

            for i in range(5, 0, -1):
                alpha = int(255 / (i * 2))
                inflate = i * 4
                glow_rect = pygame.Rect(
                    glow_padding - inflate // 2,
                    glow_padding - inflate // 2,
                    rect.w + inflate,
                    rect.h + inflate
                )
                pygame.draw.rect(
                    glow_surf,
                    (*neon_color, alpha),
                    glow_rect,
                    border_radius=border_radius + int(i * 1.5)
                )

            surface.blit(
                glow_surf,
                (rect.x - glow_padding, rect.y - glow_padding)
            )
        else:
            border_color = (50, 45, 70)

        pygame.draw.rect(
            surface, bg_color, rect, border_radius=border_radius
        )
        pygame.draw.rect(
            surface, border_color, rect,
            width=1, border_radius=border_radius
        )

        safe_label = self.clean_text(label)
        text_surf = font.render(safe_label, True, text_color)
        text_rect = text_surf.get_rect(center=rect.center)
        surface.blit(text_surf, text_rect)

    def draw_centered_btn(self, y, text, color, hover_pos):
        rect = pygame.Rect(WIDTH // 2 - 130, y, 260, 50)
        self.draw_button(
            self.screen, rect, text, color,
            self.font_small, rect.collidepoint(hover_pos)
        )

    def draw_gradient_title(self, text, y, font):
        total_width = sum([font.size(char)[0] for char in text])
        start_x = WIDTH // 2 - total_width // 2
        t = time.time() * 0.8

        palette = [
            (0, 255, 100),
            (0, 200, 255),
            (50, 50, 255),
            (150, 0, 200)
        ]

        current_x = start_x
        for i, char in enumerate(text):
            phase = t + i * 0.2
            idx = phase % len(palette)
            idx1 = int(idx)
            idx2 = (idx1 + 1) % len(palette)
            frac = idx - idx1

            c1 = palette[idx1]
            c2 = palette[idx2]
            color = (
                int(c1[0] + (c2[0] - c1[0]) * frac),
                int(c1[1] + (c2[1] - c1[1]) * frac),
                int(c1[2] + (c2[2] - c1[2]) * frac)
            )

            shadow = font.render(char, True, (0, 0, 0))
            self.screen.blit(shadow, (current_x + 3, y + 3))

            char_surf = font.render(char, True, color)
            self.screen.blit(char_surf, (current_x, y))

            current_x += font.size(char)[0]

    def draw(self):
        self.screen.fill(BG_COLOR)

        for bp in self.bg_particles:
            bp.update()
            bp.draw(self.screen)

        mx, my = pygame.mouse.get_pos()

        if self.info_timer > 0:
            safe_msg = self.clean_text(self.info_msg)
            msg = self.font_small.render(safe_msg, True, self.info_color)
            self.screen.blit(msg, (WIDTH // 2 - msg.get_width() // 2, 20))
            self.info_timer -= 1

        if self.state == "MENU":
            self.draw_gradient_title(
                "MATCH-3", HEIGHT // 2 - 200, self.font_title
            )

            cy = HEIGHT // 2 - 100
            self.draw_centered_btn(
                cy, "NEW GAME", BTN_COLOR, (mx, my)
            )
            self.draw_centered_btn(
                cy + 70, "CONTINUE GAME", BTN_COLOR, (mx, my)
            )
            self.draw_centered_btn(
                cy + 140, "LEADERBOARD", BTN_COLOR, (mx, my)
            )

        elif self.state == "INPUT_NAME":
            title = self.font_big.render("ENTER NICKNAME:", True, TEXT_COLOR)
            tx = WIDTH // 2 - title.get_width() // 2
            ty = HEIGHT // 2 - 100
            self.screen.blit(title, (tx, ty))

            pygame.draw.rect(
                self.screen, (30, 40, 60),
                (WIDTH // 2 - 150, HEIGHT // 2 - 20, 300, 50), border_radius=10
            )
            pygame.draw.rect(
                self.screen, ACCENT,
                (WIDTH // 2 - 150, HEIGHT // 2 - 20, 300, 50),
                2, border_radius=10
            )

            display_name = self.clean_text(self.player_name)
            cursor = "_" if time.time() % 1 > 0.5 else ""
            name_txt = self.font_big.render(
                display_name + cursor, True, (255, 255, 255)
            )
            nx = WIDTH // 2 - name_txt.get_width() // 2
            ny = HEIGHT // 2 - 15
            self.screen.blit(name_txt, (nx, ny))

            self.draw_centered_btn(
                HEIGHT // 2 + 80, "CONFIRM", BTN_COLOR, (mx, my)
            )
            self.draw_centered_btn(
                HEIGHT // 2 + 150, "BACK", BTN_COLOR, (mx, my)
            )

        elif self.state == "SLOTS":
            self.draw_gradient_title(
                "SELECT SAVE SLOT", HEIGHT // 2 - 180, self.font_big
            )

            cy = HEIGHT // 2 - 80
            for i in range(3):
                rect = pygame.Rect(WIDTH // 2 - 150, cy + i * 70, 300, 50)
                clean_name = self.clean_text(self.slot_names[i])
                if clean_name != "EMPTY":
                    text = f"SLOT {i + 1}: {clean_name}"
                else:
                    text = f"SLOT {i + 1} (EMPTY)"
                self.draw_button(
                    self.screen, rect, text, BTN_COLOR,
                    self.font_small, rect.collidepoint(mx, my)
                )

            self.draw_centered_btn(cy + 210, "BACK", BTN_COLOR, (mx, my))

        elif self.state == "LEADERBOARD":
            self.draw_gradient_title("LEADERBOARD", 90, self.font_title)

            display_lb = [
                {'name': k, 'score': v} for k, v in self.best_scores.items()
            ]
            display_lb.sort(key=lambda x: x['score'], reverse=True)

            while len(display_lb) < 5:
                display_lb.append({'name': 'EMPTY', 'score': 0})

            display_lb = display_lb[:5]

            sy = 180
            for idx, entry in enumerate(display_lb):
                clean_name = entry['name']
                if clean_name != "EMPTY":
                    txt_color = TEXT_COLOR
                else:
                    txt_color = (100, 110, 140)
                txt = self.font_small.render(
                    f"{idx + 1}. {clean_name} - {entry['score']}",
                    True, txt_color
                )
                tx = WIDTH // 2 - txt.get_width() // 2
                self.screen.blit(txt, (tx, sy))
                sy += 35

            self.draw_centered_btn(HEIGHT - 180, "BACK", BTN_COLOR, (mx, my))

        elif self.state == "PLAYING":
            clean_name = self.clean_text(self.player_name)
            hint_txt = self.font_hint.render(
                f"PLAYER: {clean_name}", True, (100, 110, 140)
            )
            self.screen.blit(hint_txt, (10, 10))

            menu_btn_rect = pygame.Rect(WIDTH - 120, 10, 100, 40)
            self.draw_button(
                self.screen, menu_btn_rect, "MENU", BTN_COLOR,
                self.font_small, menu_btn_rect.collidepoint(mx, my)
            )

            score_txt = self.font_big.render(
                f"SCORE: {self.score}", True, (255, 255, 255)
            )
            sx = WIDTH // 2 - score_txt.get_width() // 2
            sy_score = OFFSET_Y - 50
            self.screen.blit(score_txt, (sx, sy_score))

            if (not self.busy and not self.exiting_game and
                    self.connected and self.is_board_stable()):
                if time.time() - self.last_action_time > 10:
                    if not self.hint_cells:
                        self.hint_cells = self.find_possible_move()
                else:
                    self.hint_cells = None
            else:
                self.last_action_time = time.time()
                self.hint_cells = None

            for r in range(BOARD_SIZE):
                for c in range(BOARD_SIZE):
                    x = OFFSET_X + c * CELL_SIZE
                    y = OFFSET_Y + r * CELL_SIZE
                    pygame.draw.rect(
                        self.screen, (30, 38, 70),
                        (x, y, CELL_SIZE, CELL_SIZE), border_radius=12
                    )

                    cell = self.board[r][c]
                    cell.update()
                    cell.draw(self.screen)

                    if self.selected == (r, c):
                        pygame.draw.rect(
                            self.screen, ACCENT,
                            (x, y, CELL_SIZE, CELL_SIZE), 3, border_radius=12
                        )

            if self.hint_cells:
                overlay = pygame.Surface(
                    (CELL_SIZE, CELL_SIZE), pygame.SRCALPHA
                )
                pygame.draw.rect(
                    overlay, (255, 255, 255, 30),
                    overlay.get_rect(), 3, border_radius=12
                )
                snake_surf = pygame.Surface(
                    (CELL_SIZE, CELL_SIZE), pygame.SRCALPHA
                )
                pygame.draw.rect(
                    snake_surf, (255, 255, 255, 255),
                    snake_surf.get_rect(), 4, border_radius=12
                )
                mask = pygame.Surface(
                    (CELL_SIZE, CELL_SIZE), pygame.SRCALPHA
                )
                center = (CELL_SIZE / 2, CELL_SIZE / 2)
                angle = (time.time() * 5) % (2 * math.pi)

                segments = 25
                for i in range(segments):
                    a1 = angle - (i * 0.1)
                    a2 = angle - ((i + 1) * 0.1)
                    alpha = max(0, 255 - int(i * (255 / segments)))
                    R = CELL_SIZE * 1.5
                    p1 = center
                    p2 = (
                        center[0] + R * math.cos(a1),
                        center[1] + R * math.sin(a1)
                    )
                    p3 = (
                        center[0] + R * math.cos(a2),
                        center[1] + R * math.sin(a2)
                    )
                    pygame.draw.polygon(
                        mask, (255, 255, 255, alpha), [p1, p2, p3]
                    )

                snake_surf.blit(
                    mask, (0, 0), special_flags=pygame.BLEND_RGBA_MULT
                )
                overlay.blit(snake_surf, (0, 0))

                for (hr, hc) in self.hint_cells:
                    hx = OFFSET_X + hc * CELL_SIZE
                    hy = OFFSET_Y + hr * CELL_SIZE
                    self.screen.blit(overlay, (hx, hy))

            for p in self.particles[:]:
                p.update()
                p.draw(self.screen)
                if p.life <= 0:
                    self.particles.remove(p)

            for ft in self.floating_texts[:]:
                ft.update()
                ft.draw(self.screen)
                if ft.life <= 0:
                    self.floating_texts.remove(ft)

        panel_y = HEIGHT - 70
        pygame.draw.rect(self.screen, PANEL_COLOR, (0, panel_y, WIDTH, 70))
        pygame.draw.line(
            self.screen, (40, 45, 75), (0, panel_y), (WIDTH, panel_y), 2
        )

        port_name = "NO PORT"
        text_col = TEXT_COLOR

        if self.available_ports:
            self.selected_port_index = (
                self.selected_port_index % len(self.available_ports)
            )
            port_info = self.available_ports[self.selected_port_index]
            device = port_info["device"]

            is_active = (
                self.connected and self.ser and
                hasattr(self.ser, 'port') and self.ser.port == device
            )

            if is_active:
                port_name = f"{device} [ACTIVE]"
                text_col = (100, 255, 100)
            elif port_info["is_board"]:
                port_name = f"{device} [BOARD DETECTED]"
                text_col = (100, 200, 255)
            else:
                port_name = f"{device}"

        port_txt = self.font_hint.render(port_name, True, text_col)
        txt_x = (
            self.rect_left.right +
            (self.rect_right.left - self.rect_left.right) // 2 -
            port_txt.get_width() // 2
        )
        self.screen.blit(port_txt, (txt_x, panel_y + 25))

        self.draw_button(
            self.screen, self.rect_left, "<", BTN_COLOR,
            self.font_small, self.rect_left.collidepoint(mx, my)
        )
        self.draw_button(
            self.screen, self.rect_right, ">", BTN_COLOR,
            self.font_small, self.rect_right.collidepoint(mx, my)
        )

        btn_text = "CONNECT" if not self.connected else "DISCONNECT"
        self.draw_button(
            self.screen, self.rect_conn, btn_text, BTN_COLOR,
            self.font_small, self.rect_conn.collidepoint(mx, my)
        )

        pygame.display.flip()

    def run(self):
        global WIDTH, HEIGHT

        while self.running:
            self.process_uart()
            self.draw()

            for e in pygame.event.get():
                if e.type == pygame.QUIT:
                    self.running = False

                if e.type == pygame.VIDEORESIZE:
                    if not self.is_fullscreen:
                        WIDTH, HEIGHT = e.w, e.h
                        self.screen = pygame.display.set_mode(
                            (WIDTH, HEIGHT), pygame.RESIZABLE
                        )
                        self.update_layout()

                if e.type == pygame.KEYDOWN:
                    self.last_action_time = time.time()
                    self.hint_cells = None
                    self.handle_keydown(e.key, e.unicode)

                    if e.key == pygame.K_F11:
                        self.toggle_fullscreen()
                    elif e.key == pygame.K_ESCAPE:
                        if self.is_fullscreen and self.state == "PLAYING":
                            self.toggle_fullscreen()
                        elif self.state == "PLAYING" and not self.exiting_game:
                            threading.Thread(
                                target=self.safe_exit_to_menu, daemon=True
                            ).start()

                if e.type == pygame.MOUSEBUTTONDOWN and e.button == 1:
                    self.click(e.pos)

                if e.type == pygame.USEREVENT + 1:
                    if self.connected:
                        self.send(0x15)

            self.clock.tick(60)

        pygame.quit()


if __name__ == "__main__":
    Match3Game().run()
