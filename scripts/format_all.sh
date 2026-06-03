#!/bin/bash
# Run clang-format on all C/C++ files in the repo, including untracked files, excluding submodules

# Get tracked and untracked files, filter for C/C++ files
git ls-files --cached --others --exclude-standard | grep -E '\.(c|cpp|cc|h|hpp|hh)$' | while read -r file; do
  clang-format -i "$file"
done