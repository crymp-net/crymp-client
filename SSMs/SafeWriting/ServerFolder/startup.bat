@echo off
set GameFolder=..\
set ServerFolder=%CD%

title SafeWriting Server

goto BIN64

:BIN64
cd %GameFolder%
echo Server is starting
CryMP-Server64.exe -root %ServerFolder% -ssm SafeWriting +exec "server.cfg"
echo Server was closed
goto END

:END
cd %ServerFolder%