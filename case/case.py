# Handheld case for the ESP32-2432S028R ("CYD"). Draft 4: a sandwich of four printed layers -
# face (screen side), frame, back with the battery step, battery lid - held by four M3 screws through the board holes.
# Draft 4 vs 3: walls 2.8 instead of 2 (4.5 at the antenna end), thicker face and back, stiffening ribs inside the back,
# and a stiff battery lid held by two hooks at the antenna end and one M3 screw at the other end instead of a sliding door.
# A bought 2 x AA holder (59 x 33 x 16) lies across the back at the antenna end; the rest of the back stays thin.
# Screws run through all layers: heads sunk in the face, nuts in open hex pockets (back face / holder shelf).
# Frame: X to the right as you look at the screen, Y along the case (antenna end at 0, USB end at L),
# Z from the front face (0) into the case. That frame is left-handed, so every solid is mirrored in X on export.
# All numbers are millimetres. Sized for a resin printer: metal nuts instead of threads in plastic, 0.15-0.2 gaps.
import cadquery as cq
from pathlib import Path

# ---------------- measured or taken from the board drawing ----------------
PCB_W, PCB_L, PCB_T = 50.0, 86.0, 1.6
HOLE_INSET = 4.0                    # mounting holes, 78 x 42 between centres, 3.2 dia
GLASS_TO_PCB_BACK = 5.0
TALLEST_BACK = 5.0                  # speaker connector above the board
PLUG_EXTRA = 2.0                    # speaker plug and wire above the connector
P3_H = 4.3                          # P3 / CN1 connectors on the long edge near the antenna end
USB_FROM_SD_EDGE = 15.4
P1_FROM_SD_EDGE = 36.9
PANEL_Y0, PANEL_Y1 = 8.6, 77.5
ACTIVE_X0, ACTIVE_X1 = 3.4, 46.6
ACTIVE_Y0, ACTIVE_Y1 = 11.3, 68.9
HOLDER_X, HOLDER_Y, HOLDER_Z = 59.0, 33.0, 16.0  # measured 2 x AA holder; leads leave one short end
STYLUS_L, STYLUS_D, HEAD_L, HEAD_A, HEAD_B = 87.5, 5.0, 12.0, 8.0, 5.0
SW_BRACKET_L, SW_BODY_L, SW_BODY_W, SW_BODY_H, SW_LEVER = 19.7, 10.6, 6.0, 5.0, 3.0
SPK_X, SPK_Y, SPK_T = 15.0, 10.0, 3.0                 # speaker turned across the case
BOOST_X, BOOST_Y, BOOST_T = 17.8, 11.3, 5.6      # Adafruit MiniBoost 5V (TPS61023, product 4654), turned across the case
NUT_AF, NUT_H = 5.5, 2.4                         # M3 nut
SCREW_USB, SCREW_ANT, SCREW_LID = 12.0, 8.0, 8.0     # M3: x12 at the USB end (nut outside on the back), x8 at the antenna end (nut on the holder shelf), x8 for the lid
HEAD_SEAT = 3.3                                  # screw heads sunk into the face down to here

