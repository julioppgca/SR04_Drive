# invoke SourceDir generated makefile for period_rtos.pem4f
period_rtos.pem4f: .libraries,period_rtos.pem4f
.libraries,period_rtos.pem4f: package/cfg/period_rtos_pem4f.xdl
	$(MAKE) -f C:\Users\Julio\workspace_v8\Period_Meas/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\Julio\workspace_v8\Period_Meas/src/makefile.libs clean

