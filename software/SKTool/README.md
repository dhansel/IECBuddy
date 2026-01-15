## SKTool

SKTool allows you to manage the files stored on the IECBuddy directly from your PC instead of going through the RAD menu system.
SKTool is a command-line tool for Windows. You can either downaload a compiled executable [here](https://github.com/dhansel/IECBuddy/raw/refs/heads/main/software/SKTool/SKTool.exe)
or build it from scratch using Visual Studio 2022 or later or g++ under MSys.

```
Usage: SKTool [-p COMx] dir|ls|rm|del|delete|cp|copy|boot

       SKTool ls|dir [patterns]            Display directory of files on IECBuddy
       SKTool rm|del patterns              Delete files matching pattern on IECBuddy
       SKTool cp|copy [-f] patterns SK:    Copy local files matching pattern to IECBuddy
       SKTool cp|copy [-f] patterns dir    Copy IECBuddy files matching pattern to local directory dir
       SKTool cp|copy [-f] name1 SK:name2  Copy local file name1 to IECBuddy as name2
       SKTool cp|copy [-f] name1 name2     Copy IECBuddy file file name1 to local file name2
       SKTool boot                         Switch IECBuddy to boot mode for firmware upload

       If -p COMx is given then use port COMx, otherwise auto-detect
       If -f is given for cp/copy then overwrite destination file if it exists
       File name patterns can include '*' and '?' with their usual meaning.
```