# ---------------- case ----------------
WALL, WALL_END, R = 2.8, 4.5, 3.5    # side and USB-end walls; the antenna-end wall also holds the lid hooks
W = HOLDER_X + 1.0 + 2 * WALL        # the holder needs the width, plus 1 mm for its leads
FRONT = 1.8
SKIN = 2.0                           # back plate
LID_T, LEDGE = 2.6, 1.2              # battery lid thickness; ledge the lid rests on, cut into the walls
GAP = 0.15
USB_GAP = 4.0                        # board edge to the inner face of the USB end wall (P1 plug and its leads)
Z_GLASS = FRONT + 0.3
Z_PCB_B = Z_GLASS + GLASS_TO_PCB_BACK                  # 6.8: face / frame split
Z_PCB_F = Z_PCB_B - PCB_T
Z_LID = Z_PCB_B + TALLEST_BACK + PLUG_EXTRA + 0.6      # 14.4: frame / back split
T = Z_LID + SKIN                                       # 16.0: thin part
Z_FLOOR = Z_PCB_B + P3_H + 0.2                         # 11.3: underside of the holder shelf
Z_SHELF = Z_FLOOR + 1.5                                # holder sits here
Z_DOOR = Z_SHELF + HOLDER_Z + 0.2                      # underside of the battery lid
Z_TOP = Z_DOOR + LID_T                                 # battery step
PCB_X0, PCB_Y0 = WALL + 0.3, WALL_END + 0.3
PCB_Y1 = PCB_Y0 + PCB_L
L = PCB_Y1 + USB_GAP + WALL
BAY_Y1 = WALL_END + HOLDER_Y + 0.6                     # end of the holder pocket
STEP_Y = BAY_Y1 + 8.5                                  # end of the step: solid block that takes the lid screw
LID_SCREW = (W / 2, BAY_Y1 + 3.75)
TAB_XS, TAB_W, TAB_L, TAB_T = (W / 2 - 15.0, W / 2 + 15.0), 8.0, 1.6, 1.2   # lid hooks at the antenna end
RAMP = 8.0                                             # slope from the step down to the thin back
STRIP_X0 = PCB_X0 + PCB_W + 0.3                        # stylus strip beside the board
CH_X1 = W - WALL - 0.8
CH_X0 = CH_X1 - STYLUS_D - 0.6
CH_Z0 = 1.2
SLOT_W, SLOT_L = HEAD_B + 0.3, HEAD_L + 0.3
DIP_L, DIP_Z1, DIP_R = 20.0, 10.5, 3.0                 # finger dip around the stylus head: length, depth into the case, corner radius
CH_FRONT, CH_BACK, CH_STEP = 2.0, 1.2, 1.2             # edge chamfers
SW_Y, SW_Z0 = PCB_Y0 + 54.7, Z_PCB_B + 0.8             # switch centre; it stands on the roof of the stylus channel
RIB_H, RIB_W = 0.6, 3.0
RIB_YS = tuple(6.0 + 6.0 * i for i in range(int((STEP_Y - 10.5) / 6.0) + 1))   # grip ribs on both long sides of the battery step
STIFF_H, STIFF_W = 2.0, 1.4                            # stiffening ribs under the thin back
GROOVE_W, GROOVE_D = 1.0, 0.45                         # grooves on the face below the screen
LIP_IN, LIP_OUT, LIP_H = WALL - 0.85, WALL - 0.15, 1.15   # tongue between layers, in the inner millimetre of the wall
HOLES = [(PCB_X0 + x, PCB_Y0 + y) for x in (HOLE_INSET, PCB_W - HOLE_INSET) for y in (HOLE_INSET, PCB_L - HOLE_INSET)]
USB_X = PCB_X0 + PCB_W - USB_FROM_SD_EDGE
P1_X = PCB_X0 + PCB_W - P1_FROM_SD_EDGE
WIN = (PCB_X0 + ACTIVE_X0 - 0.8, PCB_X0 + ACTIVE_X1 + 0.8, PCB_Y0 + ACTIVE_Y0 - 0.8, PCB_Y0 + ACTIVE_Y1 + 0.8)
SPK_C = (PCB_X0 + 25.0, STEP_Y + RAMP + 1.35 + 0.8 + SPK_Y / 2)
BOOST_C = (PCB_X0 + 22.0, SPK_C[1] + SPK_Y / 2 + 1.35 + 1.0 + 1.4 + BOOST_Y / 2)
HOLDER_BOX = (WALL + 0.5, WALL + 0.5 + HOLDER_X, WALL_END + 0.3, WALL_END + 0.3 + HOLDER_Y, Z_SHELF, Z_SHELF + HOLDER_Z)


def box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))


def cyl(x, y, z0, z1, d):
    return cq.Workplane("XY").circle(d / 2).extrude(z1 - z0).translate((x, y, z0))


def prism_xz(pts, y0, y1):
    wire = cq.Wire.makePolygon([cq.Vector(x, y0, z) for x, z in pts], close=True)
    return cq.Workplane("XY").add(cq.Solid.extrudeLinear(cq.Face.makeFromWires(wire), cq.Vector(0, y1 - y0, 0)))


def prism_yz(pts, x0, x1):
    wire = cq.Wire.makePolygon([cq.Vector(x0, y, z) for y, z in pts], close=True)
    return cq.Workplane("XY").add(cq.Solid.extrudeLinear(cq.Face.makeFromWires(wire), cq.Vector(x1 - x0, 0, 0)))


def hexagon(x, y, z0, z1, across_flats):
    return cq.Workplane("XY").polygon(6, across_flats / 0.8660254).extrude(z1 - z0).translate((x, y, z0))


