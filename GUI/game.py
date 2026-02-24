import pygame
import pygame.gfxdraw
import serial
import serial.tools.list_ports
import threading
import time
from collections import deque
import random
import math
import json
import os

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
    return [p.device for p in serial.tools.list_ports.comports()]

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
            pygame.draw.circle(surface, self.color, (int(self.x), int(self.y)), 3)

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
        if self.color == 0: return
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

        self.screen = pygame.display.set_mode((WIDTH, HEIGHT), pygame.RESIZABLE)
        pygame.display.set_caption("STM32 3 IN A ROW ")
        self.clock = pygame.time.Clock()

        self.font_title = pygame.font.SysFont("Segoe UI", 48, bold=True)
        self.font_big = pygame.font.SysFont("Segoe UI", 36, bold=True)
        self.font_small = pygame.font.SysFont("Segoe UI", 18, bold=True)
        self.font_hint = pygame.font.SysFont("Segoe UI", 14)

        self.board = [[AnimCell(r, c) for c in range(BOARD_SIZE)] for r in range(BOARD_SIZE)]
        self.particles = []
        
        self.state = "MENU"
        self.menu_action_target = "" 
        
        self.player_name = ""
        self.incoming_name = "" # Буфер для безпечного завантаження імені з плати
        self.score = 0
        self.selected = None
        self.busy = False
        self.running = True
        
        self.pending_explosions = set()
        self.pending_swap = None
        self.received_0x16_during_busy = False
        
        self.info_msg = ""
        self.info_timer = 0
        
        self.leaderboard = self.load_leaderboard()
        self.slot_names = self.load_slot_names()
        self.last_loaded_slot = None

        self.update_layout()

        # UART
        self.available_ports = get_ports()
        self.selected_port_index = 0
        self.connected = False
        self.ser = None
        self.queue = deque()
        self.lock = threading.Lock()

        self.reader_thread = threading.Thread(target=self.reader_loop, daemon=True)
        self.reader_thread.start()

        pygame.time.set_timer(pygame.USEREVENT + 1, 1000)

    def clean_text(self, text):
        """Видаляє нульові байти та невидимі символи"""
        return "".join(c for c in str(text) if c.isprintable()).strip()

    def load_leaderboard(self):
        if os.path.exists("leaderboard.json"):
            try:
                with open("leaderboard.json", "r") as f:
                    data = json.load(f)
                    for d in data:
                        d['name'] = self.clean_text(d['name'])
                    return data
            except:
                return []
        return []

    def save_leaderboard(self):
        if self.player_name and self.score > 0:
            found = False
            for entry in self.leaderboard:
                if entry['name'] == self.player_name:
                    if self.score > entry['score']:
                        entry['score'] = self.score
                    found = True
                    break
            if not found:
                self.leaderboard.append({'name': self.player_name, 'score': self.score})
            
            self.leaderboard.sort(key=lambda x: x['score'], reverse=True)
            self.leaderboard = self.leaderboard[:10]
            
            try:
                with open("leaderboard.json", "w") as f:
                    json.dump(self.leaderboard, f)
            except:
                pass

    def load_slot_names(self):
        default_slots = ["EMPTY", "EMPTY", "EMPTY"]
        if os.path.exists("slots_info.json"):
            try:
                with open("slots_info.json", "r") as f:
                    loaded = json.load(f)
                    if isinstance(loaded, list):
                        for i in range(min(3, len(loaded))):
                            default_slots[i] = self.clean_text(loaded[i])
            except:
                pass
        return default_slots

    def save_slot_names(self):
        try:
            with open("slots_info.json", "w") as f:
                json.dump(self.slot_names, f)
        except:
            pass

    def show_msg(self, text, duration=120):
        self.info_msg = text
        self.info_timer = duration

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
        self.rect_left = pygame.Rect(WIDTH // 2 - 180, panel_y + 20, 40, 35)
        self.rect_right = pygame.Rect(WIDTH // 2 - 20, panel_y + 20, 40, 35)
        self.rect_conn = pygame.Rect(WIDTH // 2 + 50, panel_y + 15, 150, 40)

    def toggle_fullscreen(self):
        global WIDTH, HEIGHT
        self.is_fullscreen = not self.is_fullscreen
        if self.is_fullscreen:
            info = pygame.display.Info()
            WIDTH, HEIGHT = info.current_w, info.current_h
            self.screen = pygame.display.set_mode((WIDTH, HEIGHT), pygame.FULLSCREEN)
        else:
            WIDTH, HEIGHT = self.windowed_w, self.windowed_h
            self.screen = pygame.display.set_mode((WIDTH, HEIGHT), pygame.RESIZABLE)
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
        if not self.connected: return
        try:
            p = bytearray([cmd, b1, b2, b3, b4])
            p.append(self.crc8(p))
            self.ser.write(p)
        except:
            self.disconnect()

    def send_player_name(self):
        padded = self.player_name.ljust(15, '\x00')
        for i in range(5):
            chunk = padded[i*3 : i*3+3]
            self.send(0x20, i, ord(chunk[0]), ord(chunk[1]), ord(chunk[2]))
            time.sleep(0.05)

    def reader_loop(self):
        while self.running:
            if self.connected and self.ser:
                try:
                    raw = self.ser.read(6)
                    if len(raw) == 6 and self.crc8(raw[:5]) == raw[5]:
                        with self.lock:
                            self.queue.append(raw)
                except:
                    self.disconnect()
            else:
                time.sleep(0.05)

    def connect(self):
        if self.connected or not self.available_ports: return
        try:
            port = self.available_ports[self.selected_port_index]
            self.ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
            self.connected = True
            self.show_msg("CONNECTED TO " + port)
            
            for r in range(BOARD_SIZE):
                for c in range(BOARD_SIZE):
                    self.board[r][c].color = 0
        except:
            self.connected = False
            self.show_msg("CONNECTION FAILED!")

    def disconnect(self):
        try:
            if self.ser: self.ser.close()
        except: pass
        self.connected = False
        self.busy = False
        self.state = "MENU"

    def start_new_game(self):
        self.score = 0
        self.busy = False
        self.selected = None
        self.pending_explosions.clear()
        self.pending_swap = None
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                self.board[r][c].color = 0
        
        self.send_player_name()
        self.send(0x10)
        self.state = "PLAYING"

    def create_explosion(self, x, y, color):
        for _ in range(18):
            self.particles.append(Particle(x, y, color))
            
    def detect_and_save_matches(self):
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE - 2):
                color = self.board[r][c].color
                if color != 0 and self.board[r][c+1].color == color and self.board[r][c+2].color == color:
                    self.pending_explosions.update([(r, c), (r, c+1), (r, c+2)])
        for c in range(BOARD_SIZE):
            for r in range(BOARD_SIZE - 2):
                color = self.board[r][c].color
                if color != 0 and self.board[r+1][c].color == color and self.board[r+2][c].color == color:
                    self.pending_explosions.update([(r, c), (r+1, c), (r+2, c)])

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
                                self.create_explosion(cell.x, cell.y, COLORS[old_color])
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
                    self.score = (p[1] << 24) | (p[2] << 16) | (p[3] << 8) | p[4]
                    self.save_leaderboard()

                elif cmd == 0x30:
                    slot = p[1]
                    if slot < 3:
                        self.slot_names[slot] = self.player_name
                        self.save_slot_names()
                    self.show_msg("GAME SAVED TO SLOT!")
                    self.state = "MENU"

                elif cmd == 0x31:
                    if p[4] == 0xEE:
                        self.show_msg("ERROR: SLOT IS EMPTY!")
                    else:
                        self.show_msg("GAME LOADED!")
                        self.state = "PLAYING"

                elif cmd == 0x32:
                    idx = p[1]
                    # Збираємо ім'я в тимчасову змінну
                    if idx == 0: 
                        self.incoming_name = ""
                    chars = [chr(c) for c in p[2:5] if 32 <= c <= 126]
                    self.incoming_name += "".join(chars)
                    
                    # Коли всі пакети імені прийшли - безпечно застосовуємо
                    if idx == 5:
                        self.incoming_name = self.clean_text(self.incoming_name)
                        if self.incoming_name: 
                            self.player_name = self.incoming_name
                            
                        # Синхронізуємо локальний кеш слотів із тим, що прийшло з STM32
                        if self.last_loaded_slot is not None:
                            self.slot_names[self.last_loaded_slot] = self.player_name
                            self.save_slot_names()
                            self.last_loaded_slot = None

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
                        self.start_new_game()
            elif len(unicode) > 0 and unicode.isprintable() and len(self.player_name) < 15:
                if ord(unicode) < 128:
                    self.player_name += unicode

    def click(self, pos):
        if self.rect_left.collidepoint(pos):
            self.available_ports = get_ports() 
            if self.available_ports:
                self.selected_port_index = (self.selected_port_index - 1) % len(self.available_ports)
            return
        elif self.rect_right.collidepoint(pos):
            self.available_ports = get_ports()
            if self.available_ports:
                self.selected_port_index = (self.selected_port_index + 1) % len(self.available_ports)
            return
        elif self.rect_conn.collidepoint(pos):
            if self.connected: self.disconnect()
            else: self.connect()
            return

        if not self.connected:
            self.show_msg("PLEASE CONNECT DEVICE FIRST!")
            return

        if self.state == "MENU":
            cy = HEIGHT // 2 - 100
            if pygame.Rect(WIDTH//2 - 100, cy, 200, 50).collidepoint(pos):
                self.menu_action_target = "NEW"
                self.state = "INPUT_NAME"
            elif pygame.Rect(WIDTH//2 - 100, cy + 70, 200, 50).collidepoint(pos):
                self.menu_action_target = "LOAD"
                self.state = "SLOTS"
            elif pygame.Rect(WIDTH//2 - 100, cy + 140, 200, 50).collidepoint(pos):
                if self.player_name != "":
                    self.menu_action_target = "SAVE"
                    self.send_player_name() 
                    self.state = "SLOTS"
                else:
                    self.show_msg("NO ACTIVE GAME TO SAVE!")
            elif pygame.Rect(WIDTH//2 - 100, cy + 210, 200, 50).collidepoint(pos):
                self.state = "LEADERBOARD"

        elif self.state == "INPUT_NAME":
            if pygame.Rect(WIDTH//2 - 100, HEIGHT//2 + 80, 200, 50).collidepoint(pos):
                if len(self.player_name) > 0:
                    if self.menu_action_target == "NEW":
                        self.start_new_game()
            elif pygame.Rect(WIDTH//2 - 100, HEIGHT//2 + 150, 200, 50).collidepoint(pos):
                self.state = "MENU"

        elif self.state == "SLOTS":
            cy = HEIGHT // 2 - 80
            for i in range(3):
                if pygame.Rect(WIDTH//2 - 150, cy + i*70, 300, 50).collidepoint(pos):
                    if self.menu_action_target == "SAVE":
                        self.slot_names[i] = self.player_name
                        self.save_slot_names()
                        self.send(0x30, i)
                    elif self.menu_action_target == "LOAD":
                        # Миттєво застосовуємо ім'я зі слота, щоб не чекати на відповідь плати
                        if self.slot_names[i] != "EMPTY":
                            self.player_name = self.slot_names[i]
                            
                        self.last_loaded_slot = i
                        self.send(0x31, i)
            if pygame.Rect(WIDTH//2 - 100, cy + 210, 200, 50).collidepoint(pos):
                self.state = "MENU"

        elif self.state == "LEADERBOARD":
            if pygame.Rect(WIDTH//2 - 100, HEIGHT - 180, 200, 50).collidepoint(pos):
                self.state = "MENU"

        elif self.state == "PLAYING":
            menu_btn_rect = pygame.Rect(WIDTH - 120, 10, 100, 40)
            if menu_btn_rect.collidepoint(pos):
                self.state = "MENU"
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
                        self.pending_swap = ((r1, c1), (r, c), c1_color, c2_color)
                    
                    self.send(0x11, r1, c1, r, c)
                self.selected = None

    def draw_button(self, surface, rect, label, base_color, font, hover=False):
        shadow_surface = pygame.Surface((rect.w + 4, rect.h + 4), pygame.SRCALPHA)
        pygame.draw.rect(shadow_surface, (0, 0, 0, 80), shadow_surface.get_rect(), border_radius=10)
        surface.blit(shadow_surface, (rect.x, rect.y + 2))

        color = base_color
        if hover:
            color = (min(255, base_color[0] + 35), min(255, base_color[1] + 35), min(255, base_color[2] + 35))

        pygame.draw.rect(surface, color, rect, border_radius=10)
        pygame.draw.rect(surface, (255, 255, 255), rect, 2, border_radius=10)

        safe_label = self.clean_text(label)
        text_surf = font.render(safe_label, True, (255, 255, 255))
        text_rect = text_surf.get_rect(center=rect.center)

        shadow_txt = font.render(safe_label, True, (0, 0, 0))
        surface.blit(shadow_txt, (text_rect.x + 1, text_rect.y + 1))
        surface.blit(text_surf, text_rect)

    def draw_centered_btn(self, y, text, color, hover_pos):
        rect = pygame.Rect(WIDTH//2 - 100, y, 200, 50)
        self.draw_button(self.screen, rect, text, color, self.font_small, rect.collidepoint(hover_pos))

    def draw(self):
        self.screen.fill(BG_COLOR)
        mx, my = pygame.mouse.get_pos()

        if self.info_timer > 0:
            safe_msg = self.clean_text(self.info_msg)
            msg = self.font_small.render(safe_msg, True, (255, 255, 0))
            self.screen.blit(msg, (WIDTH // 2 - msg.get_width() // 2, 20))
            self.info_timer -= 1

        if self.state == "MENU":
            title = self.font_title.render("STM32 MATCH-3", True, ACCENT)
            self.screen.blit(title, (WIDTH // 2 - title.get_width() // 2, HEIGHT // 2 - 200))
            
            cy = HEIGHT // 2 - 100
            self.draw_centered_btn(cy, "NEW GAME", (40, 150, 80), (mx, my))
            self.draw_centered_btn(cy + 70, "CONTINUE GAME", (200, 120, 20), (mx, my))
            self.draw_centered_btn(cy + 140, "SAVE GAME", (40, 100, 180), (mx, my))
            self.draw_centered_btn(cy + 210, "LEADERBOARD", (120, 60, 180), (mx, my))

        elif self.state == "INPUT_NAME":
            title = self.font_big.render("ENTER NICKNAME:", True, TEXT_COLOR)
            self.screen.blit(title, (WIDTH // 2 - title.get_width() // 2, HEIGHT // 2 - 100))
            
            pygame.draw.rect(self.screen, (30, 40, 60), (WIDTH//2 - 150, HEIGHT//2 - 20, 300, 50), border_radius=10)
            pygame.draw.rect(self.screen, ACCENT, (WIDTH//2 - 150, HEIGHT//2 - 20, 300, 50), 2, border_radius=10)
            
            display_name = self.clean_text(self.player_name)
            name_txt = self.font_big.render(display_name + ("_" if time.time() % 1 > 0.5 else ""), True, (255, 255, 255))
            self.screen.blit(name_txt, (WIDTH // 2 - name_txt.get_width() // 2, HEIGHT // 2 - 15))

            self.draw_centered_btn(HEIGHT//2 + 80, "CONFIRM", (40, 150, 80), (mx, my))
            self.draw_centered_btn(HEIGHT//2 + 150, "BACK", (180, 60, 60), (mx, my))

        elif self.state == "SLOTS":
            title = self.font_big.render("SELECT SAVE SLOT", True, ACCENT)
            self.screen.blit(title, (WIDTH // 2 - title.get_width() // 2, HEIGHT // 2 - 160))
            
            cy = HEIGHT // 2 - 80
            for i in range(3):
                rect = pygame.Rect(WIDTH//2 - 150, cy + i*70, 300, 50)
                clean_name = self.clean_text(self.slot_names[i])
                text = f"SLOT {i+1}: {clean_name}" if clean_name != "EMPTY" else f"SLOT {i+1} (EMPTY)"
                self.draw_button(self.screen, rect, text, (60, 100, 150), self.font_small, rect.collidepoint(mx, my))
            
            self.draw_centered_btn(cy + 210, "BACK", (180, 60, 60), (mx, my))

        elif self.state == "LEADERBOARD":
            title = self.font_big.render("LEADERBOARD", True, (255, 215, 0))
            self.screen.blit(title, (WIDTH // 2 - title.get_width() // 2, 100))

            sy = 180
            for idx, entry in enumerate(self.leaderboard):
                clean_name = self.clean_text(entry['name'])
                txt = self.font_small.render(f"{idx+1}. {clean_name} - {entry['score']}", True, TEXT_COLOR)
                self.screen.blit(txt, (WIDTH // 2 - txt.get_width() // 2, sy))
                sy += 35

            self.draw_centered_btn(HEIGHT - 180, "BACK", (180, 60, 60), (mx, my))

        elif self.state == "PLAYING":
            clean_name = self.clean_text(self.player_name)
            hint_txt = self.font_hint.render(f"PLAYER: {clean_name} | F11 - FULLSCREEN", True, (100, 110, 140))
            self.screen.blit(hint_txt, (10, 10))
            
            menu_btn_rect = pygame.Rect(WIDTH - 120, 10, 100, 40)
            self.draw_button(self.screen, menu_btn_rect, "MENU", (180, 60, 60), self.font_small, menu_btn_rect.collidepoint(mx, my))

            score_txt = self.font_big.render(f"SCORE: {self.score}", True, ACCENT)
            self.screen.blit(score_txt, (WIDTH // 2 - score_txt.get_width() // 2, OFFSET_Y - 50))

            for r in range(BOARD_SIZE):
                for c in range(BOARD_SIZE):
                    x = OFFSET_X + c * CELL_SIZE
                    y = OFFSET_Y + r * CELL_SIZE
                    pygame.draw.rect(self.screen, (30, 38, 70), (x, y, CELL_SIZE, CELL_SIZE), border_radius=12)
                    
                    cell = self.board[r][c]
                    cell.update()
                    cell.draw(self.screen)

                    if self.selected == (r, c):
                        pygame.draw.rect(self.screen, ACCENT, (x, y, CELL_SIZE, CELL_SIZE), 3, border_radius=12)

            for p in self.particles[:]:
                p.update()
                p.draw(self.screen)
                if p.life <= 0:
                    self.particles.remove(p)

        panel_y = HEIGHT - 70
        pygame.draw.rect(self.screen, PANEL_COLOR, (0, panel_y, WIDTH, 70))
        pygame.draw.line(self.screen, (40, 45, 75), (0, panel_y), (WIDTH, panel_y), 2)

        port_name = "NO PORT"
        if self.available_ports:
            self.selected_port_index = self.selected_port_index % len(self.available_ports)
            port_name = self.available_ports[self.selected_port_index]

        port_txt = self.font_hint.render(f"PORT: {port_name}", True, TEXT_COLOR)
        txt_x = self.rect_left.right + (self.rect_right.left - self.rect_left.right) // 2 - port_txt.get_width() // 2
        self.screen.blit(port_txt, (txt_x, panel_y + 27))

        self.draw_button(self.screen, self.rect_left, "<", ACCENT, self.font_small, self.rect_left.collidepoint(mx, my))
        self.draw_button(self.screen, self.rect_right, ">", ACCENT, self.font_small, self.rect_right.collidepoint(mx, my))

        btn_color = (30, 180, 100) if not self.connected else (220, 60, 60)
        btn_text = "CONNECT" if not self.connected else "DISCONNECT"
        self.draw_button(self.screen, self.rect_conn, btn_text, btn_color, self.font_small, self.rect_conn.collidepoint(mx, my))

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
                        self.screen = pygame.display.set_mode((WIDTH, HEIGHT), pygame.RESIZABLE)
                        self.update_layout()

                if e.type == pygame.KEYDOWN:
                    self.handle_keydown(e.key, e.unicode)
                    
                    if e.key == pygame.K_F11:
                        self.toggle_fullscreen()
                    elif e.key == pygame.K_ESCAPE:
                        if self.is_fullscreen and self.state == "PLAYING":
                            self.toggle_fullscreen()
                        elif self.state == "PLAYING":
                            self.state = "MENU"

                if e.type == pygame.MOUSEBUTTONDOWN and e.button == 1:
                    self.click(e.pos)

                if e.type == pygame.USEREVENT + 1 and self.state == "PLAYING":
                    self.send(0x15)

            self.clock.tick(60)

        self.save_leaderboard()
        pygame.quit()

if __name__ == "__main__":
    Match3Game().run()
