@echo off
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"

set "TOOL=%CD%\aytpcbuild.exe"
if not exist "%TOOL%" (
    echo ERROR: aytpcbuild.exe introuvable dans "%CD%".
    exit /b 1
)

:main
cls
echo ============================================================
echo  AYT PC BUILD - menu interactif Windows
echo ============================================================
echo.
echo Ce script lance aytpcbuild.exe uniquement.
echo Il n assemble rien et ne cree pas de *-demo.bin.
echo.

call :select_ayt || goto end
call :select_case || goto end
call :ask_common || goto end
call :ask_output || goto end
call :run_tool || goto end

echo.
set "AGAIN="
set /p "AGAIN=Faire une autre generation ? [o/N] "
if /i "!AGAIN!"=="O" goto main
if /i "!AGAIN!"=="Y" goto main

:end
echo.
echo Fin.
exit /b 0

:select_ayt
echo Fichiers AYT disponibles dans dist\ayt:
echo.
set /a AYT_COUNT=0
for %%F in ("%CD%\ayt\*.ayt") do (
    if exist "%%~fF" (
        set /a AYT_COUNT+=1
        set "AYT!AYT_COUNT!=%%~fF"
        echo   !AYT_COUNT!. %%~nxF
    )
)
echo   C. Chemin personnalise
echo.

if "!AYT_COUNT!"=="0" (
    echo Aucun fichier .ayt trouve dans "%CD%\ayt".
)

:select_ayt_prompt
set "AYT_CHOICE="
set /p "AYT_CHOICE=Choix AYT [1] : "
if not defined AYT_CHOICE set "AYT_CHOICE=1"

if /i "!AYT_CHOICE!"=="C" (
    set "INPUT="
    set /p "INPUT=Chemin complet ou relatif du fichier .ayt : "
    set "INPUT=!INPUT:"=!"
    if not exist "!INPUT!" (
        echo Fichier introuvable.
        goto select_ayt_prompt
    )
) else (
    set "INPUT=!AYT%AYT_CHOICE%!"
    if not defined INPUT (
        echo Choix invalide.
        goto select_ayt_prompt
    )
)

for %%I in ("!INPUT!") do (
    set "MUSIC=%%~nI"
    set "INPUT=%%~fI"
)

echo.
echo AYT choisi : !INPUT!
echo Nom        : !MUSIC!
echo.
exit /b 0

:select_case
echo Cas de generation:
echo.
echo   1. CALL   - deux fichiers player + runtime AYT
echo   2. CALL   - bundle compact
echo   3. SP/JP  - deux fichiers player + runtime AYT
echo   4. SP/JP  - bundle compact
echo.

:select_case_prompt
set "CASE_CHOICE="
set /p "CASE_CHOICE=Choix [1-4] : "
if not defined CASE_CHOICE set "CASE_CHOICE=1"

if "!CASE_CHOICE!"=="1" (
    set "MODE=call"
    set "KIND=two"
    set "CASE_DIR=call_two"
) else if "!CASE_CHOICE!"=="2" (
    set "MODE=call"
    set "KIND=bundle"
    set "CASE_DIR=call_bundle"
) else if "!CASE_CHOICE!"=="3" (
    set "MODE=jp"
    set "KIND=two"
    set "CASE_DIR=sp_two"
) else if "!CASE_CHOICE!"=="4" (
    set "MODE=jp"
    set "KIND=bundle"
    set "CASE_DIR=sp_bundle"
) else (
    echo Choix invalide.
    goto select_case_prompt
)

echo.
echo Cas choisi : !CASE_DIR!
echo.
exit /b 0

:ask_common
set "PLAYER_ADDR="
set /p "PLAYER_ADDR=Adresse CPC du player [0x0040] : "
if not defined PLAYER_ADDR set "PLAYER_ADDR=0x0040"

set "AYT_ADDR="
if "!KIND!"=="two" (
    set /p "AYT_ADDR=Adresse CPC du runtime AYT [0x1000] : "
) else (
    echo En bundle, le runtime AYT sera recalcule juste apres le player.
    set /p "AYT_ADDR=Adresse AYT initiale pour l'outil [0x1000] : "
)
if not defined AYT_ADDR set "AYT_ADDR=0x1000"

set "LOOPS="
set /p "LOOPS=Nombre de lectures / loops [2] : "
if not defined LOOPS set "LOOPS=2"

set "PROGRAM_ADDR="
set "RETURN_ADDR="
if "!MODE!"=="jp" (
    call :ask_jp_options || exit /b 1
)