def round_corners(solid, corners, r):
    for cx, cy in corners:
        solid = solid.edges(cq.selectors.BoxSelector((cx - 0.05, cy - 0.05, -100), (cx + 0.05, cy + 0.05, 100))).fillet(r)
    return solid


def plate(inset, z0, z1):
    """The case outline shrunk by `inset`, as a slab between z0 and z1."""
    return (cq.Workplane("XY").rect(W - 2 * inset, L - 2 * inset, centered=False).extrude(z1 - z0)
            .edges("|Z").fillet(R - inset).translate((inset, inset, z0)))


def ring(i0, i1, z0, z1):
    return plate(i0, z0, z1).cut(plate(i1, z0 - 1, z1 + 1))


def back_outline():
    """Outer shape of the back layer: thin plate, the battery step at the antenna end and a slope between them.
    Built with depth u measured from the back face (u = T - z), then flipped into place."""
    hump, zb = Z_TOP - T, SKIN
    slab = cq.Workplane("XY").rect(W, L, centered=False).extrude(zb).edges("|Z").fillet(R).faces("<Z").chamfer(CH_BACK)
    block = round_corners(box(0, W, 0, STEP_Y + 5, -hump, zb), [(0, 0), (W, 0)], R).faces("<Z").chamfer(CH_STEP)
    block = block.cut(box(-1, W + 1, STEP_Y, STEP_Y + 6, -hump - 1, zb + 1))
    y0, y1 = STEP_Y, STEP_Y + RAMP
    ramp = prism_yz([(y0, -hump), (y1, 0.0), (y1, zb), (y0, zb)], 0, W)

    def corner(y, right):
        t = (y - y0) / (y1 - y0)
        zf, c = -hump * (1 - t), CH_STEP + (CH_BACK - CH_STEP) * t
        pts = [(-1.0, zf - 1.0), (c + 1.0, zf - 1.0), (-1.0, zf + c + 1.0)]
        return cq.Wire.makePolygon([cq.Vector(W - x if right else x, y, z) for x, z in pts], close=True)
    for right in (False, True):
        ramp = ramp.cut(cq.Workplane("XY").add(cq.Solid.makeLoft([corner(y0 - 0.5, right), corner(y1 + 0.5, right)], True)))
    shape = slab.union(block).union(ramp)
    return shape.mirror("XY").translate((0, 0, T))


def lid_outline(gap, z0, z1):
    """Battery lid seen from the back: the pocket plus a LEDGE all round, and an ear over the lid screw."""
    g = gap
    main = round_corners(box(WALL - LEDGE + g, W - WALL + LEDGE - g, WALL_END - LEDGE + g, BAY_Y1 + LEDGE - g, z0, z1),
                         [(WALL - LEDGE + g, WALL_END - LEDGE + g), (W - WALL + LEDGE - g, WALL_END - LEDGE + g)], 1.5)
    ear = round_corners(box(W / 2 - 7 + g, W / 2 + 7 - g, BAY_Y1, LID_SCREW[1] + 3.75 - g, z0, z1),
                        [(W / 2 - 7 + g, LID_SCREW[1] + 3.75 - g), (W / 2 + 7 - g, LID_SCREW[1] + 3.75 - g)], 2.0)
    return main.union(ear)


def finger_dip():
    """Dip in the side wall around the stylus head: open towards the USB end and to the face, 2.8 deep."""
    d = box(CH_X1, W + 1, L - DIP_L, L + 1, -1, DIP_Z1)
    return d.edges("|X").edges("<Y").fillet(DIP_R)


def openings(s):
    """Cuts shared by the face and the frame."""
    s = s.cut(box(USB_X - 6.25, USB_X + 6.25, L - WALL - 0.2, L + 1, Z_PCB_B + 1.3 - 4.25, Z_PCB_B + 1.3 + 4.25))
    zs = CH_Z0 + 0.25 + STYLUS_D / 2
    s = s.cut(box(CH_X0, CH_X1, L - 6, L + 1, CH_Z0, Z_PCB_B))                       # stylus way out through the tongue
    s = s.cut(box(CH_X1 - 0.1, W + 1, L - SLOT_L, L + 1, zs - SLOT_W / 2, zs + SLOT_W / 2))
    return s.cut(finger_dip())


def true_shape(s):
    return s.mirror("YZ", (W / 2, 0, 0))


# ---------------- surface texture ----------------
def _ribs(points, sx, sy, z0, z1):
    return cq.Workplane("XY").pushPoints(points).rect(sx, sy).extrude(z1 - z0).translate((0, 0, z0))


