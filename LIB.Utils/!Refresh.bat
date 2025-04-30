del /Q *.cpp
del /Q *.h

xcopy /Y %LIB_UTILS%\utilsChrono.*
xcopy /Y %LIB_UTILS%\utilsException.*
xcopy /Y %LIB_UTILS%\utilsExits.*
xcopy /Y %LIB_UTILS%\utilsLog.*
xcopy /Y %LIB_UTILS%\utilsPacketMQTTv3_1_1.*
xcopy /Y %LIB_UTILS%\utilsStd.*
xcopy /Y %LIB_UTILS%\utilsTime.*

rem pause
