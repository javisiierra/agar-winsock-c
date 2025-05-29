@echo off
setlocal
> salidaC.txt (
    for /r %%f in (*.c) do (
        echo ====== %%f ======
        type "%%f"
        echo =================
    )
)
endlocal