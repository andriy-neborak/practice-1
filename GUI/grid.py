import tkinter as tk

GRID_SIZE = 8
CELL_SIZE = 60

class GameGrid:
    def __init__(self, root):
        self.root = root
        self.root.title("Grid 8x8")

        canvas_size = GRID_SIZE * CELL_SIZE
        self.canvas = tk.Canvas(root, width=canvas_size, height=canvas_size, bg="white")
        self.canvas.pack()

        self.create_grid()

    def create_grid(self):
        for row in range(GRID_SIZE):
            for col in range(GRID_SIZE):
                x1 = col * CELL_SIZE
                y1 = row * CELL_SIZE
                x2 = x1 + CELL_SIZE
                y2 = y1 + CELL_SIZE
                self.canvas.create_rectangle(x1, y1, x2, y2, outline="black")


if __name__ == "__main__":
    root = tk.Tk()
    game = GameGrid(root)
    root.mainloop()
