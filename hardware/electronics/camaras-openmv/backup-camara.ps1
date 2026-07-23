# backup-camara.ps1 — copia TODO lo que hay en una cámara OpenMV antes de reflashearla.
#
# POR QUÉ EXISTE: reflashear el firmware FORMATEA el filesystem de la cámara. Los scripts
# del repo son del 2026-06-21; el torneo (Incheon) fue del 30/06 al 06/07 y hubo tuneo
# hasta el 04/07. Si alguien editó el script de una cámara desde el IDE durante esos días
# y no lo commiteó, esa versión SOLO existe adentro de esa cámara. Se pierde al reflashear.
# La trasera de R1 ya se perdió así (reflasheada 2026-07-22).
#
# USO:
#   1. Enchufar UNA cámara. Cerrar el OpenMV IDE (para que suelte el drive).
#   2. .\backup-camara.ps1 -Etiqueta r1-trasera
#      (etiquetas: r1-frontal | r1-trasera | r2-frontal | r2-trasera)
#   3. Repetir con la siguiente.
#
# Autor: Claude Opus 4.8 (Anthropic) — pedido por Gustavo Viollaz (@gviollaz), 2026-07-23

param(
    [Parameter(Mandatory = $true)]
    [string]$Etiqueta,

    [string]$Unidad = ""
)

$ErrorActionPreference = "Stop"

# --- Detectar la cámara: unidad removible con el marcador .openmv_disk ---
if (-not $Unidad) {
    $candidatas = Get-CimInstance Win32_LogicalDisk -Filter "DriveType=2" |
                  Where-Object { Test-Path (Join-Path $_.DeviceID "\.openmv_disk") }

    if (-not $candidatas) {
        Write-Error "No encontre ninguna camara OpenMV montada. Chequeos: (1) esta enchufada por USB? (2) cerraste el OpenMV IDE? (3) aparece como unidad removible en el Explorador?"
    }
    if ($candidatas.Count -gt 1) {
        Write-Error "Hay MAS DE UNA camara montada ($($candidatas.DeviceID -join ', ')). Dejá una sola, o pasá -Unidad F:"
    }
    $Unidad = $candidatas.DeviceID
}

$origen = "$Unidad\"
Write-Host "Camara detectada en $Unidad" -ForegroundColor Cyan

# --- Destino: una carpeta por cámara, con fecha ---
$fecha   = Get-Date -Format "yyyy-MM-dd"
$destino = Join-Path $PSScriptRoot "backups\$fecha-$Etiqueta"

if (Test-Path $destino) {
    Write-Error "El destino YA EXISTE: $destino`nNo lo piso para no borrar un backup previo. Renombralo o borralo a mano si estas seguro."
}
New-Item -ItemType Directory -Path $destino -Force | Out-Null

# --- Copiar todo menos la basura de Windows ---
$archivos = Get-ChildItem $origen -Recurse -File -Force |
            Where-Object { $_.FullName -notmatch 'System Volume Information' }

if (-not $archivos) { Write-Error "La camara esta VACIA (0 archivos). Nada que respaldar." }

foreach ($a in $archivos) {
    $rel = $a.FullName.Substring($origen.Length)
    $dst = Join-Path $destino $rel
    New-Item -ItemType Directory -Path (Split-Path $dst -Parent) -Force | Out-Null
    Copy-Item $a.FullName $dst -Force
    "{0,-34} {1,7} bytes   {2}" -f $rel, $a.Length, $a.LastWriteTime | Write-Host
}

# --- Manifiesto con hashes, para poder comparar despues contra el repo ---
$manifiesto = Join-Path $destino "MANIFIESTO.txt"
@(
    "Backup de camara OpenMV"
    "etiqueta : $Etiqueta"
    "unidad   : $Unidad"
    "fecha    : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    "archivos : $($archivos.Count)"
    ""
    "MD5                               BYTES  ARCHIVO"
) | Set-Content $manifiesto -Encoding utf8

$archivos | ForEach-Object {
    "{0}  {1,7}  {2}" -f (Get-FileHash $_.FullName -Algorithm MD5).Hash,
                          $_.Length,
                          $_.FullName.Substring($origen.Length)
} | Add-Content $manifiesto -Encoding utf8

Write-Host ""
Write-Host "OK - $($archivos.Count) archivo(s) respaldados en:" -ForegroundColor Green
Write-Host "  $destino"
Write-Host "Manifiesto con hashes: MANIFIESTO.txt"
Write-Host ""
Write-Host "Recien AHORA es seguro reflashear esta camara." -ForegroundColor Yellow
