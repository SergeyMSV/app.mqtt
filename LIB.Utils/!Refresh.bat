del /Q *.cpp
del /Q *.h

xcopy /Y %LIB_UTILS%\utilsExits.*
xcopy /Y %LIB_UTILS%\utilsPacketMQTT.*

pause
