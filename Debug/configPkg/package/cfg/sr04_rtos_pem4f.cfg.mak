# invoke SourceDir generated makefile for sr04_rtos.pem4f
sr04_rtos.pem4f: .libraries,sr04_rtos.pem4f
.libraries,sr04_rtos.pem4f: package/cfg/sr04_rtos_pem4f.xdl
	$(MAKE) -f C:\Users\Julio\workspace_v8\SR04_Drive/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\Julio\workspace_v8\SR04_Drive/src/makefile.libs clean

