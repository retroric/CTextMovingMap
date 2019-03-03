@ECHO OFF

::
:: Set the build paremeters
::
SET OSDKADDR=$800
SET OSDKNAME=MovingMap
:: Set OISDK Compilation option (-O2=standard optimization, -O3=aggressive optimization)
SET OSDKCOMP=-O3
::
SET OSDKFILE=main
