@echo off
rem Runs every sample program and compares the output with tests\expected\.
rem After an intended change, check the new output by hand, then copy it over the expected file.
setlocal
set fail=0
for %%f in (examples\demo examples\errors examples\semantic examples\main tests\optimizer tests\return_outside tests\main_errors tests\scope) do (
    .\minicc.exe %%f.mc > tests\out.txt
    fc tests\out.txt tests\expected\%%~nf.txt > nul && echo PASS %%~nf || (echo FAIL %%~nf & set fail=1)
)
del tests\out.txt
if %fail%==1 exit /b 1
echo All tests passed.