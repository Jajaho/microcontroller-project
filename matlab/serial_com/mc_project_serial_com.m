% Author: Jakob Holz
% Date: 01.07.23
% Info: Script for extracting data from serial port.
% Reset microcontroller before rerunning the script.

port = serialport("COM7",9600);

NOP = 360;

PeakMeas = [1:360];

while 1 
    data = "no_data";
    data = read(port, 1000, "char");
    if data == "PeakMeas:"
        for k = 1:NOP
            PeakMeas(k) = str2double(readline(port));
            break
        end
    end
end



%while 1
%    data = realine(port);
%    if data ~= ""
%        break
%    else
        
%    end
%end

%data75 = read(port,349114,"char");