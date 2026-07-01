---
title: "Machete de comandos — VSCode + Git + PlatformIO"
tipo: guia-practica
audiencia: equipo (Elías, Virginia, alumnos)
actualizado: 2026-07-01
---

# 🛠️ Machete de comandos — VSCode + Git + PlatformIO

Guía práctica para trabajar con el repo. Todo desde la **terminal integrada de VSCode**
(menú `Terminal → New Terminal`, o `` Ctrl+ñ ``). Editá este archivo cuando aprendas algo nuevo.

---

## 0. El alias de `pio` (una vez por terminal nueva)

```powershell
Set-Alias pio "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
```
> El `pio.exe` normal quedó roto (apuntaba a un Python que ya no está). Este alias usa el
> PlatformIO propio (`penv`), que sí anda. Hay que ponerlo **cada vez que abrís una PowerShell nueva**.

---

## 1. Git — el día a día

```powershell
git status                    # ¿qué cambié? ¿en qué rama estoy?
git branch --show-current     # solo el nombre de la rama actual
git log --oneline -5          # últimos 5 commits
git pull                      # bajar lo último de GitHub (de tu rama)
```

## 2. Cambiar de rama

```powershell
git checkout main                   # ir a main
git checkout ultrasonido            # ir a la rama ultrasonido
git checkout apuntar-proporcional   # ir a la del apuntado proporcional
git fetch origin                    # enterarte de ramas/commits nuevos (sin bajarlos aún)
```
> En VSCode también: **abajo a la izquierda** dice la rama → clic → elegís otra.

## 3. Guardar tu trabajo (commit + push)

```powershell
git add -A                                 # marca TODOS tus cambios
git commit -m "lo que hiciste en pocas palabras"
git push                                   # sube a GitHub (a tu rama actual)
```

## 4. Cuando `git pull` se queja de "local changes"

```powershell
git diff                 # ver QUÉ cambiaste (apretá q para salir)
```
Después, una de dos:
```powershell
# A) NO te importan tus cambios → tirarlos y traer lo nuevo:
git checkout -- .
git pull

# B) SÍ querés conservarlos:
git stash                # los guarda aparte
git pull                 # trae lo nuevo
git stash pop            # los vuelve a poner encima (si hay conflicto, resolvés en VSCode)
```

---

## 5. PlatformIO — compilar / flashear / ver

```powershell
cd "software\teensy\Soccer 2026"                 # SIEMPRE parado acá (donde está platformio.ini)

pio run -e central_robot1_mix_ultra -t upload    # compilar + subir a la CENTRAL (Teensy)
pio device monitor -b 115200                     # ver lo que imprime (Ctrl+C para salir)

Remove-Item -Recurse -Force .pio                 # borrar caché si sospechás que flashea VIEJO
```

### Envs que usás
| Env | Qué es |
|---|---|
| `central_robot1_mix_ultra` | centralmix (delantero) + anti-choque por ultrasonido |
| `central_robot1_mix_bno` | centralmix con heading del BNO del TOP (compila el mismo código) |
| `central_robot1_test` | banco de pruebas con estados (mix_seguir) — rama `lightweight` |

---

## 6. Buscar en VSCode (sin comandos)

- `Ctrl+P` → escribís `mix_seguir.cpp` y te lleva al archivo.
- `Ctrl+Shift+F` → busca un texto en TODO el repo (ej. `MIX_APUNTAR_KP`).
- `Ctrl+F` → busca dentro del archivo abierto.
- Panel **Source Control** (ícono de ramitas, izquierda) → ver cambios, commitear, pushear con clics.

---

## 7. Reglas de oro (los errores que ya pasaron)

1. **`pio run` solo funciona parado en `...\Soccer 2026`** (donde está `platformio.ini`).
   Si dice `NotPlatformIOProjectError` → hacé `cd` a esa carpeta.
2. **No podés flashear con el monitor abierto** → `Ctrl+C` primero. Si dice "puerto ocupado/busy", es eso.
3. **Si flashea código VIEJO** → `Remove-Item -Recurse -Force .pio` y volvé a subir. En la salida
   tiene que decir `Compiling ...test_banco_central.cpp` (si dice **solo** `Linking`, no compiló tus cambios).
4. **Trabajá siempre en UNA sola carpeta** del repo. Borrá los clones duplicados/anidados
   (el `open-soccer-robocup-team2026/` adentro de otro es basura).
5. **`git status` antes de pullear** → así sabés si tenés cambios sin guardar y no te agarra el conflicto.

---

## 8. El ciclo típico de trabajo, todo junto

```powershell
git status                                 # ¿limpio? ¿qué rama?
git pull                                   # traer lo último
# ...editás código en VSCode...
cd "software\teensy\Soccer 2026"
Set-Alias pio "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
pio run -e central_robot1_mix_ultra -t upload   # probar en la Teensy
# Ctrl+C si tenías el monitor abierto
git add -A && git commit -m "lo que hice" && git push   # guardar en GitHub
```

---

## 9. Clonar el repo en otra compu (o de cero)

```powershell
cd C:\Users\Pc                 # carpeta VACÍA (nunca dentro de otro repo)
git clone https://github.com/IITA-Proyectos/open-soccer-robocup-team2026.git
cd open-soccer-robocup-team2026
```
> La primera vez te pide loguearte a GitHub (se abre el navegador). Si pide contraseña en la
> terminal, NO es la de tu cuenta: es un **Personal Access Token** (se crea en GitHub → Settings →
> Developer settings → Personal access tokens).

---

_Editá este machete libremente: agregá comandos, envs o trucos que vayas descubriendo._
