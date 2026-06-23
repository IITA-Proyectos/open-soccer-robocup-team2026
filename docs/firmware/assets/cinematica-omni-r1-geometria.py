import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle, FancyArrow, Rectangle
import math

def rot(x, y, deg):
    a = math.radians(deg)
    return (x*math.cos(a)-y*math.sin(a), x*math.sin(a)+y*math.cos(a))

# --- datos de la geometria (anclados a banco) ---
Rc = 1.0                      # radio del chasis (normalizado)
# Posiciones FISICAS de cada rueda (angulo desde +X, CCW)
pos = {"M1":150, "M2":30, "M3":270}
# Direccion de EMPUJE con +PWM (= roll dir = posicion - 90, tangente CW)
push = {"M1":60, "M2":-60, "M3":180}
names = {"M1":"delantera IZQUIERDA (U5)",
         "M2":"delantera DERECHA (U17)",
         "M3":"TRASERA (U7)"}
col = {"M1":"#1f6feb", "M2":"#d1242f", "M3":"#1a7f37"}

fig, (axA, axB) = plt.subplots(1, 2, figsize=(15, 7.6))

def draw_chassis(ax, title):
    ax.add_patch(Circle((0,0), Rc, fill=False, lw=2.5, ec="#444"))
    # ejes del robot
    ax.annotate("", xy=(1.55,0), xytext=(0,0),
                arrowprops=dict(arrowstyle="->", lw=2, color="#888"))
    ax.annotate("", xy=(0,1.55), xytext=(0,0),
                arrowprops=dict(arrowstyle="->", lw=2, color="#888"))
    ax.text(1.6,0.02,"+X (derecha)", color="#666", fontsize=12, va="bottom")
    ax.text(0.04,1.6,"+Y (FRENTE)", color="#666", fontsize=12, ha="left")
    ax.text(0,1.78,"FRENTE DEL ROBOT", ha="center", fontsize=12, weight="bold", color="#333")
    ax.set_xlim(-2.1,2.3); ax.set_ylim(-2.1,2.1); ax.set_aspect("equal")
    ax.axis("off"); ax.set_title(title, fontsize=14, weight="bold")

# ---------- Panel A: geometria + empuje +PWM ----------
draw_chassis(axA, "A) Geometria fisica + empuje con +PWM")
for k in ["M1","M2","M3"]:
    px, py = rot(Rc, 0, pos[k])
    # dibujar la rueda como rectangulo orientado segun roll (perpendicular al eje)
    rdir = push[k]
    w, h = 0.42, 0.16
    rect = Rectangle((-w/2,-h/2), w, h, angle=rdir, rotation_point="center",
                     facecolor=col[k], edgecolor="black", lw=1.5, alpha=0.9)
    t = matplotlib.transforms.Affine2D().translate(px,py) + axA.transData
    rect.set_transform(t)
    axA.add_patch(rect)
    # flecha de empuje +PWM (desde el centro de la rueda en direccion roll)
    dx, dy = rot(0.6, 0, rdir)
    axA.annotate("", xy=(px+dx, py+dy), xytext=(px,py),
                 arrowprops=dict(arrowstyle="-|>", lw=2.8, color=col[k]))
    # etiqueta
    lx, ly = rot(1.42, 0, pos[k])
    axA.text(lx, ly, k, color=col[k], fontsize=17, weight="bold",
             ha="center", va="center",
             bbox=dict(boxstyle="circle,pad=0.25", fc="white", ec=col[k], lw=1.8))
    # nombre fisico afuera
    nx, ny = rot(2.02, 0, pos[k])
    axA.text(nx, ny, names[k], color=col[k], fontsize=9.5, ha="center", va="center")
axA.text(0,-2.02,"flecha = hacia donde EMPUJA esa rueda con +PWM",
         ha="center", fontsize=10, style="italic", color="#555")

# ---------- Panel B: avanzar() = forward ----------
draw_chassis(axB, "B) avanzar()  =  +M1, -M2, 0·M3  ->  FRENTE")
combos = {"M1": +1, "M2": -1, "M3": 0}   # signos de avanzar()
res = np.array([0.0,0.0])
for k in ["M1","M2","M3"]:
    s = combos[k]
    px, py = rot(Rc, 0, pos[k])
    lx, ly = rot(1.42, 0, pos[k])
    axB.text(lx, ly, k, color=col[k], fontsize=15, weight="bold",
             ha="center", va="center",
             bbox=dict(boxstyle="circle,pad=0.22", fc="white", ec=col[k], lw=1.6))
    if s == 0:
        axB.text(px, py, "0", color="#888", fontsize=13, ha="center", va="center",
                 bbox=dict(boxstyle="round,pad=0.2", fc="#eee", ec="#aaa"))
        continue
    rdir = push[k] if s>0 else (push[k]+180)
    dx, dy = rot(0.62, 0, rdir)
    axB.annotate("", xy=(px+dx, py+dy), xytext=(px,py),
                 arrowprops=dict(arrowstyle="-|>", lw=2.8, color=col[k]))
    sign = "+" if s>0 else "−"
    axB.text(px+dx*1.5, py+dy*1.5, sign+k, color=col[k],
             fontsize=13, ha="center", weight="bold")
    fx, fy = rot(1.0, 0, rdir)
    res += np.array([fx, fy])
# resultante (a la derecha, lejos del texto FRENTE)
axB.annotate("", xy=(res[0]*0.55, res[1]*0.55), xytext=(0,0),
             arrowprops=dict(arrowstyle="-|>", lw=4.5, color="#0a7"))
axB.text(1.0, 0.62, "RESULTANTE\n= FRENTE (+Y)",
         color="#076", fontsize=12.5, weight="bold", va="center", ha="left")

# matriz al pie
mtx = (r"$w_{M1} = +0.5\,v_x \; + 0.866\,v_y \; + \omega$"  "\n"
       r"$w_{M2} = +0.5\,v_x \; - 0.866\,v_y \; + \omega$"  "\n"
       r"$w_{M3} = -1.0\,v_x \;\;\;\;\;\;\;\;\;\;\;\;\;\;\; + \omega$")
fig.text(0.5, -0.02, "Cinematica inversa (PWM):   "+mtx.replace("\n","      "),
         ha="center", fontsize=12,
         bbox=dict(boxstyle="round,pad=0.5", fc="#fffbe6", ec="#e6c200"))
fig.suptitle("Geometria omni-3 del robot R1  (vx=+derecha, vy=+frente)  —  anclada a banco: avanzar / girar / retroceder",
             fontsize=13, weight="bold", y=1.02)
plt.tight_layout()
out="/tmp/claude-0/-home-user-open-soccer-robocup-team2026/4a2c922a-7d17-58ed-8650-0e478a96d6c8/scratchpad/geometria_robot.png"
plt.savefig(out, dpi=130, bbox_inches="tight", facecolor="white")
print("OK", out)
