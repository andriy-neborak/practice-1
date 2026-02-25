import tkinter as tk
import random

# Конфігурація
GRID_SIZE = 8
CELL_SIZE = 60
BALL_COLORS = ["red", "green", "blue", "yellow", "purple", "orange"]

class Match3GUI:
    def __init__(self, root):
        self.root = root
        self.root.title("8x8")

        canvas_size = GRID_SIZE * CELL_SIZE
        self.canvas = tk.Canvas(root, width=canvas_size, height=canvas_size, bg="white")
        self.canvas.pack()

        self.board = [[None for _ in range(GRID_SIZE)] for _ in range(GRID_SIZE)]

        self.create_grid()
        self.place_balls()

    def create_grid(self):
        for row in range(GRID_SIZE):
            for col in range(GRID_SIZE):
                x1 = col * CELL_SIZE
                y1 = row * CELL_SIZE
                x2 = x1 + CELL_SIZE
                y2 = y1 + CELL_SIZE
                self.canvas.create_rectangle(x1, y1, x2, y2, outline="black")

    def place_balls(self):
        padding = 10
        for row in range(GRID_SIZE):
            for col in range(GRID_SIZE):
                color = random.choice(BALL_COLORS)

                x1 = col * CELL_SIZE + padding
                y1 = row * CELL_SIZE + padding
                x2 = (col + 1) * CELL_SIZE - padding
                y2 = (row + 1) * CELL_SIZE - padding

                ball = self.canvas.create_oval(x1, y1, x2, y2, fill=color, outline="")
                self.board[row][col] = ball


if __name__ == "__main__":
    root = tk.Tk()
    game = Match3GUI(root)
    root.mainloop()
