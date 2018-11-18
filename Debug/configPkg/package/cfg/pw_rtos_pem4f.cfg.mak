# invoke SourceDir generated makefile for pw_rtos.pem4f
pw_rtos.pem4f: .libraries,pw_rtos.pem4f
.libraries,pw_rtos.pem4f: package/cfg/pw_rtos_pem4f.xdl
	$(MAKE) -f C:\Users\Julio\workspace_v8\PulseWidth_Meas/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\Julio\workspace_v8\PulseWidth_Meas/src/makefile.libs clean

