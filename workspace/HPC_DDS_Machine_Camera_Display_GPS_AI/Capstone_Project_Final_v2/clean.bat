@echo off
setlocal EnableDelayedExpansion

REM Windows Batch script to clean LaTeX auxiliary files
REM Run this script from the repository root directory

echo Cleaning LaTeX auxiliary files...

set "count=0"
set "extensions=*.toc *.fls *.log *.out *.aux *.fdb_latexmk *.synctex.gz *.bbl *.blg *.nav *.snm *.vrb *.lof *.lot *.bcf *.run.xml *.gz *.acn *.acr *.alg *.glg *.glo *.gls *.ist *.idx *.ilg *.ind"

for %%e in (%extensions%) do (
    for /r %%f in (%%e) do (
        if exist "%%f" (
            echo Deleting: %%f
            del /f /q "%%f"
            set /a count+=1
        )
    )
)

echo.
echo Cleaning complete! Deleted !count! file(s).
endlocal
