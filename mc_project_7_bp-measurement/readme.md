## Exercise 7.3.0
A reading is taken from the MPRLS pressure sensor and buffered in the ``uint16_t measSample[12000]`` array but also saved to ``File32 myFile`` on the fat file system unless the *interrupt button* is pressed hich sets the ``volatile bool flagInterrupt``.  

## Exercise 7.3.1
Test code that sends the contents of the *pressureMeasurement.txt* file that is supposed to be located on the flash in a fat file system. A file with test data can be used by uploading the ``mass_storage.cpp`` code to the board which will present the external flash as a mass storage device so the test file can simply be copied over.

