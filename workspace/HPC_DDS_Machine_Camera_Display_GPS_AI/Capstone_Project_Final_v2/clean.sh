#!/bin/bash

# Shell script to clean LaTeX auxiliary files
# Run this script from the repository root directory

# Colors
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

echo -e "${CYAN}Cleaning LaTeX auxiliary files...${NC}"

# Define file extensions to remove
extensions=(
    "*.toc" "*.fls" "*.log" "*.out" "*.aux" 
    "*.fdb_latexmk" "*.synctex.gz" "*.bbl" "*.blg"
    "*.nav" "*.snm" "*.vrb" "*.lof" "*.lot"
    "*.bcf" "*.run.xml" "*.gz" "*.acn" "*.acr"
    "*.alg" "*.glg" "*.glo" "*.gls" "*.ist"
    "*.idx" "*.ilg" "*.ind"
)

# Counter for deleted files
count=0

# Remove files matching each extension
for ext in "${extensions[@]}"; do
    # Use find to locate files recursively
    # -name is case sensitive (use -iname for case insensitive if needed, but Windows/PS behavior varies. PS is usually case-insensitive. find -name matches PS typical behavior if we assume lowercase extensions output or precise matching.)
    # Actually, PS match is case-insensitive on Windows. `find` is case sensitive on Linux.
    # To be safe and closer to Windows behavior, we can use -iname or just -name if we trust the extensions.
    # Given the standard latex extensions are lowercase, -name is likely fine, but -iname is safer.
    
    while IFS= read -r file; do
        echo -e "${YELLOW}Deleting: $file${NC}"
        rm -f "$file"
        ((count++))
    done < <(find . -type f -name "$ext")
done

echo -e "\n${GREEN}Cleaning complete! Deleted $count file(s).${NC}"
