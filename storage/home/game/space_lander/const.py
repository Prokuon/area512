CHARACTER_HEIGHT = 10
BOARD_WIDTH = 240
BOARD_HEIGHT = 130
STATUS_Y = 119
FONT_SIZE = 12
FRAME_MILLISECONDS = 35
SHIP_FOOT_X = 7
SHIP_FOOT_Y = 10

COLOR_BACKGROUND = 0x000000
COLOR_AMBER = 0xF5972D
COLOR_DIM = 0xCFA45F
COLOR_GOLD = 0xFFD966

COLOR_TEXT = COLOR_DIM
COLOR_SHIP = COLOR_GOLD
COLOR_FLAME = COLOR_AMBER
COLOR_GROUND = COLOR_DIM
COLOR_PAD = COLOR_GOLD
COLOR_CRASH = COLOR_AMBER
COLOR_DEBRIS = COLOR_AMBER

WORLD_WIDTH = 240.0
GRAVITY = 0.035
MAIN_THRUST = 0.105
SIDE_THRUST = 0.038
DRAG = 0.998
GROUND_Y = 101

STATE_READY = 0
STATE_PLAY = 1
STATE_LANDED = 2
STATE_CRASHED = 3


def pick_random_below(limit):
    if limit <= 0:
        return 0

    return RNG.random_int() % limit


def format_one_decimal(value):
    scaled = int(value * 10.0)

    if scaled < 0:
        return "-{}.{}".format(-scaled // 10, -scaled % 10)

    return "{}.{}".format(scaled // 10, scaled % 10)