def rib_set():
    """Grip ribs on both long sides of the battery step, mirror images of each other. They are trimmed to an envelope
    that follows the case outline RIB_H further out, with chamfers parallel to the case's own, so each rib runs over
    the front and back edges."""
    t, k = RIB_H, RIB_H * (1 - 2 ** 0.5)
    env = plate(-t, 0, Z_TOP).faces("<Z").chamfer(CH_FRONT + k).faces(">Z").chamfer(CH_STEP + k)
    depth = t + 0.4
    pts = [(-t + depth / 2, y) for y in RIB_YS] + [(W + t - depth / 2, y) for y in RIB_YS]
    r = cq.Workplane("XY").pushPoints(pts).rect(depth, RIB_W).extrude(42).translate((0, 0, -1)).edges("|Z").fillet(0.35)
    return r.intersect(env)


RIBS = None


def add_ribs(part, z0, z1):
    global RIBS
    if RIBS is None:
        RIBS = rib_set()
    return part.union(RIBS.intersect(box(-5, W + 5, -5, L + 5, z0, z1)))


# ---------------- printed parts ----------------
def make_face():
    f = plate(0, 0, Z_PCB_B).faces("<Z").chamfer(CH_FRONT)
    f = f.cut(round_corners(box(WALL, STRIP_X0, WALL_END, L - WALL, FRONT, Z_PCB_B + 1), [(WALL, WALL_END), (WALL, L - WALL)], 0.8))
    f = f.cut(ring(WALL - 1.0, WALL + 0.01, Z_PCB_B - LIP_H - 0.05, Z_PCB_B + 1))   # seat for the frame's tongue
    y_ch = L - STYLUS_L - 0.8
    zs = CH_Z0 + 0.25 + STYLUS_D / 2
    f = f.cut(box(CH_X0, CH_X1, y_ch, L + 1, CH_Z0, Z_PCB_B + 1))                   # stylus channel, roofed by the frame
    f = f.cut(box(CH_X1 - 0.1, W + 1, L - SLOT_L, L + 1, zs - SLOT_W / 2, zs + SLOT_W / 2))
    wx0, wx1, wy0, wy1 = WIN
    f = f.cut(box(wx0, wx1, wy0, wy1, -1, FRONT + 0.1))
    bevel = cq.Workplane("XY").rect(wx1 - wx0 + 2.0, wy1 - wy0 + 2.0).extrude(1.0, taper=45).translate(((wx0 + wx1) / 2, (wy0 + wy1) / 2, 0))
    f = f.cut(bevel)
    for x, y in HOLES:                                                              # posts under the board
        f = f.union(cyl(x, y, FRONT - 0.01, Z_PCB_F, 8.0))
    for x, y in HOLES:                                                              # M3 from the face, head sunk flush
        f = f.cut(cyl(x, y, -1, Z_PCB_F + 0.1, 3.4)).cut(cyl(x, y, -1, HEAD_SEAT, 6.4))
    for i in range(6):                                                              # grooves on the chin below the screen
        y = WIN[3] + 4.0 + i * 2.5
        f = f.cut(box(12.0, 42.0, y, y + GROOVE_W, -1, GROOVE_D))
    return openings(add_ribs(f, 0, Z_PCB_B))


def make_frame():
    fr = ring(0, WALL, Z_PCB_B, Z_LID)
    fr = fr.union(ring(LIP_IN, LIP_OUT, Z_PCB_B - LIP_H, Z_PCB_B + 0.01))           # tongue into the face
    fr = fr.cut(ring(WALL - 1.0, WALL + 0.01, Z_LID - LIP_H - 0.05, Z_LID + 1))     # seat for the back's tongue
    fr = fr.union(box(WALL - 0.01, W - WALL + 0.01, WALL - 0.01, WALL_END, Z_PCB_B, Z_FLOOR - 0.05))   # thicker antenna-end wall
    fr = fr.cut(ring(WALL - 1.0, WALL + 0.01, Z_FLOOR - 0.05, Z_LID + 1).intersect(box(-1, W + 1, -1, STEP_Y, 0, 40)))   # deeper seat by the batteries
    fr = fr.union(box(STRIP_X0, W - WALL + 0.01, WALL - 0.01, L - WALL + 0.01, Z_PCB_B, SW_Z0))   # roof over the stylus channel
    fr = fr.union(box(STRIP_X0 + 0.4, W - WALL + 0.01, L - DIP_L - 4, L - WALL + 0.01, SW_Z0 - 0.01, DIP_Z1 + 2.0))  # backing for the dip
    xw = W - WALL
    for s in (-1, 1):                                                               # holders for the switch ears
        ya, yb = sorted((SW_Y + s * 5.6, SW_Y + s * 10.4))
        fr = fr.union(box(xw - 2.0, xw - 0.8, ya, yb, SW_Z0 - 0.01, SW_Z0 + SW_BODY_W))
    fr = fr.cut(box(xw - 0.5, W + 1, SW_Y - 3.6, SW_Y + 3.6, SW_Z0 + 0.8, Z_LID + 1))  # switch drops in from the back
    return openings(add_ribs(fr, Z_PCB_B, Z_LID))


