$files = Get-ChildItem include, tests -Recurse -Include *.hpp, *.cpp | ForEach-Object FullName
& "C:\Program Files\LLVM\bin\clang-format.exe" -i --style=file $files
foreach ($f in $files) {
    (Get-Content $f -Raw) -replace '\[\[= ', '[[=' -replace ', = (?=[A-Z])', ', =' | Set-Content $f -NoNewline -Encoding utf8
}