echo.
exit /b 0

:ask_jp_options
echo.
echo Mode SP/JP: --return-addr est obligatoire.
echo Pour les ASM generes par l'outil: return_addr = program_addr + #24.
echo.
set /p "PROGRAM_ADDR=--program-addr optionnel, vide pour ne pas le passer : "

:ask_return_addr
set /p "RETURN_ADDR=--return-addr obligatoire : "
if not defined RETURN_ADDR (
    echo return_addr est obligatoire en mode SP/JP.
    goto ask_return_addr
)
exit /b 0

:ask_output
set "OUT_ROOT="
set /p "OUT_ROOT=Dossier racine de sortie [results\manual] : "
if not defined OUT_ROOT set "OUT_ROOT=results\manual"

set "OUT_DIR=!OUT_ROOT!\!CASE_DIR!\!MUSIC!"
set /p "OUT_DIR=Dossier final [!OUT_DIR!] : "
if not defined OUT_DIR set "OUT_DIR=!OUT_ROOT!\!CASE_DIR!\!MUSIC!"

set "GEN_ASM="
set /p "GEN_ASM=Generer le mini ASM de test ? [O/n] : "
if not defined GEN_ASM set "GEN_ASM=O"

set "GEN_REPORT="
set /p "GEN_REPORT=Generer le rapport TXT ? [O/n] : "
if not defined GEN_REPORT set "GEN_REPORT=O"

if not exist "!OUT_DIR!\" mkdir "!OUT_DIR!"
if not exist "!OUT_DIR!\" (
    echo Impossible de creer le dossier "!OUT_DIR!".
    exit /b 1
)

echo.
echo Dossier de sortie : !OUT_DIR!
echo.
exit /b 0

:run_tool
set "JP_ARGS="
if "!MODE!"=="jp" (
    set "JP_ARGS=--return-addr !RETURN_ADDR!"
    if defined PROGRAM_ADDR set "JP_ARGS=!JP_ARGS! --program-addr !PROGRAM_ADDR!"
)

set "ASM_ARGS="
if /i not "!GEN_ASM!"=="N" (
    if "!KIND!"=="two" (
        set "ASM_ARGS=--out-asm-two ^"!OUT_DIR!\!MUSIC!.two.asm^""
    ) else (
        set "ASM_ARGS=--out-asm-bundle ^"!OUT_DIR!\!MUSIC!.bundle.asm^""
    )
)

set "REPORT_ARGS="
if /i not "!GEN_REPORT!"=="N" (
    set "REPORT_ARGS=--report ^"!OUT_DIR!\!MUSIC!.report.txt^""
)

echo Commande lancee:
echo.

if "!KIND!"=="two" (
    echo "%TOOL%" --input "!INPUT!" --player-addr !PLAYER_ADDR! --ayt-addr !AYT_ADDR! --loops !LOOPS! --mode !MODE! !JP_ARGS! --out-player "!OUT_DIR!\!MUSIC!.player.bin" --out-ayt-runtime "!OUT_DIR!\!MUSIC!.runtime.ayt" !ASM_ARGS! !REPORT_ARGS!
    echo.
    "%TOOL%" --input "!INPUT!" --player-addr !PLAYER_ADDR! --ayt-addr !AYT_ADDR! --loops !LOOPS! --mode !MODE! !JP_ARGS! --out-player "!OUT_DIR!\!MUSIC!.player.bin" --out-ayt-runtime "!OUT_DIR!\!MUSIC!.runtime.ayt" !ASM_ARGS! !REPORT_ARGS!
) else (
    echo "%TOOL%" --input "!INPUT!" --player-addr !PLAYER_ADDR! --ayt-addr !AYT_ADDR! --loops !LOOPS! --mode !MODE! !JP_ARGS! --out-bundle "!OUT_DIR!\!MUSIC!.bundle.bin" !ASM_ARGS! !REPORT_ARGS!
    echo.
    "%TOOL%" --input "!INPUT!" --player-addr !PLAYER_ADDR! --ayt-addr !AYT_ADDR! --loops !LOOPS! --mode !MODE! !JP_ARGS! --out-bundle "!OUT_DIR!\!MUSIC!.bundle.bin" !ASM_ARGS! !REPORT_ARGS!
)

if errorlevel 1 (
    echo.
    echo ERREUR: aytpcbuild.exe a echoue.
    exit /b 1
)

echo.
echo Generation terminee.
exit /b 0
