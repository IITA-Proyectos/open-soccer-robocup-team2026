# Configuración de protección del repositorio

**Objetivo**: Que los 2 profes y los 2 alumnos trabajen de forma segura, con historial completo de cambios y sin posibilidad de perder trabajo.

## Equipo

| Usuario | Nombre | Rol | Permiso GitHub |
|---------|--------|-----|----------------|
| `@gviollaz` | Gustavo Viollaz | Profe/Admin | **Admin** |
| `@enzzo19` | Enzo Juárez | Profe | **Maintain** |
| `@mariaviollaz` | María Virginia Viollaz | Alumna | **Write** |
| `@3liasCo` | Elías Cordero | Alumno | **Write** |

## Principios

1. **Nadie hace push directo a `main`** — todo va por Pull Request
2. **Un profe debe aprobar** cada PR antes de mergear
3. **No se puede borrar historial** — force push prohibido
4. **Cada cambio queda registrado** con autor, fecha y descripción

## Paso 1: Agregar colaboradores

Ir a: https://github.com/IITA-Proyectos/open-soccer-robocup-team2026/settings/access

Click "Add people" para cada uno:

| Usuario | Permiso |
|---------|---------|
| `enzzo19` | **Maintain** |
| `mariaviollaz` | **Write** |
| `3liasCo` | **Write** |

(`gviollaz` ya es owner de la organización)

**Permisos explicados:**
- **Admin**: Control total (settings, protección de ramas, borrar repo)
- **Maintain**: Puede mergear PRs, manejar issues, pero no cambiar settings críticos
- **Write**: Puede crear ramas, hacer push a ramas (NO a main), crear PRs

## Paso 2: Proteger la rama `main`

Ir a: https://github.com/IITA-Proyectos/open-soccer-robocup-team2026/settings/branches

### Click en "Add branch protection rule" (o "Add classic branch protection rule")

Configurar:

| Setting | Valor | Por qué |
|---------|-------|---------|
| Branch name pattern | `main` | Protege la rama principal |
| ✅ Require a pull request before merging | ON | Nadie hace push directo |
| ✅ Require approvals | **1** | Al menos 1 profe aprueba |
| ✅ Require review from Code Owners | ON | Usa el archivo CODEOWNERS |
| ✅ Dismiss stale pull request approvals | ON | Si cambia el código, hay que re-aprobar |
| ❌ Require status checks | OFF (por ahora) | No tenemos CI todavía |
| ✅ Require conversation resolution | ON | Todos los comentarios deben resolverse |
| ✅ Do not allow bypassing the above settings | ON | Ni los admins pueden saltarse las reglas |
| ❌ Allow force pushes | **OFF** | **CRÍTICO**: nadie puede reescribir historial |
| ❌ Allow deletions | **OFF** | **CRÍTICO**: nadie puede borrar la rama main |

### Click "Create" o "Save changes"

## Paso 3: Configurar la organización (opcional pero recomendado)

Ir a: https://github.com/organizations/IITA-Proyectos/settings

- **Member privileges** → Base permissions: `Read` (los miembros solo leen por defecto)
- **Repository creation** → Deshabilitar para members (solo admins crean repos)

## Flujo de trabajo diario

### Para los alumnos (Virginia @mariaviollaz y Elías @3liasCo):

```bash
# 1. Asegurarse de tener main actualizado
git checkout main
git pull origin main

# 2. Crear una rama para su trabajo
git checkout -b feature/mi-cambio

# 3. Hacer sus cambios y commits
git add .
git commit -m "feat: descripción del cambio"

# 4. Subir la rama
git push origin feature/mi-cambio

# 5. Crear Pull Request en GitHub
#    (GitHub les va a mostrar un botón "Compare & pull request")

# 6. Esperar aprobación de un profe (Gustavo o Enzo)

# 7. Después del merge, actualizar su main local
git checkout main
git pull origin main
```

### Para los profes (Gustavo @gviollaz y Enzo @enzzo19):

```bash
# Mismo flujo que los alumnos para sus propios cambios
# Adicionalmente: revisar y aprobar PRs de los alumnos en GitHub
```

### Review de un PR:

1. Ir al PR en GitHub
2. Click en "Files changed" para ver los cambios
3. Dejar comentarios si hay algo para corregir
4. Click en "Review changes" → "Approve" (si está bien) o "Request changes" (si hay que corregir)
5. Una vez aprobado, el autor o el profe puede hacer "Merge pull request"

## Qué NO se puede hacer (por diseño)

- ❌ Hacer push directo a `main`
- ❌ Borrar commits o reescribir historial (`git push --force`)
- ❌ Borrar la rama `main`
- ❌ Mergear sin aprobación de un profe
- ❌ Mergear con comentarios sin resolver

## Qué SÍ se puede hacer

- ✅ Crear ramas libremente
- ✅ Hacer push a ramas personales
- ✅ Crear Pull Requests
- ✅ Comentar y discutir cambios
- ✅ Ver todo el historial de cambios
- ✅ Revertir cambios (creando un nuevo commit, no borrando)

## Recuperar trabajo perdido

Gracias a la protección, es prácticamente imposible perder trabajo. Pero si algo sale mal:

1. **Commit borrado por accidente en una rama**: `git reflog` muestra todos los commits recientes
2. **Rama borrada**: GitHub guarda las ramas de PRs mergeados
3. **Archivo borrado**: ver historial del archivo en GitHub → restaurar desde cualquier commit anterior
4. **Quiero volver a una versión anterior**: `git revert <commit>` crea un nuevo commit que deshace los cambios

---

*Documento creado por Claude (Anthropic) bajo supervisión de Gustavo Viollaz (@gviollaz)*
