' Launch build.bat in a clean CMD environment (no MSYS leakage)
Dim WshShell
Set WshShell = CreateObject("WScript.Shell")
WshShell.CurrentDirectory = "E:\Downloads\Compressed\esp32_esurfing"
WshShell.Run "cmd.exe /c build.bat", 1, True
Set WshShell = Nothing