def make_back():
    b = back_outline()
    b = b.union(ring(LIP_IN, LIP_OUT, Z_LID - LIP_H, Z_LID + 0.01))                 # tongue into the frame
    # holder pocket: shelf and its end wall reach down inside the frame
    shelf = box(WALL + GAP, W - WALL - GAP, WALL + GAP, STEP_Y, Z_FLOOR, Z_LID + 0.01)
    b = b.union(round_corners(shelf, [(WALL + GAP, WALL + GAP), (W - WALL - GAP, WALL + GAP)], R - WALL - GAP))
    b = b.union(ring(LIP_IN, WALL + GAP + 0.01, Z_FLOOR, Z_LID + 0.01).intersect(box(-1, W + 1, -1, STEP_Y, 0, 40)))   # skirt: shelf meets the wall
    b = b.cut(round_corners(box(WALL, W - WALL, WALL_END, BAY_Y1, Z_SHELF, Z_TOP + 1), [(WALL, WALL_END), (W - WALL, WALL_END), (WALL, BAY_Y1), (W - WALL, BAY_Y1)], 1.0))
    b = b.cut(box(W - WALL - 9.5, W - WALL - 0.3, WALL_END + 8, WALL_END + 25, Z_FLOOR - 1, Z_SHELF + 0.1))   # leads go down here
    b = b.cut(lid_outline(0, Z_DOOR, Z_TOP + 1))                                   # recess the lid sits in, on a LEDGE all round
    for tx in TAB_XS:                                                               # slots for the lid hooks in the antenna-end wall
        b = b.cut(box(tx - TAB_W / 2 - 0.4, tx + TAB_W / 2 + 0.4, WALL_END - LEDGE - TAB_L - 0.3, WALL_END - LEDGE + 0.1, Z_DOOR - 0.05, Z_DOOR + TAB_T + 0.25))
    lx, ly = LID_SCREW                                                              # lid screw: hole and a nut slot opening into the pocket
    tip = Z_TOP - 1.3 - SCREW_LID
    b = b.cut(cyl(lx, ly, tip - 1.0, Z_DOOR + 1, 3.4))
    b = b.cut(box(lx - (NUT_AF + 0.3) / 2, lx + (NUT_AF + 0.3) / 2, BAY_Y1 - 0.1, ly + 3.3, tip + 0.3, tip + 0.3 + NUT_H + 0.3))
    # posts on the back of the board; each holds a nut that the board keeps in place
    for x, y in HOLES:
        top = Z_FLOOR + 0.01 if y < L / 2 else Z_LID + 0.01
        post = cyl(x, y, Z_PCB_B, top, 8.5)
        if x < W / 2 and y < L / 2:      # flat towards the RESET button, about 3 mm from this hole
            post = post.cut(box(x - 5, x + 5, y + 3.05, y + 6, Z_PCB_B - 1, Z_PCB_B + 2.5))
        b = b.union(post)
    for x, y in HOLES:      # screws go right through; nuts drop into hex pockets from outside, 2 mm short of the screw tip
        if y > L / 2:       # USB end: nut on the back face
            b = b.cut(cyl(x, y, Z_PCB_B - 0.1, T + 1, 3.4)).cut(hexagon(x, y, HEAD_SEAT + SCREW_USB - 2.0, T + 1, NUT_AF + 0.25))
        else:               # antenna end: nut on the holder shelf, under the holder
            b = b.cut(cyl(x, y, Z_PCB_B - 0.1, Z_SHELF + 1, 3.4)).cut(hexagon(x, y, HEAD_SEAT + SCREW_ANT - 2.0, Z_SHELF + 1, NUT_AF + 0.25))
    # pockets for the speaker and the converter, as in draft 2
    sx, sy = SPK_C
    b = b.union(box(sx - SPK_X / 2 - 1.35, sx + SPK_X / 2 + 1.35, sy - SPK_Y / 2 - 1.35, sy + SPK_Y / 2 + 1.35, Z_LID - 2.4, Z_LID + 0.01))
    b = b.cut(box(sx - SPK_X / 2 - 0.15, sx + SPK_X / 2 + 0.15, sy - SPK_Y / 2 - 0.15, sy + SPK_Y / 2 + 0.15, Z_LID - 3, Z_LID))
    b = b.cut(box(sx - 1.2, sx + 1.2, sy - SPK_Y / 2 - 2, sy - SPK_Y / 2 + 1, Z_LID - 3, Z_LID))
    for dx, dy in ((0, 0), (0, 3.2), (0, -3.2), (2.6, 1.6), (2.6, -1.6), (-2.6, 1.6), (-2.6, -1.6)):
        b = b.cut(cyl(sx + dx, sy + dy, Z_LID - 1, T + 1, 1.6))
    bx, by = BOOST_C
    b = b.union(box(bx - BOOST_X / 2 - 1.4, bx + BOOST_X / 2 + 1.4, by - BOOST_Y / 2 - 1.4, by + BOOST_Y / 2 + 1.4, Z_LID - 2.2, Z_LID + 0.01))
    b = b.cut(box(bx - BOOST_X / 2 - 0.2, bx + BOOST_X / 2 + 0.2, by - BOOST_Y / 2 - 0.2, by + BOOST_Y / 2 + 0.2, Z_LID - 3, Z_LID))
    b = b.cut(box(bx - BOOST_X / 2 - 2, bx + BOOST_X / 2 + 2, by - 4, by + 4, Z_LID - 3, Z_LID))   # wires leave at both short ends
    # plug that closes the top of the switch slot in the frame
    b = b.union(box(W - WALL - 0.35, W - 0.01, SW_Y - 3.45, SW_Y + 3.45, SW_Z0 + SW_BODY_W + 0.15, Z_LID + 0.01))
    # stiffening ribs under the thin back, kept clear of the tall parts on the board and of the leads to the switch
    st = None
    for x in (PCB_X0 + 8.7, PCB_X0 + 38.8, PCB_X0 + 45.7):
        r = box(x - STIFF_W / 2, x + STIFF_W / 2, STEP_Y, L - WALL + 0.01, Z_LID - STIFF_H, Z_LID + 0.01)
        st = r if st is None else st.union(r)
    keep_out = [box(P1_X - 6, P1_X + 6, PCB_Y1 - 7, L, 0, 40), box(USB_X - 5, USB_X + 5, PCB_Y1 - 6, L, 0, 40),
                box(PCB_X0 - 2, PCB_X0 + 6, PCB_Y0 + 54, PCB_Y0 + 63.5, 0, 40),
                box(bx - BOOST_X / 2 - 0.3, bx + BOOST_X / 2 + 0.3, by - BOOST_Y / 2 - 0.3, by + BOOST_Y / 2 + 0.3, 0, Z_LID),
                box(sx - SPK_X / 2 - 0.2, sx + SPK_X / 2 + 0.2, sy - SPK_Y / 2 - 0.2, sy + SPK_Y / 2 + 0.2, 0, Z_LID)]
    for k in keep_out:
        st = st.cut(k)
    for x, y in HOLES:
        st = st.cut(cyl(x, y, 0, 40, 9.0))
    b = b.union(st)
    return add_ribs(b, Z_LID, Z_TOP + 1)


