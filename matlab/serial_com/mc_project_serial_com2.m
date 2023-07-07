% Author: Jakob Holz
% Date: 07.07.23
% Info: Script for extracting data from serial port.
% Reset microcontroller before rerunning the script.
% Second version. Based on https://makersportal.com/blog/2019/7/3/matlab-datalogger-with-arduino

% MATLAB data for reading Arduino serial prinout data
instrreset % reset all serial ports
clear all
close all
clc

NOP = 360;
msg = "";

column = 1;
row = 1;
data = zeros(4, NOP);

% ------ Serial COM establishment ------
ports = serialportlist("available");
[m,nPorts] = size(ports);
if nPorts > 1
    warning("Multiple COM ports detected. Ensure the correct one is used!")
end
port = serialport(ports(1,1), 115200);    % Select first detected COM port
disp("Connection succesful.")

write(port,"s","char");

while 1
    if port.NumBytesAvailable > 0 
        msg = readline(port, 1, "char");
        disp(msg);
    end
end