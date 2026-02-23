import pygame
import pygame.gfxdraw
import serial
import serial.tools.list_ports
import threading
import time
from collections import deque
import random
import math

BAUD_RATE = 38400

BOARD_SIZE = 8
CELL_SIZE = 60

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
            pygame.draw.circle(surface, self.color,
                               (int(self.x), int(self.y)), 3)


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
            self.scale = 0.2
            return old
        return None

    def update_position(self, new_r):
        self.r = new_r
        self.target_y = OFFSET_Y + new_r * CELL_SIZE + CELL_SIZE // 2

    def update(self):
        self.y += (self.target_y - self.y) * 0.18
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
        self.screen = pygame.display.set_mode((WIDTH, HEIGHT))
        pygame.display.set_caption("STM32 3 IN A ROW")
        self.clock = pygame.time.Clock()

        self.font_big = pygame.font.SysFont("Segoe UI", 36, bold=True)
        self.font_small = pygame.font.SysFont("Segoe UI", 18)

        self.board = [[AnimCell(r, c) for c in range(BOARD_SIZE)]
                      for r in range(BOARD_SIZE)]

        self.particles = []
        self.score = 0
        self.selected = None
        self.busy = False
        self.running = True

        self.flash_alpha = 0
        self.flash_decay = 14

        # UART
        self.available_ports = get_ports()
        self.selected_port_index = 0
        self.connected = False
        self.ser = None
        self.queue = deque()
        self.lock = threading.Lock()
        self.last_rx_time = 0

        self.reader_thread = threading.Thread(
            target=self.reader_loop,
            daemon=True
        )
        self.reader_thread.start()

        pygame.time.set_timer(pygame.USEREVENT + 1, 1000)
        pygame.time.set_timer(pygame.USEREVENT + 2, 5000)


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
        except:
            self.disconnect()

    def reader_loop(self):
        while self.running:
            if self.connected and self.ser:
                try:
                    raw = self.ser.read(6)
                    if len(raw) == 6 and self.crc8(raw[:5]) == raw[5]:
                        with self.lock:
                            self.queue.append(raw)
                        self.last_rx_time = time.time()
                except:
                    self.disconnect()
            else:
                time.sleep(0.05)

    def connect(self):
        if self.connected or not self.available_ports:
            return
        try:
            port = self.available_ports[self.selected_port_index]
            self.ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
            self.connected = True
            self.last_rx_time = time.time()

            for r in range(BOARD_SIZE):
                for c in range(BOARD_SIZE):
                    self.board[r][c].color = 0

            self.new_game()
        except:
            self.connected = False

    def disconnect(self):
        try:
            if self.ser:
                self.ser.close()
        except:
            pass
        self.connected = False
        self.busy = False


    def new_game(self):
        self.score = 0
        self.busy = False
        self.selected = None

        # форсуємо всі клітини на 0
        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                self.board[r][c].color = 0

        self.send(0x10)

    def create_explosion(self, x, y, color):
        for _ in range(18):
            self.particles.append(Particle(x, y, color))
        self.flash_alpha = 170

    def process_uart(self):
        with self.lock:
            while self.queue:
                p = self.queue.popleft()
                cmd = p[0]

                if cmd == 0x16:
                    r, c, color = p[1], p[2], p[3]
                    if 0 <= r < BOARD_SIZE and 0 <= c < BOARD_SIZE:
                        cell = self.board[r][c]
                        old_color = cell.sync(color)
                        if color == 0 and old_color:
                            self.create_explosion(
                                cell.x,
                                cell.y,
                                COLORS[old_color]
                            )
                        cell.update_position(r)

                elif cmd == 0x11:
                    self.busy = False

                elif cmd == 0x15:
                    self.score = (
                        (p[1] << 24) |
                        (p[2] << 16) |
                        (p[3] << 8) |
                        p[4]
                    )

        if self.connected and time.time() - self.last_rx_time > 1.5:
            self.disconnect()


    def click(self, pos):

        panel_y = HEIGHT - 70

        # left arrow
        if 200 <= pos[0] <= 240 and panel_y + 20 <= pos[1] <= panel_y + 55:
            if self.available_ports:
                self.selected_port_index = \
                    (self.selected_port_index - 1) % len(self.available_ports)

        # right arrow
        elif 360 <= pos[0] <= 400 and panel_y + 20 <= pos[1] <= panel_y + 55:
            if self.available_ports:
                self.selected_port_index = \
                    (self.selected_port_index + 1) % len(self.available_ports)

        # connect button
        elif 430 <= pos[0] <= 580 and panel_y + 15 <= pos[1] <= panel_y + 55:
            if self.connected:
                self.disconnect()
            else:
                self.connect()

        else:
            if self.busy or not self.connected:
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
                    self.send(0x11, r1, c1, r, c)
                self.selected = None


    def draw(self):

        self.screen.fill(BG_COLOR)

        title = self.font_big.render("3 IN A ROW", True, ACCENT)
        self.screen.blit(title,
                         (WIDTH // 2 - title.get_width() // 2, 50))

        score_txt = self.font_small.render(f"SCORE: {self.score}",
                                           True, TEXT_COLOR)
        self.screen.blit(score_txt,
                         (WIDTH // 2 - score_txt.get_width() // 2, 110))

        for r in range(BOARD_SIZE):
            for c in range(BOARD_SIZE):
                x = OFFSET_X + c * CELL_SIZE
                y = OFFSET_Y + r * CELL_SIZE

                pygame.draw.rect(self.screen,
                                 (30, 38, 70),
                                 (x, y, CELL_SIZE, CELL_SIZE),
                                 border_radius=12)

                cell = self.board[r][c]
                cell.update()
                cell.draw(self.screen)

                if self.selected == (r, c):
                    pygame.draw.rect(self.screen,
                                     ACCENT,
                                     (x, y, CELL_SIZE, CELL_SIZE),
                                     3,
                                     border_radius=12)

        for p in self.particles[:]:
            p.update()
            p.draw(self.screen)
            if p.life <= 0:
                self.particles.remove(p)

        if self.flash_alpha > 0:
            flash_surface = pygame.Surface((WIDTH, HEIGHT))
            flash_surface.fill((255, 255, 255))
            flash_surface.set_alpha(self.flash_alpha)
            self.screen.blit(flash_surface, (0, 0))
            self.flash_alpha -= self.flash_decay
            if self.flash_alpha < 0:
                self.flash_alpha = 0

        panel_y = HEIGHT - 70
        pygame.draw.rect(self.screen, PANEL_COLOR,
                         (0, panel_y, WIDTH, 70))

        port_name = "NO PORT"
        if self.available_ports:
            port_name = self.available_ports[self.selected_port_index]

        port_txt = self.font_small.render(
            f"PORT: {port_name}",
            True,
            TEXT_COLOR
        )
        self.screen.blit(port_txt, (250, panel_y + 25))

        pygame.draw.rect(self.screen, ACCENT,
                         (200, panel_y + 20, 40, 35),
                         border_radius=6)
        pygame.draw.rect(self.screen, ACCENT,
                         (360, panel_y + 20, 40, 35),
                         border_radius=6)

        self.screen.blit(self.font_small.render("<", True, (0, 0, 0)),
                         (212, panel_y + 23))
        self.screen.blit(self.font_small.render(">", True, (0, 0, 0)),
                         (372, panel_y + 23))

        btn_color = (0, 200, 120) if not self.connected else (200, 70, 70)
        pygame.draw.rect(self.screen, btn_color,
                         (430, panel_y + 15, 150, 40),
                         border_radius=8)

        label = "CONNECT" if not self.connected else "DISCONNECT"
        self.screen.blit(self.font_small.render(label, True, (0, 0, 0)),
                         (450, panel_y + 25))

        pygame.display.flip()


    def run(self):
        while self.running:

            self.process_uart()
            self.draw()

            for e in pygame.event.get():
                if e.type == pygame.QUIT:
                    self.running = False

                if e.type == pygame.MOUSEBUTTONDOWN and e.button == 1:
                    self.click(e.pos)

                if e.type == pygame.USEREVENT + 1:
                    self.send(0x15)

                if e.type == pygame.USEREVENT + 2:
                    self.send(0x12)

            self.clock.tick(60)

        pygame.quit()


if __name__ == "__main__":
    Match3Game().run()