def make_lid():
    """Stiff battery lid: hooks at the antenna end go under the end wall, one M3 x 8 holds the other end.
    Surface texture is added to the mesh by texture.py."""
    d = lid_outline(GAP, Z_DOOR, Z_TOP)
    for tx in TAB_XS:
        d = d.union(box(tx - TAB_W / 2, tx + TAB_W / 2, WALL_END - LEDGE - TAB_L, WALL_END - LEDGE + GAP + 0.5, Z_DOOR, Z_DOOR + TAB_T))
    lx, ly = LID_SCREW
    return d.cut(cyl(lx, ly, Z_DOOR - 1, Z_TOP + 1, 3.4)).cut(cyl(lx, ly, Z_TOP - 1.3, Z_TOP + 1, 6.4))


# ---------------- stand-ins for the bought parts (checking and pictures only) ----------------
def make_board():
    x0, y0 = PCB_X0, PCB_Y0
    pcb = box(x0, x0 + PCB_W, y0, PCB_Y1, Z_PCB_F, Z_PCB_B)
    for x, y in HOLES:
        pcb = pcb.cut(cyl(x, y, Z_PCB_F - 1, Z_PCB_B + 1, 3.2))
    panel = box(x0, x0 + PCB_W, y0 + PANEL_Y0, y0 + PANEL_Y1, Z_GLASS, Z_PCB_F)
    zb = Z_PCB_B

    def part(xl0, xl1, yl0, yl1, h):
        return box(x0 + xl0, x0 + xl1, y0 + yl0, y0 + yl1, zb, zb + h)
    comps = part(18, 36, 0, 25.5, 3.2)                                        # ESP-WROOM-32
    comps = comps.union(part(35, 50, 40, 55, 1.9))                            # card slot
    comps = comps.union(part(45.5, 50, 9.5, 19.5, P3_H)).union(part(45.5, 50, 26, 36, P3_H))   # P3, CN1
    comps = comps.union(part(2.5, 5.5, 7.2, 14.5, 2.0))                       # RESET, BOOT
    comps = comps.union(part(0.2, 3.8, 56.5, 61.0, TALLEST_BACK + PLUG_EXTRA))
    comps = comps.union(part(8, 34, 58, 79, 1.75))
    comps = comps.union(box(P1_X - 4, P1_X + 4, PCB_Y1 - 5.5, PCB_Y1 - 0.5, zb, zb + 4.3))   # P1
    comps = comps.union(box(P1_X - 3, P1_X + 3, PCB_Y1 - 0.5, PCB_Y1 + 2.0, zb + 0.5, zb + 4.0))   # its plug, leads bent sharply
    comps = comps.union(box(USB_X - 3.75, USB_X + 3.75, PCB_Y1 - 4.5, PCB_Y1 + 1.0, zb, zb + 2.6))
    return pcb, panel, comps


