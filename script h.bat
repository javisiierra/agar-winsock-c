@echo off
setlocal
> salidaH.txt (
    for /r %%f in (*.h) do (
        echo ====== %%f ======
        type "%%f"
        echo =================
    )
)
endlocal