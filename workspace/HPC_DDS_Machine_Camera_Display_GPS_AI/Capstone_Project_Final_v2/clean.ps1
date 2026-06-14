# PowerShell script to clean LaTeX auxiliary files
# Run this script from the repository root directory

Write-Host "Cleaning LaTeX auxiliary files..." -ForegroundColor Cyan

# Define file extensions to remove
$extensions = @(
    "*.toc", "*.fls", "*.log", "*.out", "*.aux", 
    "*.fdb_latexmk", "*.synctex.gz", "*.bbl", "*.blg",
    "*.nav", "*.snm", "*.vrb", "*.lof", "*.lot",
    "*.bcf", "*.run.xml", "*.gz", "*.acn", "*.acr",
    "*.alg", "*.glg", "*.glo", "*.gls", "*.ist",
    "*.idx", "*.ilg", "*.ind"
)

# Counter for deleted files
$totalDeleted = 0

# Remove files matching each extension
foreach ($ext in $extensions) {
    $files = Get-ChildItem -Path . -Filter $ext -Recurse -File -ErrorAction SilentlyContinue
    
    foreach ($file in $files) {
        Write-Host "Deleting: $($file.FullName)" -ForegroundColor Yellow
        Remove-Item -Path $file.FullName -Force
        $totalDeleted++
    }
}

Write-Host "`nCleaning complete! Deleted $totalDeleted file(s)." -ForegroundColor Green