def make_parts():
    holder = box(*HOLDER_BOX)
    cx, cz = (CH_X0 + CH_X1) / 2, CH_Z0 + 0.25 + STYLUS_D / 2
    stylus = cq.Workplane("XZ").circle(STYLUS_D / 2).extrude(-(STYLUS_L - HEAD_L)).translate((cx, L - STYLUS_L, cz))
    stylus = stylus.union(box(cx - STYLUS_D / 2, cx - STYLUS_D / 2 + HEAD_A, L - HEAD_L, L, cz - HEAD_B / 2, cz + HEAD_B / 2))
    xw = W - WALL
    sw = box(xw - 0.7, xw - 0.2, SW_Y - SW_BRACKET_L / 2, SW_Y + SW_BRACKET_L / 2, SW_Z0, SW_Z0 + SW_BODY_W)
    sw = sw.union(box(xw - 0.7 - SW_BODY_H, xw - 0.7, SW_Y - SW_BODY_L / 2, SW_Y + SW_BODY_L / 2, SW_Z0, SW_Z0 + SW_BODY_W))
    sw = sw.union(box(xw - 0.7 - SW_BODY_H - 2.8, xw - 0.7 - SW_BODY_H, SW_Y - 4.5, SW_Y + 4.5, SW_Z0 + 2.5, SW_Z0 + 3.5))
    sw = sw.union(box(xw - 0.2, xw + 3.8, SW_Y - 2.5, SW_Y + 0.5, SW_Z0 + 1.2, SW_Z0 + 4.8))
    sx, sy = SPK_C
    spk = box(sx - SPK_X / 2, sx + SPK_X / 2, sy - SPK_Y / 2, sy + SPK_Y / 2, Z_LID - SPK_T, Z_LID)
    bx, by = BOOST_C
    boost = box(bx - BOOST_X / 2, bx + BOOST_X / 2, by - BOOST_Y / 2, by + BOOST_Y / 2, Z_LID - BOOST_T, Z_LID)
    screws = None
    for x, y in HOLES:
        ln = SCREW_USB if y > L / 2 else SCREW_ANT
        n0 = HEAD_SEAT + ln - 2.0
        s = cyl(x, y, HEAD_SEAT, HEAD_SEAT + ln, 2.9).union(cyl(x, y, HEAD_SEAT - 1.65, HEAD_SEAT, 5.6))   # button head
        nut = hexagon(x, y, n0 + 0.05, n0 + 0.05 + NUT_H, NUT_AF).cut(cyl(x, y, 0, 40, 3.0))
        s = s.union(nut)
        screws = s if screws is None else screws.union(s)
    lx, ly = LID_SCREW
    tip = Z_TOP - 1.3 - SCREW_LID
    s = cyl(lx, ly, tip, Z_TOP - 1.3, 2.9).union(cyl(lx, ly, Z_TOP - 1.3, Z_TOP + 0.35, 5.6))
    s = s.union(hexagon(lx, ly, tip + 0.4, tip + 0.4 + NUT_H, NUT_AF).rotate((lx, ly, 0), (lx, ly, 1), 30).cut(cyl(lx, ly, 0, 40, 3.0)))
    screws = screws.union(s)
    return holder, stylus, sw, spk, boost, screws


