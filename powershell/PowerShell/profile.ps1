# Shows navigable menu of all options when hitting Tab
Set-PSReadlineKeyHandler -Key Tab -Function MenuComplete

# Autocompletion for arrow keys
Set-PSReadlineKeyHandler -Key UpArrow -Function HistorySearchBackward
Set-PSReadlineKeyHandler -Key DownArrow -Function HistorySearchForward

New-Alias sqlitebrowser "C:\Users\ThaiPham\Downloads\DB Browser for SQLite\DB Browser for SQLite.exe"
New-Alias psexec "C:\Users\ThaiPham\Downloads\psTools\PSTools\PsExec.exe"
New-Alias oesis-tool "E:\1_OADT\OESISAdvancedDebuggingTools_1.0.420_x64_internal\oesis-advanced-debugging-tools.exe"
New-Alias qmlscene "E:\Qt\5.14.2\msvc2017\bin\qml.exe"