if __name__ == "__main__":
    out = Path(__file__).parent / "out"
    out.mkdir(exist_ok=True)
    face, frame, back, door = make_face(), make_frame(), make_back(), make_lid()
    pcb, panel, comps = make_board()
    holder, stylus, sw, spk, boost, screws = make_parts()
    print(f"case {W:.1f} x {L:.1f}; thin part {T:.1f}, battery step {Z_TOP:.1f} over y 0..{STEP_Y:.1f}, slope to {STEP_Y + RAMP:.1f}")
    print(f"layers: face 0..{Z_PCB_B:.1f}, frame {Z_PCB_B:.1f}..{Z_LID:.1f}, back {Z_LID:.1f}..{T:.1f} (step to {Z_TOP:.1f}), lid {Z_DOOR:.1f}..{Z_TOP:.1f}")
    print(f"speaker at y {SPK_C[1]:.1f}, converter at y {BOOST_C[1]:.1f}, switch at y {SW_Y:.1f}, grip ribs at {RIB_YS}")
    print(f"holder shelf z {Z_FLOOR:.1f}..{Z_SHELF:.1f}, holder {Z_SHELF:.1f}..{Z_SHELF + HOLDER_Z:.1f}")
    for nm, ln, face_z in (("USB end", SCREW_USB, T), ("antenna end", SCREW_ANT, Z_SHELF)):
        n0 = HEAD_SEAT + ln - 2.0
        print(f"{nm} M3x{ln:.0f}: tip z={HEAD_SEAT + ln:.1f}, nut z {n0:.1f}..{n0 + NUT_H:.1f}, pocket opens at z={face_z:.1f}")

    def vol(a, b):
        try:
            return a.intersect(b).val().Volume()
        except Exception:
            return 0.0
    printed = {"face": face, "frame": frame, "back": back, "battery-lid": door}
    bought = {"board": pcb, "screen": panel, "components": comps, "holder": holder, "stylus": stylus,
              "switch": sw, "speaker": spk, "converter": boost, "screws-nuts": screws}
    allp = {**printed, **bought}
    names = list(allp)
    bad = 0
    skip = {("board", "screws-nuts")}
    for i, a in enumerate(names):
        for b2 in names[i + 1:]:
            if (a, b2) in skip:
                continue
            v = vol(allp[a], allp[b2])
            if v > 0.05:
                bad += 1
                bb = allp[a].intersect(allp[b2]).val().BoundingBox()
                print(f"  OVERLAP {a} / {b2}: {v:.2f} mm3 at x {bb.xmin:.1f}..{bb.xmax:.1f} y {bb.ymin:.1f}..{bb.ymax:.1f} z {bb.zmin:.1f}..{bb.zmax:.1f}")
    print("overlapping pairs:", bad)
    for n, s in printed.items():
        s = true_shape(s)
        v = s.val()
        bb = v.BoundingBox()
        print(f"  {n}: {v.Volume()/1000:.1f} cm3, solids={len(s.solids().vals())}, valid={v.isValid()}, box {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f}")
        cq.exporters.export(s, str(out / f"cyd-case-{n}.stl"), tolerance=0.02, angularTolerance=0.2)
        cq.exporters.export(s, str(out / f"cyd-case-{n}.step"))
    for n, s in bought.items():
        cq.exporters.export(true_shape(s), str(out / f"ref-{n}.stl"), tolerance=0.05, angularTolerance=0.3)
    asm = cq.Assembly()
    for n, s in allp.items():
        asm.add(true_shape(s), name=n)
    asm.save(str(out / "cyd-case-assembly.step"))
    print("exported to", out)
